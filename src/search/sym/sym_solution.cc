#include "sym_solution.h"

#include <vector>       // std::vector
#include "sym_bdexp.h"
#include "../state_registry.h"

using namespace std;

void SymSolution::getPlan(vector<const Operator *> & path) const {
	if (path.empty()) { //This code should be modified to allow appending things to paths
		exp->getPlan(cut, g, h, path);
	}
}

ADD SymSolution::getADD() const {
	vector<const Operator *> path;
	exp->getPlan(cut, g, h, path);

	SymVariables * vars = exp->getManager()->getVars();
	ADD hADD = vars->getADD(-1);
	int h_val = g + h;

	State s(g_initial_state());
	BDD sBDD = vars->getStateBDD(s);
	hADD += sBDD.Add() * (vars->getADD(h_val + 1));
	for (auto op : path) {
		h_val -= op->get_cost();
		s = g_state_registry->get_successor_state(s, *op);
		sBDD = vars->getStateBDD(s);
		hADD += sBDD.Add() * (vars->getADD(h_val + 1));
	}
	return hADD;
}
