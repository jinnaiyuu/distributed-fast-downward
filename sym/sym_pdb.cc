#include "sym_pdb.h"

#include "sym_util.h"
#include "../globals.h"
#include "sym_transition.h"

using namespace std;

SymPDB::SymPDB(SymVariables * bdd_vars) :
		SymAbstraction(bdd_vars, AbsTRsStrategy(0)) {

	for (int i = 0; i < g_variable_name.size(); i++) {
		fullVars.insert(i);
	}

	nonRelVarsCube = bdd_vars->oneBDD();
	nonRelVarsCubeWithPrimes = bdd_vars->oneBDD();
	if (!nonRelVarsCube.IsCube()) {
		cout << "Error in sym_pdb: nonRelVars should be a cube";
		nonRelVarsCube.print(0, 1);
		cout << endl;
		exit(-1);
	}

}
SymPDB::SymPDB(SymVariables * _vars, AbsTRsStrategy absTRsStrategy,
		const set<int> & relevantVars) :
		SymAbstraction(_vars, absTRsStrategy) {
	fullVars = relevantVars;
	for (int i = 0; i < g_variable_name.size(); i++) {
		if (!fullVars.count(i)) {
			nonRelVars.insert(i);
		}
	}

	nonRelVarsCube = _vars->getCubePre(nonRelVars); // * vars->getCubep(nonRelVars);
	nonRelVarsCubeWithPrimes = nonRelVarsCube * _vars->getCubeEff(nonRelVars);
	if (!nonRelVarsCube.IsCube()) {
		cout << "Error in sym_pdb: nonRelVars should be a cube";
		nonRelVarsCube.print(0, 1);
		cout << endl;
		exit(-1);
	}
}

BDD SymPDB::shrinkExists(const BDD & bdd, int maxNodes) const {
	return bdd.ExistAbstract(nonRelVarsCube, maxNodes);
}

BDD SymPDB::shrinkTBDD(const BDD & bdd, int maxNodes) const {
	return bdd.ExistAbstract(nonRelVarsCubeWithPrimes, maxNodes);
}

BDD SymPDB::shrinkForall(const BDD & bdd, int maxNodes) const {
	return bdd.UnivAbstract(nonRelVarsCube, maxNodes);
}

/*void SymPDB::getTransitions(map<int, std::vector <SymTransition> > & trs) const{
 cout << "Initialize trs "<< *this << endl;
 for(int i = 0; i < g_operators.size(); i++){
 const Operator * op = &(g_operators[i]);
 // Skip spurious operators
 if (op->spurious){
 continue;
 }
 int cost = op->get_cost();

 if(cost == 0){
 trs[0].push_back(SymTransition(vars, op, cost, *this));
 }else{
 trs[cost].push_back(SymTransition(vars, op, cost, *this));
 }
 }
 }*/

BDD SymPDB::getInitialState() const {
	vector<pair<int, int> > abstract_ini;
	for (int var : fullVars) {
		abstract_ini.push_back(pair<int, int>(var, g_initial_state()[var]));
	}
	cout << "VARS" << endl;
	cout << "VARS: " << vars << endl;
	return vars->getPartialStateBDD(abstract_ini);

}

BDD SymPDB::getGoal() const {
	vector<pair<int, int> > abstract_goal;
	for (auto goal_var : g_goal) {
		if (isRelevantVar(goal_var.first)) {
			abstract_goal.push_back(goal_var);
		}
	}
	return vars->getPartialStateBDD(abstract_goal);
}

std::string SymPDB::tag() const {
	return "PDB";
}

void SymPDB::print(std::ostream &os, bool fullInfo) const {
	os << "PDB " << fullVars.size() << "," << nonRelVars.size();
	if (fullInfo && !nonRelVars.empty()) {
		os << " [";
		for (int v : fullVars)
			os << v << " ";
		os << "]";
		os << endl << "Abstracted propositions: ";
		for (int v : nonRelVars) {
			os << v << ": ";
			for (auto & prop : g_fact_names[v])
				cout << prop << ", ";
			os << endl;
		}
		os << endl << "Considered propositions: ";
		for (int v : fullVars) {
			os << v << ": ";
			for (auto & prop : g_fact_names[v])
				os << prop << ", ";
			os << endl;
		}
		os << endl;
	}
}

ADD SymPDB::getExplicitHeuristicADD(bool /*fw*/) {
	return vars->getADD(0);
}
void SymPDB::getExplicitHeuristicBDD(bool /*fw*/, map<int, BDD> & res) {
	res[0] = vars->oneBDD();
}
