/*
 * ZobristHash.cc
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#include "freq_depend_hash.h"
#include "distribution_hash.h"
#include "../plugin.h"
#include "../operator.h"
#include "../domain_transition_graph.h"

#include "cut_strategy.h"

#include <stdio.h>
#include <string>

using namespace std;

FrequentDependentHash::FrequentDependentHash(const Options &opts) :
		MapBasedHash(opts) {

	ignore_threshold = opts.get<double>("ignore_threshold");
	structure_threshold = opts.get<double>("structure_threshold");

	CutStrategy* cut_strategy = opts.get<CutStrategy *>("cut_strategy");

	// TODO: Implement sparsest cut as a plug-in.
	// So far we only need a single string for a parameter.
	// TODO: integrate ICAPS cut into SparsestCut.

	printf("ignore_threshold = %f\n", ignore_threshold);
	printf("structure_threshold = %f\n", structure_threshold);

	vector<int> tmp = get_frequency_rank();
	vector<double> n_op_functioning_rank;

	for (int i = 0; i < tmp.size(); ++i) {
		n_op_functioning_rank.push_back(
				(double) tmp[i] / (double) g_variable_domain.size());
	}

	g_rng.seed(717);

	for (int i = 0; i < map.size(); ++i) {
//		printf("n_op[%d] = %d\n", i, n_op_functioning[n_op_functioning_rank[i]].first);
		printf("n_op_rank[%d] = %f\n", i, n_op_functioning_rank[i]);
//		double irank = (double) n_op_functioning_rank[i] / g_variable_domain.size();

		if (n_op_functioning_rank[i] > ignore_threshold) {
			// ignore (abstract out) variable with too high freq.
			continue;
		} else if (n_op_functioning_rank[i] >= structure_threshold
				&& map[i].size() > 2) {

			vector<vector<unsigned int> > structures;

			cut_strategy->cut(i, structures);

//			printf("structure size=%lu\n", structures.size());
			for (int j = 0; j < structures.size(); ++j) {
//				printf("structure[j] size=%lu\n", structures[j].size());
				unsigned int r = g_rng.next32();
				for (int k = 0; k < structures[j].size(); ++k) {
//					printf("%u ", structures[j][k]);
					map[i][structures[j][k]] = r;
				}
//				printf("\n");
			}
		} else {
			for (int j = 0; j < map[i].size(); ++j) {
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

std::string FrequentDependentHash::hash_name() {
	std::ostringstream os;
	os << ignore_threshold << "-" << structure_threshold;
	std::string str = "freq_depend(" + os.str() + ")";
	return str;
}

static DistributionHash* _parse_freq_depend(OptionParser &parser) {
	parser.document_synopsis("FrequentDependentHash",
			"It deploy different structure for each feature"
					" according to the number of operator changing its value.");

	parser.add_option<double>("ignore_threshold",
			"Ignore variables ranked higher than this threshold.", "0.6");

	parser.add_option<double>("structure_threshold",
			"Build feature-based structure for varialbe ranked higher than this threshold.",
			"0.0");

	parser.add_option<CutStrategy *>("cut_strategy",
			"Cut method for each domain transition graph", "random_updating");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new FrequentDependentHash(opts);
}

static Plugin<DistributionHash> _plugin_freq_depend("freq_depend",
		_parse_freq_depend);
