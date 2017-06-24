#ifndef SYM_SOLUTION_H
#define SYM_SOLUTION_H

#include "sym_variables.h"
#include <vector> 

class SymBDExp;

class SymSolution {
	SymBDExp * exp;
	int g, h;
	BDD cut;
public:
	SymSolution() :
			g(-1), h(-1) {
	} //No solution yet

	SymSolution(SymBDExp * e, int g_val, int h_val, BDD S) :
			exp(e), g(g_val), h(h_val), cut(S) {
	}

	void getPlan(std::vector<const Operator *> & path) const;

	ADD getADD() const;

	inline bool solved() {
		return g + h >= 0;
	}

	inline int getCost() const {
		return g + h;
	}
};
#endif
