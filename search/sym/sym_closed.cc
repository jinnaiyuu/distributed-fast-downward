#include "sym_closed.h"

#include "sym_solution.h"
#include "sym_exploration.h"
#include "sym_bdexp.h"
#include "sym_util.h"
#include "../timer.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include "../successor_generator.h"
#include "test/sym_test.h" 

using namespace std;

SymClosed::SymClosed() :
		mgr(nullptr), exploration(nullptr), parent(nullptr) {
}

void SymClosed::init(SymExploration * exp, SymManager * manager) {
	exploration = exp;
	mgr = manager;
	set<int>().swap(h_values);
	map<int, BDD>().swap(closedUpTo);
	map<int, vector<BDD>>().swap(zeroCostClosed);
	map<int, BDD>().swap(closed);
	closedTotal = mgr->zeroBDD();
	hNotClosed = 0;
	fNotClosed = 0;

	//Initialization in an abstract search
	if (exp->isAbstracted() && exp->getParent()) {
		//if(!exp->getParent()->isAbstracted()){
		//hNotClosed = exp->getParent()->getClosed()->hNotClosed;
		//fNotClosed = exp->getParent()->getClosed()->fNotClosed;
		closedTotal = exp->getBDExp()->getClosedByParent(exp->isFW());
		// }else{
		//   closedTotal = exp->getBDExp()->getClosedByParent();
		// }
		closed[0] = closedTotal;
		newHValue(0);
		DEBUG_MSG(
				cout << "Closed total initialized to: " << closedTotal.nodeCount() << endl;);
	}
}

void SymClosed::newHValue(int h_value) {
	if (!h_values.count(h_value)) {
		h_values.insert(h_value);
		if (parent)
			parent->newHValue(h_value);
	}
}

// BDD SymClosed::getBDD(const BDD & bdd, int /*fVal*/, int hVal) const {
//   if (closed.count(hVal)){
//     return bdd*closed.at(hVal);
//   }else{
//     return mgr->zeroBDD();
//   }
// }

void SymClosed::insert(int h, const BDD & S) {
//  DEBUG_MSG(cout << "Inserting on closed " << *this << " " << (exploration->isFW() ? " fw " : " bw ")
//	    << "g=" << h << ": " << S.nodeCount() << " nodes and "
//	    << mgr->getVars()->numStates(S) << " states" << endl;);

#ifdef DEBUG_GST
	gst_plan.checkClose(S, exploration);
#endif
	// if(!exploration->isFW() && h ==6){
	//   BDD accum = mgr->zeroBDD();
	//   for(const SymTransition & tr : mgr->getIndividualTRs().at(6)){
	//     BDD succ =tr.image(S);
	//     BDD intersection = succ*closedTotal;
	//     BDD succ2 = tr.preimage(intersection);
	//     DEBUG_MSG(
	// 		if(!intersection.IsZero()){
	// 		  cout << "S: "; S.print(0, 1);
	// 		  cout << "Image computed: "; succ.print(0,1);
	// 		  cout << "closed at newh: "; closed.at(0).print(0, 1);
	// 		  cout << "Intersection: "; intersection.print(0,1);
	// 		  cout << "Res: "; succ2.print(0,1);
	// 		}
	// 		);
	//     accum += succ2;
	//   }
	//   if(accum != S){
	//     if(accum < S){
	// 	cout << "He perdido estados" << endl;
	// 	cout << mgr->getVars()->numStates(S) << endl;
	// 	cout << mgr->getVars()->numStates(accum) << endl;
	//     }else if (accum > S){
	// 	cout << "He ganado estados" << endl;
	//     }else{
	// 	cout << "Ni lo uno ni lo otro" << endl;
	//     }
	//     exit(-1);
	//   }
	// }
	if (closed.count(h)) {
		closed[h] += S;
		if (!h_values.count(h)) {
			cerr << "Assertion error: h_value not present but bucket present: "
					<< h << " in " << *this << endl;
			exit(-1);
		}
	} else {
		closed[h] = S;
		newHValue(h);
	}

	zeroCostClosed[h].push_back(S);
	closedTotal += S;

	//Introduce in closedUpTo
	auto c = closedUpTo.lower_bound(h);
	while (c != end(closedUpTo)) {
		c->second += S;
		c++;
	}

	if (exploration->isAbstracted()) {
		// Update evals removing all states that cannot be pruned with this search as heuristic
		for (auto & ev : evals) {
			Evaluation & eval = ev.second;
			for (int i = 0; i < eval.bucket.size(); ++i) {
				BDD prunedBDD = S * eval.bucket[i];
				if (!prunedBDD.IsZero() && eval.h >= h) {
					eval.bucket[i] -= prunedBDD;
				}
			}
			exploration->removeZero(eval.bucket); //Delete zero values
		}
	}
}

void SymClosed::setHNotClosed(int newHNotClosed) {
	DEBUG_MSG(cout << "H NOT CLOSED SET TO " << newHNotClosed << endl;);
	if (newHNotClosed > hNotClosed) {
		hNotClosed = newHNotClosed;
		newHValue(newHNotClosed);
		if (exploration->isAbstracted()) {
			vector<SymExploration *> expsToNotify;
			DEBUG_MSG(
					cout << "SetHNotClosed. I have " << evals.size() << " explorations to (maybe) notify" << endl;);
			for (auto & e : evals) {
				expsToNotify.push_back(e.first);
			}
			for (auto exp : expsToNotify) {
				if (!evals.count(exp))
					continue;
				Evaluation eval = evals.at(exp);
				DEBUG_MSG(cout << "HERE2: " << *exp << endl;);
				if (eval.f != exp->getF()
						|| eval.h != (exp->getF() - exp->getG())) {
					cerr << "Assertion failed: in setHNotClosed, eval has f="
							<< eval.f << " and the exploration has eval: "
							<< exp->getF() << endl;
					cout << "We were exploring " << *(exp) << endl;
					cout << "Bad eval is on " << *exploration << endl;
					exit(-1);
				}
				if (hNotClosed > eval.h) { //Automatically prune all not closed
					cleanEvals(exp);
					// This assertion is outdated. We can notify
					// notClosed because my h or f value was not
					// enough to prune beforehand
					//DEBUG_MSG(for(BDD bdd :
					// eval.bucket) if((!(bdd*closedTotal)).IsZero()){
					// cerr << "ASSERT SETHNOTCLOSED NOTIFYING CLOSED
					// STATES" << endl; exit(-1); });
					exp->notify(eval.bucket, 0);
					DEBUG_MSG(cout << "SetHNotClosed: notified done" << endl;);
				}
			}
		} else if (exploration->getOpposite()) {
			DEBUG_MSG(
					cout << "I am in the original state space, so just notify the f and h" << endl;);
			exploration->getOpposite()->notifyNotClosed(fNotClosed, hNotClosed);
		}
	}DEBUG_MSG(cout << "END SET H NOT CLOSED" << endl;);
}

void SymClosed::setFNotClosed(int f) {
	DEBUG_MSG(
			cout <<"CALL SET F NOT CLOSED: " << *this << " ; " << evals.size() << endl;);
	if (f > fNotClosed) {
		DEBUG_MSG(
				cout << (exploration->isFW() ? " fw " : " bw ") << " fNotClosed set to " << f << endl;);
		fNotClosed = f;

		if (exploration->isAbstracted()) {
			vector<SymExploration *> expsToNotify;
			for (auto & e : evals) {
				expsToNotify.push_back(e.first);
			}
			for (auto exp : expsToNotify) {
				if (!evals.count(exp))
					continue;
				Evaluation eval = evals.at(exp);

				if (eval.f != exp->getF()) {
					cerr << "Assertion failed: in setFNotClosed, eval has f="
							<< eval.f << " and the exploration has eval: "
							<< exp->getF() << endl;
					cout << "We were exploring " << *exp << endl;
					cout << "Bad eval is on " << *exploration << endl;
					exit(-1);
				}

				if (fNotClosed > eval.f) { //Automatically prune all not closed
					DEBUG_MSG(
							cout << " HERE3: " << *exp << " " << eval.exp << " in " << this << ": " << *this << endl; cout << "NOTIFY FNOTCLOSED: " << fNotClosed << " > " << eval.f << " FROM: " << *exploration << " TO " << *exp << endl;);
					cleanEvals(exp);
					// This assertion is outdated. We can notify
					// notClosed because my h or f value was not
					// enough to prune beforehand
					// for(BDD bdd : eval.bucket)
					// 	if (!(bdd*closedTotal).IsZero()){
					// 	    cerr << "ASSERT SETFNOTCLOSED NOTIFYING CLOSED STATES" << endl;
					// 	    exit(-1);
					// 	}
					exp->notify(eval.bucket, fNotClosed);
					DEBUG_MSG(cout << "SetFNotClosed: notified done" << endl;);
				}
			}
		} else if (exploration->getOpposite()) {
			DEBUG_MSG(cout << "Notify not closed " << endl;);
			exploration->getOpposite()->notifyNotClosed(fNotClosed, hNotClosed);
		}
	}DEBUG_MSG(cout << "END OF CALL SET F NOT CLOSED" << endl;);
}

void SymClosed::extract_path(const BDD & c, int h, bool fw,
		vector<const Operator *> & path) const {
	if (!mgr)
		return;DEBUG_MSG(
			cout << "Sym closed extract path h="<< h << " notClosed: " << hNotClosed << endl; cout << "Closed: "; for (auto & c : closed) cout << c.first << " "; cout << endl;);
	const map<int, vector<SymTransition>> & trs = mgr->getIndividualTRs();
	BDD cut = c;
	int steps0 = 0;

	if (zeroCostClosed.count(h)) {
		//DEBUG_MSG(cout << "Check " << steps0 << " of " << zeroCostClosed.at(h).size() << endl;);
		while (steps0 < zeroCostClosed.at(h).size()
				&& (cut * zeroCostClosed.at(h)[steps0]).IsZero()) {
			//DEBUG_MSG(cout << "Steps0 is not " << steps0<< " of " << zeroCostClosed.at(h).size() << endl;);
			steps0++;
		}
		//DEBUG_MSG(cout << "Steps0 of h=" << h << " is " << steps0 << endl;);
		if (steps0 < zeroCostClosed.at(h).size()) {
			cut *= zeroCostClosed.at(h)[steps0];
		} else {
			DEBUG_MSG(cout << "cut not found with steps0" << endl;);
			steps0 = 0;
			//steps0 = zeroCostClosed.size();
		}
	}

	while (h > 0 || steps0 > 0) {
		DEBUG_MSG(cout << "h=" << h << " and steps0=" << steps0 << endl;
		//cout << "CUT: "; cut.print(0, 1);
				);
		if (steps0 > 0) {
			bool foundZeroCost = false;
			//Apply 0-cost operators
			if (trs.count(0)) {
				for (const SymTransition & tr : trs.at(0)) {
					if (foundZeroCost)
						break;
					BDD succ;
					if (fw) {
						succ = tr.preimage(cut);
					} else {
						succ = tr.image(cut);
					}
					if (succ.IsZero()) {
						continue;
					}

					//DEBUG_MSG(cout << "SUCC: "; succ.print(0, 1););

					for (int newSteps0 = 0; newSteps0 < steps0; newSteps0++) {
						BDD intersection = succ
								* zeroCostClosed.at(h)[newSteps0];
						if (!intersection.IsZero()) {
							steps0 = newSteps0;
							cut = succ;
							//DEBUG_MSG(cout << "Adding " << (*(tr.getOps().begin()))->get_name() << endl;);
							path.push_back(*(tr.getOps().begin()));
							foundZeroCost = true;
							break;
						}
					}
				}
			}

			if (!foundZeroCost) {
				/*    DEBUG_MSG(
				 cout << "Force steps0 = 0" << endl;
				 for (int newSteps0 = 0; newSteps0 <= steps0; newSteps0++){
				 cout << "Steps0: " << newSteps0 << ": "; zeroCostClosed.at(h)[newSteps0].print(0,2);
				 cout << "CUT: "; cut.print(0, 1);
				 }
				 );*/
				steps0 = 0;
			}
		}

		if (h > 0 && steps0 == 0) {
			bool found = false;
			for (auto key : trs) { //TODO: maybe is best to use the inverse order
				if (found)
					break;
				int newH = h - key.first;
				if (key.first == 0 || closed.count(newH) == 0)
					continue;
				for (SymTransition & tr : key.second) {

					//DEBUG_MSG(cout << "Check " << tr.getOps().size() << " " << (*(tr.getOps().begin()))->get_name() << " of cost " << key.first << " in h=" << newH << endl;);
					BDD succ;
					if (fw) {
						succ = tr.preimage(cut);
					} else {
						succ = tr.image(cut);
					}
					BDD intersection = succ * closed.at(newH);
					/*DEBUG_MSG(cout << "Image computed: "; succ.print(0,1);
					 cout << "closed at newh: "; closed.at(newH).print(0,1);
					 cout << "Intersection: "; intersection.print(0,1););*/
					if (!intersection.IsZero()) {
						h = newH;
						cut = succ;
						steps0 = 0;
						while ((cut * zeroCostClosed.at(h)[steps0]).IsZero()) {
							//DEBUG_MSG(cout << "r Steps0 is not " << steps0<< " of " << zeroCostClosed.at(h).size() << endl;);
							steps0++;
//	      assert(steps0 < zeroCostClosed[newH].size());
						}

						//DEBUG_MSG(cout << "r Steps0 of h=" << h << " is " << steps0 << endl;);

						cut *= zeroCostClosed.at(h)[steps0];
						path.push_back(*(tr.getOps().begin()));

						//DEBUG_MSG(cout << "Selected " << path.back()->get_name() << endl;);

						found = true;
						break;
					}
				}
			}
			if (!found) {
				cerr << "Error: Solution reconstruction failed: "
						<< dirname(exploration->isFW()) << endl;
				exit(-1);
			}
		}
	}

	DEBUG_MSG(cout << "Sym closed extracted path" << endl;);
}

SymSolution SymClosed::checkCut(const BDD & states, int g, bool fw) const {
	BDD cut_candidate = states * closedTotal;
	if (cut_candidate.IsZero()) {
		return SymSolution(); //No solution yet :(
	}
	for (const auto & closedH : closed) {
		int h = closedH.first;

		DEBUG_MSG(cout << "Check cut of g=" << g << " with h=" << h << endl;);
		BDD cut = closedH.second * cut_candidate;
		if (!cut.IsZero()) {
			if (fw)
				return SymSolution(exploration->getBDExp(), g, h, cut);
			else
				return SymSolution(exploration->getBDExp(), h, g, cut);
		}
	}
	cerr << "Error: Cut with closedTotal but not found on closed" << endl;
	exit(-1);
}

/*int SymClosed::getH(const State & state) {
 for(const auto & c : closed){
 if(mgr->getVars()->isIn(state, c.second)){
 return c.first;
 }
 }
 return hNotClosed;
 }
 */

std::ostream & operator<<(std::ostream &os, const SymClosed & c) {
	return os << "Heuristic of: " << *(c.exploration);
}

/*void SymClosed::extract_path(const State & state, int ini_h,
 vector <const Operator *> & path){
 State s = state;
 int h = ini_h;
 int steps0 = 0;
 cout << "Extract solution starting on h=" << h << endl;
 cout << "0-cost steps=" << steps0 << endl;

 vector<const Operator *> applicable_ops;

 while(h > 0 || steps0 > 0){
 cout << "Extract sol iterations: " << h << " " << steps0 << endl;
 g_successor_generator->generate_applicable_ops(s, applicable_ops);
 for (auto op : applicable_ops){
 if(steps0){
 if(op->get_cost() > 0) continue;
 State succ_state(s, *op);
 if(mgr->getVars()->isIn(succ_state, zeroCostClosed.at(h)[steps0-1])){
 steps0--;
 s = succ_state;
 path.push_back(op);
 break;
 }
 }else{
 if(op->get_cost() == 0) continue;
 State succ_state(s, *op);
 int newH = h - op->get_cost();
 cout << "Apply " << op->get_name() << " to reduce h=" << newH<< endl;
 if(closed.count(newH)){
 if(mgr->getVars()->isIn(succ_state, closed[newH])){
 h = newH;
 steps0 = 0;
 s = succ_state;
 while(!mgr->getVars()->isIn(succ_state, zeroCostClosed[newH][steps0])){
 steps0++;
 assert(steps0 < zeroCostClosed[newH].size());
 }

 path.push_back(op);
 break;
 }
 }
 }
 }
 applicable_ops.clear();
 }
 }*/

void SymClosed::getHeuristic(vector<ADD> & heuristics,
		vector<int> & maxHeuristicValues) const {
	int previousMaxH = 0; //Get the previous value of max h
	if (!maxHeuristicValues.empty()) {
		previousMaxH = maxHeuristicValues.back();
	}
	/*If we did not complete one step, and we do not surpass the previous maxH
	 we do not have heuristic*/
	if (closed.size() <= 1 && hNotClosed <= previousMaxH) {
		cout << "Heuristic not inserted: " << hNotClosed << " " << closed.size()
				<< endl;
		return;
	}

	ADD h = getHeuristic(previousMaxH);

	//  closed.clear(); //The heuristic values have been stored in the ADD.
	cout << "Heuristic with maxValue: " << hNotClosed << " ADD size: "
			<< h.nodeCount() << endl;

	maxHeuristicValues.push_back(hNotClosed);
	heuristics.push_back(h);
}

ADD SymClosed::getHeuristic(int previousMaxH /*= -1*/) const {
	/* When zero cost operators have been expanded, all the states in non reached
	 have a h-value strictly greater than frontierCost.
	 They can be frontierCost + min_action_cost or the least bucket in open. */
	/*  int valueNonReached = frontierCost;
	 if(frontierCost >= 0 && zeroCostExpanded){
	 cout << "Frontier cost is " << frontierCost << endl;
	 closed[frontierCost] = S;
	 valueNonReached = frontierCost + mgr->getMinTransitionCost();
	 if(!open.empty()){
	 valueNonReached = min(open.begin()->first,
	 valueNonReached);
	 }
	 }*/
	BDD statesWithHNotClosed = !closedTotal;
	ADD h = mgr->mgr()->constant(-1);
	for (auto & it : closed) {
		cout << "Adding states with h = " << it.first << endl;
		int h_val = it.first;

		/*If h_val < previousMaxH we can put it to that value
		 However, we only do so if it is less than hNotClosed
		 (or we will think that we have not fully determined the value)*/
		if (h_val < previousMaxH && previousMaxH < hNotClosed) {
			h_val = previousMaxH;
		}
		if (h_val != hNotClosed) {
			h += it.second.Add() * mgr->mgr()->constant(h_val + 1);
		} else {
			statesWithHNotClosed += it.second;
		}
	}

	cout << "Adding not closedTotal to " << hNotClosed << endl;
	if (hNotClosed != numeric_limits<int>::max() && hNotClosed >= 0
			&& !statesWithHNotClosed.IsZero()) {
		h += statesWithHNotClosed.Add() * mgr->mgr()->constant(hNotClosed + 1);
	}

	return h;
}

// void SymClosed::storeHeuristic(const string & filename) const {
//    // When zero cost operators have been expanded, all the states in non reached 
//    // have a h-value strictly greater than frontierCost.
//    // They can be frontierCost + min_action_cost or the least bucket in open.

//   /*  int valueNonReached = frontierCost; 
//   if(frontierCost >= 0 && zeroCostExpanded){
//     cout << "Frontier cost is " << frontierCost << endl;
//     closed[frontierCost] = S;
//     valueNonReached = frontierCost + mgr->getMinTransitionCost();
//     if(!open.empty()){
//       valueNonReached = min(open.begin()->first,
// 			    valueNonReached);
//     }
//     }*/

//   BDD statesWithHNotClosed = !closedTotal;

//   for(auto & it : closed){
//     int h_val = it.first;
//     if(h_val == hNotClosed){
//       statesWithHNotClosed += it.second;
//     }else{
//       std::ostringstream ssBDD;
//       ssBDD << filename << "_" << h_val; 
//       cout << "Writing: " << ssBDD.str() << endl;
//       it.second.write(ssBDD.str());
//     }
//   }

//   cout << "Adding not closedTotal to " << hNotClosed << endl;
//   if(hNotClosed >= 0){
//     std::ostringstream ssBDD;
//     ssBDD << filename << "_" << hNotClosed; 
//     cout << "Writing: " << ssBDD.str() << endl;
//     statesWithHNotClosed.write(ssBDD.str());

//   }
//   cout << "Heuristic with maxValue: " << hNotClosed << endl;
// }

// int SymClosed::getH (const BDD & bdd) const {
//   for (auto & bh : closed){
//     BDD conj = bdd*bh.second;
//     if(!conj.IsZero()){
//       return bh.first;
//     }
//   }
//   return hNotClosed;
// }

void SymClosed::write(const string & fname, ofstream & file) const {
	file << "hNotClosed: " << hNotClosed << endl;
	file << "fNotClosed: " << fNotClosed << endl;
	mgr->getVars()->writeMapBucket(fname + "zeroCostClosed", file,
			zeroCostClosed);
	mgr->getVars()->writeMap(fname + "_closed", file, closed);
	closedTotal.write(fname + "_closedTotal");
}

void SymClosed::init(SymExploration * exp, SymManager * manager,
		const string & fname, ifstream & file) {
	exploration = exp;
	mgr = manager;
	set<int>().swap(h_values);
	map<int, BDD>().swap(closedUpTo);
	map<int, vector<BDD>>().swap(zeroCostClosed);
	map<int, BDD>().swap(closed);
	closedTotal = mgr->zeroBDD();

	hNotClosed = getData<int>(file, ":");
	fNotClosed = getData<int>(file, ":");
	mgr->getVars()->readMapBucket(file, zeroCostClosed);
	mgr->getVars()->readMap(file, closed);
	closedTotal = mgr->getVars()->readBDD(fname + "_closedTotal");
}

void SymClosed::cleanEvals(SymExploration * exp) {

	if (evals.count(exp)) {
		DEBUG_MSG(
				cout << "Cleaning data from " << *exp << " in "<< *this << endl;);
		evals.erase(exp);
	}

	//Is possible that the children have an eval even if I dont (because
	//other child pruned me some states after evaluating the first child
	for (auto c : children) {
		c->cleanEvals(exp);
	}
}

void SymClosed::setEvaluationF(int f, int h, SymExploration * exp) {
	if (evals.count(exp)) {
		evals.at(exp).f = f;
		evals.at(exp).h = h;
		for (auto c : children) {
			c->setEvaluationF(f, h, exp);
		}
	}
}

void SymClosed::potentiallyUsefulFor(SymExploration * exp,
		vector<SymExploration *> & useful_exps) const {
	if (evals.count(exp) && !evals.at(exp).bucket.empty()) {
		useful_exps.push_back(exploration);
		for (auto c : children) {
			c->potentiallyUsefulFor(exp, useful_exps);
		}
	}
}

bool SymClosed::isUsefulAfterRelax(double ratio,
		const std::vector<BDD> & newFrontier) const {
	for (auto & e : evals) {
		//I am useful only I can prune the non-abstract search.
		if (!e.first->isAbstracted()) {
			vector<BDD> bucketwoFrontier;
			for (const BDD & bdd : e.second.bucket) {
				BDD bddwoFrontier = bdd;
				for (const BDD & fBDD : newFrontier) {
					bddwoFrontier -= fBDD;
				}
				if (!bddwoFrontier.IsZero()) {
					bucketwoFrontier.push_back(bddwoFrontier);
				}
			}
			//Filters all the states that are no useful anymore
			double rUseful = e.first->ratioUseful(bucketwoFrontier);
			if (rUseful > 0 && rUseful >= ratio) {
				return true;
			}
		}
	}
	return false;

}

bool SymClosed::isUseful(double ratio) {
	for (auto & e : evals) {
		//I am useful only I can prune the non-abstract search.
		if (!e.first->isAbstracted()) {
			//Filters all the states that are no useful anymore
			double rUseful = e.first->ratioUseful(e.second.bucket);
			if (rUseful > 0 && rUseful >= ratio) {
				DEBUG_MSG(
						cout << *exploration << " is Useful because it has a ratio: " << rUseful << endl;);
				return true;
			}
		}
	}
	return false;
}

void SymClosed::setEvals(
		const std::map<SymExploration *, Evaluation> & otherEvals) {
	for (const auto & e : otherEvals) {
		if (e.first->isAbstracted())
			continue;
		evals.insert(make_pair(e.first, e.second));
		Evaluation & eval = evals.at(e.first);
		for (int i = 0; i < eval.bucket.size(); i++) {
			eval.bucket[i] -= closedTotal;
		}
	}
}

bool SymClosed::can_prune(int fVal, int hVal) {
	if (hVal < hNotClosed || fVal < fNotClosed) {
		return true;
	}

	for (int i = 0; i < children.size(); i++) {
		if (children[i]->can_prune(fVal, hVal))
			return true;
	}
	return false;
}
//Evaluates a BDD and returns the BDD of states that are pruned
BDD SymClosed::evaluate(const BDD & bdd, int fVal, int hVal,
		SymExploration * expAsking) {
	if (expAsking->isAbstracted()
			&& expAsking->getBDExp() != exploration->getBDExp()) {
		return mgr->zeroBDD();
	}
	//TODO: transform in assertion
	if (expAsking->isFW() == exploration->isFW()) {
		cerr
				<< "Assertion failed, using exploration in the same direction as heuristic"
				<< endl;
		exit(-1);
	}
	BDD evalStates = bdd * notClosed();
	BDD closedStates = bdd - evalStates;

	//Remove closed states with h >= hVal
	BDD prunedStates = evaluate(closedStates, hVal);

	DEBUG_MSG(
			cout << "   Evaluation of " << hVal << " is eval=" << evalStates.nodeCount() << ", closed=" << closedStates.nodeCount() << ", pruned=" << prunedStates.nodeCount() << " " ;);
	//If hVal < hNotClosed or fVal < fNotClosed, I can automatically
	//prune all not closed states
	if (hVal < hNotClosed || fVal < fNotClosed) {
		DEBUG_MSG(
				cout << " pruned all eval, fVal=" << fVal << ", fNotclosed=" << fNotClosed << ", hVal=" << hVal << ", hNotClosed=" << hNotClosed << " ";);
		prunedStates += evalStates;
		if (fVal < fNotClosed) {
			expAsking->notifyPrunedBy(fNotClosed, fVal - hVal);
		}
	} else {
		if (!evalStates.IsZero()) {
			for (int i = 0; i < children.size(); i++) {
				BDD pruned = children[i]->evaluate(evalStates, fVal, hVal,
						expAsking);
				prunedStates += pruned;
				if (i < children.size() - 1) {
					evalStates -= pruned;
				}
			}
			if (!can_prune(fVal, hVal)) {
				evalStates += prunedStates;
				prunedStates = mgr->zeroBDD();
			}
			//If there are relevant states after evaluating children
			//heuristics, they are stored for notification purposes
			if (!evalStates.IsZero()) {
				if (!evals.count(expAsking)) {
					DEBUG_MSG(
							cout << "INSERTING EVAL: " << expAsking << " in " << this << endl;);
					evals.insert(
							make_pair(expAsking,
									Evaluation(expAsking, fVal, hVal)));
				}
				if (hVal != evals.at(expAsking).h
						|| fVal != evals.at(expAsking).f) {
					cerr << "Assertion failed: in evaluate hVal: " << hVal
							<< ", eval.h: " << evals.at(expAsking).h
							<< ", fVal: " << fVal << ", eval.f: "
							<< evals.at(expAsking).f << endl;
					cout << "failed in " << *this << " because of data "
							<< *expAsking << endl;
					exit(-1);
				}
				evals.at(expAsking).bucket.push_back(evalStates);
			}
		}
	}

	//This assertion does not make sense anymore.
	// Now, it is possible to have states closed in eval if my f < fVal.
	// if(evals.count(expAsking)){
	// 	auto & eval = evals.at(expAsking);
	// 	for(BDD bdd : eval.bucket)
	// 	    if (!(bdd*closedTotal).IsZero()){
	// 		cerr << "ASSERT EVALUATE CLOSED STATES IN EVAL" << endl;
	// 		exit(-1);
	// 	    }
	// }
	return prunedStates;
}

BDD SymClosed::evaluate(const BDD & bdd, int hVal) {
	//Return the ones closed with more than hVal
	BDD closedUp = mgr->zeroBDD();

	if (!closed.empty() && hVal >= closed.begin()->first) {
		if (!closedUpTo.count(hVal)) {
			auto entry = closedUpTo.lower_bound(hVal);
			int hAux = -1;
			if (entry != closedUpTo.begin()) {
				--entry;
				hAux = (*entry).first;
			}

			if (hAux >= 0) {
				DEBUG_MSG(
						cout << "closed up for " << hVal << " initialized in " << hAux << " and we add ";);
				closedUp = closedUpTo[hAux];
			}

			for (auto it = closed.lower_bound(hAux);
					it != closed.end() && (*it).first <= hVal; ++it) {
				DEBUG_MSG(cout << " " << (*it).first;);
				closedUp += (*it).second;
				closedUpTo[(*it).first] = closedUp;
			}DEBUG_MSG(
					cout << " from a closed list that has values: "; for(auto & c : closed) cout << c.first << " ";);
		} else {
			closedUp = closedUpTo[hVal];
			DEBUG_MSG(
					cout << "COMES FROM closedUpTo["<< hVal<< "]: " << closedUp.nodeCount() << endl;);
		}
	}
	return bdd * (!closedUp);
}

bool SymClosed::accept(int f, int h) const {
	DEBUG_MSG(cout << "accepting f=" << f << ", h=" << h << endl;);
	if (fNotClosed == f || hNotClosed == h || closed.count(h)
			|| h_values.count(h)) { //Not sure if this check
			//is redundant
		return true;
	}
	for (auto c : children) {
		if (c->accept(f, h)) {
			return true;
		}
	}DEBUG_MSG(cout << "NOT ACCEPTED" << endl;);
	return false;
}

void SymClosed::getNextF(int f, pair<int, int> & upper_bound) const {
	for (auto c : children) {
		DEBUG_MSG(
				cout << "I am " << *this << endl << "Check child " << c << endl << " is: " << *c << endl;);
		c->getNextF(f, upper_bound);
	}DEBUG_MSG(
			cout << "Check " << *this << " for an f value greater than " << f << " and lower than " << upper_bound.first << ". Mine is: " << fNotClosed << endl;);
	if (fNotClosed > f && fNotClosed < upper_bound.first) {
		upper_bound.first = fNotClosed;
	}
}

