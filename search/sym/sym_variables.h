#ifndef SYM_VARIABLES_H
#define SYM_VARIABLES_H

#include "../cudd-2.5.0/include/cuddObj.hh"

#include "../timer.h"
#include "../globals.h"
#include "../operator.h"
#include <math.h>
#include <ext/hash_map>
#include <memory>
#include <utility>
#include <iostream> 
#include <fstream> 
#include <sstream> 
#include <vector> 
#include <string> 
#include <map>
#include <set>

/*
 * BDD-Variables for a symbolic exploration.
 * This information is global for every class using symbolic search.
 * The only decision fixed here is the variable ordering, which is assumed to be always fixed. 
 */
struct BDDError {
};
extern void exceptionError(std::string message);

class SymParamsMgr;

class SymVariables {
	std::unique_ptr<Cudd> _manager; //_manager associated with this symbolic search

	int numBDDVars; //Number of binary variables (just one set, the total number is numBDDVars*3
	std::vector<BDD> variables; // BDD variables

	//The variable order must be complete.
	std::vector<int> var_order; //Variable(FD) order in the BDD
	std::vector<std::vector<int> > bdd_index_pre, bdd_index_eff, bdd_index_abs; //vars(BDD) for each var(FD)

	std::vector<std::vector<BDD> > preconditionBDDs; // BDDs associated with the precondition of a predicate
	std::vector<std::vector<BDD> > effectBDDs; // BDDs associated with the effect of a predicate
	std::vector<BDD> biimpBDDs; //BDDs associated with the biimplication of one variable(FD)

	//Vector to store the binary description of an state
	//Avoid allocating memory during heuristic evaluation
	std::vector<int> binState;

public:
	void init(const std::vector<int> & v_order, const SymParamsMgr & params);
	BDD readBDD(const std::string & filename) const;
	void readBucket(std::ifstream & filenames, std::vector<BDD> & bucket) const;
	void writeBucket(const std::string & filename, std::ofstream & filenames,
			const std::vector<BDD> & bucket) const;

	void writeMap(const std::string & fname, std::ofstream & filenames,
			const std::map<int, BDD> & m) const;

	void readMap(std::ifstream & filenames, std::map<int, BDD> & m) const;

	void writeMapBucket(const std::string & fname, std::ofstream & filenames,
			const std::map<int, std::vector<BDD>> & mb) const;

	void readMapBucket(std::ifstream & filenames,
			std::map<int, std::vector<BDD>> & mb) const;

	State getStateFrom(const BDD & bdd) const;
	BDD getStateBDD(const State & state) const;
	BDD getPartialStateBDD(
			const std::vector<std::pair<int, int> > & state) const;
	double numStates(const BDD & bdd) const; //Returns the number of states in a BDD
	bool isIn(const State & state, const BDD & bdd) const;

	inline const std::vector<int> & vars_index_pre(int variable) const {
		return bdd_index_pre[variable];
	}
	inline const std::vector<int> & vars_index_eff(int variable) const {
		return bdd_index_eff[variable];
	}
	inline const std::vector<int> & vars_index_abs(int variable) const {
		return bdd_index_abs[variable];
	}

	// Return a BDD which represents a precondition vi = xij.
	// i = variable, j = value
	inline const BDD &preBDD(int variable, int value) const {
		return preconditionBDDs[variable][value];
	}

	inline const BDD &effBDD(int variable, int value) const {
		return effectBDDs[variable][value];
	}

	// BDD which represents a variable of the precondition vi.
	inline BDD getCubePre(int var) {
		return getCube(var, bdd_index_pre);
	}
	inline BDD getCubePre(const std::set<int> & vars) {
		return getCube(vars, bdd_index_pre);
	}

	inline BDD getCubeEff(int var) {
		return getCube(var, bdd_index_eff);
	}
	inline BDD getCubeEff(const std::set<int> & vars) {
		return getCube(vars, bdd_index_eff);
	}

	inline BDD getCubeAbs(int var) {
		return getCube(var, bdd_index_abs);
	}
	inline BDD getCubeAbs(const std::set<int> & vars) {
		return getCube(vars, bdd_index_abs);
	}

	inline const BDD & biimp(int variable) const {
		return biimpBDDs[variable];
	}

	inline long totalNodes() const {
		return _manager->ReadNodeCount();
	}

	inline std::vector<BDD> getBDDVarsPre() const {
		return getBDDVars(var_order, bdd_index_pre);
	}
	inline std::vector<BDD> getBDDVarsEff() const {
		return getBDDVars(var_order, bdd_index_eff);
	}
	inline std::vector<BDD> getBDDVarsAbs() const {
		return getBDDVars(var_order, bdd_index_abs);
	}
	inline std::vector<BDD> getBDDVarsPre(const std::vector<int> & vars) const {
		return getBDDVars(vars, bdd_index_pre);
	}
	inline std::vector<BDD> getBDDVarsEff(const std::vector<int> & vars) const {
		return getBDDVars(vars, bdd_index_eff);
	}
	inline std::vector<BDD> getBDDVarsAbs(const std::vector<int> & vars) const {
		return getBDDVars(vars, bdd_index_abs);
	}

	inline unsigned long totalMemory() const {
		return _manager->ReadMemoryInUse();
	}

	inline BDD zeroBDD() const {
		return _manager->bddZero();
	}

	inline BDD oneBDD() const {
		return _manager->bddOne();
	}

	inline Cudd * mgr() const {
		return _manager.get();
	}

	inline BDD bddVar(int index) const {
		return variables[index];
	}

	inline int usedNodes() const {
		return _manager->ReadSize();
	}

	inline void setTimeLimit(int maxTime) {
		_manager->SetTimeLimit(maxTime);
		_manager->ResetStartTime();
	}

	inline void unsetTimeLimit() {
		_manager->UnsetTimeLimit();
	}

	inline int * getBinaryDescription(const State & state) {
		int pos = 0;
		//  cout << "State " << endl;
		for (int i = 0; i < var_order.size(); i++) {
			int v = var_order[i];
			//cout << v << "=" << state[v] << " " << g_variable_domain[v] << " assignments and  " << binary_len[v] << " variables   " ;
			//preconditionBDDs[v] [state[v]].PrintMinterm();

			for (int j = 0; j < bdd_index_pre[v].size(); j++) {
				binState[pos++] = ((state[v] >> j) % 2);
				binState[pos++] = 0; //Skip interleaving variable
			}
		}
		/* cout << "Binary description: ";
		 for(int i = 0; i < pos; i++){
		 cout << binState[i];
		 }
		 cout << endl;*/

		return &(binState[0]);
	}

	inline ADD getADD(int value) {
		return _manager->constant(value);
	}

	inline ADD getADD(std::map<int, BDD> heur) {
		ADD h = getADD(-1);
		for (const auto & entry : heur) {
			int distance = 1 + entry.first;
			h += entry.second.Add() * getADD(distance);
		}
		return h;
	}

private:
	//Auxiliar function helping to create precondition and effect BDDs
	//Generates value for bddVars.
	BDD generateBDDVar(const std::vector<int> & _bddVars, int value) const;
	BDD getCube(int var, const std::vector<std::vector<int>> & v_index) const;
	BDD getCube(const std::set<int> & vars,
			const std::vector<std::vector<int>> & v_index) const;
	BDD createBiimplicationBDD(const std::vector<int> & vars,
			const std::vector<int> & vars2) const;
	std::vector<BDD> getBDDVars(const std::vector<int> & vars,
			const std::vector<std::vector<int>> & v_index) const;

	// variable = vi = {x0, x1,...}
	// value    = xj
	// Generate BDDVar which represents precondition that: vi=xj.
	inline BDD createPreconditionBDD(int variable, int value) const {
		return generateBDDVar(bdd_index_pre[variable], value);
	}

	// Generate BDDVar which represents action effect that: vi=xj.
	inline BDD createEffectBDD(int variable, int value) const {
		return generateBDDVar(bdd_index_eff[variable], value);
	}

	inline int getNumBDDVars() const {
		return numBDDVars;
	}
};

#endif

//BDD cube; //Set of variables representing S
//BDD cubep; //Set of variables representing S'
//BDD s2sp; // Permutation for S -> S'
//BDD sp2s; // Permutation for S' -> S
//std::vector<BDD> S; // S variables (current state)
//std::vector<BDD> Sp; // S' variables (next state)

