#ifndef SYM_HEURISTIC_H
#define SYM_HEURISTIC_H

#include "sym_variables.h"
#include <map>
#include <set>

//SymHeuristic represents a heuristic in the form of 

class SymHeuristic {
	BDD zeroBDD;
	BDD spuriousStates;
	std::map<int, BDD> heuristic;
	std::set<int> hValues; //Possible h values
public:
	SymHeuristic(const SymVariables & vars, const std::map<int, BDD> & heur);

	//Return pruned states (with h > hVal)
	BDD prunedStates(int hVal) const; //Set of states with h(s) > h

	const std::set<int> & getHValues() const {
		return hValues;
	}

	int getMaxValue() const {
		if (hValues.empty()) {
			return 0;
		} else {
			return *(hValues.rbegin());
		}
	}

	const BDD & getDeadEnds() const {
		return spuriousStates;
	}

	//BDD getBDD(const BDD & bdd, int hVal) const; //Subset of states with h(s) == h
	//Return the minimum h value in the set of states
	//virtual int getH (const BDD & bdd) const;

	friend std::ostream & operator<<(std::ostream &os, const SymHeuristic & h);
};

#endif
