/*
 * auto_selection_hash.h
 *
 *  Created on: Oct 23, 2015
 *      Author: yuu
 */

#ifndef AUTO_SELECTION_HASH_H_
#define AUTO_SELECTION_HASH_H_

#include "distribution_hash.h"
#include "../state.h"
#include "../state_var_t.h"
#include "../globals.h"
#include "../rng.h"

class OptionParser;
class Options;
class Operator;

// TODO: implement selection strategy
class AutoSelectionHash: public DistributionHash {
public:
	AutoSelectionHash(const Options &opt);

	unsigned int hash(const State& state);
	unsigned int hash(const state_var_t* state);
	unsigned int hash_incremental(const State& state,
			const unsigned int parent_d_hash, const Operator* op);
	std::string hash_name();
private:
//	void rewrite_option();
	void select_best_hash(std::vector<DistributionHash *>& hashes);
	DistributionHash* selected_hash;
};

#endif /* AUTO_SELECTION_HASH_H_ */
