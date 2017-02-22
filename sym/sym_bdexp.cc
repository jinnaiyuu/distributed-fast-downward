#include "sym_bdexp.h"

#include "../debug.h"
#include "sym_engine.h"
#include <algorithm>    // std::reverse
#include "../operator.h"

#ifdef __CDT_PARSER__
#undef DEBUG_MSG
#define DEBUG_MSG(a)  ;
#endif

using namespace std;

SymBDExp::SymBDExp(SymController * engine, const SymParamsSearch & params,
		Dir dir) :
		hnode(nullptr), parent(nullptr), fw(new SymExploration(engine, params)), bw(
				new SymExploration(engine, params)), searchDir(dir), mayRelax(
				true), fMainDiagonal(-1) {
}

SymBDExp::SymBDExp(SymBDExp * other, const SymParamsSearch & params, Dir dir) :
		hnode(nullptr), parent(other), fw(
				new SymExploration(other->getFw()->getEngine(), params)), bw(
				new SymExploration(other->getFw()->getEngine(), params)), searchDir(
				dir), mayRelax(true), fMainDiagonal(-1) {
	fw->init(this, other->fw.get());
	bw->init(this, other->bw.get());
	fw->init2(bw.get());
	bw->init2(fw.get());
	SymBDExp * lastParent = parent;
	while (lastParent->getParent())
		lastParent = lastParent->getParent();

	Bucket frontierFW, frontierBW;
	other->fw->getNotExpanded(frontierFW);
	other->bw->getNotExpanded(frontierBW);

	BDD closedByFW = lastParent->fw->getClosedTotal();

	for (int i = 0; i < frontierFW.size(); ++i) {
		DEBUG_MSG(
				cout << "Removing frontierFW (" << g_timer() << "s): " << frontierFW[i].nodeCount(););

		closedByFW -= frontierFW[i];
		DEBUG_MSG(
				cout << " => " << closedByFW.nodeCount() << "   " << g_timer() << "s" <<endl;);
	}
	BDD closedByBW = lastParent->bw->getClosedTotal();
	for (int i = 0; i < frontierBW.size(); ++i) {
		DEBUG_MSG(
				cout << "Removing frontierBW (" << g_timer() << "s): " << frontierBW[i].nodeCount(););

		closedByBW -= frontierBW[i];
		DEBUG_MSG(
				cout << " => " << closedByBW.nodeCount() << "   " << g_timer() << "s" <<endl;);

	}

	if (!other->isAbstracted()) {
		closedByParent = closedByFW + closedByBW;
		closedByParentBw = closedByParent;
	} else {
		closedByParent = closedByFW;
		closedByParentBw = closedByBW;
	}DEBUG_MSG(
			cout << "Closed by original wo frontier: " << closedByParent.nodeCount() << " total_time: " << g_timer() << endl;);

}

bool SymBDExp::initFrontier(SymHNode * node, int maxTime, int maxNodes) {
	//Set the new abstract state space
	hnode = node;
	if (!parent) { //Init of new search
		bool init_fw = fw->init(this, hnode->getManager(), true);
		bool init_bw = bw->init(this, hnode->getManager(), false);

		fw->setPerfectHeuristic(bw->getClosed());
		bw->setPerfectHeuristic(fw->getClosed());
		DEBUG_MSG(
				cout << "Initialized " << fw.get() << " and " << bw.get() << endl;);

		//Consider correctly initialized if one has been initialized
		return (searchDir != Dir::BW && init_fw)
				|| (searchDir != Dir::FW && init_bw);
	} else { //Init of perimeter search
		DEBUG_MSG(cout << "Init frontier with " << *node << endl;);

		//Relax the frontier
		if (fw->relaxFrontier(hnode->getManager(), maxTime, maxNodes)
				&& bw->relaxFrontier(hnode->getManager(), maxTime, maxNodes)) {

			hnode->getManager()->setTimeLimit(maxTime);
			try {
				if (closedByParent == closedByParentBw) {
					closedByParent = hnode->getManager()->shrinkForall(
							closedByParent, maxNodes);
					closedByParentBw = closedByParent;
				} else {
					closedByParent = hnode->getManager()->shrinkForall(
							closedByParent, maxNodes);
					closedByParentBw = hnode->getManager()->shrinkForall(
							closedByParentBw, maxNodes);
				}DEBUG_MSG(
						cout << "Shrunk closed by parent: " << closedByParent.nodeCount() << " (" << g_timer() << "s)" << endl;);

				hnode->getManager()->unsetTimeLimit();
			} catch (BDDError e) {
				hnode->getManager()->unsetTimeLimit();
				return false;
			}

			fw->relaxClosed();
			bw->relaxClosed();

			DEBUG_MSG(
					cout << "Closed by original exp relaxed to: " << closedByParent.nodeCount() << endl;);

			return true;
		}
		return false;
	}
}

bool SymBDExp::initAll(int maxTime, int maxNodes) {
	if (parent) {
		//Relax all the search
		if (!fw->relax(maxTime, maxNodes) || !bw->relax(maxTime, maxNodes)) {
			return false;
		}

		//Connect both explorations
		fw->setPerfectHeuristic(bw->getClosed());
		bw->setPerfectHeuristic(fw->getClosed());
	}
	return true;
}

std::ostream & operator<<(std::ostream &os, const SymBDExp & bdexp) {
	os << "BD exp [" << *(bdexp.fw.get()) << ",  " << *(bdexp.bw.get()) << "]"
			<< (bdexp.mayRelax ? " is abstractable" : " no abstractable");
	return os;
}

void SymBDExp::getPlan(const BDD & cut, int g, int h,
		std::vector<const Operator *> & path) const {
	DEBUG_MSG(cout << "Extract path forward: " << g << endl;);

	fw->getClosed()->extract_path(cut, g, true, path);
	std::reverse(path.begin(), path.end());
	State s(g_initial_state());
	//Get state
	for (auto op : path) {
		/*DEBUG_MSG(
		 BDD state = hnode->getManager()->getVars()->getStateBDD(s);
		 BDD res = state*hnode->getManager()->getNotMutexBDDs(false)[0];
		 if (res.IsZero()){
		 cout << "BOOM" << endl;
		 s.dump_pddl();

		 for(auto & mg : g_mutex_groups){
		 int count = 0;
		 for(auto f : mg.getFacts()){
		 if(s[f.first] == f.second){
		 count ++;
		 }
		 }
		 if(count > 1){
		 cout <<endl << mg << endl;
		 exit(0);
		 }else if (count == 0 && mg.isExactlyOne()){
		 cout << "EXACTLY ONE: " << endl << mg << endl;
		 exit(0);
		 }
		 }
		 cout << "No mutex group " << endl;
		 exit(0);
		 }
		 cout << op->get_name() << endl;
		 if(!op->is_applicable(s)){
		 cout << "ERROR: bad plan reconstruction" << endl;
		 cout << op->get_name() << " is not applicable" << endl;
		 exit(-1);
		 });*/
		s = g_state_registry->get_successor_state(s, *op);
//		s = State(s, *op);
	}

	BDD newCut = hnode->getManager()->getVars()->getStateBDD(s);

	DEBUG_MSG(cout << "Extract path backward: "<< h << endl;);

	bw->getClosed()->extract_path(newCut, h, false, path);
	/*DEBUG_MSG(cout << "Path extracted" << endl;
	 State s2 (*g_initial_state);
	 //Get state
	 for(auto op : path){
	 cout << op->get_name() << endl;
	 if(!op->is_applicable(s2)){
	 cout << "ERROR: bad plan reconstruction" << endl;
	 cout << op->get_name() << " is not applicable" << endl;
	 exit(-1);
	 }
	 s2 = State(s2, *op);
	 }
	 if(!test_goal(s2)){
	 cout << "ERROR: bad plan reconstruction" << endl;
	 cout << "The plan ends on a non-goal state" << endl;
	 exit(-1);
	 });*/

}

bool SymBDExp::isExpFor(SymBDExp * bdExp) const {
	if (!parent) {
		return false;
	}
	SymBDExp * exp = parent;
	while (exp && exp != bdExp) {
		exp = exp->parent;
	}

	return exp != nullptr;
}

void SymBDExp::setHeuristic(SymBDExp & other) {
	DEBUG_MSG(
			cout << "Set BDExp as heuristic " << other << " to " << *this << endl;);
	fw->setChild(other.fw.get());
	bw->setChild(other.bw.get());
}

SymExploration * SymBDExp::selectBestDirection(bool skipUseful) const {
	if (searchDir == Dir::FW) {
		return fw.get();
	} else if (searchDir == Dir::BW) {
		return bw.get();
	}

	if (!skipUseful) {
		bool fwUseful = fw->isUseful();
		bool bwUseful = bw->isUseful();
		if (fwUseful && !bwUseful) {
			return fw.get();
		} else if (bwUseful && !fwUseful) {
			return bw.get();
		}
	}

	if (fMainDiagonal != -1) {
		if (fw->getF() > fMainDiagonal && bw->getF() <= fMainDiagonal) {
			return bw.get();
		} else if (bw->getF() > fMainDiagonal && fw->getF() <= fMainDiagonal) {
			return fw.get();
		}
	}

	bool fwSearchable = fw->isSearchable();
	bool bwSearchable = bw->isSearchable();
	if (fwSearchable && !bwSearchable) {
		return fw.get();
	} else if (!fwSearchable && bwSearchable) {
		return bw.get();
	}

	return (fw->isBetter(*bw) ? fw.get() : bw.get());
}

void SymBDExp::write(const string & filename) const {
	fw->write(filename + "fw");
	bw->write(filename + "bw");
	//  hnode->getManager().write(filename + "mgr");
}

SymBDExp::SymBDExp(SymController * engine, const SymParamsSearch & params,
		Dir dir, const string & filename, SymHNode * node) :
		hnode(nullptr), parent(nullptr), fw(new SymExploration(engine, params)), bw(
				new SymExploration(engine, params)), searchDir(dir), mayRelax(
				true), fMainDiagonal(-1) {
	hnode = node;

	cout << "Loading fw: " << endl;
	fw->init(this, hnode->getManager(), filename + "fw");
	cout << "Loading bw: " << endl;
	bw->init(this, hnode->getManager(), filename + "bw");
	cout << "Loaded: " << *this << endl;

	fw->setPerfectHeuristic(bw->getClosed());
	bw->setPerfectHeuristic(fw->getClosed());
	//hnode->getManager().read(filename + "mgr");
	printf("SymBDExp::SymBDExp init_mutex removed for now\n");
//	hnode->getManager()->init_mutex(g_mutex_groups, false, true);
	hnode->getManager()->init_transitions();
}

bool SymBDExp::finished() const {
	return (searchDir == Dir::BW || fw->finished())
			&& (searchDir == Dir::FW || bw->finished());
}

bool SymBDExp::finishedMainDiagonal() const {
	return finished()
			|| ((searchDir == Dir::BW || fw->finished()
					|| (fMainDiagonal != -1 && fw->getF() > fMainDiagonal))
					&& (searchDir == Dir::FW || bw->finished()
							|| (fMainDiagonal != -1
									&& bw->getF() > fMainDiagonal)));
}
