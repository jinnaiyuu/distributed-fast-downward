/*
 * ZobristHash.cc
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#include "feature_and_action_hash.h"
#include "distribution_hash.h"
#include "../plugin.h"
#include "../operator.h"
#include "../domain_transition_graph.h"

#include <stdio.h>
#include <string>

using namespace std;

FeatureAndActionStructureHash::FeatureAndActionStructureHash(const Options &opts) :
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
				// TODO: here we apply ActionBasedStructure
				// For all actions which moves from
				map[i][structures[j][k]] = r;
			}
//			printf("\n");
		}

	}

	for (int i = 0; i < map.size(); ++i) {
		for (int j = 0; j < map[i].size(); ++j) {
			printf("%u ", map[i][j]);
		}
		printf("\n");
	}

}

std::string FeatureAndActionStructureHash::hash_name() {
	std::ostringstream os;
	os << abstraction;
	std::string str = "feature_action(" + os.str() + ")";
	return str;
}

static DistributionHash*_parse_feature_action(OptionParser &parser) {
	parser.document_synopsis("FrequentDependentHash",
			"It deploy different structure for each feature"
					" according to the number of operator changing its value.");

	parser.add_option<double>("abstraction",
			"Ignore variables ranked higher than this threshold.", "0.7");

	parser.add_option<bool>("isPolynomial",
			"Use polynomial hashing for underlying load balancing scheme.", "false");
//	parser.add_option<double>("structure_threshold",
//			"Build feature-based structure for varialbe ranked higher than this threshold.",
//			"0.3");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new FeatureAndActionStructureHash(opts);
}

static Plugin<DistributionHash> _plugin_freq_depend("feature_action",
		_parse_feature_action);
