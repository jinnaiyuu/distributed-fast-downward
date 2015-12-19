/*
 * ZobristHash.h
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#ifndef FREQ_DEPEND_HASH_H_
#define FREQ_DEPEND_HASH_H_

#include "../state.h"
#include "../state_var_t.h"
#include "../globals.h"
#include "../rng.h"
#include "distribution_hash.h"

#include <vector>

class OptionParser;
class Options;
class Operator;
class DomainTransitionGraph;

class FrequentDependentHash: public MapBasedHash {
public:
	FrequentDependentHash(const Options &options);
	std::string hash_name();
private:
//	void divideIntoTwo(unsigned int var,
//			std::vector<std::vector<unsigned int> >& structures);
	double ignore_threshold;
	double structure_threshold;
};

#endif /* ZOBRISTHASH_H_ */
