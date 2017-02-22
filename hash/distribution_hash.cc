/*
 * ZobristHash.cc
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#include "distribution_hash.h"
#include "../plugin.h"
#include "../operator.h"
#include "../domain_transition_graph.h"
#include "../successor_generator.h"

#include <stdio.h>
#include <string>

using namespace std;

template<typename T>
bool compare_pair(const pair<int, T> a, const pair<int, T> b) {
	return a.first < b.first;
}

DistributionHash::DistributionHash(const Options & options) {
	// We may be able to come up with hash without map.
	// Therefore we do not refactor out map to DistributionHash.
}

DistributionHash::~DistributionHash() {
}

MapBasedHash::MapBasedHash(const Options & options) :
		DistributionHash(options) {
	isPolynomial = options.get<bool>("isPolynomial");

	// Initialize map with 0 filled.
	map.resize(g_variable_domain.size());
	for (int i = 0; i < map.size(); ++i) {
		map[i].resize(g_variable_domain[i]);
		std::fill(map[i].begin(), map[i].end(), 0);
	}
}

unsigned int MapBasedHash::hash(const State& state) {
	unsigned int r = 0;

	if (!isPolynomial) {
		for (int i = 0; i < map.size(); ++i) {
			r = r ^ map[i][state[i]];
		}
	} else {
		unsigned int last_size = 1;
		for (int i = 0; i < map.size(); ++i) {
			if (map[i][0] != 0) {
				r *= last_size;
				r += map[i][state[i]];
				last_size = map[i].size();
			}
		}
	}
	return r;
}

unsigned int MapBasedHash::hash(const state_var_t* state) {
	unsigned int r = 0;
	if (!isPolynomial) {
		for (int i = 0; i < map.size(); ++i) {
			r = r ^ map[i][state[i]];
		}
	} else {
		unsigned int last_size = 1;
		for (int i = 0; i < map.size(); ++i) {
			if (map[i][0] != 0) {
				r *= last_size;
				r += map[i][state[i]];
				last_size = map[i].size();
			}
		}
	}
	return r;
}

// TODO: not sure this is actually saving up time, or just messing up things.
// we CANNOT precompute inc_hash for each operator as we are not sure
// whether each effect CHANGES the state or not.
unsigned int MapBasedHash::hash_incremental(const State& parent,
		const unsigned int parent_d_hash, const Operator* op) {
	unsigned int ret = parent_d_hash;

	if (!isPolynomial) {
		for (size_t i = 0; i < op->get_pre_post().size(); ++i) {
			const PrePost &pre_post = op->get_pre_post()[i];
			if (pre_post.does_fire(parent)
					&& parent[pre_post.var] != pre_post.post) {
				ret = ret ^ map[pre_post.var][pre_post.post]
						^ map[pre_post.var][parent[pre_post.var]];
			}

		}
	} else {
		state_var_t* child;
		child = new state_var_t[g_variable_domain.size()];
		for (int i = 0; i < g_variable_domain.size(); ++i) {
			child[i] = parent[i];
		}
		for (size_t i = 0; i < op->get_pre_post().size(); ++i) {
			const PrePost &pre_post = op->get_pre_post()[i];
			if (pre_post.does_fire(parent)
					&& parent[pre_post.var] != pre_post.post) {
				child[pre_post.var] = pre_post.post;
			}
		}

		ret = 0;
		unsigned int last_size = 1;
		for (int i = 0; i < map.size(); ++i) {
			if (map[i][0] != 0) {
				ret *= last_size;
				ret += map[i][child[i]];
				last_size = map[i].size();
			}
		}
		delete child;
	}

	return ret;

//	state_var_t* ops = new state_var_t[g_variable_domain.size()];
//	for (int i = 0; i < g_variable_domain.size(); ++i) {
//		ops[i] = parent[i];
//	}
//
//	unsigned int hp = hash(ops);
//
//
//	for (size_t i = 0; i < op->get_pre_post().size(); ++i) {
//		const PrePost &pre_post = op->get_pre_post()[i];
//		if (pre_post.does_fire(parent)) {
//			ops[pre_post.var] = pre_post.post;
//		}
//	}
//	unsigned int hc = hash(ops);
//
//
//	if (parent_d_hash == hp) {
////		printf("%10u == %10u\n", ret, h);
//	} else {
//		printf("%10u != %10u PARENT!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", parent_d_hash, hp);
//
//	}
//
//
//	if (ret == hc) {
////		printf("%10u == %10u\n", ret, h);
//	} else {
//		printf("%10u != %10u CHILD!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", ret, hc);
//	}
//	delete[] ops;
//	return hc;
}

// TODO: This method is messy. As it is not the core of this program, ill let it for now.
void MapBasedHash::divideIntoTwo(unsigned int var,
		vector<vector<unsigned int> >& structures) {
	DomainTransitionGraph* g = g_transition_graphs[var];

	std::vector<unsigned int> structure; // = structure_index
	std::vector<unsigned int> transitions;
	for (int p = 0; p < map[var].size(); ++p) {
		vector<int> successors;
		g->get_successors(p, successors);
		transitions.insert(transitions.end(), successors.begin(),
				successors.end());
	}
//		std::cout << "transition" << std::endl;
	unsigned int least_connected = 100000;
	unsigned int least_connected_node = 0; // in index
	for (int p = 0; p < map[var].size(); ++p) {
		int c = std::count(transitions.begin(), transitions.end(), p);
		vector<int> successors;
		g->get_successors(p, successors);
		c += successors.size();
//			std::cout << xor_groups[gs][p] << ": " << c << "edges" << std::endl;
		if (c < least_connected) {
			least_connected = c;
			least_connected_node = p;
		}
	}
	structure.push_back(least_connected_node);

	while (structure.size() < map[var].size() / 2) {
		transitions.clear();
		// 2. add a node which is mostly connected to strucuture
		for (int p = 0; p < structure.size(); ++p) {
			vector<int> successors;
			g->get_successors(p, successors);
			transitions.insert(transitions.end(), successors.begin(),
					successors.end());
		}

		// count the most connected nodes
		unsigned int most_connected = 0;
		unsigned int most_connected_node = 0; // in index
		for (int p = 0; p < map[var].size(); ++p) {
			// if p is already in the group, then skip that.
			if (find(structure.begin(), structure.end(), p)
					!= structure.end()) {
				continue;
			}
			int c = std::count(transitions.begin(), transitions.end(), p);
			vector<int> successors;
			g->get_successors(p, successors);
			c += successors.size();

			if (c > most_connected) {
				most_connected = c;
				most_connected_node = p;
			}
		}

		// if the no nodes connected, then return
		if (most_connected == 0) {
			break;
		}
		structure.push_back(most_connected_node);
	}
	std::sort(structure.begin(), structure.end());
	structures.push_back(structure);

	// the rest of the predicates will be in the second structure.
	// TODO: not sure this assumption is right or not.
	//       maybe not right.
	std::vector<unsigned int> structure2; // = xor_groups[gs] - structure;

	unsigned int most_connected = 0;
	unsigned int most_connected_node = 0; // in index

	for (int p = 0; p < map[var].size(); ++p) {
		if (find(structure.begin(), structure.end(), p) == structure.end()) {
			most_connected_node = p;
			break;
		}
	}

	transitions.clear();
	// 2. add a node which is mostly connected to strucuture
	for (int p = 0; p < structure.size(); ++p) {
		vector<int> successors;
		g->get_successors(p, successors);
		transitions.insert(transitions.end(), successors.begin(),
				successors.end());
	}

	for (int p = 0; p < map[var].size(); ++p) {
		int c = std::count(transitions.begin(), transitions.end(), p);
		vector<int> successors;
		g->get_successors(p, successors);
		c += successors.size();
		if (c > most_connected
				&& find(structure.begin(), structure.end(), p)
						== structure.end()) {
			most_connected = c;
			most_connected_node = p;
		}
	}
	structure2.push_back(most_connected_node);

//	while (structure.size() + structure2.size() < map[var].size()) {
	while (true) {
//		printf("%lu+%lu ?= %lu\n", structure.size(), structure2.size(),
//				map[var].size());
		if (structure.size() + structure2.size() >= map[var].size()) {
			break;
		}
		transitions.clear();
		// 2. add a node which is mostly connected to strucuture
		for (int p = 0; p < structure2.size(); ++p) {
			vector<int> successors;
			g->get_successors(p, successors);
			transitions.insert(transitions.end(), successors.begin(),
					successors.end());
		}
		// count the most connected nodes
		unsigned int most_connected = 0;
		unsigned int most_connected_node = 0; // in index
		for (int p = 0; p < map[var].size(); ++p) {
			// if p is already in the group, then skip that.
			if (find(structure2.begin(), structure2.end(), p)
					!= structure2.end()
					|| find(structure.begin(), structure.end(), p)
							!= structure.end()) {
				continue;
			}
			int c = std::count(transitions.begin(), transitions.end(), p);
			vector<int> successors;
			vector<int> inter;
			g->get_successors(p, successors);
			inter.resize(successors.size() + structure2.size());

			vector<int>::iterator it = set_intersection(successors.begin(),
					successors.end(), structure2.begin(), structure2.end(),
					inter.begin());
			c += (it - inter.begin());
			if (c > most_connected) {
				most_connected = c;
				most_connected_node = p;
			}
		}

		// if the no nodes connected, then return
		if (most_connected == 0) {
			break;
		}
		structure2.push_back(most_connected_node);
	}

//		std::set_difference(xor_groups[gs].begin(), xor_groups[gs].end(),
//				structure.begin(), structure.end(),
//				std::inserter(structure2, structure2.begin()));

	if (structure2.size() > 1) {
		std::sort(structure2.begin(), structure2.end());
		structures.push_back(structure2);
	}
}

vector<int> DistributionHash::get_frequency_rank() {
	// 1.  Count the number of operator which functions the variable.
	vector<pair<int, int> > n_op_functioning;
	for (int i = 0; i < g_variable_domain.size(); ++i) {
		n_op_functioning.push_back(pair<int, int>(0, i));
	}

	for (int i = 0; i < g_operators.size(); ++i) {
		const Operator op = g_operators[i];
		for (int j = 0; j < op.get_pre_post().size(); ++j) {
			PrePost pp = op.get_pre_post()[j];
			++n_op_functioning[pp.var].first;
		}
	}

	std::sort(n_op_functioning.begin(), n_op_functioning.end(),
			&compare_pair<int>);

	for (int i = 0; i < g_variable_domain.size(); ++i) {
		printf("var %d: rank %d, %d ops\n", n_op_functioning[i].second, i,
				n_op_functioning[i].first);
	}

	vector<int> n_op_functioning_rank(g_variable_domain.size());
	for (int i = 0; i < g_variable_domain.size(); ++i) {
		n_op_functioning_rank[n_op_functioning[i].second] = i;
	}

	return n_op_functioning_rank;
}

vector<int> MapBasedHash::reverse_iter_to_val(vector<int> in) {
	vector<int> r;
	r.resize(in.size());
	for (int i = 0; i < in.size(); ++i) {
		r[in[i]] = i;
	}
	return r;
}

ZobristHash::ZobristHash(const Options &opts) :
		MapBasedHash(opts) {
	// Need to think about seeds later, but for this prototype we put a fixed number.
	g_rng.seed(717);

	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			map[i][j] = g_rng.next32();
		}
	}

//	for (int i = 0; i < map.size(); ++i) {
//		for (int j = 0; j < map[i].size(); ++j) {
//			printf("%u ", map[i][j]);
//		}
//		printf("\n");
//	}

}

AbstractionHash::AbstractionHash(const Options &opts) :
		MapBasedHash(opts) {

	abstraction = opts.get<int>("abstraction");
	printf("abstraction = %d\n", abstraction);

//	unsigned int whole_variable_space_size = 1;
//	for (int i = 0; i < map.size(); ++i) {
//		whole_variable_space_size *= map[i].size();
//	}

//	unsigned int abstraction_size = abstraction;
	unsigned int current_size = 1;

	vector<int> n_op_functioning_rank = get_frequency_rank();
	vector<int> rank = reverse_iter_to_val(n_op_functioning_rank);

	g_rng.seed(717);

	// Abstract out from the most influent variables.

	for (int i = 0; i < map.size(); ++i) {
		int k = rank[i];
//		printf("current_size=%u\n", current_size);
		if (current_size * map[k].size() > abstraction) {
			continue;
		}
		current_size *= map[k].size();

		for (int j = 0; j < map[k].size(); ++j) {
			map[k][j] = g_rng.next32();
		}
	}

	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			printf("%u ", map[i][j]);
		}
		printf("\n");
	}

}

AdaptiveAbstractionHash::AdaptiveAbstractionHash(const Options &opts) :
		MapBasedHash(opts) {

	abstraction = opts.get<double>("abstraction");
	printf("abstraction = %f\n", abstraction);

	unsigned int whole_variable_space_size = 1;
	for (int i = 0; i < map.size(); ++i) {
		whole_variable_space_size += map[i].size();
	}

	unsigned int abstraction_size = (double) whole_variable_space_size
			* (1.0 - abstraction);
	unsigned int current_size = 0;

	unsigned int abstraction_graph_size = 1;

	vector<int> n_op_functioning_rank = get_frequency_rank();
	vector<int> rank = reverse_iter_to_val(n_op_functioning_rank);

	g_rng.seed(717);

	// Abstract out from the most influent variables.
	printf("mapsize=%lu\n", map.size());

	printf("whole_variable_space_size=%u\n", whole_variable_space_size);
	printf("abstraction_size=%u\n", abstraction_size);

	for (int i = 0; i < map.size(); ++i) {
		int k = rank[i];
		printf("map[%d]size=%lu\n", k, map[k].size());
//		printf("current_size=%u\n", current_size);
		if (current_size > abstraction_size) {
			continue;
		}

		current_size += map[k].size();
		abstraction_graph_size *= map[k].size();

//		printf("current_size=%u\n", current_size);
//		printf("abstraction_graph_size=%u\n", abstraction_graph_size);

		for (int j = 0; j < map[k].size(); ++j) {
			map[k][j] = g_rng.next32();
		}
	}

	printf("abstraction_graph_size = %u\n", abstraction_graph_size);

//	exit(0);
	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			printf("%u ", map[i][j]);
		}
		printf("\n");
	}

}

FeatureBasedStructuredZobristHash::FeatureBasedStructuredZobristHash(
		const Options &opts) :
		MapBasedHash(opts) {
	abstraction = opts.get<double>("abstraction");
	printf("abstraction = %f\n", abstraction);

	unsigned int whole_variable_space_size = 1;
	for (int i = 0; i < map.size(); ++i) {
		whole_variable_space_size += map[i].size();
	}

	unsigned int abstraction_size = whole_variable_space_size * abstraction;
	unsigned int current_size = 1;

	// Need to think about seeds later, but for this prototype we put a fixed number.
	g_rng.seed(717);

	// TODO: read my last stupid code and implement while i know this is suboptimal.
	for (int i = 0; i < map.size(); ++i) {
		if (map[i].size() <= 2) {
			continue;
		}
		current_size += map[i].size();
		if (current_size > abstraction_size) {
			break;
		}
		vector<vector<unsigned int> > structures;
		divideIntoTwo(i, structures);
//		printf("structure size=%lu\n", structures.size());
		for (int j = 0; j < structures.size(); ++j) {
//			printf("structure[j] size=%lu\n", structures[j].size());
			unsigned int r = g_rng.next32();
			for (int k = 0; k < structures[j].size(); ++k) {
//				printf("%u ", structures[j][k]);
				map[i][structures[j][k]] = r;
			}
//			printf("\n");
		}

	}

	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			if (map[i][j] == 0) {
				map[i][j] = g_rng.next32();
			}
		}
	}

	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			printf("%u ", map[i][j]);
		}
		printf("\n");
	}
}

ActionBasedStructuredZobristHash::ActionBasedStructuredZobristHash(
		const Options &opts) :
		MapBasedHash(opts) {

	vector<pair<int, const Operator *> > ops;

	g_successor_generator->get_op_depths(0, ops);
//	g_successor_generator->_dump("");

//	for (int i = 0; i < ops.size(); ++i) {
//		printf("%d %s\n", ops[i].first, ops[i].second->get_name().c_str());
//	}

	sort(ops.begin(), ops.end(), &compare_pair<const Operator *>);

	// Need to think about seeds later, but for this prototype we put a fixed number.
	g_rng.seed(717);

	// First, initialize the map as in ZobristHash. We start it from this values.
	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			map[i][j] = g_rng.next32();
		}
	}

	abstraction = opts.get<double>("abstraction");
	printf("abstraction = %f\n", abstraction);
	unsigned int abstraction_size = g_operators.size() * abstraction;
	unsigned int current_size = 0;

	// TODO: we need a list of grounded actions applicable here.
	// vector<Operator> g_operators

	// STRUCTURED means that it is used for building structure somewhere.
	// Changing its value will destroy it.
	vector<vector<bool> > has_structured;
	has_structured.resize(map.size());
	for (int i = 0; i < map.size(); ++i) {
		has_structured[i].resize(map[i].size());
		fill(has_structured[i].begin(), has_structured[i].end(), false);
	}

	// ad hoc numbers to terminate the algorithm after
	// structuring all possible actions.
	unsigned int failed = 0;
//	unsigned int max_failed = 60;

	// TODO: KNOWN ISSUE
	// Add effect and delete effect does not always CHANGE the state.
	// Therefore, this method does not GUARANTEE to build a structure.
	// If we could somewhat get around it, we may have a chance to optimize it more.
	int op_index = 0;
	while ((current_size < abstraction_size) && (op_index < ops.size())) {
//		unsigned int op_index = g_rng.next32() % g_operators.size();
//		printf("op = %d\n", op_index);
		Operator op = *(ops[op_index++].second);
//		printf("op=%s", op.get_name().c_str());
		vector<pair<int, int> > effects;
		for (size_t i = 0; i < op.get_pre_post().size(); ++i) {
			const PrePost &pre_post = op.get_pre_post()[i];
			if (pre_post.post >= 0) {
				pair<int, int> p(pre_post.var, pre_post.post);
				effects.push_back(p);
			}
			if (pre_post.pre >= 0) {
				pair<int, int> p2(pre_post.var, pre_post.pre);
				effects.push_back(p2);
			}
		}
//		printf("eff done\n");

		bool succeeded = false;
		// check if ANY of the predicates is NOT structured
//		printf("OPERATOR: %s\n", op.get_name().c_str());

		for (int i = 0; i < effects.size(); ++i) {
//			printf("effects = %d,%d\n", effects[i].first, effects[i].second);
//			printf("has_structured[%d][%d] = %d\n", effects[i].first,
//					effects[i].second,
//					(int) has_structured[effects[i].first][effects[i].second]);
//			printf("map[%d][%d] = %d\n", effects[i].first, effects[i].second,
//					map[effects[i].first][effects[i].second]);

			if (has_structured[effects[i].first][effects[i].second]) {
				continue;
			} else {
				unsigned int value = 0;
				for (int j = 0; j < effects.size(); ++j) {
					if (i != j) {
						value = value
								^ map[effects[j].first][effects[j].second];
					}
				}
				map[effects[i].first][effects[i].second] = value;
				succeeded = true;
				break;
			}
		}
		if (succeeded) {
//			printf(" succeeded\n");
		} else {
//			printf("\n");
		}
		if (succeeded) {
			for (int i = 0; i < effects.size(); ++i) {
//				printf("has_structured[%d][%d] = %d\n", effects[i].first,
//						effects[i].second,
//						(int) has_structured[effects[i].first][effects[i].second]);
				has_structured[effects[i].first][effects[i].second] = true;
			}
			++current_size;
			failed = 0;
		} else {
			++failed;
		}
	}

	printf("abst succeeded=%u/%u : %lu\n", current_size, abstraction_size,
			g_operators.size());

	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			printf("%u ", map[i][j]);
		}
		printf("\n");
	}
}

std::string ZobristHash::hash_name() {
	return "zobrist";
}

std::string AbstractionHash::hash_name() {
	std::ostringstream os;
	os << abstraction;
	std::string str = "abstraction(" + os.str() + ")";
	return str;
}

std::string AdaptiveAbstractionHash::hash_name() {
	std::ostringstream os;
	os << abstraction;
	std::string str = "abstraction(" + os.str() + ")";
	return str;
}

std::string FeatureBasedStructuredZobristHash::hash_name() {
	std::ostringstream os;
	os << abstraction;
	std::string str = "fstructured(" + os.str() + ")";
	return str;
}

std::string ActionBasedStructuredZobristHash::hash_name() {
	std::ostringstream os;
	os << abstraction;
	std::string str = "astructured(" + os.str() + ")";
	return str;
}

static DistributionHash*_parse_zobrist(OptionParser &parser) {
	parser.document_synopsis("Zobrist Hash", "Distribution hash for HDA*");

	parser.add_option<bool>("isPolynomial",
			"Use polynomial hashing for underlying load balancing scheme.", "false");
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new ZobristHash(opts);
}

static DistributionHash*_parse_abstraction(OptionParser &parser) {
	parser.document_synopsis("AbstractionHash", "Distribution hash for HDA*");

	parser.add_option<int>("abstraction",
			"The number of keys relative to the whole variable space."
					"If abst=1, then it is same as ZobristHash.", "5000");

	parser.add_option<bool>("isPolynomial",
			"Use polynomial hashing for underlying load balancing scheme.", "false");
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new AbstractionHash(opts);
}

static DistributionHash*_parse_adaptive_abstraction(OptionParser &parser) {
	parser.document_synopsis("AdaptiveAbstractionHash",
			"Distribution hash for HDA*");

	parser.add_option<double>("abstraction",
			"The number of keys relative to the whole variable space."
					"If abst=0, then it is same as ZobristHash.", "0.3");
	parser.add_option<bool>("isPolynomial",
			"Use polynomial hashing for underlying load balancing scheme.", "false");
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new AdaptiveAbstractionHash(opts);
}

static DistributionHash*_parse_feature_structured_zobrist(
		OptionParser &parser) {
	parser.document_synopsis("FeatureBasedStructuredZobristHash",
			"Distribution hash for HDA*");

	parser.add_option<double>("abstraction",
			"The number of features to build structure."
					"If abst=0, then it is same as ZobristHash.", "0.3");
	parser.add_option<bool>("isPolynomial",
			"Use polynomial hashing for underlying load balancing scheme.", "false");
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new FeatureBasedStructuredZobristHash(opts);
}

static DistributionHash*_parse_action_structured_zobrist(OptionParser &parser) {
	parser.document_synopsis("ActionBasedStructuredZobristHash",
			"Distribution hash for HDA*");

	parser.add_option<double>("abstraction",
			"The maximum ratio of actions to eliminate CO."
					"If abst=0, then it is same as ZobristHash.", "0.3");
	parser.add_option<bool>("isPolynomial",
			"Use polynomial hashing for underlying load balancing scheme.", "false");
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new ActionBasedStructuredZobristHash(opts);
}
static Plugin<DistributionHash> _plugin_zobrist("zobrist", _parse_zobrist);
static Plugin<DistributionHash> _plugin_abstraction("abstraction",
		_parse_abstraction);
static Plugin<DistributionHash> _plugin_adaptive_abstraction("aabstraction",
		_parse_adaptive_abstraction);
static Plugin<DistributionHash> _plugin_feature_structured_zobrist(
		"fstructured", _parse_feature_structured_zobrist);
static Plugin<DistributionHash> _plugin_action_structured_zobrist("astructured",
		_parse_action_structured_zobrist);
