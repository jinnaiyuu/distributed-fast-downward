#ifndef SYM_CLOSED_H
#define SYM_CLOSED_H

#include "../debug.h"
#include "sym_manager.h"
#include "sym_abstraction.h"
#include <vector>
#include <set>
#include <map>

class SymSolution;
class Timer;

//Auxiliar class to denote an heuristic evaluation
class Evaluation {
public:
	SymExploration * exp;
	std::vector<BDD> bucket; // States to be evaluated
	int f, h; // h and f values needed to prune states

	Evaluation(SymExploration * exploration, int fval, int hval) :
			exp(exploration), f(fval), h(hval) {
	}
};

class SymClosed /*: public SymHeuristic */{
private:
	SymManager * mgr; //Symbolic manager to perform bdd operations
	SymExploration * exploration;

	std::map<int, BDD> closed; // Mapping from cost to set of states

	// Auxiliar BDDs for the number of 0-cost action steps
	// ALERT: The information here might be wrong
	// It is just used to extract path more quickly, but the information
	// here is an (admissible) estimation and this should be taken into account
	std::map<int, std::vector<BDD>> zeroCostClosed;
	BDD closedTotal; // All closed states.

	int hNotClosed, fNotClosed; // Bounds on h and g for those states not in closed
	std::map<int, BDD> closedUpTo; // Disjunction of BDDs in closed  (auxiliar useful to take the maximum between several BDDs)
	std::set<int> h_values; //Set of h_values of the heuristic

	SymClosed * parent;
	std::vector<SymClosed *> children; // Related heuristics derived from relaxations of this search
	std::map<SymExploration *, Evaluation> evals;

public:
	SymClosed();
	void init(SymExploration * exp, SymManager * manager);
	void init(SymExploration * exp, SymManager * manager,
			const SymClosed & other);

	void insert(int h, const BDD & S);
	void setHNotClosed(int h);
	void setFNotClosed(int f);
	void newHValue(int h_value);

	bool can_prune(int fVal, int hVal);
	BDD evaluate(const BDD & bdd, int fVal, int hVal,
			SymExploration * expAsking);
	BDD evaluate(const BDD & bdd, int hVal);
	bool accept(int f, int h) const;
	void getNextF(int f, std::pair<int, int> & upper_bound) const;
	const std::set<int> & getHValues() const {
		return h_values;
	}

	void cleanEvals(SymExploration * exp);
	void setEvaluationF(int f, int h, SymExploration * exp);
	void potentiallyUsefulFor(SymExploration * exp,
			std::vector<SymExploration *> & useful_exps) const;

	bool isUsefulAfterRelax(double ratio,
			const std::vector<BDD> & newFrontier) const;
	bool isUseful(double ratio);

	//Check if any of the states is closed.
	//In case positive, return a solution pair <f_value, S>
	SymSolution checkCut(const BDD & states, int g, bool fw) const;

	void extract_path(const BDD & cut, int h, bool fw,
			std::vector<const Operator *> & path) const;

	inline BDD getClosed() const {
		return closedTotal;
	}

	inline BDD notClosed() const {
		return !closedTotal;
	}

	inline std::map<int, BDD> getClosedList() const {
		return closed;
	}

	inline int getHNotClosed() const {
		return hNotClosed;
	}

	inline int getFNotClosed() const {
		return fNotClosed;
	}

	inline SymExploration * getExploration() {
		return exploration;
	}

	inline void addChild(SymClosed * c) {
		assert(c->parent == nullptr);
		c->parent = this;
		children.push_back(c);
		c->setEvals(evals);
	}

	void desactivate() {
		parent->children.erase(
				std::remove(std::begin(parent->children),
						std::end(parent->children), this),
				std::end(parent->children));

		std::map<SymExploration *, Evaluation>().swap(evals);
	}
	void reactivate() {
		parent->children.push_back(this);
		setEvals(parent->evals);
	}

	void setEvals(const std::map<SymExploration *, Evaluation> & otherEvals);

	void write(const std::string & fname, std::ofstream & file) const;
	void init(SymExploration * exp, SymManager * manager, const std::string & fname,
			std::ifstream & file);

	ADD getHeuristic(int previousMaxH = -1) const;
	void getHeuristic(std::vector<ADD> & heuristics,
			std::vector<int> & maxHeuristicValues) const;

	friend std::ostream & operator<<(std::ostream &os, const SymClosed & c);
};

#endif // SYM_CLOSED

//  void extract_path(const State &state, std::vector <Operator *> & path);
/*  
 void storeHeuristic(const string & filename) const ;*/

//Inherited from Heuristic
//int getH (const BDD & bdd) const;
//std::set<int> getHValues() const;
//BDD getBDD(const BDD & bdd, int fVal, int hVal) const;
//BDD getBDDUpTo(const BDD & bdd, int fVal, int hVal);
//int getH(const State & state);
//virtual void notifyToExploration(SymExploration * exploration);
