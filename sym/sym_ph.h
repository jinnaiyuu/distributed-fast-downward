#ifndef SYM_PH_H
#define SYM_PH_H

#include "../option_parser.h"
#include "sym_abstraction.h"
#include "sym_enums.h" 
#include "sym_params.h"
#include "sym_bdexp.h"
#include <memory>

class SymExploration;
class SymVariables;
class SymHierarchy;
class SymHNode;
class SymController;
class SymManager;

/*
 * The hierarchy policy incrementally builds a policy by inserting
 * children nodes on the nodes.
 *
 * The default policy does not build any abstraction (for bidirectional Dijkstra)
 */
class SymPH {
protected:
	SymController * engine;
	SymVariables * vars;
	//Common parameters to every hierarchy policy
	const SymParamsMgr mgrParams;
	const SymParamsSearch searchParams; //Parameters to perform the abstract searches
	const double phTime, phMemory;

	//Maximum time and nodes to perform the whole? step? relaxation process
	const int maxRelaxTime, maxRelaxNodes;

	//How to compute the TRs of the abstract state space.
	const AbsTRsStrategy absTRsStrategy;

	//Parameters to decide the relaxation
	const bool perimeterPDBs; //Initializes explorations with the one being relaxed.
	const RelaxDirStrategy relaxDir;
	const double ratioRelaxTime, ratioRelaxNodes;

	const double shouldAbstractRatio;
	const int maxNumAbstractions;
	int numAbstractions;

	//Special parameter for spmas heuristic
	bool ignore_if_useful;

	//Other parameters to actually prove that their default values are the right ones :-)
	//const bool forceHeuristic; //always forces heuristic computation
	//const bool heuristicAbstract;  //If abstract state spaces are
	//allowed to use others as heuristic
	//const bool replaceAbstraction;   //If true, the relaxed exploration
	//substitutes the previous (instead
	//of having both)

	//The following attributes are used directly by the PH strategies:
	//BDDs to detect spurious states.
	std::vector<BDD> notMutexBDDs;

	//If should abstract is activated, we store a list with perimeters
	//of the original exploration to initialize our abstractions.
	std::vector<std::unique_ptr<SymBDExp>> expPerimeters;

	//Intermediate heuristics for fw and bw search
	std::vector<std::map<int, BDD>> intermediate_heuristics_fw,
			intermediate_heuristics_bw;

	//Abstraction that may produce an explicit heuristic
	SymAbstraction * finalAbstraction;

	//Initialization of the hierarchy policy in case that it is needed.
	virtual bool init() = 0;

public:
	SymPH(const Options & opts);
	virtual ~SymPH() {
	}

	virtual SymBDExp * relax(SymBDExp * bdExp, SymHNode * iniHNode, Dir dir,
			int num_relaxations) = 0;
	virtual void dump_options() const;

	bool init(SymController * eng, SymVariables * v, SymManager * mgr);

	// Relax bdExp to get a new exploration as heuristic Depending on
	// parameters, calls relax_binary_search, relax_one_by_one or
	// relax_one_by_one_reverse
	inline SymBDExp * relax(SymBDExp * bdExp) {
		return relax(bdExp, bdExp->getHNode(), getDir(bdExp), 1);
	}
	inline SymBDExp * relax(SymBDExp * bdExp, Dir dir) {
		return relax(bdExp, bdExp->getHNode(), dir, 1);
	}

	inline SymBDExp * relax(SymBDExp * bdExp, SymHNode * iniHNode,
			int num_relaxations) {
		return relax(bdExp, iniHNode, getDir(bdExp), num_relaxations);
	}

	//Let the hierarchy policy take the control in case she wants to do
	//something.
	void operate(SymBDExp * origSearch);

	//Ask the hierarchy policy to add new explorations or heuristics
	//Returns true if it did something
	bool askHeuristic(SymBDExp * origSearch, double allotedTime);

	static void add_options_to_parser(OptionParser & parser,
			const std::string & default_tr_st, int abstraction_limit);

	inline const SymParamsMgr & getMgrParams() const {
		return mgrParams;
	}

	inline void setIgnoreIfUseful() {
		ignore_if_useful = true;
	}

	const std::vector<std::map<int, BDD>> & get_intermediate_heuristics_fw() {
		return intermediate_heuristics_fw;
	}

	const std::vector<std::map<int, BDD>> & get_intermediate_heuristics_bw() {
		return intermediate_heuristics_bw;
	}

protected:
	Dir getDir(SymBDExp * bdExp) const;

	SymBDExp * addHeuristicExploration(SymBDExp * oldExp,
			std::unique_ptr<SymBDExp> && newExp);

	//Initializes the newBDExp to serve as heuristic for bdExp Calls
	//initFrontier and initAll.  If suceeds, returns true If it does not
	//suceed, returns false and, if the new exploration is not useful,
	//sets the hNode as notUseful.
	bool relax_in(SymBDExp * bdExp, std::unique_ptr<SymBDExp> & newExp,
			SymHNode * hNode, int num_relaxations) const;

	std::unique_ptr<SymBDExp> createBDExp(Dir dir, SymBDExp * bdExp) const;

};

#endif
