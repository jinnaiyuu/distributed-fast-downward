#ifndef SYM_PDB_H
#define SYM_PDB_H

#include "sym_abstraction.h"
#include "sym_variables.h"
#include <set>

class SymPDB: public SymAbstraction {

	BDD nonRelVarsCube; //Cube BDD representing relevantVars
	BDD nonRelVarsCubeWithPrimes; //Cube BDD representing relevantVars
	std::string abstractionName;

public:
	SymPDB(SymVariables * bdd_vars); //Creates a BDD with all variables relevant
	SymPDB(SymVariables * bdd_vars, AbsTRsStrategy absTRsStrategy,
			const std::set<int> & relVars);
	~SymPDB() {
	}
	virtual BDD shrinkExists(const BDD & bdd, int maxNodes) const;
	virtual BDD shrinkForall(const BDD & bdd, int maxNodes) const;
	virtual BDD shrinkTBDD(const BDD & tBDD, int maxNodes) const;

	virtual ADD getExplicitHeuristicADD(bool fw);
	virtual void getExplicitHeuristicBDD(bool fw, std::map<int, BDD> & res);

	//virtual void getTransitions(std::map<int, std::vector <SymTransition> > & res) const;
	virtual BDD getInitialState() const;
	virtual BDD getGoal() const;
	virtual std::string tag() const;

	inline SymVariables * getVars() const {
		return vars;
	}

	virtual void print(std::ostream & os, bool fullInfo) const;
};

#endif
