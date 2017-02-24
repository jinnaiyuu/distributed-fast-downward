#include "sym_manager.h"

#include "sym_enums.h" 
#include "../debug.h"
#include <queue>
#include "sym_abstraction.h"
#include "sym_util.h"
#include "../globals.h"
#include "../timer.h"
#include "test/sym_test_mutex.h" 

using namespace std;

#ifdef __CDT_PARSER__
#undef DEBUG_MSG
#define DEBUG_MSG(a)  ;
#endif

SymManager::SymManager(SymVariables * v, SymAbstraction * abs,
		const SymParamsMgr & params) :
		vars(v), abstraction(abs), p(params), parentMgr(nullptr), initialState(
				v->zeroBDD()), goal(v->zeroBDD()), min_transition_cost(0), hasTR0(
				false), mutexInitialized(false), mutexByFluentInitialized(false) {
	for (const auto & op : g_operators) {
		if (min_transition_cost == 0 || min_transition_cost > op.get_cost()) {
			min_transition_cost = op.get_cost();
		}
		if (op.get_cost() == 0) {
			hasTR0 = true;
		}
	}
}

SymManager::SymManager(SymManager * mgr, SymAbstraction * abs,
		const SymParamsMgr & params) :
		vars(mgr->getVars()), abstraction(abs), p(params), parentMgr(mgr), initialState(
				mgr->zeroBDD()), goal(mgr->zeroBDD()), min_transition_cost(0), hasTR0(
				false),

		mutexInitialized(false), mutexByFluentInitialized(false) {
	if (mgr) {
		min_transition_cost = mgr->getMinTransitionCost();
		hasTR0 = mgr->hasTransitions0();
	} else {
		for (const auto & op : g_operators) {
			if (min_transition_cost == 0
					|| min_transition_cost > op.get_cost()) {
				min_transition_cost = op.get_cost();
			}
			if (op.get_cost() == 0) {
				hasTR0 = true;
			}
		}
	}
}

void SymManager::init_states() {
	DEBUG_MSG(cout << "INIT STATES" << endl ;);
	if (abstraction && abstraction->isAbstracted()) {
		DEBUG_MSG(cout << "INIT STATES ABS" << endl ;);
		initialState = abstraction->getInitialState();
		goal = abstraction->getGoal();
		initialState.print(0, 1);
		goal.print(0, 1);
	} else {
		DEBUG_MSG(cout << "INIT STATES NO ABS" << endl ;);
		initialState = vars->getStateBDD(g_initial_state());
		goal = vars->getPartialStateBDD(g_goal);
	}DEBUG_MSG(cout << "INIT STATES DONE" << endl ;);
}

//TODO: WARNING INIT_MUTEX with abstractions could be better.
void SymManager::init_mutex(const std::vector<MutexGroup> & mutex_groups) {
//If (a) is initialized OR not using mutex OR edeletion does not need mutex
	if (mutexInitialized || p.mutex_type == MutexType::MUTEX_NOT)
		return; //Skip mutex initialization

//Do I really need
	if (/*p.init_mutex_from_parent &&*/parentMgr && abstraction
			&& abstraction->isAbstracted()) {
		setTimeLimit(p.max_mutex_time);
		DEBUG_MSG(cout << "Init mutex from parent" << endl ;);
		mutexInitialized = true;
		//Initialize mutexes from other manager
		try {
			for (auto & bdd : parentMgr->notMutexBDDsFw) {
				BDD shrinked = abstraction->shrinkExists(bdd, p.max_mutex_size);
				notMutexBDDsFw.push_back(shrinked);
			}
			for (auto & bdd : parentMgr->notMutexBDDsBw) {
				BDD shrinked = abstraction->shrinkExists(bdd, p.max_mutex_size);
				notMutexBDDsBw.push_back(shrinked);
			}
			unsetTimeLimit();
		} catch (BDDError e) {
			unsetTimeLimit();
			//Forget about it
			vector<BDD>().swap(notMutexBDDsFw);
			vector<BDD>().swap(notMutexBDDsBw);
			init_mutex(mutex_groups, true, false);
		}
		//We will compute mutex by fluent on demand
	} else {
		if (p.mutex_type == MutexType::MUTEX_EDELETION) {
			init_mutex(mutex_groups, true, true);
		} else {
			init_mutex(mutex_groups, true, false);
		}
	}
}

void SymManager::init_mutex(const std::vector<MutexGroup> & mutex_groups,
		bool genMutexBDD, bool genMutexBDDByFluent) {

//Check if I should initialize something and return
	if (mutexInitialized)
		genMutexBDD = false;
	if (mutexByFluentInitialized)
		genMutexBDDByFluent = false;
	if (!genMutexBDD && !genMutexBDDByFluent)
		return;
	if (genMutexBDD)
		mutexInitialized = true;
	if (genMutexBDDByFluent)
		mutexByFluentInitialized = true;

	if (genMutexBDDByFluent) {
		//Initialize structure for exactlyOneBDDsByFluent (common to both init_mutex calls)
		exactlyOneBDDsByFluent.resize(g_variable_domain.size());
		for (size_t i = 0; i < g_variable_domain.size(); ++i) {
			exactlyOneBDDsByFluent[i].resize(g_variable_domain[i]);
			for (size_t j = 0; j < g_variable_domain[i]; ++j) {
				exactlyOneBDDsByFluent[i][j] = oneBDD();
			}
		}
	}

	init_mutex(mutex_groups, genMutexBDD, genMutexBDDByFluent, false);
	init_mutex(mutex_groups, genMutexBDD, genMutexBDDByFluent, true);
}

void SymManager::init_mutex(const std::vector<MutexGroup> & mutex_groups,
		bool genMutexBDD, bool genMutexBDDByFluent, bool fw) {
	DEBUG_MSG(
			cout << "Init mutex BDDs " << (fw ? "fw" : "bw") << ": " << genMutexBDD << " " << genMutexBDDByFluent << endl ;);

	vector<vector<BDD>> & notMutexBDDsByFluent = (
			fw ? notMutexBDDsByFluentFw : notMutexBDDsByFluentBw);

	vector<BDD> & notMutexBDDs = (fw ? notMutexBDDsFw : notMutexBDDsBw);

//BDD validStates = vars->oneBDD();
	int num_mutex = 0;
	int num_invariants = 0;

	if (genMutexBDDByFluent) {
		//Initialize structure for notMutexBDDsByFluent
		notMutexBDDsByFluent.resize(g_variable_domain.size());
		for (size_t i = 0; i < g_variable_domain.size(); ++i) {
			notMutexBDDsByFluent[i].resize(g_variable_domain[i]);
			for (size_t j = 0; j < g_variable_domain[i]; ++j) {
				notMutexBDDsByFluent[i][j] = oneBDD();
			}
		}
	}

//Initialize mBDDByVar and invariant_bdds_by_fluent
	vector<BDD> mBDDByVar;
	mBDDByVar.reserve(g_variable_domain.size());
	vector<vector<BDD>> invariant_bdds_by_fluent(g_variable_domain.size());
	for (int i = 0; i < invariant_bdds_by_fluent.size(); i++) {
		mBDDByVar.push_back(oneBDD());
		invariant_bdds_by_fluent[i].resize(g_variable_domain[i]);
		for (int j = 0; j < invariant_bdds_by_fluent[i].size(); j++) {
			invariant_bdds_by_fluent[i][j] = oneBDD();
		}
	}

	for (auto & mg : mutex_groups) {
		if (mg.pruneFW() != fw)
			continue;
		const vector<pair<int, int> > & invariant_group = mg.getFacts();
		DEBUG_MSG(cout << mg << endl ;);
		if (mg.isExactlyOne()) {
			BDD bddInvariant = zeroBDD();
			int var = numeric_limits<int>::max();
			int val = 0;
			bool exactlyOneRelevant = true;

			for (auto & fluent : invariant_group) {
				if (abstraction && !abstraction->isRelevantVar(fluent.first)) {
					exactlyOneRelevant = true;
					break;
				}
				bddInvariant += vars->preBDD(fluent.first, fluent.second);
				if (fluent.first < var) {
					var = fluent.first;
					val = fluent.second;
				}
			}

			if (exactlyOneRelevant) {
				num_invariants++;
				if (genMutexBDD) {
					invariant_bdds_by_fluent[var][val] *= bddInvariant;
				}
				if (genMutexBDDByFluent) {
					for (auto & fluent : invariant_group) {
						exactlyOneBDDsByFluent[fluent.first][fluent.second] *=
								bddInvariant;
					}
				}
			}
		}

		for (int i = 0; i < invariant_group.size(); ++i) {
			int var1 = invariant_group[i].first;
			if (abstraction && !abstraction->isRelevantVar(var1))
				continue;
			int val1 = invariant_group[i].second;
			BDD f1 = vars->preBDD(var1, val1);

			for (int j = i + 1; j < invariant_group.size(); ++j) {
				int var2 = invariant_group[j].first;
				if (abstraction && !abstraction->isRelevantVar(var2))
					continue;
				int val2 = invariant_group[j].second;
				BDD f2 = vars->preBDD(var2, val2);
				BDD mBDD = !(f1 * f2);
				if (genMutexBDD) {
					num_mutex++;
					mBDDByVar[min(var1, var2)] *= mBDD;
				}
				if (genMutexBDDByFluent) {
					notMutexBDDsByFluent[var1][val1] *= mBDD;
					notMutexBDDsByFluent[var2][val2] *= mBDD;
				}
			}
		}
	}

	if (genMutexBDD) {
		for (int var = 0; var < g_variable_domain.size(); ++var) {
			if (!mBDDByVar[var].IsOne()) {
				notMutexBDDs.push_back(mBDDByVar[var]);
			}
			for (const BDD & bdd_inv : invariant_bdds_by_fluent[var]) {
				if (!bdd_inv.IsOne()) {
					notMutexBDDs.push_back(bdd_inv);
				}
			}
		}

		DEBUG_MSG(dumpMutexBDDs(fw) ;);
		merge(vars, notMutexBDDs, mergeAndBDD, p.max_mutex_time,
				p.max_mutex_size);
		std::reverse(begin(notMutexBDDs), end(notMutexBDDs));
		DEBUG_MSG(
				cout << "Mutex initialized " << (fw ? "fw" : "bw") << ". Total mutex added: " << num_mutex << " Invariant groups: " << num_invariants << endl ;);
		dumpMutexBDDs(fw);
	}

//gst_mutex.check_mutexes(*this);
}

void SymManager::dumpMutexBDDs(bool fw) const {
	if (fw) {
		cout << "Mutex BDD FW Size(" << p.max_mutex_size << "):";
		for (const auto & bdd : notMutexBDDsFw) {
			cout << " " << bdd.nodeCount();
		}
		cout << endl;
	} else {

		cout << "Mutex BDD BW Size(" << p.max_mutex_size << "):";
		for (const auto & bdd : notMutexBDDsBw) {
			cout << " " << bdd.nodeCount();
		}
		cout << endl;
	}

}

//TODO: Shrink directly the merged transitions
const map<int, vector<SymTransition> > & SymManager::getIndividualTRs() {
	if (indTRs.empty()) {
		if (abstraction && abstraction->isAbstracted() && parentMgr) {
			cout << "Initialize individual TRs of " << *abstraction
					<< " IS NOT IMPLEMENTED!!" << endl;
			exit(-1);
			/*SymManager * mgr_parent = parentMgr;
			 while(mgr_parent && mgr_parent->indTRs.empty()){
			 mgr_parent = mgr_parent->parentMgr;
			 }

			 for(const auto & trsParent : mgr_parent->indTRs){
			 int cost = trsParent.first;
			 for(const auto & trParent : trsParent.second){
			 SymTransition absTransition = SymTransition(trParent);
			 //Shrink is not very restricted
			 if(absTransition.shrink(*this, p.max_tr_time, p.max_tr_size)){
			 indTRs[cost].push_back(move(absTransition));
			 }else{
			 //TODO: Switch to other method
			 cout << "Error while shrinking transition: " << absTransition << endl;
			 exit(-1);
			 }
			 }
			 }*/
		} else {
			DEBUG_MSG(
					cout << "Initialize individual TRs of original state space" << endl ;);
			for (int i = 0; i < g_operators.size(); i++) {
				const Operator * op = &(g_operators[i]);
				// Skip spurious operators
				// There are no more spurious operators anymore (removed in preprocess)
				/*if (op->spurious){
				 continue;
				 }*/
				int cost = op->get_cost();
				DEBUG_MSG(
						cout << "Creating TR of op " << i << " of cost " << cost << endl ;);
				indTRs[cost].push_back(move(SymTransition(vars, op, cost)));
				if (p.mutex_type == MutexType::MUTEX_EDELETION) {
					indTRs[cost].back().edeletion(*this);
				}
			}
		}
	}
	return indTRs;
}

void SymManager::init_transitions() {
	if (!transitions.empty())
		return; //Already initialized!

	DEBUG_MSG(cout << "Init transitions" << endl ;);

	if (abstraction && abstraction->isAbstracted() && parentMgr) {
		DEBUG_MSG(cout << "Init transitions from parent state space" << endl ;);
		SymManager * mgr_parent = parentMgr;
		while (mgr_parent && mgr_parent->transitions.empty()) {
			mgr_parent = mgr_parent->parentMgr;
		}
		const auto & trsParent = mgr_parent->getTransitions();

		while (mgr_parent && mgr_parent->indTRs.empty()) {
			mgr_parent = mgr_parent->parentMgr;
		}
		const auto & indTRsParent = mgr_parent->getIndividualTransitions();

		abstraction->shrinkTransitions(trsParent, indTRsParent, transitions,
				p.max_tr_time, p.max_tr_size);
		return;
	}

	DEBUG_MSG(cout << "Generate individual TRs" << endl ;);
	transitions = map<int, vector<SymTransition>>(getIndividualTRs()); //Copy
	DEBUG_MSG(cout << "Individual TRs generated" << endl ;);
	min_transition_cost = 0;
	hasTR0 = transitions.count(0) > 0;

	for (map<int, vector<SymTransition> >::iterator it = transitions.begin();
			it != transitions.end(); ++it) {
		// This function merges individual transitions into a single big TR.
		// Individual TRs are removed from ::transitions and only stored in indTRs.
		merge(vars, it->second, mergeTR, p.max_tr_time, p.max_tr_size);

		if (min_transition_cost == 0 || min_transition_cost > it->first) {
			min_transition_cost = it->first;
		}
		cout << "TRs cost=" << it->first << " (" << it->second.size() << "): ";
		for (auto bdd : it->second) {
			cout << " " << bdd.nodeCount();
		}
		cout << endl;
	}
}

void SymManager::zero_preimage(const BDD & bdd, vector<BDD> & res,
		int nodeLimit) const {
	for (int i = res.size(); i < transitions.at(0).size(); i++) {
		res.push_back(transitions.at(0)[i].preimage(bdd, nodeLimit));
	}
}

void SymManager::zero_image(const BDD & bdd, vector<BDD> & res,
		int nodeLimit) const {
	for (int i = res.size(); i < transitions.at(0).size(); i++) {
		res.push_back(transitions.at(0)[i].image(bdd, nodeLimit));
	}
}

void SymManager::cost_preimage(const BDD & bdd, map<int, vector<BDD> > &res,
		int nodeLimit) const {
	for (auto trs : transitions) {
		int cost = trs.first;
		if (cost == 0)
			continue;
		for (int i = res[cost].size(); i < trs.second.size(); i++) {
			BDD result = trs.second[i].preimage(bdd, nodeLimit);
			res[cost].push_back(result);
		}
	}
}

void SymManager::cost_image(const BDD & bdd, map<int, vector<BDD> > &res,
		int nodeLimit) const {
	for (auto trs : transitions) {
		int cost = trs.first;
		if (cost == 0)
			continue;
		for (int i = res[cost].size(); i < trs.second.size(); i++) {
			//cout << "Img: " << trs.second[i].nodeCount() << " with bdd " << bdd.nodeCount() << " node limit: " << nodeLimit << endl;
			BDD result = trs.second[i].image(bdd, nodeLimit);
			//cout << "Res: " << result.nodeCount() << endl;
			res[cost].push_back(result);
		}
	}
}

BDD SymManager::filter_mutex(const BDD & bdd, bool fw, int nodeLimit,
		bool initialization) {
	BDD res = bdd;
//if(fw) return bdd; //Only filter mutex in backward direction
	const vector<BDD> & deadEndBDDs = (fw ? deadEndFw : deadEndBw);
	for (const BDD & deadEnd : deadEndBDDs) {
		DEBUG_MSG(
				cout << "Filter: " << res.nodeCount() << " and dead end " << deadEnd.nodeCount() << flush ;);
		res = res.And(!deadEnd, nodeLimit);
		DEBUG_MSG(cout << ": " << res.nodeCount() << endl ;);
	}
	const vector<BDD> & notMutexBDDs = (fw ? notMutexBDDsFw : notMutexBDDsBw);
	switch (p.mutex_type) {
	case MutexType::MUTEX_NOT:
		break;
	case MutexType::MUTEX_EDELETION:
		if (initialization) {
			for (const BDD & notMutexBDD : notMutexBDDs) {
				//cout << res.nodeCount()  << " and " <<  notMutexBDD.nodeCount()  << flush;
				res = res.And(notMutexBDD, nodeLimit);
				//cout << ": " << res.nodeCount() << endl;
			}
		}
		break;
	case MutexType::MUTEX_AND:
		for (const BDD & notMutexBDD : notMutexBDDs) {
			DEBUG_MSG(
					cout << "Filter: " << res.nodeCount() << " and " << notMutexBDD.nodeCount() << flush ;);
			res = res.And(notMutexBDD, nodeLimit);
			DEBUG_MSG(cout << ": " << res.nodeCount() << endl ;);
		}
		break;
	case MutexType::MUTEX_RESTRICT:
		for (const BDD & notMutexBDD : notMutexBDDs)
			res = res.Restrict(notMutexBDD);
		break;
	case MutexType::MUTEX_NPAND:
		for (const BDD & notMutexBDD : notMutexBDDs)
			res = res.NPAnd(notMutexBDD);
		break;
	case MutexType::MUTEX_CONSTRAIN:
		for (const BDD & notMutexBDD : notMutexBDDs)
			res = res.Constrain(notMutexBDD);
		break;
	case MutexType::MUTEX_LICOMP:
		for (const BDD & notMutexBDD : notMutexBDDs)
			res = res.LICompaction(notMutexBDD);
		break;
	}
	return res;
}

int SymManager::filterMutexBucket(vector<BDD> & bucket, bool fw,
		bool initialization, int maxTime, int maxNodes) {
	int numFiltered = 0;
	setTimeLimit(maxTime);
	try {
		for (int i = 0; i < bucket.size(); ++i) {
			DEBUG_MSG(
					cout << "Filter spurious " << (fw ? "fw" : "bw") << ": "; if (abstraction) { cout << *abstraction; } else { cout << "original state space"; } cout << " from: " << bucket[i].nodeCount() << " maxTime: " << maxTime << " and maxNodes: " << maxNodes;);

			bucket[i] = filter_mutex(bucket[i], fw, maxNodes, initialization);
			DEBUG_MSG(cout << " => " << bucket[i].nodeCount() << endl ;);
			numFiltered++;
		}
	} catch (BDDError e) {
		DEBUG_MSG(cout << " truncated." << endl ;);
	}
	unsetTimeLimit();

	return numFiltered;
}

void SymManager::init_hashFunction(int np) {
	if (hashFunction.size() > 0) {
		return;
	}
	hashFunction.resize(np);
	int nvars = ceil(log2(np));
	printf("nvars=%d\n", nvars);
	for (int i = 0; i < hashFunction.size(); ++i) {
		BDD r = oneBDD();
		int rest = i;
		printf("hash %d: ", i);
		for (int j = 0; j < nvars; ++j) {
			bool n = rest % 2;
			rest = rest / 2;
			if (n) {
				printf("%d", j);
				// BDDvariables are aligned so that every even index represent a variable for precondition.
				// Thus we need to double j.
				r = r * vars->bddVar(j * 2);
			} else {
				printf("NOT%d", j);
				r = r * !vars->bddVar(j * 2);
			}
		}
		printf("\n");
		hashFunction[i] = r;
	}
}

void SymManager::apply_hash(const BDD & states, std::vector<BDD> & res) {
	assert(res.size() == 0);
	for (int i = 0; i < hashFunction.size(); ++i) {
		BDD r = hashFunction[i].And(states);
		res.push_back(r);
	}
}
