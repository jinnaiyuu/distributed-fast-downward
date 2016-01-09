/*
 * ZobristHash.h
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#ifndef CUT_STRATEGY_H_
#define CUT_STRATEGY_H_

#include "../domain_transition_graph.h"
#include "sparsity.h"

#include <stdio.h>
#include <string>

//class OptionParser;
//class Options;

// Sparsest cut problem is to divide the graph into two subgraphs sparse to each other.

class CutStrategy {
public:
	CutStrategy(const Options &options);
	virtual ~CutStrategy();
	virtual void cut(unsigned int var,
			vector<vector<unsigned int> >& structures) = 0;
};

// Icaps
class TwoGroupsAndRest: public CutStrategy {
public:
	TwoGroupsAndRest(const Options &options);
	void cut(unsigned int var, vector<vector<unsigned int> >& structures);

};

class RandomUpdating: public CutStrategy {
public:
	RandomUpdating(const Options &options);
	void cut(unsigned int var, vector<vector<unsigned int> >& structures);

private:
	void build_transition_matrix();
	int n_update;
	Sparsity* sparsity;
	bool always_cut;
	string edge_weight;
	std::vector<std::vector<std::vector<double> > > transitions;
};


#endif /* ZOBRISTHASH_H_ */
