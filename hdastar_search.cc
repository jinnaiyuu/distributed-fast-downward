#include "hdastar_search.h"

#include "globals.h"
#include "heuristic.h"
#include "option_parser.h"
#include "successor_generator.h"
#include "g_evaluator.h"
#include "sum_evaluator.h"
#include "plugin.h"

#include <cassert>
#include <cstdlib>
#include <set>

#include <mpi/mpi.h>
using namespace std;

#define MPI_MSG_NODE 0 // node
#define MPI_MSG_INCM 1 // for updating incumbent. message is unsigned int.
#define MPI_MSG_TERM 2 // for termination.message is empty.
#define MPI_MSG_FTERM 3 // broadcast this message when process terminated.
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

	if (open_list->is_dead_end()) {
		cout << "Initial state is a dead end." << endl;
	} else {
		search_progress.get_initial_h_values();
		if (f_evaluator) {
			f_evaluator->evaluate(0, false);
			search_progress.report_f_value(f_evaluator->get_value());
		}
		search_progress.check_h_progress(0);
		SearchNode node = search_space.get_node(initial_state);
		node.open_initial(heuristics[0]->get_value());

		open_list->insert(initial_state.get_id());
	}

	///////////////////////////////
	// MPI related initialization
	///////////////////////////////
	// 1. initialize MPI
	// 2. set id, number of the nodes
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	outgo_buffer.resize(world_size);

	n_vars = g_variable_domain.size(); // number of variables for a state. used to convert Node <-> bytes
	s_var = sizeof(state_var_t); // = sizeof(state_var_t)
	node_size = n_vars * s_var + 3 * sizeof(int); // TODO: tentative to change as implementation progress!

	printf("n_vars = %d\n", n_vars);
	printf("s_var = %d\n", s_var);
	printf("node_size = %d\n", node_size);

}

void HDAStarSearch::statistics() const {

	// HACK! HACK!
	// We have to finalize MPI but could not find a proper finalizing method in searchengine.
	// As this method is always called, im gonna put finalizing methods here.
	MPI_Finalize();

	search_progress.print_statistics();
	search_space.statistics();
}

int HDAStarSearch::step() {
//	MPI_Status status;
//	MPI_Request request;
	///////////////////////////////
	// Receive from Income Queue
	///////////////////////////////
	// TODO: Implement MPI_Recv() to receive nodes from income queue.
	receive_nodes_from_queue();

	///////////////////////////////
	// OPEN List open.pop()
	///////////////////////////////
	pair<SearchNode, bool> n = fetch_next_node(); // open.pop()
//	if (!n.second) {
//		return FAILED;
//	}

	if (!n.second) {
		flush_outgo_buffers(0);
		bool has_sent_first_term = false;
		if (termination_detection(has_sent_first_term)) {
			printf("%d terminated\n", id);
			return SOLVED; // TODO: might mean FAILED
		}
		return IN_PROGRESS;
	}

	SearchNode node = n.first;

	State s = node.get_state();

	///////////////////////////////
	// Check Goal
	///////////////////////////////
	if (test_goal(s)) {
		// TODO: set incumbent cost
		cout << "Solution found!: g = " << node.get_g() << endl;
//		unsigned int incumbent = node.get_g();
		for (int i = 0; i < world_size; ++i) {
			MPI_Send(NULL, 0, MPI_BYTE, i, MPI_MSG_FTERM, MPI_COMM_WORLD);
		}
		return SOLVED; // TODO: might mean FAILED
	}
//	if (check_goal_and_set_plan(s)) {
//		return SOLVED;
//	}

	vector<const Operator *> applicable_ops;
	set<const Operator *> preferred_ops;

	g_successor_generator->generate_applicable_ops(s, applicable_ops);
	// This evaluates the expanded state (again) to get preferred ops
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

	///////////////////////////////
	// Expand node
	///////////////////////////////
	for (int i = 0; i < applicable_ops.size(); i++) {
		const Operator *op = applicable_ops[i];

		if ((node.get_real_g() + op->get_cost()) >= bound)
			continue;

		unsigned char* d = new unsigned char[node_size];
		generate_node_as_bytes(&node, op, d);

		// TODO: Zobrist Hash can be implemented as incremental hashing.
		// maybe
		MPI_Send(d, node_size, MPI_BYTE, (id + 1) % world_size, MPI_MSG_NODE,
				MPI_COMM_WORLD);

//		printf("send node from %d to %d\n", id, (id + 1) % world_size);
//		node.dump();

		////////////////////////////////////
		// TODO: operations below should be run at receiving process
		////////////////////////////////////

//		State succ_state = g_state_registry->get_successor_state(s, *op);
//		search_progress.inc_generated();
//		bool is_preferred = (preferred_ops.find(op) != preferred_ops.end());
//
//		SearchNode succ_node = search_space.get_node(succ_state);
//
//		// Previously encountered dead end. Don't re-evaluate.
//		if (succ_node.is_dead_end())
//			continue;
//
//		// update new path
//		if (use_multi_path_dependence || succ_node.is_new()) {
//			bool h_is_dirty = false;
//			for (size_t i = 0; i < heuristics.size(); ++i) {
//				/*
//				 Note that we can't break out of the loop when
//				 h_is_dirty is set to true or use short-circuit
//				 evaluation here. We must call reach_state for each
//				 heuristic for its side effects.
//				 */
//				if (heuristics[i]->reach_state(s, *op, succ_state))
//					h_is_dirty = true;
//			}
//			if (h_is_dirty && use_multi_path_dependence)
//				succ_node.set_h_dirty();
//		}
//
//		///////////////////////////////
//		// Push new nodes to a MPI_Send
//		///////////////////////////////
//		// TODO: Implement MPI
//
//		if (succ_node.is_new()) {
//			// We have not seen this state before.
//			// Evaluate and create a new node.
//			for (size_t i = 0; i < heuristics.size(); i++)
//				heuristics[i]->evaluate(succ_state);
//			succ_node.clear_h_dirty();
//			search_progress.inc_evaluated_states();
//			search_progress.inc_evaluations(heuristics.size());
//
//			// Note that we cannot use succ_node.get_g() here as the
//			// node is not yet open. Furthermore, we cannot open it
//			// before having checked that we're not in a dead end. The
//			// division of responsibilities is a bit tricky here -- we
//			// may want to refactor this later.
//			open_list->evaluate(node.get_g() + get_adjusted_cost(*op),
//					is_preferred);
//			bool dead_end = open_list->is_dead_end();
//			if (dead_end) {
//				succ_node.mark_as_dead_end();
//				search_progress.inc_dead_ends();
//				continue;
//			}
//
//			//TODO:CR - add an ID to each state, and then we can use a vector to save per-state information
//			int succ_h = heuristics[0]->get_heuristic();
//			if (do_pathmax) {
//				if ((node.get_h() - get_adjusted_cost(*op)) > succ_h) {
//					//cout << "Pathmax correction: " << succ_h << " -> " << node.get_h() - get_adjusted_cost(*op) << endl;
//					succ_h = node.get_h() - get_adjusted_cost(*op);
//					heuristics[0]->set_evaluator_value(succ_h);
//					open_list->evaluate(node.get_g() + get_adjusted_cost(*op),
//							is_preferred);
//					search_progress.inc_pathmax_corrections();
//				}
//			}
//			succ_node.open(succ_h, node, op);
//
//			///////////////////////////////
//			// Push new nodes to a MPI_Send
//			///////////////////////////////
//			// TODO: Implement MPI
//
//			open_list->insert(succ_state.get_id());
//			if (search_progress.check_h_progress(succ_node.get_g())) {
//				reward_progress();
//			}
//		} else if (succ_node.get_g() > node.get_g() + get_adjusted_cost(*op)) {
//			// We found a new cheapest path to an open or closed state.
//			if (reopen_closed_nodes) {
//				//TODO:CR - test if we should add a reevaluate flag and if it helps
//				// if we reopen closed nodes, do that
//				if (succ_node.is_closed()) {
//					/* TODO: Verify that the heuristic is inconsistent.
//					 * Otherwise, this is a bug. This is a serious
//					 * assertion because it can show that a heuristic that
//					 * was thought to be consistent isn't. Therefore, it
//					 * should be present also in release builds, so don't
//					 * use a plain assert. */
//					//TODO:CR - add a consistent flag to heuristics, and add an assert here based on it
//					search_progress.inc_reopened();
//				}
//				succ_node.reopen(node, op);
//				heuristics[0]->set_evaluator_value(succ_node.get_h());
//				// TODO: this appears fishy to me. Why is here only heuristic[0]
//				// involved? Is this still feasible in the current version?
//				open_list->evaluate(succ_node.get_g(), is_preferred);
//
//				open_list->insert(succ_state.get_id());
//			} else {
//				// if we do not reopen closed nodes, we just update the parent pointers
//				// Note that this could cause an incompatibility between
//				// the g-value and the actual path that is traced back
//				succ_node.update_parent(node, op);
//			}
//		}
	}

	return IN_PROGRESS;
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
		// TODO is there a way we can avoid creating the state here and then
		//      recreate it outside of this function with node.get_state()?
		//      One way would be to store State objects inside SearchNodes
		//      instead of StateIDs
		State s = g_state_registry->lookup_state(id);
		SearchNode node = search_space.get_node(s);

		if (node.is_closed())
			continue;

		if (use_multi_path_dependence) {
			assert(last_key_removed.size() == 2);
			int pushed_h = last_key_removed[1];
			assert(node.get_h() >= pushed_h);
			if (node.get_h() > pushed_h) {
				// cout << "LM-A* skip h" << endl;
				continue;
			}
			assert(node.get_h() == pushed_h);
			if (!node.is_closed() && node.is_h_dirty()) {
				for (size_t i = 0; i < heuristics.size(); i++)
					heuristics[i]->evaluate(node.get_state());
				node.clear_h_dirty();
				search_progress.inc_evaluations(heuristics.size());

				open_list->evaluate(node.get_g(), false);
				bool dead_end = open_list->is_dead_end();
				if (dead_end) {
					node.mark_as_dead_end();
					search_progress.inc_dead_ends();
					continue;
				}
				int new_h = heuristics[0]->get_heuristic();
				if (new_h > node.get_h()) {
					assert(node.is_open());
					node.increase_h(new_h);
					open_list->insert(node.get_state_id());
					continue;
				}
			}
		}

		node.close();
		assert(!node.is_dead_end());
		update_jump_statistic(node);
		search_progress.inc_expanded();
		return make_pair(node, true);
	}
}

/////////////////////////////
// MPI related functions
/////////////////////////////

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

// TODO: what will be the method to compile nodes into bytes and back?
/*
 * SearchNode
 *  StateID
 *   State!
 *   PerStateInfo?   (TODO: ignore this for now)
 *  SearchNodeInfo!
 *  OperatorCost?    (TODO: ignore this for now)
 *
 */
void HDAStarSearch::generate_node_as_bytes(SearchNode* parent_node,
		const Operator* op, unsigned char* d) {
	////////////////////////////
	// State
	////////////////////////////
	State s = parent_node->get_state();

	for (int i = 0; i < n_vars; ++i) {
		state_var_t si = s[i];
		typeToBytes(si, &(d[s_var * i]));
	}
////////////////////////////
// SearchNodeInfo
////////////////////////////
	int g = parent_node->get_g();
	int h = parent_node->get_h();

	typeToBytes(g, d + n_vars * s_var);
	typeToBytes(h, d + n_vars * s_var + sizeof(int));

	int op_index = op - &*g_operators.begin(); // index is universal at all processes
	typeToBytes(op_index, &(d[n_vars * s_var + 2 * sizeof(int)]));

	printf("STATE: ");
	for (int i = 0; i < n_vars; ++i) {
		printf("%d ", s[i]);
	}
	printf("TEST: ");
	for (int i = 0; i < n_vars; ++i) {
		printf("%d ", (char) d[i]);
	}
	printf("\n");

}

// TODO
StateID HDAStarSearch::bytes_to_node(unsigned char* d) {
	state_var_t* vars = new state_var_t[n_vars];

	printf("income d: ");
	for (int i = 0; i < n_vars * s_var; ++i) {
		printf("%d ", d[i]);
	}
	printf("\n");

	for (int i = 0; i < n_vars; ++i) {
		bytesToType(vars[i], &(d[i * s_var]));
	}

	int p_h, p_g, op_index;
	bytesToType(p_h, &(d[n_vars * s_var]));
	bytesToType(p_g, &(d[n_vars * s_var + sizeof(int)]));
	bytesToType(op_index, &(d[n_vars * s_var + 2 * sizeof(int)]));
	Operator *op = &(g_operators[op_index]);

	printf("vars: ");
	for (int i = 0; i < n_vars; ++i) {
		printf("%d ", vars[i]);
	}
	printf(": op = %d", op_index);
	printf("\n");



	// May need to build a node from zero.
//	State succ_state = g_state_registry->get_successor_state(s, *op);
	State succ_state = g_state_registry->build_state(vars, *op);
	search_progress.inc_generated();
//	bool is_preferred = (preferred_ops.find(op) != preferred_ops.end());

	SearchNode succ_node = search_space.get_node(succ_state);

	// Previously encountered dead end. Don't re-evaluate.
	if (succ_node.is_dead_end())
		return StateID::no_state;

	// TODO: not even sure what it does
//	// update new path
//	if (use_multi_path_dependence || succ_node.is_new()) {
//		bool h_is_dirty = false;
//		for (size_t i = 0; i < heuristics.size(); ++i) {
//			/*
//			 Note that we can't break out of the loop when
//			 h_is_dirty is set to true or use short-circuit
//			 evaluation here. We must call reach_state for each
//			 heuristic for its side effects.
//			 */
////			if (heuristics[i]->reach_state(s, *op, succ_state))
////				h_is_dirty = true;
//		}
//		if (h_is_dirty && use_multi_path_dependence)
//			succ_node.set_h_dirty();
//	}

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
		open_list->evaluate(p_g + get_adjusted_cost(*op), false);
		bool dead_end = open_list->is_dead_end();
		if (dead_end) {
			succ_node.mark_as_dead_end();
			search_progress.inc_dead_ends();
			return StateID::no_state;
		}

		//TODO:CR - add an ID to each state, and then we can use a vector to save per-state information
		int succ_h = heuristics[0]->get_heuristic();
		if (do_pathmax) {
			if ((p_h - get_adjusted_cost(*op)) > succ_h) {
				//cout << "Pathmax correction: " << succ_h << " -> " << node.get_h() - get_adjusted_cost(*op) << endl;
				succ_h = p_h - get_adjusted_cost(*op);
				heuristics[0]->set_evaluator_value(succ_h);
				open_list->evaluate(p_g + get_adjusted_cost(*op), false);
				search_progress.inc_pathmax_corrections();
			}
		}

		succ_node.open(p_g, p_h, op, succ_h);

		open_list->insert(succ_state.get_id());
		if (search_progress.check_h_progress(succ_node.get_g())) {
			reward_progress();
		}
	} else if (succ_node.get_g() > p_g + get_adjusted_cost(*op)) {
		// We found a new cheapest path to an open or closed state.
		if (reopen_closed_nodes) {
			//TODO:CR - test if we should add a reevaluate flag and if it helps
			// if we reopen closed nodes, do that
			if (succ_node.is_closed()) {
				/* TODO: Verify that the heuristic is inconsistent.
				 * Otherwise, this is a bug. This is a serious
				 * assertion because it can show that a heuristic that
				 * was thought to be consistent isn't. Therefore, it
				 * should be present also in release builds, so don't
				 * use a plain assert. */
				//TODO:CR - add a consistent flag to heuristics, and add an assert here based on it
				search_progress.inc_reopened();
			}
//			succ_node.reopen(node, op);
			heuristics[0]->set_evaluator_value(succ_node.get_h());
			// TODO: this appears fishy to me. Why is here only heuristic[0]
			// involved? Is this still feasible in the current version?
			open_list->evaluate(succ_node.get_g(), false);

			open_list->insert(succ_state.get_id());
		} else {
			// HDA* ALWAYS update new paths.
//			succ_node.update_parent(node, op);
		}
	}
	return succ_state.get_id();
}

void HDAStarSearch::bytes_to_nodes(vector<SearchNode>* ns, unsigned char* d,
		unsigned int d_size) {
//	unsigned int dp = 0;
//	unsigned int nsize = 0;
//	while (dp < d_size) {
//		SearchNode* p = new SearchNode();
//		charsToNode(&(d[dp]), *p);
//		nodes.push_back(p);
//		dp += p->size();
//	}
}

void HDAStarSearch::receive_nodes_from_queue() {
	// 1. take messages and compile them to nodes
	// 2. put each node a NodeID and register
	// 3. add NodeID to the OpenList
	// In this way we get a new nodes

	MPI_Status status;
	int has_received = 0;
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_NODE, MPI_COMM_WORLD, &has_received,
			&status);
	while (has_received != 0) {
		int d_size = 0;
		MPI_Get_count(&status, MPI_BYTE, &d_size); // TODO: = node_size?

		unsigned char *d = new unsigned char[d_size];
		MPI_Recv(d, d_size, MPI_BYTE, MPI_ANY_SOURCE, MPI_MSG_NODE,
				MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		// TODO: use Memory Pool for nodes in local
//		std::vector<SearchNode> nodes;
//		bytes_to_node(&nodes, d, d_size);
		printf("received node %d\n", id);

		StateID state_id = bytes_to_node(d);
		if (state_id == StateID::no_state) {
			printf("dead end\n");
		} else {
			State s = g_state_registry->lookup_state(state_id);
			s.dump_pddl();
//			s.dump_fdr();
		}
		delete[] d;

		has_received = 0;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_NODE, MPI_COMM_WORLD, &has_received,
				&status);
	}
}

// Mattern, Algorithm for distributed termination detection, 1987
bool HDAStarSearch::termination_detection(bool& has_sent_first_term) {
	MPI_Status status;
	int has_received = 0;

	MPI_Iprobe(MPI_ANY_SOURCE, MPI_MSG_FTERM, MPI_COMM_WORLD, &has_received,
			MPI_STATUS_IGNORE);
	if (has_received) {
		printf("received fterm\n");
		return true;
	}

	if (id == 0 && !has_sent_first_term) {
		// TODO: how can we maintain the memory while MPI_Isend is executing?
		unsigned char term = 0;
		MPI_Send(&term, 1, MPI_BYTE, (id + 1) % world_size, MPI_MSG_TERM,
				MPI_COMM_WORLD);
		printf("sent first term\n");
		has_sent_first_term = true;
		sleep(1);
	}

	has_received = 0;
	MPI_Iprobe((id - 1) % world_size, MPI_MSG_TERM, MPI_COMM_WORLD,
			&has_received, &status);
	if (has_received) {
		int term = 0;
		MPI_Recv(&term, 1, MPI_BYTE, (id - 1) % world_size, MPI_MSG_TERM,
				MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		if (id == 0) {
			++term;
		}

		printf("%d received term %d\n", id, term);
		MPI_Send(&term, 1, MPI_BYTE, (id + 1) % world_size, MPI_MSG_TERM,
				MPI_COMM_WORLD);
		if (term > 1) {
			return true;
		}

		MPI_Iprobe((id - 1) % world_size, MPI_MSG_TERM, MPI_COMM_WORLD,
				&has_received, &status);

		// This while loop is here to flush all messages
		while (has_received) {
			MPI_Recv(&term, 1, MPI_BYTE, (id - 1) % world_size, MPI_MSG_TERM,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if (term == 1) {
				printf("%d received term %d\n", id, term);
				MPI_Send(&term, 1, MPI_BYTE, (id + 1) % world_size,
						MPI_MSG_TERM, MPI_COMM_WORLD);
				return true;
			}
			has_received = 0;
			MPI_Iprobe((id - 1) % world_size, MPI_MSG_TERM, MPI_COMM_WORLD,
					&has_received, &status);
		}

	}
	return false;
}

// TODO: implement
void HDAStarSearch::flush_outgo_buffers(int threshold) {
	(void) threshold;
//	for (int i = 0; i < world_size; ++i) {
//		if (i != id && outgo_buffer[i].size() > threshold) {
//			unsigned int totsize = 0;
//			for (int j = 0; j < outgo_buffer[i].size(); ++j) {
////				totsize += outgo_buffer[i][j].size();
//			}
//			unsigned char* d = new unsigned char[totsize];
////			unsigned char* dp = d;
//			for (int j = 0; j < outgo_buffer[i].size(); ++j) {
////				SearchNode send = outgo_buffer[i];
////				node_to_bytes(&send, dp);
////						MPI_Isend(d, send.size(), MPI_BYTE, i, MPI_MSG_NODE,
////								MPI_COMM_WORLD, &request);
//				// TODO: send_size should be implemented.
////				int send_size = 0; //send.size();
////				MPI_Send(dp, send_size, MPI_BYTE, i, MPI_MSG_NODE,
////						MPI_COMM_WORLD);
////						printf("sent f,g = %d %d\n", send.f, send.g);
////				dp += send_size;
//			}
//			delete[] d;
//			outgo_buffer[i].clear();
//		}
//	}
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
	parser.add_option<bool>("pathmax", "use pathmax correction", "false");
	parser.add_option<bool>("mpd", "use multi-path dependence (LM-A*)",
			"false");

	// HDA* related options
	parser.document_note("dist option",
			"Distribution method for HDA* is core to its performance.\n"
					"dist option selects which hash function to use for distribution.");
//	parser.add_option<DistHash *>("hash", "evaluator for h-value");

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

