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
#include <math.h>

using namespace std;

Sparsity::Sparsity(const Options &options) {
}

Sparsity::~Sparsity() {
}

CutOverEdgeCostSparsity::CutOverEdgeCostSparsity(const Options &options) :
		Sparsity(options) {
	w1 = options.get<double>("w1");
	w2 = options.get<double>("w2");
}

EstimatedEfficiency::EstimatedEfficiency(const Options &options) :
		Sparsity(options) {
	weight = options.get<double>("weight");
	def = options.get<string>("def");
	unitnode = options.get<bool>("unitnode");

}

CutOverGraphSizesSparsity::CutOverGraphSizesSparsity(const Options &options) :
		Sparsity(options) {
}

CutOverSmallerGraphSizeSparsity::CutOverSmallerGraphSizeSparsity(
		const Options &options) :
		Sparsity(options) {
}

double CutOverEdgeCostSparsity::calculate_sparsity(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<bool>& which_structure) {
	double cut_edge = 0.0;
	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;

	double total_weights = 0.0;

	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			total_weights += transitions[i][j];
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

	cut_edge = cut_edge / total_weights;
	connected_edge_zero = connected_edge_zero / total_weights;
	connected_edge_one = connected_edge_one / total_weights;

	if (cut_edge == 0) {
		cut_edge = 0.001;
	}

	if (connected_edge_zero <= 0.0) {
		connected_edge_zero = 0.5 / total_weights;
	} else if (connected_edge_one <= 0.0) {
		connected_edge_one = 0.5 / total_weights;
	}

//	printf("e, lb = %.2f %.2f\n", cut_edge, (connected_edge_zero * connected_edge_one));

	return (cut_edge + w1) / ((connected_edge_zero * connected_edge_one) + w2);
}

double EstimatedEfficiency::calculate_sparsity(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<bool>& which_structure) {
	double cut_edge = 0.0;
	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;
	double total_weights = 0.0;
	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			total_weights += transitions[i][j];
			if (transitions[i][j] > 0) {
				if (which_structure[i] != which_structure[j]) {
					cut_edge += transitions[i][j];
				} else if (which_structure[i] == false) {
					if (unitnode) {
						connected_edge_zero += 1;
					} else {
						connected_edge_zero += transitions[i][j];
					}
				} else if (which_structure[i] == true) {
					if (unitnode) {
						connected_edge_one += 1;
					} else {
						connected_edge_one += transitions[i][j];
					}
				} else {
					//					printf("wtf\n");
				}
			}
		}
	}
	double co = cut_edge / total_weights;

	double zeros = count(which_structure.begin(), which_structure.end(), false);
	double ones = count(which_structure.begin(), which_structure.end(), true);
	double lb = max(zeros, ones) / ((zeros + ones) / 2.0);

	if (def == "add") {
		return weight * co + lb;

	} else if (def == "mul") {
		return (1 + weight * co) * (lb - 1);
	} else {
//		printf("ERROR: EstimatedEfficiency::calculate_sparsity");
		printf("co,lb=%.2f,%.2f, SPARSITY=%.2f\n", co, lb, weight * co + lb);

		return weight * co + lb;
//		return 0;
	}
}

double CutOverEdgeCostSparsity::calculate_upper_bound(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<int>& which_structure) {
	double cut_edge_01 = 0.0;
	double cut_edge_0 = 0.0;
	double cut_edge_1 = 0.0;

	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;
	double connected_edge_yet = 0.0;

	double total_weights = 0.0;

	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			total_weights += transitions[i][j];
			if (transitions[i][j] > 0) {
				if ((which_structure[i] == 0 && which_structure[j] == 1)
						|| (which_structure[i] == 1 && which_structure[j] == 0)) {
					cut_edge_01 += transitions[i][j];
				} else if ((which_structure[i] == 0 && which_structure[j] == -1)
						|| (which_structure[i] == -1 && which_structure[j] == 0)) {
					cut_edge_0 += transitions[i][j];
				} else if ((which_structure[i] == 1 && which_structure[j] == -1)
						|| (which_structure[i] == -1 && which_structure[j] == 1)) {
					cut_edge_1 += transitions[i][j];
				} else if (which_structure[i] == 0 && which_structure[j] == 0) {
					connected_edge_zero += transitions[i][j];
				} else if (which_structure[i] == 1 && which_structure[j] == 1) {
					connected_edge_one += transitions[i][j];
				} else if (which_structure[i] == -1
						&& which_structure[j] == -1) {
					connected_edge_yet += transitions[i][j];
				}
			}
		}
	}

	cut_edge_01 /= total_weights;
	cut_edge_0 /= total_weights;
	cut_edge_1 /= total_weights;
	connected_edge_zero /= total_weights;
	connected_edge_one /= total_weights;
	connected_edge_yet /= total_weights;

	double emax = cut_edge_01 + cut_edge_0 + cut_edge_1 + connected_edge_yet
			+ w1;

	double lbmin = (max(connected_edge_zero, connected_edge_one)
			+ connected_edge_yet) * min(connected_edge_zero, connected_edge_one)
			+ w2;

	if (lbmin < 1.0) {
		lbmin = 0.5;
	}
//	printf("emax, lbmin = %.2f %.2f\n", emax, lbmin);

	return emax / lbmin;
}

double EstimatedEfficiency::calculate_upper_bound(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<int>& which_structure) {
	double cut_edge_01 = 0.0;
	double cut_edge_0 = 0.0;
	double cut_edge_1 = 0.0;

	double connected_edge_zero = 0.0;
	double connected_edge_one = 0.0;
	double connected_edge_yet = 0.0;

	double total_weights = 0.0;

	int size = transitions.size();
	for (int i = 0; i < size; ++i) {
		for (int j = 0; j < size; ++j) {
			total_weights += transitions[i][j];
			if (transitions[i][j] > 0) {
				if ((which_structure[i] == 0 && which_structure[j] == 1)
						|| (which_structure[i] == 1 && which_structure[j] == 0)) {
					cut_edge_01 += transitions[i][j];
				} else if ((which_structure[i] == 0 && which_structure[j] == -1)
						|| (which_structure[i] == -1 && which_structure[j] == 0)) {
					cut_edge_0 += transitions[i][j];
				} else if ((which_structure[i] == 1 && which_structure[j] == -1)
						|| (which_structure[i] == -1 && which_structure[j] == 1)) {
					cut_edge_1 += transitions[i][j];
				} else if (which_structure[i] == 0 && which_structure[j] == 0) {
					connected_edge_zero += transitions[i][j];
				} else if (which_structure[i] == 1 && which_structure[j] == 1) {
					connected_edge_one += transitions[i][j];
				} else if (which_structure[i] == -1
						&& which_structure[j] == -1) {
					connected_edge_yet += transitions[i][j];
				}
			}
		}
	}

	cut_edge_01 /= total_weights;
	cut_edge_0 /= total_weights;
	cut_edge_1 /= total_weights;
	connected_edge_zero /= total_weights;
	connected_edge_one /= total_weights;
	connected_edge_yet /= total_weights;

//	double max_co = cut_edge_01 + cut_edge_0 + cut_edge_1 + connected_edge_yet;
//
//	double max_lb = (max(connected_edge_zero, connected_edge_one)
//			+ connected_edge_yet)
//			/ ((connected_edge_zero + connected_edge_one + connected_edge_yet)
//					/ 2.0);

	double min_co = cut_edge_01;

	double min_lb = 1.0;

	if (unitnode) {
		int zero = std::count(which_structure.begin(), which_structure.end(),
				0);
		int one = std::count(which_structure.begin(), which_structure.end(), 1);
		int yet = std::count(which_structure.begin(), which_structure.end(),
				-1);
//		max_lb = (max(zero, one) + yet) / (which_structure.size() / 2.0);

		if (abs(zero - one) <= yet) {
			min_lb = 1.0;
		} else {
			min_lb = max(zero, one) / (which_structure.size() / 2.0);
		}
		assert(1.0 <= min_lb && min_lb <= 2.0);
	} else {
		min_lb = fabs(
				fabs(connected_edge_zero - connected_edge_one)
						- connected_edge_yet)
				/ ((connected_edge_zero + connected_edge_one
						+ connected_edge_yet) / 2.0);
	}

	if (def == "add") {
		return weight * min_co + min_lb;
	} else if (def == "mul") {
		return (1 + weight * min_co) * (min_lb - 1);
	} else {
		printf("co,lb=%.2f,%.2f, low=%.2f\n", min_co, min_lb,
				weight * min_co + min_lb);
		return weight * min_co + min_lb;
	}
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

double CutOverGraphSizesSparsity::calculate_upper_bound(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<int>& which_structure) {
	printf("XXX: NOT YET IMPLEMENTED\n");
	return 1.0;
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

double CutOverSmallerGraphSizeSparsity::calculate_upper_bound(
		const std::vector<std::vector<double> >& transitions,
		const std::vector<int>& which_structure) {
	printf("XXX: NOT YET IMPLEMENTED\n");
	return 1.0;
}

static Sparsity* _parse_cut_over_edge_cost_sparsity(OptionParser &parser) {
	parser.document_synopsis("CutOverEdgeCostSparsity",
			"( C(E) + W1 ) / ( (C(E(S1))*C(E(S2))) + W2).");

	parser.add_option<double>("w1", "weight on LB", "0");
	parser.add_option<double>("w2", "weight on CO", "0");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new CutOverEdgeCostSparsity(opts);
}

static Sparsity* _parse_estimated_efficiency(OptionParser &parser) {
	parser.document_synopsis("EstimatedEfficiency", "w * CO + LB");

	/**
	 * Possible Estimation on Estimated Efficiency
	 * 1. add: w*CO + LB
	 * 2. mul: (1+w*CO)(LB-1)
	 */

	parser.add_option<string>("def", "definition of estimation", "add");

	parser.add_option<double>("weight", "weight on CO", "1.0");
	parser.add_option<bool>("unitnode", "true of nodes are unit cost.", "true");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new EstimatedEfficiency(opts);
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

static Plugin<Sparsity> _plugin_estimated_efficiency("esti",
		_parse_estimated_efficiency);

static Plugin<Sparsity> _plugin_cut_over_graph_sizes_sparsity(
		"cut_over_graph_sizes", _parse_cut_over_graph_sizes_sparsity);

static Plugin<Sparsity> _plugin_cut_over_smaller_graph_size_sparsity(
		"cut_over_smaller_graph_size",
		_parse_cut_over_smaller_graph_size_sparsity);
