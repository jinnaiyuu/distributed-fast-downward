#ifndef SYM_TEST_H
#define SYM_TEST_H
//Utilities for debugging!

#include <map>
#include <vector>
#include <string>

#include "../sym_variables.h"

class Operator;
class SymAbstraction;
class SymExploration;

class GSTPlanStep {
	int id, g_plan, h_plan; //g and h value in the real plan
	BDD bdd;
	std::map<SymExploration *, int> g_values;

public:

	GSTPlanStep(int _id, int _g, int _h, BDD _bdd) :
			id(_id), g_plan(_g), h_plan(_h), bdd(_bdd) {
	}

	void checkExploration(SymExploration * exp);
	bool checkClose(BDD closedStates, int g_val, bool fw, SymExploration * exp);

	void checkOpen(BDD openStates, int g_val, SymExploration * exp);

	void checkHeuristicValue(BDD states, int h, int f);
	void checkHeuristicValue(BDD states, int h, int f, SymExploration * exp);

	void checkBucket(const std::vector<BDD> & bucket, std::string name);

	inline int getId() const {
		return id;
	}
	inline BDD getBDD() const {
		return bdd;
	}

	friend std::ostream & operator<<(std::ostream &os,
			const GSTPlanStep & step);

};

class GSTPlan {
	int f;
	std::vector<GSTPlanStep> plan;

	void loadPlan(std::string filename, std::vector<Operator *> & plan);

public:
	GSTPlan() :
			f(0) {
	}
	void loadPlan(std::string filename, const SymVariables & vars);
	void checkClose(BDD closedStates, SymExploration * exp);
	void checkOpen(BDD openStates, int g, SymExploration * exp);
	void checkBDD(BDD S) const;
	void checkHeuristicValue(BDD states, int h, int f);
	void checkHeuristicValue(BDD states, int h, int f, SymExploration * exp);
	void checkExploration(SymExploration * exp);

	friend std::ostream & operator<<(std::ostream &os, const GSTPlan & plan);
};

#ifdef DEBUG_GST
extern GSTPlan gst_plan;
#endif

#endif
