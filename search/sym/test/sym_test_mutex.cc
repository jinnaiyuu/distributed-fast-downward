#include "sym_test_mutex.h"

#include "../sym_manager.h"
using namespace std;

void GSTMutex::check_mutexes(SymManager & manager) {
	SymAbstraction * abs = manager.getAbstraction();
	BDD goal = manager.getGoal();
	if (!abs->isAbstracted()) {
		notMutexBDD = manager.oneBDD();
		for (const auto & bdd : manager.getNotMutexBDDs(false)) {
			notMutexBDD *= bdd;
		}

		cout << "#MUTEX BDD: " << notMutexBDD.nodeCount() << endl;
		BDD goalwoMutex = goal * notMutexBDD;
		cout << "#MUTEX GOAL: " << goal.nodeCount() << " => "
				<< goalwoMutex.nodeCount() << endl;
	} else {
		BDD abstractNotMutexBDD = manager.oneBDD();
		for (auto & bdd : manager.getNotMutexBDDs(false)) {
			abstractNotMutexBDD *= bdd;
		}

		BDD abstractedBDD = abs->shrinkExists(notMutexBDD, 10000000);

		cout << "#MUTEX ABSTRACT " << *abs << " BDD: "
				<< (abstractedBDD == abstractNotMutexBDD ?
						" is equal " : " is different") << " abstract bdd: "
				<< abstractNotMutexBDD.nodeCount() << " shrinked original: "
				<< abstractedBDD.nodeCount() << endl;

		BDD goalAbstract = goal * abstractNotMutexBDD;
		BDD goalAbstractShrinked = goal * abstractedBDD;

		cout << "#MUTEX ABSTRACT GOAL: " << goalAbstract.nodeCount()
				<< " shrinked mutex goal: " << goalAbstractShrinked.nodeCount()
				<< endl;

	}
}

GSTMutex gst_mutex;
