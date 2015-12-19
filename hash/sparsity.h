/*
 * ZobristHash.h
 *
 *  Created on: Oct 18, 2015
 *      Author: yuu
 */

#ifndef SPARSITY_H_
#define SPARSITY_H_

#include "../domain_transition_graph.h"
#include "../option_parser.h"
#include "../plugin.h"

#include <stdio.h>
#include <string>



// Sparsest cut problem is to divide the graph into two subgraphs sparse to each other.


class Sparsity {
public:
	Sparsity(const Options &options);
	virtual ~Sparsity();
	virtual double calculate_sparsity(
			const std::vector<std::vector<double> >& transitions,
			const std::vector<bool>& which_structure) = 0;
};

class CutOverEdgeCostSparsity: public Sparsity {
public:
	CutOverEdgeCostSparsity(const Options &options);
	double calculate_sparsity(
			const std::vector<std::vector<double> >& transitions,
			const std::vector<bool>& which_structure);
};

class CutOverGraphSizesSparsity: public Sparsity {
public:
	CutOverGraphSizesSparsity(const Options &options);
	double calculate_sparsity(
			const std::vector<std::vector<double> >& transitions,
			const std::vector<bool>& which_structure);
};

class CutOverSmallerGraphSizeSparsity: public Sparsity {
public:
	CutOverSmallerGraphSizeSparsity(const Options &options);
	double calculate_sparsity(
			const std::vector<std::vector<double> >& transitions,
			const std::vector<bool>& which_structure);
};



#endif /* ZOBRISTHASH_H_ */
