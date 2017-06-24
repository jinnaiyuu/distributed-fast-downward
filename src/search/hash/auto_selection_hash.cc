/*
 * auto_selection_hash.cc
 *
 *  Created on: Oct 23, 2015
 *      Author: yuu
 */
#include "auto_selection_hash.h"
#include "../plugin.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../search_engine.h"
#include <stdio.h>
#include <mpi.h>


AutoSelectionHash::AutoSelectionHash(const Options &opts) :
		DistributionHash(opts) {
	std::vector<DistributionHash *> hashes = opts.get_list<DistributionHash *>(
			"hashes");
	printf("auto selection candidates are [ ");
	for (int i = 0; i < hashes.size(); ++i) {
		printf("%s ", hashes[i]->hash_name().c_str());
	}
	printf("]\n");

	// Information needed to simulate search engine
	// 1. domain, instance, init, goal -> all in globals
	// 2. heuristic
	// 3.
	select_best_hash(hashes);
}

// TODO: under what circumstance should we terminate the preliminary search?
void AutoSelectionHash::select_best_hash(
		std::vector<DistributionHash *>& hashes) {

	for (int i = 0; i < hashes.size(); ++i) {
	    SearchEngine *engine(0);

	    std::string arg = "hdastar(blind(),"
				+ hashes[i]->hash_name() + ")";
//	    std::string arg = "hdastar(merge_and_shrink(),"
//				+ hashes[i]->hash_name() + ")";
		printf("arg = %s\n", arg.c_str());
//		const char** fake_argv = new const char*[2];

		// TODO: merge_and_shrink is initialized for each run.
		// this is not the efficient way to implement.
		// note that time for auto selection is included in search time.
		OptionParser p(arg, false);
		engine = p.start_parsing<SearchEngine *>();
//		engine = OptionParser::parse_cmd_line(2, fake_argv, false);
		engine->search(); // TODO: termination strategy for auto selection?
		delete engine;
//		MPI_Barrier(MPI_COMM_WORLD);
	}
	printf("selected: %s\n", hashes[0]->hash_name().c_str());
	selected_hash = hashes[0];
}

//void AutoSelectionHash::rewrite_option() {
//
//}

unsigned int AutoSelectionHash::hash(const State& state) {
	return selected_hash->hash(state);
}
unsigned int AutoSelectionHash::hash(const state_var_t* state) {
	return selected_hash->hash(state);

}
unsigned int AutoSelectionHash::hash_incremental(const State& state,
		const unsigned int parent_d_hash, const Operator* op) {
	return selected_hash->hash_incremental(state, parent_d_hash, op);
}

// TODO: not sure what to print
std::string AutoSelectionHash::hash_name() {
	return "auto_selection";
}

static DistributionHash*_parse_auto_selection(OptionParser &parser) {
	parser.document_synopsis("AutoSelection", "Distribution hash for HDA*");

	parser.add_list_option<DistributionHash *>("hashes",
			"Compare the given hashes and select the hash which is most likely to be the best."
					"default is zobrist.", "[zobrist]");
	Options opts = parser.parse();
	if (parser.dry_run())
		return 0;
	else
		return new AutoSelectionHash(opts);
}

static Plugin<DistributionHash> _plugin_auto_selection("auto_selection",
		_parse_auto_selection);
