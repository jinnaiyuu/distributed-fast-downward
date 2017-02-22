/*
 * ZobristHash.cc
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#include "metis_hash.h"
#include "../plugin.h"
#include "../operator.h"
#include "../domain_transition_graph.h"
#include "../successor_generator.h"

#include <stdio.h>
#include <string>
#include <fstream>

using namespace std;

MetisHash::MetisHash(const Options &opts) :
		DistributionHash(opts) {

	string filename = opts.get<string>("file");

	std::ifstream is(filename.c_str(), std::ifstream::in);

	if (!is.is_open()) {
		printf("ERROR - no file for metis hash: %s\n", filename.c_str());
		exit(0);
	}

	while (true) {
		unsigned int hkey;
		is >> hkey;
		string sep;
		is >> sep;
		vector<state_var_t> state;
		for (int i = 0; i < g_variable_domain.size(); ++i) {
			unsigned int v;
			is >> v;
			state.push_back(v);
		}
		std::pair<vector<state_var_t>, unsigned int> p(state, hkey);
		trie.insert(p);

//		for (int i = 0; i < g_variable_domain.size(); ++i) {
//			printf("%d ", p.first[i]);
//		}
//		printf("= %u\n", p.second);

		if (is.eof())
			break;

	}

}

unsigned int MetisHash::hash(const state_var_t* state) {
	vector<state_var_t> v(state, state + g_variable_domain.size());

//	for (int i = 0; i < g_variable_domain.size(); ++i) {
//		printf("%d ", v[i]);
//	}
//	printf("\n");

	if (trie.find(v) != trie.end()) {
//		printf("found %u\n", trie[v]);
		return trie[v];
	} else {
//		printf("not in dictionary\n");
		return 0; // TODO: randomize
	}
}

unsigned int MetisHash::hash(const State& state) {
	const state_var_t* ss = state.get_raw_data();
	return hash(ss);
}

unsigned int MetisHash::hash_incremental(const State& state,
		const unsigned int parent_d_hash, const Operator* op) {
	return hash(state);
}

std::string MetisHash::hash_name() {
	return "metis";
}

static DistributionHash*_parse_metis(OptionParser &parser) {
	parser.document_synopsis("ActionBasedStructuredZobristHash",
			"Distribution hash for HDA*");

	parser.add_option<string>("file", "filename for metis  hashing",
			"metis_hashing");

	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new MetisHash(opts);
}

static Plugin<DistributionHash> _plugin_metis("metis", _parse_metis);

