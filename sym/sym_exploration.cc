#include "sym_exploration.h"
#include "sym_bdexp.h"
#include "sym_abstraction.h"
#include "../debug.h"
#include "sym_util.h"
#include "../timer.h"
#include "../globals.h"
#include "sym_solution.h" 
#include "sym_engine.h"
//#include "../cudd-2.5.0/include/dddmp.h"

#include <sstream>
#include <iostream>
#include <string>
#include <fstream>
#include <string>
#include <algorithm>
#include <memory>

#include "test/sym_test.h"

using namespace std;

#ifdef __CDT_PARSER__
#undef DEBUG_MSG
#define DEBUG_MSG(a) ;
#endif

SymExploration::SymExploration(SymController * eng,
		const SymParamsSearch & params) :
		bdExp(nullptr), mgr(nullptr), p(params), fw(true), parent(nullptr), closed(
				new SymClosed()), f(0), g(0), perfectHeuristic(nullptr), estimationCost(
				params), estimationZero(params),
		//estimationDisjCost(params), estimationDisjZero(params),
		lastStepCost(true), engine(eng) {
}

bool SymExploration::init(SymBDExp * exp, SymManager * manager, bool forward) {
	bdExp = exp;
	mgr = manager;
	fw = forward;
	lastStepCost = true;
	f = 0;
	g = 0;
	//Ensure that the mgr of the original state space is initialized (only to get the planner output cleaner)
	mgr->init();

	DEBUG_MSG(cout << "Init exploration: " << dirname(forward) << *this
	/* << " with mgr: " << manager */<< endl ;);

	if (fw) {
		BDD init = mgr->getInitialState();
		Sfilter.push_back(init);
	} else {
		Sfilter.push_back(mgr->getGoal());
	}DEBUG_MSG(
			cout << dirname(forward) << " initialized to "; Sfilter[0].print(0,1); cout << "Init closed" << endl;);

	closed->init(this, mgr);
	if (!mgr->hasTransitions0()) {
		//Heuristic value for non-goal states is the least cost action by default
		//hValues.insert(1);
		f = minG();
	} else {
		//hValues.insert(0);
	}
#ifdef DEBUG_GST
	gst_plan.checkClose(Sfilter[0], this);
#endif

	closed->insert(0, Sfilter[0]);
	closed->setHNotClosed(minG());
	closed->setFNotClosed(f);

	prepareBucket();

	if (!isAbstracted())
		engine->setLowerBound(f);

	return true;
}

/*
 * TODO: mantain what is possible in closed.  We currently empty the
 * closed list because it is not easy to handle all the interactions
 * with the other frontier, reopen and the current frontier and still
 * mantain admissibility.
 *
 * By keeping the new closed list empty, we can put everything,
 * frontier, open and reopen on the open list.
 */
void SymExploration::init(SymBDExp * exp, SymExploration * other) {
	bdExp = exp;
	fw = other->isFW();
	parent = other;
	p.inheritParentParams(parent->p);

	DEBUG_MSG(
			cout << "Init exploration " << *this << " with (" << *other << ")" << " total_time: " << g_timer() << endl ;);

	if (!other->bucketReady()) {
		cout
				<< "I should not be relaxing a exploration without bucket prepared."
				<< endl;
		exit(0); //TODO: check this exit(0);
	}

	//1) open <-- other.open
	//Get open list, removing duplicates before relaxing
	//This can have prunning power because it is performed before the shrinking.
	for (auto openIt : other->open) {
		int g_val = openIt.first;
		for (const BDD & bdd : openIt.second) {
			BDD tmp = bdd * other->closed->notClosed();
			if (!tmp.IsZero()) {
				open[g_val].push_back(tmp);
			}
		}
	}DEBUG_MSG(cout << "  open copy, total time: " << g_timer() << endl ;);

	//2) open <-- other.reopen
	for (auto openIt : other->reopen) {
		int g_val = openIt.first;
		for (const BDD & bdd : openIt.second) {
			open[g_val].push_back(bdd);
		}
	}

	DEBUG_MSG(cout << "  reopen copied, total time: " << g_timer() << endl ;);

	//3) open[other->g] <-- other.frontier
	copyBucket(other->Sfilter, open[other->g]);
	copyBucket(other->Smerge, open[other->g]);
	copyBucket(other->S, open[other->g]);
	//copyBucket(other->Szero, open[other->g]);
	DEBUG_MSG(cout << "  buckets copied, total time: " << g_timer() << endl ;);
}

void SymExploration::init2(SymExploration * /*opposite*/) {
//Reopen closed states in other that are used to evaluate the
//opposite frontier
//4) open [value] = other->opposite().frontier*other.closed[value]
	/*for(const auto & opFrontier : opposite->open){
	 for(const auto & bddOp : opFrontier.second){
	 BDD reopened = bddOp*parent->closed->getClosed();
	 if(!reopened.IsZero()){
	 for(const auto & cBucket : parent->closed->getClosedList()){
	 BDD reopenedG = cBucket.second*reopened;
	 if(!reopenedG.IsZero()){
	 DEBUG_MSG(cout << "I put some states that were closed in the relaxed open list with cost: " << cBucket.first << endl;);
	 open[cBucket.first].push_back(reopenedG);
	 }
	 }
	 }
	 }
	 }*/

//5) Set g and f
	g = open.begin()->first;
	f = g;

//6) Set new frontier
	open[g].swap(Sfilter);
	open.erase(g);

//Hack: Call to mergeBucket in parent because I do not have a manager yet.
	parent->mergeBucket(Sfilter, p.max_pop_time, p.max_pop_nodes);
	DEBUG_MSG(cout << "  merge done, total time: " << g_timer() << endl ;);
}

void SymExploration::setPerfectHeuristic(SymClosed * h) {
	if (!parent || !parent->isAbstracted()) {
		perfectHeuristic = h; //Do not use any heuristic for abstractions
//on the 2nd or more level

//Evaluate states with the perfect heuristic (to apply notifications)
		extract_states(S, f, f - g, reopen[g], false);
		S.swap(reopen[g]);
		if (reopen[g].empty()) {
			DEBUG_MSG(cout << "LALALA: I did not extract anything" << endl ;);
			reopen.erase(g);
		} else {
			DEBUG_MSG(cout << "LALALA: I extracted something" << endl ;);
			if (!open.count(g)) {
				open[g] = Bucket();
			}
			if (S.empty()) {
				pop();
			}
		}
	}

	//exit(0);
	//setF(max(f, minG() + perfectHeuristic->getHNotClosed()));
	//perfectHeuristic->notifyToExploration(this); //TODO: seguro??
}

//If failed, some BDDs may be shrinked.
bool SymExploration::relaxFrontier(SymManager * manager, int maxTime,
		int maxNodes) {
	mgr = manager;
	mgr->setTimeLimit(maxTime);
	try { //Shrink frontier
		DEBUG_MSG(
				cout <<"Shrink frontier: "; printFrontier(); cout << "time limit: " << maxTime << " node limit: " << maxNodes << endl;);
		shrinkBucket(Sfilter, maxNodes);
		shrinkBucket(Smerge, maxNodes);
		shrinkBucket(Szero, maxNodes);
		shrinkBucket(S, maxNodes);
		DEBUG_MSG(
				cout <<" => "; printFrontier(); cout << " total time: " << g_timer() << endl;);
		mgr->unsetTimeLimit();
	} catch (BDDError e) {
		mgr->unsetTimeLimit();
		cout << "Truncated while shrinking the frontier." << endl;
		return false;
	}

	if (!expansionReady() && !prepareBucket(/*maxTime, maxNodes, true*/)) {
		cout << "Truncated because the new frontier could not be prepared: "
				<< nodeCount(S) << endl;
		return false;
	}

	cout << ">> Frontier " << dirname(fw) << " relaxed to: " << nodeCount(S)
			<< " total time: " << g_timer() << endl;

	//estimationDisjCost.recalculate(parent->estimationDisjCost, nodeCount(S));
	//estimationDisjZero.recalculate(parent->estimationDisjZero, nodeCount(S));
	estimationCost.recalculate(parent->estimationCost, nodeCount(S));
	estimationZero.recalculate(parent->estimationZero, nodeCount(S));

	return true;
}

void SymExploration::getNotExpanded(Bucket & res) const {
	for (const BDD & bdd : Sfilter) {
		res.push_back(bdd);
	}
	for (const BDD & bdd : Smerge) {
		res.push_back(bdd);
	}
	for (const BDD & bdd : S) {
		res.push_back(bdd);
	}
	for (const BDD & bdd : Szero) {
		res.push_back(bdd);
	}

	for (const auto & bucket : reopen) {
		for (const BDD & bdd : bucket.second) {
			res.push_back(bdd);
		}
	}
}

void SymExploration::relaxClosed() {
	closed->init(this, mgr);
	if (!expansionReady()) {
		cout << "ERROR, there is no frontier" << endl;
		exit(-1);
	}
	if (S.empty()) {
		cerr
				<< "Assertion error: nothing was inserted on the relaxed closed list."
				<< endl;
		exit(-1);
	}
	for (const BDD & bdd : S) {
		closed->insert(g, bdd);
	}
	closed->setHNotClosed(minG());
	DEBUG_MSG(
			cout << "HNotClosed initialized to: " << closed->getHNotClosed() << ", f=" << f << " g=" << g << endl ;);
}

//If failed, some BDDs may be shrinked.
bool SymExploration::relax(int maxTime, int maxNodes) {
	DEBUG_MSG(
			cout << "RELAX with a limit of: " << maxTime << " " << maxNodes << endl ;);

	mgr->setTimeLimit(maxTime);
	//Relax the whole open list
	try {
		//    int reductionTime = min<int>(open.size(), 2); //TODO: Dont reduce
		//the time in more
		//than 10. This
		//should be a
		//parameter
//Remove closed states from open and shrink it.
//This can have prunning power because it is performed before the shrinking.
		for (auto openIt : open) {
			DEBUG_MSG(
					cout << "Shrink open bucket(" << openIt.second.size() << "): " << nodeCount(openIt.second) << "maxTime: " << maxTime/*/reductionTime*/
					<< " maxNodes: " << maxNodes << endl ;);

			shrinkBucket(openIt.second, maxNodes);
		}
		//Close all the states in the current frontier
		//    checkCut(Sfilter, g, true);
		mgr->unsetTimeLimit();
	} catch (BDDError e) {
		mgr->unsetTimeLimit();
		cout << "Truncated while shrinking open list." << endl;
		return false;
	}

#ifdef DEBUG_GST
	gst_plan.checkOpen(closed->getClosed(), g, this);
	gst_plan.checkClose(closed->getClosed(), this);
#endif

	return true;
}

void SymExploration::shrinkBucket(Bucket & bucket, int maxNodes) {
	for (int i = 0; i < bucket.size(); ++i) {
		bucket[i] = mgr->shrinkExists(bucket[i], maxNodes);
	}
}

void SymExploration::checkCut(Bucket & bucket, int g_val, bool close) {
	DEBUG_MSG(
			cout << "CHECK CUT: " << g_val << " close: " << close << ", bucket: " << nodeCount(bucket) << endl ;);

	if (close) {
		if (isAbstracted() || !lastStepCost || g > 0) {
			DEBUG_MSG(
					cout << "Insert g=" << g_val << " states into closed: " << nodeCount(bucket) << " (" << bucket.size() << " bdds)" << endl ;);

			for (const BDD & states : bucket) {
				DEBUG_MSG(cout << "Inserting: " << states.nodeCount() << endl ;);

				closed->insert(g, states);
			}DEBUG_MSG(cout << "Inserted, setHNot closed" << endl ;);

			closed->setHNotClosed(minG());
			DEBUG_MSG(cout << "hNot closed set" << endl ;);
		}
	}

	//If it is the original space, maybe we have found a solution, set upper bound
	if (!isAbstracted() && perfectHeuristic) {
		for (int i = 0; i < bucket.size(); i++) {
			SymSolution sol = perfectHeuristic->checkCut(bucket[i], g_val, fw);
			if (sol.solved()) {
				cout << "Solution found with cost " << sol.getCost()
						<< " total time: " << g_timer << endl;
// Solution found :)
				engine->new_solution(sol);
			}
			bucket[i] *= perfectHeuristic->notClosed(); //Prune everything closed in opposite direction
		}
	}

	if (close && isAbstracted() && perfectHeuristic
			&& bdExp->getFMainDiagonal() == -1) {
		for (const BDD & bdd : bucket) {
			BDD cut = perfectHeuristic->getClosed() * bdd;
			if (!cut.IsZero()) {
				bdExp->setFMainDiagonal(f);
			}
		}
	}
}

void SymExploration::setNextG() {
	DEBUG_MSG(
			cout << "SET NEXT G greater than " << g << " with f=" << f << "options: "; for(auto i : open) cout << i.first << " "; cout << endl;);
	auto nextGBucket = open.upper_bound(g); //Get next g-bucket
	while (nextGBucket != open.end()) {
		DEBUG_MSG(cout << "LETS TRY g=" << nextGBucket->first << endl ;);
		if (g > f
				|| (!isAbstracted()
						&& (perfectHeuristic->getHNotClosed()
								> f - nextGBucket->first
								|| perfectHeuristic->getFNotClosed() > f))) {
			//Do not accept any larger value on this diagonal
			//Only applicable on the original state space because of the usage of nipping.
			g = numeric_limits<int>::max();
			return;
		}
		if (acceptedValues[f].count(nextGBucket->first)
				|| hValuesExplicit.count(f - nextGBucket->first)
				|| perfectHeuristic->accept(f, f - nextGBucket->first))
			break;

		nextGBucket++;
	}
	if (nextGBucket == open.end()) {
		g = numeric_limits<int>::max();
	} else {
		g = nextGBucket->first;
	}
}

void SymExploration::setNextF() {
	//We look for min f' = g' + h' > f so that g' is in open and h' is
	//in hValues
	DEBUG_MSG(cout << "Set nextF" << endl ;);
	if (open.empty()) {
		f = numeric_limits<int>::max();
		g = numeric_limits<int>::max();
		return;
	}

	pair<int, int> upper_bound { numeric_limits<int>::max(), open.begin()->first };
	DEBUG_MSG(
			cout << "CANDIDATES FOR UPPER BOUND [" <<acceptedValues.size() << "] : "; for(const auto & candidates : acceptedValues) {cout << candidates.first << " ["; for(auto i : candidates.second) {cout << i << " ";}cout << "], ";}cout << endl;);
	const auto & candidates = acceptedValues.upper_bound(f);
	acceptedValues.erase(begin(acceptedValues), candidates);
	if (candidates != end(acceptedValues) && !candidates->second.empty()) {
		upper_bound = {candidates->first, *(candidates->second.begin())};
	}
	DEBUG_MSG(
			cout << "Initial upper bound: " << upper_bound.first << ", " << upper_bound.second << endl ;);
	if (perfectHeuristic) {
		perfectHeuristic->getNextF(f, upper_bound);
		getNextFHValues(open, perfectHeuristic->getHValues(), f, upper_bound);
	}

	DEBUG_MSG(
			cout << "Perfect h upper bound: " << upper_bound.first << ", " << upper_bound.second << endl ;);

	getNextFHValues(open, hValuesExplicit, f, upper_bound);

	DEBUG_MSG(
			cout << "Final upper bound: " << upper_bound.first << ", " << upper_bound.second << endl ;);

	//Important: set g before calling setF or pop will be called again
	//due to notifications!
	g = upper_bound.second;
	f = upper_bound.first;
}

/* pop() must recompute the current g and f value, 
 extract states with those values from open,
 remove duplicates, classify h and put them on Sfilter. */
void SymExploration::pop() {
	DEBUG_MSG(cout << "POP: bucketReady: " << bucketReady() << endl ;);
	if (bucketReady() || expansionReady() || engine->solved()) {
		return;
	}
	int prevF = f;
	while (f < numeric_limits<int>::max() && !bucketReady()) {
		//DEBUG_MSG(debug_pop(););
		//Need to compute next f,g
		DEBUG_MSG(cout << "      >> Pop " << *this ;);
		if (perfectHeuristic) {
			setNextG(); //Look for a value of g supporting f
			if (g == numeric_limits<int>::max()) {
				setNextF();
			}
			perfectHeuristic->cleanEvals(this);
		} else {
			g =
					(open.empty() ?
							numeric_limits<int>::max() : open.begin()->first);
			f = minG();
		}DEBUG_MSG(cout << " New f=" << f << " g=" << g << endl ;);

		//Extract from open
		extract_states(open[g], f, f - g, Sfilter, true);
		//Extract states from reopen
		if (reopen.count(g)) {
			extract_states(reopen[g], f, f - g, Sfilter, false);
			if (reopen[g].empty()) {
				reopen.erase(g);
			}
		}
		if (open[g].empty() && !reopen.count(g)) {
			open.erase(g);
		}
	}

	if (f > prevF) {
		//States closed, update the f value if is the original state space
		if (!isAbstracted())
			engine->setLowerBound(f);

		closed->setHNotClosed(minG());
		closed->setFNotClosed(f);
	}DEBUG_MSG(cout << " >> Poped " << *this << endl ;);

	if (f == numeric_limits<int>::max())
		return;

	//Try to prepare next Bucket
	DEBUG_MSG(cout << "I AM COMPUTING ESTIMATION BECAUSE OF POP" << endl ;);
	computeEstimation(true);
	DEBUG_MSG(cout << "Estimation computed because of pop" << endl ;);
}

bool SymExploration::prepareBucket() {
	int maxTime = p.getAllotedTime(nextStepTime());
	int maxNodes = p.getAllotedNodes(nextStepNodesResult());

	if (!bucketReady()) {
		pop();
		DEBUG_MSG(cout << "Pop done for bucket prepared" << endl ;);
	}

	if (engine->solved()) {
		DEBUG_MSG(
				cout << "SOLVED!!!: " << engine->getLowerBound() << " >= " << engine->getUpperBound() << endl ;);
		return true; //If it has been solved, return
	}

	DEBUG_MSG(
			cout << "Preparing bucket time(" << maxTime << ") nodes(" << maxNodes << ")" << endl ;);
	if (initialization()) {
//		printf("TODO: sym_exploration::prepareBucket, init_mutex is not implemented yet!!!\n");
		mgr->init_mutex(*g_mutex_groups);
	}
	Timer filterTime;
	if (!Sfilter.empty()) {
		//First, if possible, attempt to merge the g-Sopen (only uses
		//pop_time). This is only to reuse the most resources possible.
		//mergeBucket(Sfilter, p.max_pop_time, p.max_pop_nodes); it has been merged in pop
		int numFiltered = mgr->filterMutexBucket(Sfilter, fw, initialization(),
				maxTime, maxNodes);
		if (numFiltered > 0) {
			Smerge.insert(end(Smerge), begin(Sfilter),
					begin(Sfilter) + numFiltered);
		}
		if (numFiltered == Sfilter.size()) {
			Bucket().swap(Sfilter);
		} else {
			Sfilter.erase(begin(Sfilter), begin(Sfilter) + numFiltered);
			violated(TruncatedReason::FILTER_MUTEX, filterTime(), maxTime,
					maxNodes);
			return false;
		}
	}

	if (!Smerge.empty()) {
		if (Smerge.size() > 1) {
			int remainingTime = maxTime - 1000 * filterTime();
			if (remainingTime < 0
					|| !mergeBucket(Smerge, remainingTime, maxNodes)) {
				violated(TruncatedReason::MERGE_BUCKET, filterTime(), maxTime,
						maxNodes);
				return false;
			}
		}

		DEBUG_MSG(
				cout << " BUCKET MERGED: " << Smerge.size() << " " << nodeCount(Smerge) << endl ;);

		//Successfully merged
		// a) close Smerge

		//b) put result on Szero or S (or both)
		if (mgr->hasTransitions0()) {
			S.insert(end(S), begin(Smerge), end(Smerge));
			assert(Szero.empty());
			Szero.swap(Smerge);
		} else {
			S.swap(Smerge);
		}
	}

	//If there are zero cost operators, merge S
	if (mgr->hasTransitions0() && Szero.empty()) {
		if (S.size() > 1) {
			int remainingTime = maxTime - 1000 * filterTime();
			if (remainingTime < 0 || !mergeBucket(S, remainingTime, maxNodes)) {
				violated(TruncatedReason::MERGE_BUCKET_COST, filterTime(),
						maxTime, maxNodes);
				return false;
			}
		}
	}

	// if(lastStepCost){
	//   DEBUG_MSG(cout << "EST_DISJ_COST " << filterTime() << "    ";);
	//   estimationDisjCost.stepTaken(1000*filterTime(), max(nodeCount(S), nodeCount(Szero)));
	// }else{
	//   DEBUG_MSG(cout << "EST_DISJ_ZERO " << filterTime() << "    ";);
	//   estimationDisjZero.stepTaken(1000*filterTime(), max(nodeCount(S), nodeCount(Szero)));
	// }
	DEBUG_MSG(
			cout << " BUCKET PREPARED: " << S.size() << " " << nodeCount(S) << endl ;);
	return true;
}

bool SymExploration::expand_zero(int maxTime, int maxNodes) {
	//Image with respect to 0-cost actions
	assert(expansionReady() && nodeCount(Szero) <= maxNodes);
	// DEBUG_MSG(cout << "    >> Truncated. 0-Frontier size exceeded: " << nodeCount(Szero) << endl;);
	// estimationZero.violated_nodes(nodeCount(Szero));
	// return false;

	Timer image_time;

	int nodesStep = nodeCount(Szero);
	double statesStep = stateCount(Szero);
	mgr->setTimeLimit(maxTime);
	//Compute image, storing the result on Simg
	try {
		int numImagesComputed = 0;
		for (int i = 0; i < Szero.size(); i++) {
			Simg.push_back(map<int, Bucket>());
			mgr->zero_image(fw, Szero[i], Simg[i][0], maxNodes);
			DEBUG_MSG(
					for (auto bdd : Simg[i][0]) { cout << "RESULT OF ZERO_IMG: " ; bdd.print(0, 1) ; });
			++numImagesComputed;
		}
		mgr->unsetTimeLimit();
	} catch (BDDError e) {
		mgr->unsetTimeLimit();
		violated(TruncatedReason::IMAGE_ZERO, image_time(), maxTime, maxNodes);
		// Szero.erase(begin(Szero), begin(Szero) + numImagesComputed);
		return false;
	}
	lastStepCost = false; //Must be set to false before calling checkCut

	DEBUG_MSG(cout << "EXPAND ZERO HAS PUT IN Simg: " << Simg.size() << endl ;);
	Bucket().swap(Szero); //Delete Szero because it has been expanded
	long nodesRes = 0;

	// TODO: YJ: here we apply hash function to distribute BDDs!
	for (auto & resimg : Simg) {
		for (auto bdd : resimg[0]) {
			std::vector<BDD> partitions;
			mgr->apply_hash(bdd, partitions);
			printf("total = %d nodes -> ", bdd.nodeCount());
			for (int i = 0; i < partitions.size(); ++i) {
				printf(" %d", partitions[i].nodeCount());
			}
			printf("\n");
		}
	}

	//Process Simg, removing duplicates and computing h. Store in Sfilter and reopen.
	for (auto & resimg : Simg) {
		for (auto bdd : resimg[0]) {
			nodesRes = max<long>(nodesRes, bdd.nodeCount());
#ifdef DEBUG_GST
			gst_plan.checkOpen(bdd, g, this);
#endif
		}
		if (perfectHeuristic)
			perfectHeuristic->cleanEvals(this);
		extract_states(resimg[0], f, f - g, Sfilter, true);
		//Those states that were not extracted are not closed and should be put on open
		if (!resimg[0].empty()) {
			open[g].insert(end(open[g]), begin(resimg[0]), end(resimg[0]));
		}
		//    reopen[g].insert(end(reopen[g]), begin(resimg[0]), end(resimg[0]));
	}

	vector<map<int, Bucket>>().swap(Simg);
	DEBUG_MSG(cout << "EST_ZERO " << image_time() << "   " ;);
	estimationZero.stepTaken(1000 * image_time(),
			max<long>(nodesRes, nodesStep));

	//Try to prepare next Bucket
	computeEstimation(true);

	DEBUG_MSG(
			cout << "  >> 0-cost step " << (fw ? " fw " : " bw ") << ": " << nodesStep << " nodes " << statesStep << " states " << " done in " << image_time << endl ;);DEBUG_MSG(
			cout << " res: " << Sfilter.size() << endl; cout << "Prepared in Sfilter: " << Sfilter.size() << endl;);
	return true;
}

bool SymExploration::expand_cost(int maxTime, int maxNodes) {
	printf("SymExploratin::expand_cost\n");
	assert(expansionReady() && nodeCount(S) <= maxNodes);
	int nodesStep = nodeCount(S);
	double statesStep = stateCount(S);
	Timer image_time;
	DEBUG_MSG(cout << "Setting maxTime: " << maxTime << endl ;);
	mgr->setTimeLimit(maxTime);
	try {
		for (int i = 0; i < S.size(); i++) {
			Simg.push_back(map<int, Bucket>());
			mgr->cost_image(fw, S[i], Simg[i], maxNodes);
		}
		mgr->unsetTimeLimit();
	} catch (BDDError e) {
		//Update estimation
		mgr->unsetTimeLimit();
		violated(TruncatedReason::IMAGE_COST, image_time(), maxTime, maxNodes);
		return false;
	}

	// TODO: YJ: here we apply hash function to distribute BDDs!
	for (std::map<int, Bucket> & resImage : Simg) {
		for (auto & pairCostBDDs : resImage) {
//			int cost = g + pairCostBDDs.first;
			mergeBucket(pairCostBDDs.second, p.max_pop_time, p.max_pop_nodes);
			//Check the cut (removing states classified, since they do not need to be included in open)
//			checkCut(pairCostBDDs.second, cost, false); // TODO: Not sure what this function is doing here.

			for (auto & bdd : pairCostBDDs.second) {
				std::vector<BDD> partitions;
				mgr->apply_hash(bdd, partitions);
				printf("total = %d bdd-nodes.\n", bdd.nodeCount());
				message_buffer.clear();
				message_buffer.resize(partitions.size());
				for (int i = 0; i < partitions.size(); ++i) {
//					printf(" %d", partitions[i].nodeCount());

//					char buffer[] = "foobar";
//					int ch;
//					FILE* bdddata;
//					char* fname;
//					bdddata = fmemopen(buffer, strlen(buffer), "w");
//					Dddmp_cuddBddStore(mgr->mgr(), NULL, partitions[i].getNode(),
//							NULL, NULL, 0 /*mode*/, DDDMP_VARIDS,
//							fname, bdddata);
					// TODO: Currently we are writing on top of external disk.
					// This should be optimized.
					string fname = "partition_" + to_string(f) + "_"
							+ to_string(g) + "_" + to_string(i);
					partitions[i].write(fname);
					std::ifstream in(fname, ios::binary);
					in >> std::noskipws;
//					std::string contents((std::istreambuf_iterator<char>(in)),
//							std::istreambuf_iterator<char>());
//					vector<char> buffer;
					std::copy(std::istream_iterator<char>(in),
							std::istream_iterator<char>(),
							std::back_inserter(message_buffer[i]));
					in.close();

					//					message_buffer[i] = contents;
//					MPI_Bsend(contents.c_str(), contents.size(), MPI_BYTE, i,
//							MPI_MSG_NODE, MPI_COMM_WORLD);

//					BDD reader = mgr->mgr()->read_file(fname);
//					printf(" %d", reader.nodeCount());

				}
				printf("\n");
			}

		}
	}

	//Include new states in the open list
	int stepNodes = nodesStep;
	for (auto & resImage : Simg) {
		for (auto & pairCostBDDs : resImage) {
			int cost = g + pairCostBDDs.first; //Include states of the given cost
			mergeBucket(pairCostBDDs.second, p.max_pop_time, p.max_pop_nodes);

			//Check the cut (removing states classified, since they do not need to be included in open)
			checkCut(pairCostBDDs.second, cost, false);

			for (auto & bdd : pairCostBDDs.second) {
				if (!bdd.IsZero()) {
					//TODO: maybe we can also use the heuristics to prune states
					//right here. Also, we could prune duplicates. Not sure if it
					//is worth it.
					int fVal = cost;
					if (perfectHeuristic) {
						fVal += perfectHeuristic->getHNotClosed();
					}
					if (fVal < engine->getUpperBound()) {
						if (!open.count(cost)) {
							open[cost] = vector<BDD>();
						}
						stepNodes = max(stepNodes, bdd.nodeCount());
#ifdef DEBUG_GST
						gst_plan.checkOpen(bdd, cost, this);
#endif
						open[cost].push_back(bdd);
					}
				}
			}
		}
	}
	vector<map<int, Bucket>>().swap(Simg);
	Bucket().swap(S);
	DEBUG_MSG(cout << "EST_COST " << image_time() << "   " << endl ;);
	estimationCost.stepTaken(1000 * image_time(), stepNodes);
	lastStepCost = true;

	DEBUG_MSG(
			cout << "  >> cost step" << (fw ? " fw " : " bw ") << ": " << nodesStep << " nodes " << statesStep << " states " << " done in " << image_time << " total time: " << g_timer << endl ;);
	return true;
}

bool SymExploration::stepImage(int maxTime, int maxNodes) {
	printf("SymExploratin::stepImage\n");
	if (mgr->getAbstraction())
		cout << ">> Step: " << *(mgr->getAbstraction());
	else
		cout << ">> Step: original";
	cout << (fw ? " fw " : " bw ") << "f=" << f << ", g=" << g << endl;
	DEBUG_MSG(
			cout << " frontierNodes: " << frontierNodes() << " [" << frontierBuckets() << "]" << " total time: " << g_timer << " total nodes: " << mgr->totalNodes() << " total memory: " << mgr->totalMemory() << endl ;);

#ifdef DEBUG_GST
	gst_plan.checkExploration(this );
#endif

	DEBUG_MSG(
			cout << "Step " << dirname(fw) << " f: " << f << " g: " << g << endl ;);
	Timer sTime;
	DEBUG_MSG(
			cout << "preparing bucket.." << " total time: " << g_timer << endl ;);
	if (!expansionReady() && !prepareBucket(/*maxTime, maxNodes, false*/)) {
		cout << "    >> Truncated while preparing bucket" << endl;
		if (sTime() * 1000.0 > p.maxStepTime) {
			double ratio = (double) p.maxStepTime / ((double) sTime() * 1000.0);
			p.maxStepNodes *= ratio;
			DEBUG_MSG(
					cout << "MAX STEP NODES CHANGED TO: " << p.maxStepNodes << " after truncating with " << sTime() << " seconds" << endl ;);
		}
		return false;
	}DEBUG_MSG(cout << "... bucket prepared. " << endl ;);
	if (engine->solved())
		return true; //Skip image if we are done
	bool ok = true;
	mgr->init_transitions(); //Ensure that transitions have been initialized
	int stepNodes = frontierNodes();
	if (!Szero.empty()) {
		ok = expand_zero(maxTime, maxNodes);
	} else if (!S.empty()) {
		//Image with respect to cost actions
		ok = expand_cost(maxTime, maxNodes);
	} else {
		DEBUG_MSG(
				cout << "   >> All pop states were filtered: " << open.empty() << endl ;);
	}

	pop(); //We prepare the next bucket before checking time in doing
	//the step because we consider preparing the bucket as a part
	//of the step.

	if (sTime() * 1000.0 > p.maxStepTime) {
		double ratio = (double) p.maxStepTime / ((double) sTime() * 1000.0);
		p.maxStepNodes = stepNodes * ratio;
		DEBUG_MSG(
				cout << "MAX STEP NODES CHANGED TO: " << p.maxStepNodes << " after taking " << sTime() << " seconds" << endl ;);
	} else if (!ok) {
		//In case maxAllotedNodes were exceeded we reduce the maximum
		//frontier size by 3/4.  TODO: make this a parameter
		p.maxStepNodes = stepNodes * 0.75;
	}

	DEBUG_MSG(cout << "END STEP" << endl ;);
	return ok;
}

/////////////////////////////////////////////////
///  Methods related to heuristic evaluation  ///
/////////////////////////////////////////////////

void SymExploration::notifyPrunedBy(int fVal, int gVal) {
	DEBUG_MSG(
			cout << "NOTIFIED pruned by: " << fVal << ", " << gVal << " on " << *this << endl ;);
	if (gVal != g) {
		cerr << "AssertionError on notifyPrunedBy. gVal=" << gVal << " != g="
				<< g << endl;
		exit(-1);
	}
	acceptedValues[fVal].insert(gVal);
}

void SymExploration::notify(const Bucket & bucket, int fNotClosed /*=0*/) {
	DEBUG_MSG(
			cout << " NOTIFIED BUCKET to: " << *this << "; Bucket contains " << bucket.size() << " BDDs" << endl; for(auto bdd : bucket) bdd.print(0, 1); cout << "BEFORE: "; printFrontier(); cout << endl;);
	//Now, try to reduce current frontier
	if (extract_states(Sfilter, bucket, reopen[g])
			| extract_states(Smerge, bucket, reopen[g])
			| extract_states(Szero, bucket, reopen[g])
			| extract_states(S, bucket, reopen[g])) {
		if (fNotClosed) {
			DEBUG_MSG(
					cout << "NOTIFIED in notify pruned by: " << fNotClosed << ", " << g << " on " << *this << endl ;);
			acceptedValues[fNotClosed].insert(g);
		}
	}

	if (!reopen[g].empty() && !open.count(g)) {
		open[g] = Bucket();
	}

	if (!expansionReady() && !bucketReady()) {
		pop(); // Only will do something if !bucketReady
	} else {
		computeEstimation(false);
	}DEBUG_MSG(
			cout << "END NOTIFY" << endl; cout << "AFTER: "; printFrontier(); cout << endl;);
}

//In the original state space search, there cannot be any state in the
//frontier closed (because of checkCut). So fNotClosed and hNotClosed
//are the minimum f and h for sure
void SymExploration::notifyNotClosed(int fValue, int hValue) {
	DEBUG_MSG(
			cout << "NOTIFIED NOT CLOSED to: " << *this << ": f=" << fValue << " h=" << hValue << endl ;);
	if (fValue > f || hValue > f - g) {
		//We should increase our f value
		if ((open.empty() || g <= open.begin()->first) && !reopen.count(g)) {
			//We are starting the diagonal => the g should be maintained
			setF(max(fValue, g + hValue));
			if (perfectHeuristic)
				perfectHeuristic->setEvaluationF(f, f - g, this);
		} else {
			//If we are not on the lowest g, set the lowest g
			moveBucket(Sfilter, reopen[g]);
			moveBucket(Smerge, reopen[g]);
			moveBucket(Szero, reopen[g]);
			moveBucket(S, reopen[g]);
			if (!open.count(g)) {
				open[g] = Bucket();
			}
			vector<map<int, Bucket>>().swap(Simg);
			if (!bucketReady() && !expansionReady()) {
				pop(); // Only will do something if !bucketReady
			} else {
				computeEstimation(false);
			}
		}
	}DEBUG_MSG(cout << "END NOTIFY NOT CLOSED" << endl ;);
}

void SymExploration::getNextFHValues(const map<int, vector<BDD>> & open,
		const set<int> & h_values, int f, pair<int, int> & upper_bound) const {
	if (open.empty()
			|| (upper_bound.first == f + 1
					&& upper_bound.second == open.begin()->first)) {
		return;
	}

	for (const auto & bucket : open) { //For each g' in open
		int gBucket = bucket.first;
		if (gBucket > upper_bound.first)
			break;DEBUG_MSG(
				cout << "Next f greater than " << f << ": f= " << upper_bound.first << " g=" << upper_bound.second << "; hvals=["; for(auto h : h_values) cout << h << " "; cout << "] Try with g=" << gBucket << endl;);
		int hBucket = f - gBucket; // look for h' > hBucket
		auto closerH = h_values.upper_bound(hBucket);
		if (closerH != end(h_values)) {
			int newF = (
					*closerH < numeric_limits<int>::max() ?
							gBucket + *closerH : *closerH);
			if (newF < upper_bound.first
					|| (newF == upper_bound.first
							&& gBucket < upper_bound.second)) {
				upper_bound.first = newF;
				upper_bound.second = gBucket;
			}
		}
	}
}

void SymExploration::addHeuristic(const SymHeuristic & newHeuristic) {
	mgr->addDeadEndStates(fw, newHeuristic.getDeadEnds());
	if (!perfectHeuristic
			|| newHeuristic.getMaxValue() > perfectHeuristic->getHNotClosed()) {
		const auto & hVals = newHeuristic.getHValues();
		hValuesExplicit.insert(begin(hVals), end(hVals));
		heuristics.push_back(newHeuristic);
		Bucket toNotify;
		toNotify.push_back(newHeuristic.prunedStates(f - g));
		notify(toNotify);
	}
}

BDD SymExploration::compute_heuristic(const BDD & from, int fVal, int hVal) {
	assert(
			isAbstracted() || !perfectHeuristic
					|| hVal <= perfectHeuristic->getHNotClosed());

	//DEBUG_MSG(cout << "Compute heuristic: " << hVal << endl;);
	BDD pruned = mgr->zeroBDD();
	BDD notPruned = from;
	for (const auto & heur : heuristics) {
		BDD newPruned = from * heur.prunedStates(hVal);
		notPruned -= newPruned;
		pruned += newPruned;
	}

	if (perfectHeuristic) {
		pruned += perfectHeuristic->evaluate(notPruned, fVal, hVal, this);
	}

	return pruned;
}

void SymExploration::extract_states(Bucket & buck, int fVal, int hVal,
		Bucket & res, bool duplicates) {
	mergeBucket(buck, p.max_pop_time, p.max_pop_nodes);
	//For each BDD in the bucket, get states with f
	for (int i = 0; i < buck.size(); ++i) {
		DEBUG_MSG(cout << "Extract from: " << buck[i].nodeCount() ;);
		if (duplicates) {
			buck[i] *= closed->notClosed();
			DEBUG_MSG(cout << ", duplicates: " << buck[i].nodeCount() ;);
			if (perfectHeuristic
					&& perfectHeuristic->getFNotClosed()
							== numeric_limits<int>::max()) {
				buck[i] *= perfectHeuristic->getClosed();
				DEBUG_MSG(cout << ", dead ends: " << buck[i].nodeCount() ;);
			}
		}
	}
	mgr->filterMutexBucket(res, fw, initialization(), p.max_pop_time,
			p.max_pop_nodes);
	for (int i = 0; i < buck.size(); ++i) {
		BDD bddH = buck[i];

		//We left in buck all the states that have been pruned
		buck[i] = compute_heuristic(buck[i], fVal, hVal);
		//bddH contains all the extracted states (those that fit fVal and hVal)
		bddH -= buck[i];

		DEBUG_MSG(
				cout << ", h=" << hVal << ", extracted: " << bddH.nodeCount() << ", left: " << buck[i].nodeCount() << endl ;);
		if (!bddH.IsZero()) {
			res.push_back(bddH);
		}
	}
	removeZero(buck);

	if (duplicates && !res.empty()) {
		mgr->filterMutexBucket(res, fw, initialization(), p.max_pop_time,
				p.max_pop_nodes);
		mergeBucket(res, p.max_pop_time, p.max_pop_nodes);
		if (!res.empty()) {
			checkCut(res, fVal - hVal, true);
		}
	}DEBUG_MSG(cout << "Extract states done." << endl ;);
}

bool SymExploration::extract_states(Bucket & list, const Bucket & pruned,
		Bucket & res) const {
	bool somethingPruned = false;
	for (int i = 0; i < list.size(); i++) {
		BDD prun = mgr->zeroBDD();
		for (const BDD & bddPruned : pruned) {
			prun += bddPruned * list[i];
		}
		if (!prun.IsZero()) {
			somethingPruned = true;
			list[i] -= prun;
			res.push_back(prun);
		}
	}
	removeZero(list);
	return somethingPruned;
}

void SymExploration::setF(int value) {
	if (value > f) {
		f = value;
		closed->setFNotClosed(f);
	}
}

BDD SymExploration::getExpanded() const {
	BDD res = closed->getClosed();
	for (const BDD & bdd : Sfilter) {
		res *= !bdd;
	}
	for (const BDD & bdd : Smerge) {
		res *= !bdd;
	}
	for (const BDD & bdd : S) {
		res *= !bdd;
	}
	return res;
}

/////////////////////////////////////////////////
///    Methods to decide useful/searchable    ///
/////////////////////////////////////////////////

double SymExploration::ratioUseful(Bucket & bucket) const {
	//1) Remove those states from bucket that are no more useful
	for (int i = 0; i < bucket.size(); i++) {
		BDD newBDD = mgr->zeroBDD();
		for (const BDD & bdd : S)
			newBDD += bucket[i] * bdd;
		for (const BDD & bdd : Szero)
			newBDD += bucket[i] * bdd;
		for (const BDD & bdd : Sfilter)
			newBDD += bucket[i] * bdd;
		for (const BDD & bdd : Smerge)
			newBDD += bucket[i] * bdd;
		bucket[i] = newBDD;
	}
	double possibleStates = stateCount(bucket); //This count is not
	//exact so ratioUseful
	//may be greater than 1
	double totalStates = stateCount(S) + stateCount(Szero) + stateCount(Sfilter)
			+ stateCount(Smerge);

	return min<double>(1, possibleStates / totalStates);
}

bool SymExploration::isSearchableWithNodes(int maxNodes) const {
	return expansionReady()

			&& ((fw && bdExp->getDir() != Dir::BW)
					|| (!fw && bdExp->getDir() != Dir::FW))
			&& nextStepNodes() <= maxNodes;
}

void SymExploration::computeEstimation(bool prepare) {
	if (prepare) {
		prepareBucket(/*p.max_pop_time, p.max_pop_nodes, true*/);
		DEBUG_MSG(cout << " bucket prepared for compute estimation" << endl ;);
	}

	if (expansionReady()) {
		//Succeded, the estimation will be only in image
		if (nodeCount(Szero)) {
			estimationZero.nextStep(nodeCount(Szero));
		} else {
			estimationCost.nextStep(nodeCount(S));
		}
	} else {
		if (mgr->hasTransitions0()) {
			estimationZero.nextStep(
					nodeCount(Sfilter) + nodeCount(Smerge) + nodeCount(Szero));
		} else {
			estimationCost.nextStep(
					nodeCount(Sfilter) + nodeCount(Smerge) + nodeCount(S));
		}
	}DEBUG_MSG(cout << "estimation computed" << endl ;);
}

long SymExploration::nextStepTime() const {
	long estimation = 0;

	if (mgr->hasTransitions0() && (!expansionReady() || !Szero.empty())) {
		estimation += estimationZero.time();
	} else {
		estimation += estimationCost.time();
	}
	return estimation;
}

long SymExploration::nextStepNodes() const {
	if (mgr->hasTransitions0() && (!expansionReady() || !Szero.empty())) {
		return estimationZero.nextNodes();
	} else {
		return estimationCost.nextNodes();
	}
}

long SymExploration::nextStepNodesResult() const {
	long estimation = 0;

	if (mgr->hasTransitions0() && (!expansionReady() || !Szero.empty())) {
		estimation = max(estimation, estimationZero.nodes());
	} else {
		estimation = max(estimation, estimationCost.nodes());
	}
	return estimation;
}

/////////////////////////////////////////////////
////   Auxiliar methods to load/save/print   ////
/////////////////////////////////////////////////

std::ostream & operator<<(std::ostream &os, const SymExploration & exp) {
	os << "exp " << dirname(exp.isFW());
	if (exp.mgr) {
		os << " in ";
		if (exp.mgr->getAbstraction()) {
			os << "abstract ";
			os << *(exp.mgr->getAbstraction());
		} else {
			os << "original state space";
		}
		os << " f=" << exp.getF() << flush;
		os << " g=" << exp.getG() << flush;
		os << " open{";
		for (auto & o : exp.open) {
			os << o.first << " ";
		}
		os << "}";
		os << " est_time: " << exp.nextStepTime() << flush;
		os << " est_nodes: " << exp.nextStepNodes() << flush;
	}
	return os;
}

void SymExploration::printFrontier() const {
	if (!Sfilter.empty())
		cout << "Sf: " << nodeCount(Sfilter) << " ";
	if (!Smerge.empty())
		cout << "Sm: " << nodeCount(Smerge) << " ";
	if (!Szero.empty())
		cout << "Sz: " << nodeCount(Szero) << " ";
	if (!S.empty())
		cout << "S: " << nodeCount(S) << " ";
}

bool SymExploration::isBetter(const SymExploration & other) const {
	return f > other.f
			|| (f == other.f && nextStepTime() < other.nextStepTime());
}

void SymExploration::write(const string & dir) const {
	ofstream file(dir + "data.txt");
	file << "fw: " << fw << endl;
	file << "f: " << f << endl;
	file << "g: " << g << endl;
	file << "lastStepCost: " << lastStepCost << endl;
	file << "estCost: " << endl;
	estimationCost.write(file);
	file << "estZero: " << endl;
	estimationZero.write(file);
	//file << "estDisjCost: " << endl; estimationDisjCost.write(file);
	//file << "estDisjZero: " << endl; estimationDisjZero.write(file);

	mgr->getVars()->writeBucket(dir + "Sfilter", file, Sfilter);
	mgr->getVars()->writeBucket(dir + "Smerge", file, Smerge);
	mgr->getVars()->writeBucket(dir + "Szero", file, Szero);
	mgr->getVars()->writeBucket(dir + "S", file, S);

	mgr->getVars()->writeMapBucket(dir + "_open", file, open);
	mgr->getVars()->writeMapBucket(dir + "_reopen", file, reopen);

	closed->write(dir, file);

	file.close();
}

void SymExploration::init(SymBDExp * exp, SymManager * manager,
		const string & dir) {
	bdExp = exp;
	mgr = manager;
	cout << "   Open file: " << dir << "data.txt" << endl;
	ifstream file(dir + "data.txt");
	cout << "   Read data" << endl;
	fw = getData<bool>(file, ":");
	f = getData<int>(file, ":");
	g = getData<int>(file, ":");
	lastStepCost = getData<bool>(file, ":");
	cout << "   Read estimations" << endl;
	string aux;
	getline(file, aux);
	estimationCost.read(file);
	getline(file, aux);
	estimationZero.read(file);
	//getline(file, aux);
	//estimationDisjCost.read(file);
	//getline(file, aux);
	//estimationDisjZero.read(file);

	cout << "   Read frontier buckets" << endl;
	mgr->getVars()->readBucket(file, Sfilter);
	mgr->getVars()->readBucket(file, Smerge);
	mgr->getVars()->readBucket(file, Szero);
	mgr->getVars()->readBucket(file, S);

	cout << "   Read open" << endl;
	mgr->getVars()->readMapBucket(file, open);
	cout << "   Read reopen" << endl;
	mgr->getVars()->readMapBucket(file, reopen);

	cout << "   Read closed" << endl;
	closed->init(this, mgr, dir, file);
}

void SymExploration::violated(TruncatedReason reason, double ellapsed_seconds,
		int maxTime, int maxNodes) {
	//DEBUG_MSG(
	cout << "Truncated in " << reason << ", took " << ellapsed_seconds << " s,"
			<< " maxtime: " << maxTime << " maxNodes: " << maxNodes << endl;
	//);
	int time = 1 + ellapsed_seconds * 1000;

	if (mgr->hasTransitions0() && (!expansionReady() || !Szero.empty())) {
		estimationZero.violated(time, maxTime, maxNodes);
	} else {
		estimationCost.violated(time, maxTime, maxNodes);
	}

	// switch(reason){
	// case TruncatedReason::FILTER_MUTEX:
	// case TruncatedReason::MERGE_BUCKET:
	// case TruncatedReason::MERGE_BUCKET_COST:
	//   //Truncated while preparing bucket
	//   break;
	// case TruncatedReason::IMAGE_ZERO:
	// case TruncatedReason::IMAGE_COST:
	//   break;
	// default:
	//   cerr << "SymExploration truncated by unkown reason" << endl;
	//   exit(-1);
	// }
}

std::ostream & operator<<(std::ostream &os, const TruncatedReason & reason) {
	switch (reason) {
	case TruncatedReason::FILTER_MUTEX:
		return os << "filter_mutex";
	case TruncatedReason::MERGE_BUCKET:
		return os << "merge";
	case TruncatedReason::MERGE_BUCKET_COST:
		return os << "merge_cost";
	case TruncatedReason::IMAGE_ZERO:
		return os << "0-image";
	case TruncatedReason::IMAGE_COST:
		return os << "cost-image";
	default:
		cerr << "SymExploration truncated by unkown reason" << endl;
		exit(-1);
	}
}

void SymExploration::init_hashfunction(int np) {
	mgr->init_hashFunction(np);
}

// Get message (=BDD) to send to processor i.
// i should not be the sender itself.
std::vector<char> SymExploration::get_message_buffer(int i) {
	return message_buffer[i];
}
