/*
 * ZobristHash.cc
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#include "sparsity.h"
#include "../domain_transition_graph.h"
#include "../globals.h"
#include "../rng.h"
#include "../plugin.h"
#include "../operator.h"

#include <algorithm>
#include <stdio.h>
#include <string>

using namespace std;

Sparsity::Sparsity(const Options &options) {
}

Sparsity::~Sparsity() {
}

CutOverEdgeCostSparsity::CutOverEdgeCostSparsity(const Options &options) :
		Sparsity(options) {
}

CutOverGraphSizesSparsity::CutOverGraphSizesSparsity(const Options &options) :
		Sparsity(options) {
}

CutOverSmallerGraphSizeSparsity::CutOverSmallerGraphSizeSparsity(const Options &options) :
		Sparsity(options) {
}

double CutOverEdgeCostSparsity::calculate_sparsity(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<bool>& which_structure) {
	double cut_edge = 0.0;
	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;

	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			if (transitions[i][j] > 0) {
				if (which_structure[i] != which_structure[j]) {
					cut_edge += transitions[i][j];
				} else if (which_structure[i] == false) {
					connected_edge_zero += transitions[i][j];
				} else if (which_structure[i] == true) {
					connected_edge_one += transitions[i][j];
				} else {
					//					printf("wtf\n");
				}
			}
		}
	}
	return cut_edge / (connected_edge_zero * connected_edge_one);
}

double CutOverGraphSizesSparsity::calculate_sparsity(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<bool>& which_structure) {
	double cut_edge = 0.0;
	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;

	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			if (transitions[i][j] > 0) {
				if (which_structure[i] != which_structure[j]) {
					cut_edge += transitions[i][j];
				} else if (which_structure[i] == false) {
					connected_edge_zero += transitions[i][j];
				} else if (which_structure[i] == true) {
					connected_edge_one += transitions[i][j];
				} else {
					//					printf("wtf\n");
				}
			}
		}
	}

	double zeros = count(which_structure.begin(), which_structure.end(), false);
	double ones = count(which_structure.begin(), which_structure.end(), true);
	return cut_edge / (zeros * ones);
}

double CutOverSmallerGraphSizeSparsity::calculate_sparsity(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<bool>& which_structure) {
	double cut_edge = 0.0;
	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;

	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			if (transitions[i][j] > 0) {
				if (which_structure[i] != which_structure[j]) {
					cut_edge += transitions[i][j];
				} else if (which_structure[i] == false) {
					connected_edge_zero += transitions[i][j];
				} else if (which_structure[i] == true) {
					connected_edge_one += transitions[i][j];
				} else {
//					printf("wtf\n");
				}
			}
		}
	}

	double zeros = count(which_structure.begin(), which_structure.end(), false);
	double ones = count(which_structure.begin(), which_structure.end(), true);
	return cut_edge / min(zeros, ones);
}

static Sparsity* _parse_cut_over_edge_cost_sparsity(OptionParser &parser) {
	parser.document_synopsis("CutOverEdgeCostSparsity",
			"C(E)/(C(E(S1))*C(E(S2))).");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new CutOverEdgeCostSparsity(opts);
}

static Sparsity* _parse_cut_over_graph_sizes_sparsity(OptionParser &parser) {
	parser.document_synopsis("CutOverGraphSizesSparsity",
			"C(E)/(C(E(S1))*C(E(S2))).");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new CutOverGraphSizesSparsity(opts);
}

static Sparsity* _parse_cut_over_smaller_graph_size_sparsity(
		OptionParser &parser) {
	parser.document_synopsis("CutOverSmallerGraphSizeSparsity",
			"C(E)/(C(E(S1))*C(E(S2))).");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new CutOverSmallerGraphSizeSparsity(opts);
}

static Plugin<Sparsity> _plugin_cut_ver_edge_cost_sparsity("cut_over_edge_cost",
		_parse_cut_over_edge_cost_sparsity);

static Plugin<Sparsity> _plugin_cut_over_graph_sizes_sparsity(
		"cut_over_graph_sizes", _parse_cut_over_graph_sizes_sparsity);

static Plugin<Sparsity> _plugin_cut_over_smaller_graph_size_sparsity(
		"cut_over_smaller_graph_size",
		_parse_cut_over_smaller_graph_size_sparsity);
