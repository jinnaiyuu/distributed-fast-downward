/*
 * ZobristHash.cc
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#include "cut_strategy.h"
#include "../domain_transition_graph.h"
#include "../globals.h"
#include "../rng.h"
#include "../operator.h"

#include <algorithm>
#include <stdio.h>
#include <string>

using namespace std;

CutStrategy::CutStrategy(const Options &options) {
	// none
}

CutStrategy::~CutStrategy() {
	// none
}

RandomUpdating::RandomUpdating(const Options &options) :
		CutStrategy(options) {
	n_update = options.get<int>("update");
	sparsity = options.get<Sparsity *>("sparsity");
	always_cut = options.get<bool>("always_cut");
	edge_weight = options.get<string>("edge_weight");
	// TODO: update number and sparsity
	build_transition_matrix();
}

void RandomUpdating::build_transition_matrix() {
	transitions.resize(g_variable_domain.size());
	for (int var = 0; var < g_variable_domain.size(); ++var) {

//		DomainTransitionGraph* g = g_transition_graphs[var];
		int size = g_variable_domain[var];
		std::vector<std::vector<double> > tmatrix; // Edge matrix
		tmatrix.resize(size);
		for (int i = 0; i < size; ++i) {
			tmatrix[i].resize(size, 0.0);
		}
		transitions[var] = tmatrix;

	}

	if (edge_weight.compare("ground_action") == 0) {
		// TODO: duplication of counting for each domain transition graph.

		for (int i = 0; i < g_operators.size(); ++i) {
			Operator op = g_operators[i];
			vector<bool> do_change(g_variable_domain.size(), false);

			for (int j = 0; j < op.get_prevail().size(); ++j) {
				Prevail pv = op.get_prevail()[j];
				do_change[pv.var] = true;
				for (int k = 0; k < g_variable_domain[pv.var]; ++k) {
					++transitions[pv.var][k][pv.prev];
				}
			}
			for (int j = 0; j < op.get_pre_post().size(); ++j) {
				PrePost pp = op.get_pre_post()[j];
				do_change[pp.var] = true;
				if (pp.pre == -1) {
					for (int k = 0; k < g_variable_domain[pp.var]; ++k) {
						++transitions[pp.var][k][pp.post];
					}
				} else {
					++transitions[pp.var][pp.pre][pp.post];
				}
			}

			// Calculate the number of self loops.
			// TODO: It may need to be shrank down.
			for (int j = 0; j < g_variable_domain.size(); ++j) {
				if (!do_change[j]) {
					for (int k = 0; k < g_variable_domain[j]; ++k) {
						++transitions[j][k][k];
					}
				}
			}
		}
		// TODO: shrink down diagonal line

	} else if (edge_weight.compare("unit_cost") == 0) {
		for (int var = 0; var < g_variable_domain.size(); ++var) {
			DomainTransitionGraph* g = g_transition_graphs[var];
			int size = g_variable_domain[var];
			for (int i = 0; i < size; ++i) {
				//		printf("i=%d size=%d\n", i, size);
				vector<int> successors;
				g->get_successors(i, successors);
				for (int j = 0; j < successors.size(); ++j) {
					transitions[var][i][successors[j]] = 1.0; // TODO: no edge weight
				}
				transitions[var][i][i] = 1.0; // TODO: edge weight
			}
		}

	} else {
		cout << "error on building transition matrix" << endl;
	}

	for (int var = 0; var < g_variable_domain.size(); ++var) {
		printf("%d: \n", var);
		for (int i = 0; i < g_variable_domain[var]; ++i) {
			for (int j = 0; j < g_variable_domain[var]; ++j) {
				printf("%-5.2f ", transitions[var][i][j]);
			}
			printf("\n");
		}
		printf("\n");
	}
}

void RandomUpdating::cut(unsigned int var,
		vector<vector<unsigned int> >& structures) {

	int size = g_variable_domain[var];

// 1. For each node randomly assign which structure to belong.
	std::vector<bool> which_structure(size);
	for (int p = 0; p < size; ++p) {
		which_structure[p] = g_rng.next32() % 2;
//		printf("%d ", (int) which_structure[p]);
	}

//	printf("\n");

	double incumbent = sparsity->calculate_sparsity(transitions[var],
			which_structure);

// 2. Select one node randomly and assign to a structure which gains lower sparsity.
	int ntest = size * n_update; // number of test to

	for (int i = 0; i < ntest; ++i) {
//		printf("incumbent = %.2f\n", incumbent);
		int node = g_rng.next32() % size;

		std::vector<bool> test_structure(which_structure);
		test_structure[node] = !which_structure[node]; // flip the node

		if (always_cut) {
			if (std::adjacent_find(test_structure.begin(), test_structure.end(),
					std::not_equal_to<bool>()) == test_structure.end()) {
				continue;
			}
		}

		double test_value = sparsity->calculate_sparsity(transitions[var],
				test_structure);

		if (test_value < incumbent) {
			incumbent = test_value;
			which_structure = test_structure;
		}
	}

	printf("sparsity for %d = %.2f\n", var, incumbent);
	structures.resize(2);
	for (int i = 0; i < size; ++i) {
		if (which_structure[i]) {
			structures[0].push_back(i);
		} else {
			structures[1].push_back(i);
		}
	}
}

TwoGroupsAndRest::TwoGroupsAndRest(const Options &options) :
		CutStrategy(options) {
// none
}

void TwoGroupsAndRest::cut(unsigned int var,
		vector<vector<unsigned int> >& structures) {
	DomainTransitionGraph* g = g_transition_graphs[var];
	int size = g_variable_domain[var];

	std::vector<unsigned int> structure; // = structure_index
	std::vector<unsigned int> transitions;
	for (int p = 0; p < size; ++p) {
		vector<int> successors;
		g->get_successors(p, successors);
		transitions.insert(transitions.end(), successors.begin(),
				successors.end());
	}
//		std::cout << "transition" << std::endl;
	unsigned int least_connected = 100000;
	unsigned int least_connected_node = 0; // in index
	for (int p = 0; p < size; ++p) {
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

	while (structure.size() < size / 2) {
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
		for (int p = 0; p < size; ++p) {
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

	for (int p = 0; p < size; ++p) {
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

	for (int p = 0; p < size; ++p) {
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

	while (true) {

		if (structure.size() + structure2.size() >= size) {
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
		for (int p = 0; p < size; ++p) {
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

	if (structure2.size() > 1) {
		std::sort(structure2.begin(), structure2.end());
		structures.push_back(structure2);
	}
}

static CutStrategy* _parse_two_groups_and_rest(OptionParser &parser) {
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new TwoGroupsAndRest(opts);
}

static CutStrategy* _parse_random_updating(OptionParser &parser) {
	parser.add_option<Sparsity *>("sparsity",
			"definition of sparsity, what to minimize", "cut_over_edge_cost");
	parser.add_option<int>("update",
			"apply random update for |V| * update where |V| "
					"is the size of the domain transition graph.", "10");
	parser.add_option<bool>("always_cut", "Always assume to cut the domain. "
			"Do not consider to put all values to one structure.", "false");
	parser.add_option<string>("edge_weight",
			"Policy on edge weight."
					"Choose what to be the cost of the edge in DTG. Default is unit cost."
					"unit_cost/ground_action", "unit_cost");

	Options opts = parser.parse();

	if (parser.dry_run())
		return 0;
	else
		return new RandomUpdating(opts);
}

static Plugin<CutStrategy> _plugin_two_groups_and_rest("two_groups_and_rest",
		_parse_two_groups_and_rest);
static Plugin<CutStrategy> _plugin_random_updating("random_updating",
		_parse_random_updating);
