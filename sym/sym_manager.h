#ifndef SYM_MANAGER_H
#define SYM_MANAGER_H

#include "sym_abstraction.h"
#include "sym_variables.h"
#include "sym_transition.h"
#include "sym_params.h"
#include "../mutex_group.h"
#include <vector>
#include <map>

/*
 * All the methods may throw exceptions in case the time or nodes are exceeded.
 *
 */
//class SymbolicAbstraction;
class SymExploration;

class SymManager {
private:
	SymVariables * vars;
	SymAbstraction *abstraction;
	SymParamsMgr p;

	//Useful for initialization of mutexes and TRs by relaxation
	SymManager * parentMgr;

	BDD initialState; // initial state
	BDD goal; // bdd representing the true (i.e. not simplified) goal-state

	// ->first is the cost of the transitions.
	std::map<int, std::vector<SymTransition> > transitions; //TRs
	int min_transition_cost; //minimum cost of non-zero cost transitions
	bool hasTR0; //If there is transitions with cost 0
	//Individual TRs: Useful for shrink and plan construction
	// ->first is the cost of the transitions.
	std::map<int, std::vector<SymTransition> > indTRs;

	bool mutexInitialized, mutexByFluentInitialized;

	//BDD representation of valid states (wrt mutex) for fw and bw search
	std::vector<BDD> notMutexBDDsFw, notMutexBDDsBw;

	//Dead ends for fw and bw searches. They are always removed in
	//filter_mutex (it does not matter which mutex_type we are using).
	std::vector<BDD> deadEndFw, deadEndBw;

	//notMutex relative for each fluent
	std::vector<std::vector<BDD>> notMutexBDDsByFluentFw,
			notMutexBDDsByFluentBw;
	std::vector<std::vector<BDD>> exactlyOneBDDsByFluent;

	void init_states();

	void zero_preimage(const BDD & bdd, std::vector<BDD> & res,
			int maxNodes) const;
	void cost_preimage(const BDD & bdd, std::map<int, std::vector<BDD> > & res,
			int maxNodes) const;
	void zero_image(const BDD & bdd, std::vector<BDD> & res,
			int maxNodes) const;
	void cost_image(const BDD & bdd, std::map<int, std::vector<BDD> > & res,
			int maxNodes) const;

public:
	SymManager(SymManager * mgr, SymAbstraction * abs,
			const SymParamsMgr & params);
	SymManager(SymVariables * v, SymAbstraction * abs,
			const SymParamsMgr & params);

	/* SymManager (const SymManager & o) = delete; */
	/* SymManager (SymManager && ) = default; */
	/* SymManager& operator=(const SymManager& ) = delete; */
	/* SymManager& operator=(SymManager &&) = default; */
	/* ~SymManager(){} */

	void init() {
		// init_mutex(g_inconsistent_groups);
//		init_mutex(g_mutex_groups);
		init_transitions();
	}

	//Be careful of calling init_mutex and init_transitions before actually calling filter_mutex or image
	void init_mutex(const std::vector<MutexGroup> & mutex_groups);
	void init_mutex(const std::vector<MutexGroup> & mutex_groups,
			bool genMutexBDDs, bool genMutexBDDsByFluent);
	void init_mutex(const std::vector<MutexGroup> & mutex_groups,
			bool genMutexBDD, bool genMutexBDDByFluent, bool fw);

	//void mutexRegression(int maxTime, int maxNodes);
	void init_transitions();

	const std::map<int, std::vector<SymTransition> > & getIndividualTRs();

	void addDeadEndStates(bool fw, const BDD & bdd) {
		//There are several options here, we could follow with edeletion
		//and modify the TRs, so that the new spurious states are never
		//generated. However, the TRs are already merged and the may get
		//too large. Therefore we just keep this states in another vectors
		//and spurious states are always removed. TODO: this could be
		//improved.
		if (fw) {
			deadEndFw.push_back(bdd);
		} else {
			deadEndBw.push_back(bdd);
		}

	}

	inline BDD shrinkExists(const BDD & bdd, int maxNodes) const {
		return abstraction->shrinkExists(bdd, maxNodes);
	}

	inline BDD shrinkForall(const BDD & bdd, int maxNodes) const {
		return abstraction->shrinkForall(bdd, maxNodes);
	}

	inline long totalNodes() const {
		return vars->totalNodes();
	}

	inline unsigned long totalMemory() const {
		return vars->totalMemory();
	}

	inline const BDD & getGoal() {
		if (goal.IsZero()) {
			init_states();
		}
		return goal;
	}

	inline const BDD & getInitialState() {
		if (initialState.IsZero()) {
			init_states();
		}
		return initialState;
	}

	//Update binState
	inline int * getBinaryDescription(const State & state) const {
		return vars->getBinaryDescription(state);
	}

	inline BDD getBDD(int variable, int value) const {
		return vars->preBDD(variable, value);
	}

	inline Cudd * mgr() const {
		return vars->mgr();
	}

	inline BDD zeroBDD() const {
		return vars->zeroBDD();
	}

	inline BDD oneBDD() const {
		return vars->oneBDD();
	}

	inline const std::vector<BDD> & getNotMutexBDDs(bool fw) {
//		init_mutex(g_mutex_groups);
		return (fw ? notMutexBDDsFw : notMutexBDDsBw);
	}

	inline const std::vector<int> & vars_index_pre(int variable) const {
		return vars->vars_index_pre(variable);
	}

	inline const std::vector<int> & vars_index_eff(int variable) const {
		return vars->vars_index_eff(variable);
	}

	inline const std::vector<int> & vars_index_abs(int variable) const {
		return vars->vars_index_abs(variable);
	}

	inline SymVariables * getVars() const {
		return vars;
	}

	inline SymAbstraction * getAbstraction() const {
		return abstraction;
	}

	inline const SymParamsMgr & getParams() const {
		return p;
	}

	void dumpMutexBDDs(bool fw) const;

	//Methods that require of TRs initialized
	inline int getMinTransitionCost() {
		//init_transitions(); WARNING: not initializing transitions may return a lower min transition cost
		return min_transition_cost;
	}
	inline bool hasTransitions0() {
		//init_transitions(); WARNING: not initializing transitions may cause the mgr to say that has 0-cost transitions even if it does not (the 0 cost transitions are related to non-abstracted variables
		return hasTR0;
	}

	inline const std::map<int, std::vector<SymTransition> > & getTransitions() const {
		return transitions;
	}

	inline const std::map<int, std::vector<SymTransition> > & getIndividualTransitions() const {
		return indTRs;
	}

	inline void zero_image(bool fw, const BDD & bdd, std::vector<BDD> & res,
			int maxNodes) {
		init_transitions();
		if (fw)
			zero_image(bdd, res, maxNodes);
		else
			zero_preimage(bdd, res, maxNodes);
	}

	inline void cost_image(bool fw, const BDD & bdd,
			std::map<int, std::vector<BDD> > & res, int maxNodes) {
		init_transitions();
		if (fw) {
			cost_image(bdd, res, maxNodes);
		} else {
			cost_preimage(bdd, res, maxNodes);
		}
	}

	//Methods that require of mutex initialized
	inline const BDD & getNotMutexBDDFw(int var, int val) {
		init_mutex(*g_mutex_groups, false, true);
		return notMutexBDDsByFluentFw[var][val];
//		printf("TODO: getNotMutetBDDFw\n");
//		return vars->preBDD(0,0);
	}

	//Methods that require of mutex initialized
	inline const BDD & getNotMutexBDDBw(int var, int val) {
		init_mutex(*g_mutex_groups, false, true);
		return notMutexBDDsByFluentBw[var][val];
//		printf("TODO: getNotMutexBDDBw\n");
//		return vars->preBDD(0,0);
	}

	//Methods that require of mutex initialized
	inline const BDD & getExactlyOneBDD(int var, int val) {
		init_mutex(*g_mutex_groups, false, true);
		return exactlyOneBDDsByFluent[var][val];
//		printf("TODO: getExactlyOneBDD\n");
//		return vars->preBDD(0,0);
	}

	BDD filter_mutex(const BDD & bdd, bool fw, int maxNodes,
			bool initialization);

	int filterMutexBucket(std::vector<BDD> & bucket, bool fw, bool initialization,
			int maxTime, int maxNodes);

	inline void setTimeLimit(int maxTime) {
		vars->setTimeLimit(maxTime);
	}

	inline void unsetTimeLimit() {
		vars->unsetTimeLimit();
	}

};

#endif

