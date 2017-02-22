#ifndef SYM_EXPLORATION_H
#define SYM_EXPLORATION_H

#include "../debug.h" 
#include "sym_heuristic.h"
#include "sym_manager.h"
#include "sym_closed.h"
#include "sym_estimate.h"
#include "sym_util.h"
#include <vector>
#include <map>
#include <memory>

/*
 * This class allows to perform a BDD search.  It is designed to
 * mantain the current state in the search.  We consider four
 * different points at which we may truncate the search:
 * pop(), filter_mutex(), expand_zero(), expand_cost()
 * We mantain 3 BDDs to know the current state: Stmp, S and Szero.
 * Briefly:
 * 1) if Sfilter, Szero and S are empty => pop() => Szero.
 * 2) else if Stfilter => filter_mutex() => Szero
 * 3) else if Szero => expand_zero => S (passing by Sfilter)
 * 4) else (S must have something) => expand_cost()
 * 
 * Zero cost operators have been expanded iff !S.IsZero() && Szero.IsZero()
 */
class SymController;
class SymBDExp;

//We use this enumerate to know why the current operation was truncated
enum class TruncatedReason {
	FILTER_MUTEX, MERGE_BUCKET, MERGE_BUCKET_COST, IMAGE_ZERO, IMAGE_COST
};
std::ostream & operator<<(std::ostream &os, const TruncatedReason & dir);

typedef std::vector<BDD> Bucket;
class SymExploration {
	//Attributes that characterize the search:
	SymBDExp * bdExp;
	SymManager * mgr; //Symbolic manager to perform bdd operations
	SymParamsSearch p;
	bool fw; //Direction of the search. true=forward, false=backward
	SymExploration * parent; //Parent of the search

	//Current state of the search:
	std::map<int, Bucket> open; //States in open with unkwown h-value
	//Auxiliar open list for states that have been closed (we are sure about their optimal cost) but not expanded (truncated and then improved the heuristic value).
	std::map<int, Bucket> reopen;

	Bucket Sfilter; //current g-bucket without duplicates and h-classified (still not filtered mutexes)
	Bucket Smerge; // bucket before applying merge
	Bucket Szero; // bucket to expand 0-cost transitions
	Bucket S; // bucket to expand cost transitions

	//bucket to store temporary image results in expand_zero() and expand_cost()
	//For each BDD in Szero or S, stores a map with pairs <cost, resImage>
	std::vector<std::map<int, Bucket>> Simg;

	std::unique_ptr<SymClosed> closed; // Closed list
	int f, g; // f and g value of current bucket (S, Szero, Sfilter and Smerge)
	std::map<int, std::set<int>> acceptedValues;

	//To seek the next f,g bucket, we have two different sets: hValues.
	//hValues contains all the possible h-values returned by the heuristics.
	//std::set<int> hValues;   //Possible h-values
	SymClosed * perfectHeuristic; //The perfect heuristic for this state space
	std::vector<SymHeuristic> heuristics; //List of non-perfect heuristics
	std::set<int> hValuesExplicit;

	SymStepCostEstimation estimationCost, estimationZero; //Time/nodes estimated
	// NOTE: This was used to estimate the time and nodes needed to
	//perform a step in case that the next bucket is still not prepared.
	//Now, we always prepare the next bucket and when that fails no
	//estimation is needed (the exploration is deemed as not searchable
	//and is worse than any other exploration which has its next bucket
	//to expand ready)
	//SymStepCostEstimation estimationDisjCost, estimationDisjZero;
	bool lastStepCost; //If the last step was a cost step (to know if we are in estimationDisjCost or Zero
	SymController * engine; //Access to the bound and notification of new solutions

	bool bucketReady() const {
		/*cout << "bucket ready " << !(Szero.empty() && S.empty() &&
		 Sfilter.empty() && Smerge.empty()) << endl;*/
		return !(Szero.empty() && S.empty() && Sfilter.empty() && Smerge.empty());
	}

	inline bool expansionReady() const {
		return Sfilter.empty() && Smerge.empty()
				&& !(Szero.empty() && S.empty());
	}
	inline bool initialization() const {
		return g == 0 && lastStepCost;
	}

	/*
	 * Check generated or closed states with other frontiers.  In the
	 * original state space we obtain a solution (maybe suboptimal if
	 * the states are not closed).  In an abstract state space, it just
	 * recomputes the current S bucket to expand of less abstracted
	 * state spaces.  Returns those states that were not cut by the
	 * other search. If close_states is true, it closes the states.
	 */
	void checkCut(Bucket & bucket, int g, bool close_states);

	/*Get the next set from the open list and update g and f.
	 Remove duplicate and spurious states. */
	void pop();
	void setNextG();
	void setNextF();

	void getNextFHValues(const std::map<int, std::vector<BDD>> & open,
			const std::set<int> & h_values, int f,
			std::pair<int, int> & upper_bound) const;

	bool prepareBucket(/*int maxTime, int maxNodes, bool afterPop*/);

	/* Apply 0-cost operators over Szero. */
	/* Puts the result on Szero. */
	/* Includes the result on S or open (depends on the heuristic value).*/
	bool expand_zero(int maxTime, int maxNodes);

	/* Apply cost-operators over S. */
	/* Insert S on closed. */
	/* Insert successors on open.  */
	bool expand_cost(int maxTime, int maxNodes);

	// Returns the subset with h_value h
	BDD compute_heuristic(const BDD & from, int fVal, int hVal);

	void computeEstimation(bool prepare);

	//void debug_pop();

	//////////////////////////////////////////////////////////////////////////////
public:
	SymExploration(SymController * eng, const SymParamsSearch & params);
	SymExploration(const SymExploration &) = delete;
	SymExploration(SymExploration &&) = default;
	SymExploration& operator=(const SymExploration&) = delete;
	SymExploration& operator=(SymExploration &&) = default;
	~SymExploration() {
		DEBUG_MSG(std::cout << "DELETED EXPLORATION: " << *this << std::endl
		;
	);}

inline bool finished() const {
	return open.empty() && !bucketReady();
}

const std::map<int, Bucket> & getOpen() const {
	return open;
}

const std::map<int, Bucket> & getReopen() const {
	return reopen;
}

bool stepImage() {
	return stepImage(p.getAllotedTime(nextStepTime()),
			p.getAllotedNodes(nextStepNodesResult()));
}
bool stepImage(int maxTime, int maxNodes);

bool init(SymBDExp * exp, SymManager * manager, bool fw); //Init forward or backward search

//Initialize another search process by reutilizing information of this search
//calls to 5 methods are needed.
//1) init(), prepares the data of the other exploration.
void init(SymBDExp * exp, SymExploration * other);
//2) init2() reopens closed states in other frontier and initializes g, f
//Should be called right after init is executed on both frontiers.
void init2(SymExploration * opposite);
//Then, relaxFrontier only relaxes the first bucket to expand.
//The caller should check if expansion is feasible and useful
//Finally, all the open list is relaxed to the new abstract state space
bool relaxFrontier(SymManager * manager, int maxTime, int maxNodes);
bool relax(int maxTime, int maxNodes);
void relaxClosed();

void addHeuristic(const SymHeuristic & heuristic);
void setPerfectHeuristic(SymClosed * h);

//Adds a new heuristic to evaluate States
void setChild(SymExploration * child) {
	closed->addChild(child->getClosed());
}

// void getUsefulExplorations(set <SymExploration *> & explorations, double minRatioUseful);

//double computeRatioUseful(SymHeuristic * h) const;

//void notifyH(SymClosed * heur, int value, bool isNotClosed);
//void notifyF(SymClosed * heur, int value);
void notifyPrunedBy(int fVal, int gVal);
void notify(const Bucket & bucket, int fNotClosed = 0); //May prune
void notifyNotClosed(int fValue, int hValue);

void getPossiblyUsefulExplorations(
		std::vector<SymExploration *> & potentialExps) {
	perfectHeuristic->potentiallyUsefulFor(this, potentialExps);
}

bool isBetter(const SymExploration & other) const;

SymExploration * getOpposite() const {
	if (perfectHeuristic)
		return perfectHeuristic->getExploration();
	else
		return nullptr;
}

inline bool isSearchable() const {
	return isSearchableWithNodes(p.maxStepNodes);
}

inline bool isSearchableAfterRelax(int num_relaxations) const {
	double maxNodes = p.maxStepNodes;
	if (num_relaxations) {
		maxNodes *= pow(p.ratioAfterRelax, num_relaxations);
	}
	return isSearchableWithNodes((int) maxNodes);
}

bool isSearchableWithNodes(int maxNodes) const;

inline bool isUseful() {
	return isUseful(p.ratioUseful);
}

inline bool isUseful(double ratio) {
	return !isAbstracted() || closed->isUseful(ratio);
}

double ratioUseful(Bucket & bucket) const;

// Pointer to the closed list Used to set as heuristic of other explorations.
inline SymClosed * getClosed() const {
	return closed.get();
}

inline const SymManager * get_mgr() const {
	return mgr;
}

inline SymExploration * getParent() const {
	return parent;
}

inline Bucket getSfilter() const {
	return Sfilter;
}

inline Bucket getSmerge() const {
	return Smerge;
}

inline Bucket getSzero() const {
	return Szero;
}

inline Bucket getS() const {
	return S;
}

inline int getF() const {
	return f;
}

inline int getG() const {
	return g;
}

inline bool isFW() const {
	return fw;
}

inline bool isAbstracted() const {
	return mgr->getAbstraction() != nullptr
			&& mgr->getAbstraction()->isAbstracted();
}

SymAbstraction * getAbstraction() const {
	return mgr->getAbstraction();

}

inline SymBDExp * getBDExp() const {
	return bdExp;
}

inline BDD getClosedTotal() {
	return closed->getClosed();
}

inline BDD notClosed() {
	return closed->notClosed();
}

void desactivate() {
	closed->desactivate();
}

void reactivate() {
	closed->reactivate();
}

long nextStepTime() const;
long nextStepNodes() const;
long nextStepNodesResult() const;

//Returns the nodes that have been expanded by the algorithm (closed without the current frontier)
BDD getExpanded() const;
void getNotExpanded(Bucket & res) const;

void write(const std::string & file) const;
void init(SymBDExp * exp, SymManager * manager, const std::string & file);

inline void removeZero(Bucket & bucket) const {
	bucket.erase(
			remove_if(begin(bucket), end(bucket),
					[] (BDD & bdd) {return bdd.IsZero();}), end(bucket));
}

inline SymController * getEngine() const {
	return engine;
}

private:

inline int nodeCount(const Bucket & bucket) const {
	int sum = 0;
	for (const BDD & bdd : bucket) {
		sum += bdd.nodeCount();
	}
	return sum;
}

inline double stateCount(const Bucket & bucket) const {
	double sum = 0;
	for (const BDD & bdd : bucket) {
		sum += mgr->getVars()->numStates(bdd);
	}
	return sum;
}

void shrinkBucket(Bucket & bucket, int maxNodes);

inline void moveBucket(Bucket & bucket, Bucket & res) {
	copyBucket(bucket, res);
	Bucket().swap(bucket);
}

inline void copyBucket(Bucket & bucket, Bucket & res) {
	if (!bucket.empty()) {
		res.insert(end(res), begin(bucket), end(bucket));
	}
}

//  void addCountStates(SymHeuristic * h, const Bucket & bucket, double & possible, double & total) const;

//BDD closedByParents(const BDD & bdd, SymHeuristic * heur) const;
//BDD notClosedByParents(const BDD & bdd, SymHeuristic * heur) const;

inline int minG() {
	int minG = g;
	/*cout << "MIN_G" << endl;
	 for (auto & op : open){
	 cout << op.first << " ";
	 }
	 cout << endl;*/
	if (!open.empty()) {
		minG = std::min(minG, open.begin()->first);
	}
	if (!mgr->hasTransitions0()) {
		//Compute the max to avoid exceed into negative numbers
		minG = std::max(minG, minG + mgr->getMinTransitionCost());
	}
	return minG;
}

bool mergeBucket(Bucket & bucket, int maxTime, int maxNodes) {
	auto mergeBDDs = [] (BDD bdd, BDD bdd2, int maxNodes) {
		return bdd.Or(bdd2, maxNodes);
	};
	merge(mgr->getVars(), bucket, mergeBDDs, maxTime,
			std::min(maxNodes, p.max_disj_nodes));
	removeZero(bucket); //Be sure that we do not contain only the zero BDD
	//cout << "BUCKET MERGED TO: " << bucket.size() << endl;
	return maxNodes >= p.max_disj_nodes || bucket.size() <= 1;
}

//Extract states with h-value from list, removing duplicates
void extract_states(Bucket & list, int fVal, int hVal, Bucket & res,
		bool duplicates);

bool extract_states(Bucket & list, const Bucket & pruned, Bucket & res) const;

//Extract states without h-value from list using sym heuristic
/* void extract_states(Bucket & list, int fVal, int hVal, */
/* 		      Bucket & res, SymClosed * heur);  */

void setF(int value);

/* bool stepFDiagonal(int maxTime, int maxNodes); */
/* bool explore(int maxTime, int maxNodes, */
/* 	       int maxStepTime, BDD target = BDD(), */
/* 	       int fTarget = 0, int gTarget = 0); */

/* bool stepExpand(int maxTime, int maxNodes); */

void printFrontier() const;

int frontierNodes() const {
	if (!Szero.empty()) {
		return nodeCount(Szero);
	} else if (!S.empty()) {
		return nodeCount(S);
	} else {
		return nodeCount(Sfilter) + nodeCount(Smerge);
	}
}

int frontierBuckets() const {
	if (!Szero.empty()) {
		return Szero.size();
	} else if (!S.empty()) {
		return S.size();
	} else {
		return Sfilter.size() + Smerge.size();
	}
}

void violated(TruncatedReason reason, double time, int maxTime, int maxNodes);

friend std::ostream & operator<<(std::ostream &os,
		const SymExploration & bdexp);
};
#endif // SYMBOLIC_EXPLORATION

