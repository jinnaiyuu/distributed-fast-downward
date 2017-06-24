/*
 * ZobristHash.h
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#ifndef METISHASH_H_
#define METISHASH_H_

#include "../state.h"
#include "../state_var_t.h"
#include "../globals.h"
#include "../rng.h"
#include "distribution_hash.h"

#include <vector>
#include <map>

class OptionParser;
class Options;
class Operator;
class DomainTransitionGraph;


// TODO: Maybe we can group up map based hashes (zbr, abst, sz,...)
class MetisHash: public DistributionHash {
public:
	MetisHash(const Options &opts);
	unsigned int hash(const State& state);
	unsigned int hash(const state_var_t* state);
	unsigned int hash_incremental(const State& state,
			const unsigned int parent_d_hash, const Operator* op);
	std::string hash_name();
protected:
	std::map<std::vector<state_var_t>, unsigned int> trie;
};

#endif /* ZOBRISTHASH_H_ */
