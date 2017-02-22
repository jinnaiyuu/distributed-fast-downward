#ifndef SYM_TRANSITION_H
#define SYM_TRANSITION_H

#include "../operator.h"
#include "sym_variables.h"
//#include "../cudd-2.5.0/cudd/cudd.h"
//#include "../cudd-2.5.0/obj/cuddObj.hh"

#include <set>
#include <vector>

class SymAbstraction;
class SymManager;
class SymPDB;
class SymSMAS;
/*
 * Represents a symbolic transition.
 * It has two differentiated parts: label and abstract state transitions
 * Label refers to variables not considered in the merge-and-shrink
 * Each label has one or more abstract state transitions
 */
class SymTransition {
	SymVariables * sV; //To call basic BDD creation methods
	int cost; // transition cost
	BDD tBDD; // bdd for making the relprod

	std::vector<int> effVars; //FD Index of eff variables. Must be sorted!!
	BDD existsVars, existsBwVars; // Cube with variables to existentialize
	std::vector<BDD> swapVarsS, swapVarsSp; // Swap variables s to sp and viceversa
	std::vector<BDD> swapVarsA, swapVarsAp; // Swap abstraction variables

	std::set<const Operator *> ops; //List of operators represented by the TR

	const SymAbstraction * absAfterImage;
public:
	//Constructor for abstraction transitions
	/*SymTransition(const SymVariables * sV, const Operator * op,
	 int cost_, const SymAbstraction & abs);*/

	//Constructor for transitions irrelevant for the abstraction
	SymTransition(SymVariables * sVars, const Operator * op, int cost_);

	//Copy constructor
	SymTransition(const SymTransition &) = default;

	BDD image(const BDD & from) const;
	BDD preimage(const BDD & from) const;
	BDD image(const BDD & from, int maxNodes) const;
	BDD preimage(const BDD & from, int maxNodes) const;

	void edeletion(SymManager & mgr); //Includes mutex into the transition

	void merge(const SymTransition & t2, int maxNodes);

	//shrinks the transition to another abstract state space (useful to preserve edeletion)
	void shrink(const SymAbstraction & abs, int maxNodes);

	bool setMaSAbstraction(SymAbstraction * abs, const BDD & bddSrc,
			const BDD & bddTarget);

	inline void setAbsAfterImage(const SymAbstraction * abs) {
		absAfterImage = abs;
	}

	inline int getCost() const {
		return cost;
	}
	inline int nodeCount() const {
		return tBDD.nodeCount();
	}
	inline const std::set<const Operator *> &getOps() const {
		return ops;
	}

	inline bool hasOp(std::set<const Operator *> ops) const {
		for (const auto & op : ops) {
			if (ops.count(op)) {
				return true;
			}
		}
		return false;
	}

	inline const BDD & getBDD() const {
		return tBDD;
	}

	friend std::ostream & operator<<(std::ostream &os,
			const SymTransition & tr);
};

#endif
