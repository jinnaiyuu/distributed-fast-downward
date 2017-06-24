#include "hdastar_search.h"

#include "globals.h"
#include "heuristic.h"
#include "option_parser.h"
#include "successor_generator.h"
#include "g_evaluator.h"
#include "sum_evaluator.h"
#include "plugin.h"
#include "per_state_information.h"

#include <cassert>
#include <cstdlib>
#include <set>
#include <climits>

#include <mpi.h>
using namespace std;

//#define TRACE_EXEC
#ifndef TRACE_EXEC
#define dbgprintf   1 ? (void) 0 : (void)
#else // #ifdef DEBUG
#define dbgprintf   printf
#endif // #ifdef DEBUG
#define MPI_MSG_NODE 0 // node
#define MPI_MSG_INCM 1 // for updating incumbent. message is unsigned int.
#define MPI_MSG_TERM 2 // for termination.message is empty.
#define MPI_MSG_FTERM 3 // broadcast this message when process terminated.
#define MPI_MSG_P_INCM 4
#define MPI_MSG_PLAN 5 // plan construction
#define MPI_MSG_PLAN_TERM 6 // plan construction
HDAStarSearch::HDAStarSearch(const Options &opts) :
		SearchEngine(opts), reopen_closed_nodes(
				opts.get<bool>("reopen_closed")), do_pathmax(
				opts.get<bool>("pathmax")), use_multi_path_dependence(
				opts.get<bool>("mpd")), open_list(
				opts.get<OpenList<StateID> *>("open")) {
	if (opts.contains("f_eval")) {
		f_evaluator = opts.get<ScalarEvaluator *>("f_eval");
	} else {
		f_evaluator = 0;
	}
	if (opts.contains("preferred")) {
		preferred_operator_heuristics = opts.get_list<Heuristic *>("preferred");
	}

	if (opts.contains("distribution")) {
//		printf("SELECTED DISTRIBUTION HASH\n");
		hash = opts.get<DistributionHash*>("distribution");
	} else {
//		printf("DEFAULT HASH\n");
		hash = new ZobristHash(opts);
	}

	if (opts.contains("threshold")) {
		threshold = opts.get<unsigned int>("threshold");
	} else {
		threshold = 0;
	}

	if (opts.contains("metis")) {
		metis = opts.get<bool>("metis");
	} else {
		metis = false;
	}

	if (opts.contains("self_send")) {
		self_send = opts.get<bool>("self_send");
	} else {
		self_send = true;
	}

	if (opts.contains("pi")) {
		calc_pi = opts.get<bool>("pi");
		pi = 3252;
	} else {
		calc_pi = false;
	}

	node_sent = 0;
	msg_sent = 0;
	term_msg_sent = 0;
	termination_counter = 0;
}

void HDAStarSearch::initialize() {

	cout << "Conducting best first search"
			<< (reopen_closed_nodes ? " with" : " without")
			<< " reopening closed nodes, (real) bound = " << bound << endl;
	if (do_pathmax)
		cout << "Using pathmax correction" << endl;
	if (use_multi_path_dependence)
		cout << "Using multi-path dependence (LM-A*)" << endl;
	assert(open_list != NULL);

	set<Heuristic *> hset;
	open_list->get_involved_heuristics(hset);

	for (set<Heuristic *>::iterator it = hset.begin(); it != hset.end(); it++) {
		estimate_heuristics.push_back(*it);
		search_progress.add_heuristic(*it);
	}

	// add heuristics that are used for preferred operators (in case they are
	// not also used in the open list)
	hset.insert(preferred_operator_heuristics.begin(),
			preferred_operator_heuristics.end());

	// add heuristics that are used in the f_evaluator. They are usually also
	// used in the open list and hence already be included, but we want to be
	// sure.
	if (f_evaluator) {
		f_evaluator->get_involved_heuristics(hset);
	}

	for (set<Heuristic *>::iterator it = hset.begin(); it != hset.end(); it++) {
		heuristics.push_back(*it);
	}

	assert(!heuristics.empty());

	const State &initial_state = g_initial_state();
	for (size_t i = 0; i < heuristics.size(); i++)
		heuristics[i]->evaluate(initial_state);
	open_list->evaluate(0, false);
	search_progress.inc_evaluated_states();
	search_progress.inc_evaluations(heuristics.size());

	///////////////////////////////
	// MPI related initialization
	///////////////////////////////
	// 1. initialize MPI
	// 2. set id, number of the nodes
	int initialized;
	MPI_Initialized(&initialized);
	if (!initialized) {
		MPI_Init(NULL, NULL);
	}

	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	printf("%d/%d processes\n", id, world_size);
	outgo_buffer.resize(world_size);

	n_vars = g_variable_domain.size(); // number of variables for a state. used to convert Node <-> bytes
	s_var = sizeof(state_var_t); // = sizeof(state_var_t)
	node_size = n_vars * s_var + 6 * sizeof(int);

	incumbent = INT_MAX;
	has_sent_first_term = false;

	income_counter = 0;
	incumbent_counter = 0;

	incumbent_goal_state = pair<unsigned int, unsigned int>(0, INT_MAX);

	track_function = false;

	g_state_registry->subscribe(&distribution_hash_value);
	g_state_registry->subscribe(&parent_node_process_id);

	printf("n_vars = %d\n", n_vars);
	printf("s_var = %d\n", s_var);
	printf("node_size = %d\n", node_size);

	// TODO: not sure we need this or Buffer_attach will do that for us.
	unsigned int buffer_size = (node_size + MPI_BSEND_OVERHEAD) * world_size
			* 100
			+ (node_size * threshold + MPI_BSEND_OVERHEAD) * world_size * 10000;


	unsigned int buffer_max = 400000000; // 400 MB TODO: not sure this is enough or too much.

//	printf("buffersize=%u\n", buffer_size,
//			buffer_size < buffer_max ? buffer_size : buffer_max);

	if (buffer_size > buffer_max) {
		buffer_size = 400000000;
	}


	mpi_buffer = new unsigned char[buffer_size];
	fill(mpi_buffer, mpi_buffer + buffer_size, 0);
	MPI_Buffer_attach((void *) mpi_buffer, buffer_size);
//	MPI_Buffer_attach(malloc(buffer_size), buffer_size - MPI_BSEND_OVERHEAD);

	unsigned int d_hash = hash->hash(initial_state);

	// Put initial state into open_list ONLY for id == 0
	if (id == (d_hash % world_size)) {
		if (open_list->is_dead_end()) {
			cout << "Initial state is a dead end." << endl;
		} else {
			printf("init state start at %d\n", id);
			search_progress.get_initial_h_values();
			if (f_evaluator) {
				f_evaluator->evaluate(0, false);
				search_progress.report_f_value(f_evaluator->get_value());
			}
			search_progress.check_h_progress(0);
			SearchNode node = search_space.get_node(initial_state);

			// PerStateInformationForInitialState
			distribution_hash_value[initial_state] = d_hash;

			parent_node_process_id[initial_state] = mpi_state_id(0, 0);

			node.open_initial(heuristics[0]->get_value());
			open_list->insert(initial_state.get_id());
		}
	}

	MPI_Barrier (MPI_COMM_WORLD);

	timer.reset();
}

void HDAStarSearch::InitMPI() {

}

void HDAStarSearch::statistics() const {

	search_progress.print_statistics();
	search_space.statistics();
	printf("Sent %u nodes.\n", node_sent);
	printf("Sent %u messages.\n", msg_sent);
	printf("Sent %u termination messages.\n", term_msg_sent);
}

int HDAStarSearch::step() {
//	++income_counter;
//	if (income_counter % 100000 == 0) {
//		printf("step %d\n", id);
//	}
//	printf("step %d\n", id);
//	MPI_Status status;
//	MPI_Request request;
	///////////////////////////////
	// Receive from Income Queue
	///////////////////////////////
//	if (id == 0) {
//		dbgprintf ("cc%d\n", 1);
//	}
	receive_nodes_from_queue();
//	if (id == 0) {
//		dbgprintf ("cc%d\n", 2);
//	}
	update_incumbent();
	///////////////////////////////
	// OPEN List open.pop()
	///////////////////////////////
//	if (id == 0) {
//		dbgprintf ("cc%d\n", 3);
//	}

	pair<SearchNode, bool> n = fetch_next_node(); // open.pop()
//	if (!n.second) {
//		return FAILED;
//	}

//	if (id == 0) {
//		dbgprintf ("cc%d\n", 4);
//	}

	if (!n.second) {
//		printf("null %d\n", id);
//		if (id == 0) {
//			printf("cc%d\n", 41);
//		}

		if (flush_outgo_buffers(0)) {
			return IN_PROGRESS;
		}
//		income_counter = INT_MAX;
//		if (id == 0) {
//			printf("cc%d\n", 42);
//		}

//		has_sent_first_term = false;
		if (termination_detection(has_sent_first_term)) {
			printf("%d terminated\n", id);
			Plan plan;
			State s = n.first.get_state();
//			search_space.trace_path(s, plan);
			set_plan(plan); // TODO: this plan is ad hoc void plan.

			for (int i = 0; i < world_size; ++i) {
				if (i != id) {
					MPI_Bsend(NULL, 0, MPI_BYTE, i, MPI_MSG_FTERM,
							MPI_COMM_WORLD);
				}
			}

			if (incumbent != INT_MAX) {
				termination();
				return SOLVED;
			} else {
				termination();
				return FAILED;
			}
		}
//		if (id == 0) {
//			printf("cc%d\n", 43);
//		}

		return IN_PROGRESS;
	}

	has_sent_first_term = false;
//	termination_counter = 0;

	SearchNode node = n.first;

//	if (id == 0) {
//		dbgprintf ("cc%d\n", 5);
//	}

	State s = node.get_state();

	///////////////////////////////
	// Check Goal
	///////////////////////////////
//	if (id == 0) {
//		dbgprintf ("cc%d\n", 6);
//	}

	if (test_goal(s)) {
		cout << id << " found a solution!: g = " << node.get_g() << " [t="
				<< g_timer << "]" << endl;

		if (node.get_g() < incumbent) {
			search_time = timer();

			incumbent = node.get_g();
			for (int i = 0; i < world_size; ++i) {
				if (i != id) {
					MPI_Bsend(&incumbent, 1, MPI_INT, i, MPI_MSG_INCM,
							MPI_COMM_WORLD);
				}
			}
			// Each process owns the shortest path they found.
			if (incumbent < incumbent_goal_state.second) {
				incumbent_goal_state = std::pair<unsigned int, int>(
						s.get_id().hash(), incumbent);
			}
		}
		Plan plan;
//		search_space.trace_path(s, plan); // TODO: need to implement trace_path for HDA*
		set_plan(plan); // TODO: this plan is ad hoc void plan.

		return IN_PROGRESS;
	}
//	if (check_goal_and_set_plan(s)) {
//		return SOLVED;
//	}
//	if (id == 0) {
//		dbgprintf ("cc%d\n", 7);
//	}

	vector<const Operator *> applicable_ops;
	set<const Operator *> preferred_ops;

	g_successor_generator->generate_applicable_ops(s, applicable_ops);
	// This evaluates the expanded state (again) to get preferred ops

//	if (id == 0) {
//		dbgprintf ("cc%d\n", 8);
//	}

	for (int i = 0; i < preferred_operator_heuristics.size(); i++) {
		Heuristic *h = preferred_operator_heuristics[i];
		h->evaluate(s);
		if (!h->is_dead_end()) {
			// In an alternation search with unreliable heuristics, it is
			// possible that this heuristic considers the state a dead end.
			vector<const Operator *> preferred;
			h->get_preferred_operators(preferred);
			preferred_ops.insert(preferred.begin(), preferred.end());
		}
	}
	search_progress.inc_evaluations(preferred_operator_heuristics.size());

//	if (id == 0) {
//		dbgprintf ("cc%d\n", 9);
//	}
//	printf("h = %lu %u\n", s.get_id().hash() + 1,
//			distribution_hash_value[s]);

	if (metis) {
		printf("ms %lu ", s.get_id().hash() + 1);
		s.dump_raw();
		printf("mh %lu %u\n", s.get_id().hash() + 1,
				distribution_hash_value[s]);
	}

	///////////////////////////////
	// Expand node
	///////////////////////////////
	for (int i = 0; i < applicable_ops.size(); i++) {
		if (calc_pi) {
			calculate_pi();
		}
		const Operator *op = applicable_ops[i];

		if ((node.get_real_g() + op->get_cost()) >= incumbent) {
			continue;
		}

		unsigned int d_hash = hash->hash_incremental(s,
				distribution_hash_value[s], op); // TODO: not sure about int <-> uint.
		unsigned int d_process = d_hash % world_size;

//		printf("%u --expd-> %u\n", distribution_hash_value[s], d_hash);

		search_progress.inc_generated();

		if (self_send || (d_process != id)) {
//			++node_sent;
			// TODO: TODO: optimize self_send later.
//		if (true) {
			if (d_process != id) {
				++node_sent;
			}
//			if (id == 0) {
//				dbgprintf ("cc%d\n", 10);
//			}

			// TODO: this allocation is not efficient
//			unsigned char* d = new unsigned char[node_size];
			unsigned int size = outgo_buffer[d_process].size();
			outgo_buffer[d_process].resize(size + node_size);

			unsigned char* p = outgo_buffer[d_process].data();
			p += size;
			// TODO: just put into outgo_buffer space rather than allocating d.
//			if (id == 0) {
//				dbgprintf ("cc%.1f\n", 10.1);
//			}
			if (generate_node_as_bytes(&node, op, p, d_hash)) {
//				if (id == 0) {
//					dbgprintf ("cc%.1f\n", 10.2);
//				}
//				unsigned int s = outgo_buffer[d_process].size();
//				outgo_buffer[d_process].resize(s + node_size);
//				std::copy(d, d + node_size, &(outgo_buffer[d_process][s]));
			} else {
//				if (id == 0) {
//					dbgprintf ("cc%.1f\n", 10.3);
//				}

				outgo_buffer[d_process].resize(size);
				// the node is >= incumbent
				// Throw away a node if its over incumbent!
//				State succ_state = g_state_registry->get_successor_state(s,
//						*op);
//				search_progress.inc_generated();
//				SearchNode succ_node = search_space.get_node(succ_state);
//				if (succ_node.is_dead_end()) {
//					continue;
//				}
			}

//			delete[] d;
		} else {
//			if (id == 0) {
//				dbgprintf ("cc%d\n", 11);
//			}
			State succ_state = g_state_registry->get_successor_state(s, *op);
//			search_progress.inc_generated();
			bool is_preferred = (preferred_ops.find(op) != preferred_ops.end());

			SearchNode succ_node = search_space.get_node(succ_state);

//			printf("metis %lu %lu %u\n", succ_state.get_id().hash() + 1,
//					s.get_id().hash() + 1, op->get_cost());

			if (metis) {
				printf("ms %lu ", succ_state.get_id().hash() + 1);
				succ_state.dump_raw();
				printf("mh %lu %u\n", succ_state.get_id().hash() + 1, d_hash);
				printf("m: %lu %lu\n", s.get_id().hash() + 1,
						succ_state.get_id().hash() + 1); // op->get_cost(): edge cost has nothing to do
			}
			// same as A*

			if (succ_node.is_new()) {
				// We have not seen this state before.
				// Evaluate and create a new node.
				for (size_t i = 0; i < heuristics.size(); i++)
					heuristics[i]->evaluate(succ_state);
				succ_node.clear_h_dirty();
				search_progress.inc_evaluated_states();
				search_progress.inc_evaluations(heuristics.size());

				// Note that we cannot use succ_node.get_g() here as the
				// node is not yet open. Furthermore, we cannot open it
				// before having checked that we're not in a dead end. The
				// division of responsibilities is a bit tricky here -- we
				// may want to refactor this later.
				open_list->evaluate(node.get_g() + get_adjusted_cost(*op),
						is_preferred);
				bool dead_end = open_list->is_dead_end();
				if (dead_end) {
					succ_node.mark_as_dead_end();
					search_progress.inc_dead_ends();
					continue;
				}

				int succ_h = heuristics[0]->get_value();

				succ_node.open(succ_h, node, op);
				distribution_hash_value[succ_state] = d_hash;
				parent_node_process_id[succ_state] = mpi_state_id(id,
						s.get_id().hash());

				open_list->insert(succ_state.get_id());
				if (search_progress.check_h_progress(succ_node.get_g())) {
					reward_progress();
				}

//				printf("\n");
//				printf("\n");
//				printf("PARENT: %lu\n", s.get_id().hash());
//				s.dump_pddl();
//				printf("\n");
//				printf("CHILD: %lu\n", succ_state.get_id().hash());
//				succ_state.dump_pddl();

			} else if (succ_node.get_g()
					> node.get_g() + get_adjusted_cost(*op)) {
				// We found a new cheapest path to an open or closed state.
				if (reopen_closed_nodes) {

					// if we reopen closed nodes, do that
					if (succ_node.is_closed()) {
						search_progress.inc_reopened();
					}
					succ_node.reopen(node, op);
					heuristics[0]->set_evaluator_value(succ_node.get_h());

					open_list->evaluate(succ_node.get_g(), is_preferred);

					open_list->insert(succ_state.get_id());
				} else {
					// hdastar always reopens closed nodes
				}
			} else {
				// pruned duplicate state
//				printf("pruned\n");
			}
		}
	}

//	if (id == 0) {
//		dbgprintf ("cc%d\n", 12);
//	}

	flush_outgo_buffers(threshold);

	return IN_PROGRESS;
}

bool HDAStarSearch::check_terminate() {
}

bool HDAStarSearch::update_incumbent() {

}

pair<SearchNode, bool> HDAStarSearch::fetch_next_node() {
	/* TODO: The bulk of this code deals with multi-path dependence,
	 which is a bit unfortunate since that is a special case that
	 makes the common case look more complicated than it would need
	 to be. We could refactor this by implementing multi-path
	 dependence as a separate search algorithm that wraps the "usual"
	 search algorithm and adds the extra processing in the desired
	 places. I think this would lead to much cleaner code. */

	while (true) {
		if (open_list->empty()) {
//			cout << "Completely explored state space -- no solution!" << endl;
			// HACK! HACK! we do this because SearchNode has no default/copy constructor
			SearchNode dummy_node = search_space.get_node(g_initial_state());
			return make_pair(dummy_node, false);
		}
		vector<int> last_key_removed;
		StateID id = open_list->remove_min(
				use_multi_path_dependence ? &last_key_removed : 0);
		State s = g_state_registry->lookup_state(id);
		SearchNode node = search_space.get_node(s);

		// The first found path is not always the optimal path in HDA*.
		if (node.get_g() + node.get_h() >= incumbent) {
//			printf("overincumbent\n");
			SearchNode dummy_node = search_space.get_node(g_initial_state());
			return make_pair(dummy_node, false);
		}

		if (node.is_closed())
			continue;

		node.close();
		assert(!node.is_dead_end());
		update_jump_statistic(node);
		search_progress.inc_expanded();
//		s.dump_raw();
		return make_pair(node, true);
	}
}

/////////////////////////////
// MPI related functions
/////////////////////////////

/**
 * To send nodes via MPI we align them as a uchar vector for efficiency.
 * In this function we generate a node and cast as a uchar vector.
 */
bool HDAStarSearch::generate_node_as_bytes(SearchNode* parent_node,
		const Operator* op, unsigned char* d, unsigned int d_hash) {
	////////////////////////////
	// State
	////////////////////////////
//	if (id == 0) {
//		dbgprintf ("cc%.2f\n", 10.11);
//	}
	State parent_s = parent_node->get_state();

//	if (id == 0) {
//		dbgprintf ("cc%.2f\n", 10.12);
//	}

	State s = g_state_registry->get_successor_state_by_dummy(parent_s, *op);

//    printf("\n");
//    printf("\n");
//    printf("PARENT: %lu\n", parent_s.get_id().hash());
//    parent_s.dump_pddl();
//    printf("\n");
//    printf("CHILD: %lu\n", s.get_id().hash());
//    s.dump_pddl();

//	(void *) child;

//	State s = g_state_registry->get_successor_state(parent_s, *op);

//	if (id == 0) {
//		dbgprintf ("cc%.2f\n", 10.13);
//	}

	// First check if its f value is over/equal incumbent.
	int g = parent_node->get_g() + get_adjusted_action_cost(*op, cost_type);
	for (size_t i = 0; i < heuristics.size(); i++) {
		heuristics[i]->evaluate(s);
	}
	int h = heuristics[0]->get_value();
	if (g + h >= incumbent) {
//		g_state_registry->reset_dummy_state();
		return false;
	}
	search_progress.inc_evaluated_states();
	search_progress.inc_evaluations(heuristics.size());

	// what we need for the state
	// 1. state_var_t*: can implement
	// 2. heuristics[i]->evaluate(s): need to pass State
	// 3. thats all!

	// 1. get buffer from parent
	// 2. copy to new buffer
	// 3. build state from the new buffer
	// 4. calculate heuristic
	// 5.

//	if (id == 0) {
//		dbgprintf ("cc%.2f\n", 10.14);
//	}

	memcpy(d, s.get_raw_data(), n_vars * s_var);
//	for (int i = 0; i < n_vars; ++i) {
//		state_var_t si = s[i];
//		typeToBytes(si, &(d[s_var * i]));
//	}

	////////////////////////////
	// SearchNodeInfo
	////////////////////////////
	// Other than the state, we are going to send informations below
	// 1. g
	// 2. h
	// 3. op_index
	// 4. distribution_hash_value
	// 5. parent_node_processer_id

//	typeToBytes(g, d + n_vars * s_var);
//	typeToBytes(h, d + n_vars * s_var + sizeof(int));

//	if (id == 0) {
//		dbgprintf ("cc%.2f\n", 10.15);
//	}

	int op_index = op - &*g_operators.begin(); // index is universal at all processes
//	typeToBytes(op_index, &(d[n_vars * s_var + 2 * sizeof(int)]));
//	typeToBytes(d_hash, &(d[n_vars * s_var + 3 * sizeof(int)]));

//	printf("pre = %d, post = %d\n", distribution_hash_value[parent_s], d_hash);

//	typeToBytes(id, &(d[n_vars * s_var + 4 * sizeof(int)]));
	int state_id = parent_s.get_id().hash();

//	typeToBytes(state_id, &(d[n_vars * s_var + 5 * sizeof(int)]));

	int info[6];
	info[0] = g;
	info[1] = h;
	info[2] = op_index;
	info[3] = d_hash;
	info[4] = id;
	info[5] = state_id;
	memcpy(d + n_vars * s_var, info, 6 * sizeof(int));

//	printf("d_hash = %x -> %x\n", d_hash, info[3]);
//	printf("d_hash = %u -> %d\n", d_hash, info[3]);

//	if (id == 0) {src
//		dbgprintf ("cc%.2f\n", 10.16);
//	}

//	g_state_registry->reset_dummy_state();

//	printf("id %d send to id %d: state:", id, d_hash % world_size);
//	for (int i = 0; i < n_vars; ++i) {
//		printf(" %d", s[i]);
//	}
//	printf(", g %d, h %d, op %2d, d_hash %-11u, ppid %d, psid %d\n", g, h, op_index,
//			d_hash, id, state_id);
//	if (id == 0) {
//		dbgprintf ("cc%.2f\n", 10.17);
//	}

	return true;
}

void HDAStarSearch::receive_nodes_from_queue() {
	// 1. take messages and compile them to nodes
	// 2. put each node a NodeID and register
	// 3. add NodeID to the OpenList
	// In this way we get a new nodes

	MPI_Status status;
	int has_received = 0;

//	if (income_counter < 0) {
//		++income_counter;
//		return;
//	}
//	income_counter = 0;

	MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_NODE, MPI_COMM_WORLD, &has_received,
			&status);
	while (has_received) {
		int d_size = 0;
		int source = status.MPI_SOURCE;
		MPI_Get_count(&status, MPI_BYTE, &d_size); // TODO: = node_size?

		// TODO: this buffer does not need to be allocated every time.
		//       put it as a static vector?
		unsigned char *d = new unsigned char[d_size];
		MPI_Recv(d, d_size, MPI_BYTE, source, MPI_MSG_NODE, MPI_COMM_WORLD,
				MPI_STATUS_IGNORE);

//		std::vector<SearchNode> nodes;
//		bytes_to_node(&nodes, d, d_size);
//		printf("received node %d\n", id);
		bytes_to_nodes(d, d_size);

		delete[] d;

		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_NODE, MPI_COMM_WORLD, &has_received,
				&status);
	}
//	++income_counter;
}

void HDAStarSearch::bytes_to_nodes(unsigned char* d, unsigned int d_size) {
	unsigned int n_nodes = d_size / node_size;
	for (int i = 0; i < n_nodes; ++i) {
		bytes_to_node(&(d[node_size * i]));
	}
//	printf("%d recieved %d nodes\n", id, n_nodes);
}

StateID HDAStarSearch::bytes_to_node(unsigned char* d) {
	state_var_t* vars = new state_var_t[n_vars];

//	printf("income d: ");
//	for (int i = 0; i < n_vars * s_var; ++i) {
//		printf("%d ", d[i]);
//	}
//	printf("\n");

	memcpy(vars, d, n_vars * s_var);

//	for (int i = 0; i < n_vars; ++i) {
//		bytesToType(vars[i], &(d[i * s_var]));
//	}

	// g -> h -> op_index
	int g, h, op_index;
	unsigned int d_hash, parent_process_id, parent_state_id;
//	bytesToType(g, &(d[n_vars * s_var]));
//	bytesToType(h, &(d[n_vars * s_var + sizeof(int)]));
//	bytesToType(op_index, &(d[n_vars * s_var + 2 * sizeof(int)]));
//	bytesToType(d_hash, &(d[n_vars * s_var + 3 * sizeof(int)]));
//	bytesToType(parent_process_id, &(d[n_vars * s_var + 4 * sizeof(int)]));
//	bytesToType(parent_state_id, &(d[n_vars * s_var + 5 * sizeof(int)]));

	int info[6];
	memcpy(info, d + n_vars * s_var, 6 * sizeof(int));
	g = info[0];
	h = info[1];
	op_index = info[2];
	d_hash = info[3]; // TODO: this sounds strange for me.
	parent_process_id = info[4];
	parent_state_id = info[5];

//	printf("%x -> %x = d_hash\n", info[3], d_hash);
//	printf("%d -> %u = d_hash\n", d_hash, info[3]);

	Operator *op = &(g_operators[op_index]);

//	printf("g,h = %d,%d\n", g,h);
//	printf("op = %d\n", op_index);
//	printf("d_hash = %u\n", d_hash);
//	printf("parent = %d,%d\n", parent_process_id, parent_state_id);

//	printf("id = %d, vars = ", id);
//	for (int i = 0; i < n_vars; ++i) {
//		printf("%d ", vars[i]);
//	}
//	printf(", op = %d", op_index);
//	printf("\n");

	// May need to build a node from zero.
//	State succ_state = g_state_registry->get_successor_state(s, *op);
	State succ_state = g_state_registry->build_state(vars);
//	search_progress.inc_generated();
//	bool is_preferred = (preferred_ops.find(op) != preferred_ops.end());

	SearchNode succ_node = search_space.get_node(succ_state);

//	printf("id %d recv fr id %d: state:", id, parent_process_id);
//	for (int i = 0; i < n_vars; ++i) {
//		printf(" %d", succ_state[i]);
//	}
//	printf(", g %d, h %d, op %2d, d_hash %-11u, ppid %d, psid %d\n", g, h, op_index,
//			d_hash, parent_process_id, parent_state_id);

	// Previously encountered dead end. Don't re-evaluate.
	if (succ_node.is_dead_end())
		return StateID::no_state;

	if (succ_node.is_new()) {
//	if (true) {
		distribution_hash_value[succ_state] = d_hash;
		parent_node_process_id[succ_state] = mpi_state_id(parent_process_id,
				parent_state_id);

		heuristics[0]->set_evaluator_value(h);
		succ_node.clear_h_dirty();

		open_list->evaluate(g, false);
		bool dead_end = open_list->is_dead_end();
		if (dead_end) {
			succ_node.mark_as_dead_end();
			search_progress.inc_dead_ends();
			return StateID::no_state;
		}

		succ_node.open(g, h, op);

//		succ_node.dump();

		open_list->insert(succ_state.get_id());

		if (search_progress.check_h_progress(succ_node.get_g())) {
			reward_progress();
		}
	} else if (succ_node.get_g() > g) {
		parent_node_process_id[succ_state] = mpi_state_id(parent_process_id,
				parent_state_id);

		if (succ_node.is_closed()) {
			search_progress.inc_reopened();
		}
		// TODO: need to reopen nodes
		succ_node.reopen(g, h, op);
		heuristics[0]->set_evaluator_value(h);
		// TODO: this appears fishy to me. Why is here only heuristic[0]
		// involved? Is this still feasible in the current version?
		open_list->evaluate(g, false);
		open_list->insert(succ_state.get_id());
//		printf("reopened\n");
	} else {
//		printf("pruned\n");
	}
	return succ_state.get_id();
}

// Mattern, Algorithm for distributed termination detection, 1987
// TODO: I decided to implement simple two-round algorithm, as it won't be the bottleneck.
//       It is correct, but not efficient
bool HDAStarSearch::termination_detection(bool& has_sent_first_term) {
	MPI_Status status;
	int has_received = 0;
//	printf("%d termination detection\n", id);

//	if (id == 0) {
//		printf("cc%d\n", 421);
//	}

//	if (income_counter % 100000 == 0) {
//		printf("id %d: termination detection\n", id);
//	}

	MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_FTERM, MPI_COMM_WORLD, &has_received,
			MPI_STATUS_IGNORE);
	if (has_received) {
		printf("received fterm\n");
		return true;
	}

//	if (id == 0) {
//		printf("cc%d\n", 422);
//		printf("%d\n", (id - 1) % world_size);
//		printf("%d\n", (id + world_size - 1) % world_size);
//
//	}

	//	if (id == 0 && !has_sent_first_term) {

	has_received = 0;
	MPI_Iprobe((id + world_size - 1) % world_size, MPI_MSG_TERM, MPI_COMM_WORLD,
			&has_received, &status);
	if (has_received) {
//		if (id == 0) {
//			printf("cc%d\n", 423);
//		}

		unsigned char term = 0;
		unsigned char term2 = 0;
		MPI_Recv(&term, 1, MPI_BYTE, (id + world_size - 1) % world_size,
		MPI_MSG_TERM, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//		printf("%d recieved message %d\n", id, term);

		has_received = 0;

		MPI_Iprobe((id + world_size - 1) % world_size, MPI_MSG_TERM,
				MPI_COMM_WORLD, &has_received, &status);

//		if (id == 0) {
//			printf("cc%d\n", 423);
//		}

		// This while loop is here to flush all messages
		while (has_received) {
			MPI_Recv(&term2, 1, MPI_BYTE, (id + world_size - 1) % world_size,
			MPI_MSG_TERM, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			term = (term > term2 ? term : term2);
			has_received = 0;
			MPI_Iprobe((id + world_size - 1) % world_size, MPI_MSG_TERM,
					MPI_COMM_WORLD, &has_received, &status);
		}

		if (term > 2) {
//			printf("%d terminates..\n", id);
			MPI_Bsend(&term, 1, MPI_BYTE, (id + 1) % world_size, MPI_MSG_TERM,
					MPI_COMM_WORLD);
			return true;
		}

		// here term is the received message.
		// if term == terminate_count ->     send increment
		// if term >  terminate_count + 1 -> send terminate_count
		// if term <  terminate_count ->     send term

		if (term > termination_counter + 1) {
			term = termination_counter + 1;
		}

		termination_counter = term;

		if (id == 0) {
			++term;
		}

//		printf("%d received term %d\n", id, term);
//		printf("%d sent message %d\n", id, term);
		++term_msg_sent;
		MPI_Bsend(&term, 1, MPI_BYTE, (id + 1) % world_size, MPI_MSG_TERM,
				MPI_COMM_WORLD);
		if (term > 2) {
			return true;
		}
	} else {
//		printf("%d received no term messages\n", id);
//		termination_counter = 0;
	}
//	if (id == 0) {
//		printf("cc%d\n", 424);
//	}

	if (id == 0 && !has_sent_first_term) {
//	if (id == 0) {
		if (income_counter >= 100) {
//		printf("%d termination detection\n", id);
			unsigned char term = 1;
			MPI_Bsend(&term, 1, MPI_BYTE, (id + 1) % world_size, MPI_MSG_TERM,
					MPI_COMM_WORLD);
//		printf("sent first term %d to %d\n", term, (id + 1) % world_size);
			has_sent_first_term = true;
			income_counter = 0;
			++term_msg_sent;
		} else {
		}
		++income_counter;
//		sleep(2);
	}

	return false;
}

void HDAStarSearch::update_incumbent() {
	MPI_Status status;

//	int incumb_count_max = (incumbent == INT_MAX) ? 1000 : 20;
//
//	if (incumbent_counter < incumb_count_max) {
//		++incumbent_counter;
//		return;
//	}
//	incumbent_counter = 0;

	int has_received = 0;
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_INCM, MPI_COMM_WORLD, &has_received,
			&status);

	if (has_received) {
		int newincm = 0;
		int source = status.MPI_SOURCE;
		MPI_Recv(&newincm, 1, MPI_INT, source, MPI_MSG_INCM, MPI_COMM_WORLD,
				MPI_STATUS_IGNORE);
		if (incumbent > newincm) {
			incumbent = newincm;
		}
		printf("id %d: incumbent = %d [t=%.2f]\n", id, incumbent, g_timer());
		search_time = timer();

	}
//	++incumbent_counter;
}

bool HDAStarSearch::flush_outgo_buffers(int f_threshold) {
	bool flushed = false;
	for (int i = 0; i < world_size; i++) {
		if (self_send || i != id) {
//		if (true) {
			if (outgo_buffer[i].size() > f_threshold * node_size) {
				unsigned char* d = outgo_buffer[i].data();
//				printf("send..");
				MPI_Bsend(d, outgo_buffer[i].size(), MPI_BYTE, i, MPI_MSG_NODE,
						MPI_COMM_WORLD);
//				printf("done!\n");

//				printf("%d sent %lu nodes to %d\n", id,
//						outgo_buffer[i].size() / node_size, i);
				++msg_sent;

				outgo_buffer[i].clear(); // no need to delete d
				flushed = true;
			}
		}
	}
	return flushed;
}

int HDAStarSearch::termination() {

	printf("Actual search wall time: %.2f [t=%.2f]\n", search_time, g_timer());

	printf("barrier %d\n", id);
	MPI_Barrier (MPI_COMM_WORLD);

	construct_plan();

	/**
	 * TODO: Flushing message is NOT REQUIRED for many cases.
	 * It is to guarantee that MPI processes do not become zombies especially for MPICH3.
	 */
	MPI_Status status;
	printf("Flushing all incoming messages... ");

	int has_received = 1;
	unsigned char* dummy = new unsigned char[node_size * 10000];
	while (has_received) {
		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_FTERM, MPI_COMM_WORLD, &has_received,
				MPI_STATUS_IGNORE);
		if (has_received) {
			MPI_Recv(dummy, 0, MPI_BYTE, MPI_ANY_SOURCE, MPI_MSG_FTERM,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}

	printf("FTERM... ");

	has_received = 1;
	while (has_received) {
		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_INCM, MPI_COMM_WORLD, &has_received,
				MPI_STATUS_IGNORE);
		if (has_received) {
			MPI_Recv(dummy, 1, MPI_INT, MPI_ANY_SOURCE, MPI_MSG_INCM,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}
	printf("INCM... ");

	has_received = 1;
	while (has_received) {
		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_NODE, MPI_COMM_WORLD, &has_received,
				&status);
		if (has_received) {
			int source = status.MPI_SOURCE;
			int d_size;
			MPI_Get_count(&status, MPI_BYTE, &d_size); // TODO: = node_size?
			MPI_Recv(dummy, d_size, MPI_BYTE, source, MPI_MSG_NODE,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}
	printf("NODE... ");

	has_received = 1;
	while (has_received) {
		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_TERM, MPI_COMM_WORLD, &has_received,
				MPI_STATUS_IGNORE);
		if (has_received) {
			MPI_Recv(dummy, 1, MPI_INT, MPI_ANY_SOURCE, MPI_MSG_TERM,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}
	printf("TERM... ");

	has_received = 1;
	while (has_received) {
		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_PLAN, MPI_COMM_WORLD, &has_received,
				MPI_STATUS_IGNORE);
		if (has_received) {
			int source = status.MPI_SOURCE;
			int d_size;
			MPI_Get_count(&status, MPI_INT, &d_size); // TODO: = node_size?
			MPI_Recv(dummy, d_size, MPI_INT, source, MPI_MSG_PLAN,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}
	printf("PLAN... ");

	has_received = 1;
	while (has_received) {
		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_PLAN_TERM, MPI_COMM_WORLD,
				&has_received, MPI_STATUS_IGNORE);
		if (has_received) {
			MPI_Recv(dummy, 0, MPI_BYTE, MPI_ANY_SOURCE, MPI_MSG_PLAN_TERM,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}

	printf("PLAN_TERM... ");

	printf("done!\n");
	printf("Buffer_detach %d\n", id);

	int buffer_size;
	MPI_Buffer_detach(&mpi_buffer, &buffer_size);
//
	delete[] mpi_buffer;

	printf("finalize %d\n", id);

//	for (int i = 0; i < )

	MPI_Finalize();
	printf("finalize done%d\n", id);
	printf("pi = %d\n", pi);

	return 0;
}

void HDAStarSearch::construct_plan() {
	if (world_size == 1) {
		return;
	}

	/*
	 * message: [stateID(k)] [op(k)] ... [op(n-1)] [op(n)]
	 *
	 */
	if (incumbent == incumbent_goal_state.second) {
		int gid = incumbent_goal_state.first;
		State s = g_state_registry->lookup_state(gid);
		SearchNode s_node = search_space.get_node(s);
		int op_index = s_node.get_creating_op_index();
		int p[2];
		p[1] = op_index;

		std::pair<unsigned int, unsigned int> parentid =
				parent_node_process_id[s];
		p[0] = parentid.second;

//		printf("stateid=%d: op=%d\n", p[0], p[1]);

		MPI_Bsend(p, 2, MPI_INT, parentid.first, MPI_MSG_PLAN, MPI_COMM_WORLD);
	}

	MPI_Status status;
	int has_received = 0;
	int* pln;
	while (true) {
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_PLAN, MPI_COMM_WORLD, &has_received,
				&status);
		if (has_received) {
			int size;
			MPI_Get_count(&status, MPI_INT, &size);
			int source = status.MPI_SOURCE;
			pln = new int[size + 1];
			MPI_Recv(&(pln[1]), size, MPI_INT, source, MPI_MSG_PLAN,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			int stateid = pln[1];
//			printf("stateid=%d: op=", stateid);
//			for (int i = 0; i < size - 1; ++i) {
//				printf("%d ", pln[2 + i]);
//			}
//			printf("\n");

			if (stateid == g_initial_state().get_id().hash()) {
				Plan pplan;
				for (int i = 0; i < size - 1; ++i) {
					const Operator* op = &g_operators[pln[2 + i]];
					pplan.push_back(op);
				}
				set_plan(pplan);
//				printf("done: ");
//				for (int i = 0; i < size - 1; ++i) {
//					printf("%d ", pln[2 + i]);
//				}
//				printf("\n");
				for (int i = 0; i < world_size; ++i) {
					if (i != id) {
						MPI_Bsend(NULL, 0, MPI_BYTE, i, MPI_MSG_PLAN_TERM,
								MPI_COMM_WORLD);
					}
				}
				delete[] pln;
				return;
			}

			State s = g_state_registry->lookup_state(stateid);
//			s.dump_pddl();
			SearchNode s_node = search_space.get_node(s);
			int op_index = s_node.get_creating_op_index();
			pln[1] = op_index;

			std::pair<unsigned int, unsigned int> parentid =
					parent_node_process_id[s];
			pln[0] = parentid.second;

			MPI_Bsend(pln, size + 1, MPI_INT, parentid.first, MPI_MSG_PLAN,
					MPI_COMM_WORLD);
			delete[] pln;
		}

		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_PLAN_TERM, MPI_COMM_WORLD,
				&has_received, &status);
		if (has_received) {
			int source = status.MPI_SOURCE;
			MPI_Recv(NULL, 0, MPI_BYTE, source, MPI_MSG_PLAN_TERM,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			return;
		}
	}
}

template<typename T>
void HDAStarSearch::typeToBytes(T& p, unsigned char* d) const {
	int n = sizeof p;
	for (int y = 0; n-- > 0; y++) {
		d[y] = (p >> (n * 8)) & 0xff;
	}
}

template<typename T>
void HDAStarSearch::bytesToType(T& p, unsigned char* d) const {
	int n = sizeof p;
	p = 0;
	for (int i = 0; i < n; ++i) {
		p += (T) d[i] << (8 * (n - i - 1));
	}
}

void HDAStarSearch::calculate_pi() {
	for (int i = 0; i < 10000000; ++i) {
		pi *= 4 + 4223.15;
		pi /= 3;
		pi *= pi;
		pi %= 13424;
	}
}

void HDAStarSearch::reward_progress() {
	// Boost the "preferred operator" open lists somewhat whenever
	// one of the heuristics finds a state with a new best h value.
	open_list->boost_preferred();
}

void HDAStarSearch::dump_search_space() {
	search_space.dump();
}

void HDAStarSearch::update_jump_statistic(const SearchNode &node) {
	if (f_evaluator) {
		heuristics[0]->set_evaluator_value(node.get_h());
		f_evaluator->evaluate(node.get_g(), false);
		int new_f_value = f_evaluator->get_value();
		search_progress.report_f_value(new_f_value);
//		printf("id %d: f = %d\n", id, new_f_value);
	}
}

void HDAStarSearch::print_heuristic_values(const vector<int> &values) const {
	for (int i = 0; i < values.size(); i++) {
		cout << values[i];
		if (i != values.size() - 1)
			cout << "/";
	}
}

static SearchEngine *_parse_hdastar(OptionParser &parser) {
	parser.document_synopsis("HDA* search (eager)",
			"A* is a special case of eager best first search that uses g+h "
					"as f-function. "
					"We break ties using the evaluator. Closed nodes are re-opened.");
	parser.document_note("mpd option",
			"This option is currently only present for the A* algorithm and not "
					"for the more general eager search, "
					"because the current implementation of multi-path depedence "
					"does not support general open lists.");
	parser.document_note("Equivalent statements using general eager search",
			"\n```\n--search astar(evaluator)\n```\n"
					"is equivalent to\n"
					"```\n--heuristic h=evaluator\n"
					"--search eager(tiebreaking([sum([g(), h]), h], unsafe_pruning=false),\n"
					"               reopen_closed=true, pathmax=false, progress_evaluator=sum([g(), h]))\n"
					"```\n", true);
	parser.add_option<ScalarEvaluator *>("eval", "evaluator for h-value");

	parser.add_option<DistributionHash *>("distribution",
			"distribution function for hdastar", "zobrist");

	parser.add_option<unsigned int>("threshold",
			"The number of nodes to queue up in the local outgo_buffer.", "0");

	parser.add_option<bool>("metis", "print lines for sparsity analysis",
			"false");

	parser.add_option<bool>("self_send",
			"If true, processes use MPI to send node to myself.", "false");

	parser.add_option<bool>("pi",
			"Add needless calculation to slow down node expansion.", "false");

	parser.add_option<bool>("pathmax", "use pathmax correction", "false");
	parser.add_option<bool>("mpd", "use multi-path dependence (LM-A*)",
			"false");

	// HDA* related options
	parser.document_note("dist option",
			"Distribution method for HDA* is core to its performance.\n"
					"dist option selects which hash function to use for distribution.");

	SearchEngine::add_options_to_parser(parser);
	Options opts = parser.parse();

	HDAStarSearch *engine = 0;
	if (!parser.dry_run()) {
		GEvaluator *g = new GEvaluator();
		vector<ScalarEvaluator *> sum_evals;
		sum_evals.push_back(g);
		ScalarEvaluator *eval = opts.get<ScalarEvaluator *>("eval");
		sum_evals.push_back(eval);
		ScalarEvaluator *f_eval = new SumEvaluator(sum_evals);

		// use eval for tiebreaking
		std::vector<ScalarEvaluator *> evals;
		evals.push_back(f_eval);
		evals.push_back(eval);
		OpenList<StateID> *open = new TieBreakingOpenList<StateID>(evals, false,
				false);

		opts.set("open", open);
		opts.set("f_eval", f_eval);
		opts.set("reopen_closed", true);
		engine = new HDAStarSearch(opts);
	}

	return engine;
}

static Plugin<SearchEngine> _plugin_hdastar("hdastar", _parse_hdastar);

