#ifndef SYM_PSEL_SYMBA_H
#define SYM_PSEL_SYMBA_H

#include "sym_engine.h"
#include <set>

class SymExploration;
class SymHNode;

class SymBA: public SymEngine {
	// If g_timer() > t_orig => force to search on original state space.
	// A hack to avoid wasting the last remaining time in abstract state space searches
	double t_orig;
	int currentPH;

	//Parameters to control how much to relax the search => moved to PH
	//int maxRelaxTime, maxRelaxNodes; // maximum allowed nodes to relax the search
	//int maxAfterRelaxTime, maxAfterRelaxNodes; // maximum allowed nodes to accept the abstraction after relaxing the search
	//double ratioRelaxTime, ratioRelaxNodes;
	//double ratioAfterRelaxTime, ratioAfterRelaxNodes;
	// Proportion of time in an abstract state space wrt the original state space.
	// If ratio is 0.5 then it is better to explore original with 10 seconds than abstract with 5.1 seconds
	//double ratioAbstract;
	//Percentage of nodes that can potentially prune in the frontier for an heuristic to be useful
	// double percentageUseful;
	//Other parameters to actually prove that their default values are the right ones :-)
	// bool forceHeuristic; //always forces heuristic computation
	//bool heuristicAbstract;  //If abstract state spaces are allowed to use others as heuristic

	inline bool forceOriginal() const {
		return g_timer() > t_orig;
	}

	//Functions that determine the criterion
	//bool canExplore(const SymExploration & exp);

	SymExploration * selectExploration();

public:
	SymBA(const Options &opts);
	virtual ~SymBA() {
	}

	virtual void initialize();
	virtual int step();

	virtual void print_options() const;
};

#endif
