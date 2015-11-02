/*
 * ZobristHash.h
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#ifndef DISTRIBUTIONHASH_H_
#define DISTRIBUTIONHASH_H_

#include "../state.h"
#include "../state_var_t.h"
#include "../globals.h"
#include "../rng.h"

#include <vector>

class OptionParser;
class Options;
class Operator;
class DomainTransitionGraph;


class DistributionHash {
public:
	DistributionHash(const Options &options);
	virtual ~DistributionHash();
	virtual unsigned int hash(const State& state) = 0;
	virtual unsigned int hash(const state_var_t* state) = 0;

	virtual unsigned int hash_incremental(const State& state,
			const unsigned int parent_d_hash, const Operator* op) = 0;

	virtual std::string hash_name() = 0;

	// TODO: implement incremental hashing for all those applicable
//	bool incremental;
//	virtual unsigned int hash_incremental(const State& state) = 0;

};

// TODO: Maybe we can group up map based hashes (zbr, abst, sz,...)
class MapBasedHash: public DistributionHash {
public:
	MapBasedHash(const Options &opt);
	unsigned int hash(const State& state);
	unsigned int hash(const state_var_t* state);
	unsigned int hash_incremental(const State& state,
			const unsigned int parent_d_hash, const Operator* op);
protected:
	std::vector<std::vector<unsigned int> > map;
};

class ZobristHash: public MapBasedHash {
public:
	ZobristHash(const Options &opts);
	std::string hash_name();
};

// Well, let's call it AbstractionHash as Abstraction is way too overloaded.
class AbstractionHash: public MapBasedHash {
public:
	AbstractionHash(const Options &opts);
	std::string hash_name();

private:
	double abstraction;
};

class FeatureBasedStructuredZobristHash: public MapBasedHash {
public:
	FeatureBasedStructuredZobristHash(const Options &opts);
	std::string hash_name();

private:
	void divideIntoTwo(unsigned int var,
			std::vector<std::vector<unsigned int> >& structures);
	double abstraction;

};

class ActionBasedStructuredZobristHash: public MapBasedHash {
public:
	ActionBasedStructuredZobristHash(const Options &opts);
	std::string hash_name();
private:
	double abstraction;
};

#endif /* ZOBRISTHASH_H_ */
