#include "sym_test.h"

#include <fstream>
#include "../../globals.h"
#include "../sym_abstraction.h"
#include "../sym_exploration.h"
#include "../../state_registry.h"
#include <sstream>
using namespace std;

void GSTPlan::checkHeuristicValue(BDD states, int h, int f) {
	//cout << "# Check heuristic value: " << h << endl;
	for (auto & step : plan) {
		step.checkHeuristicValue(states, h, f);
	}
}

void GSTPlan::checkHeuristicValue(BDD states, int h, int f,
		SymExploration * exp) {
	//cout << "# Check heuristic value: " << h << " in " << *exp << endl;
	for (auto & step : plan) {
		step.checkHeuristicValue(states, h, f, exp);
	}
}

void GSTPlanStep::checkHeuristicValue(BDD states, int h, int f) {
	BDD c = bdd * states;
	if (!c.IsZero()) {
		cout << "# " << *this << " has h <=" << h << " and f <= " << f << endl;
	}
}

void GSTPlanStep::checkHeuristicValue(BDD states, int h, int /*f*/,
		SymExploration * exp) {
	BDD c = bdd * states;
	if (!c.IsZero()) {
		if (g_values.count(exp)) {
			//Check that the heuristic value is right
			if ((exp->isFW() && h < g_values[exp])
					|| (!exp->isFW() && h < g_plan + h_plan - g_values[exp])) {
				cout << "## ERROR: " << *this << " assigned h: " << h << endl;
				exit(-1);
			}
		}
		/*    if (f == numeric_limits<int>::max()){
		 cout << "## ERROR: " << *this << " assigned f: " << f << endl;
		 exit(-1);
		 }*/
	}
}

void GSTPlanStep::checkExploration(SymExploration * exp) {
	for (auto aux : exp->getOpen()) {
		stringstream ss;
		ss << "open[" << aux.first << "]";
		checkBucket(aux.second, ss.str());
	}

	for (auto aux : exp->getReopen()) {
		stringstream ss;
		ss << "reopen[" << aux.first << "]";
		checkBucket(aux.second, ss.str());
	}

	checkBucket(exp->getSfilter(), "Sfilter");
	checkBucket(exp->getSmerge(), "Smerge");
	checkBucket(exp->getSzero(), "Szero");
	checkBucket(exp->getS(), "S");
}

void GSTPlanStep::checkBucket(const vector<BDD> & bucket, string name) {
	for (auto bddAux : bucket) {
		BDD c = bdd * bddAux;
		if (!c.IsZero()) {
			cout << "State " << id << " in " << name << "   ";
		}
	}
}

void GSTPlanStep::checkOpen(BDD openStates, int g_val, SymExploration * exp) {
	BDD coincidence = bdd * openStates;
	if (!coincidence.IsZero()) {
		cout << "#Abstract state " << id << " opened by " << *exp << " with g= "
				<< g_val << endl;
	}
}

bool GSTPlanStep::checkClose(BDD closedStates, int g_val, bool fw,
		SymExploration * exp) {

	BDD coincidence = bdd * closedStates;
	//  cout << id << " " << coincidence.nodeCount() << endl;
	if (!coincidence.IsZero()) {
		g_values[exp] = g_val;
		if (!exp->isAbstracted()) {
			if (g_val != g_plan) {
				int f_val = exp->getF();
				int h_val = f_val - g_val;
				cout << "#State " << id << " closed with wrong g_value: "
						<< g_val << " instead of " << g_plan
						<< ", heuristic value is: " << h_val << ". Closed by: "
						<< *exp << endl;
				cout << "#The state " << id << " was also closed by: " << endl;
				for (auto aux : g_values) {
					if (aux.first->isFW()) {
						cout << "  #g=" << aux.second << " by " << *(aux.first)
								<< endl;
					} else {
						cout << "  #h=" << g_plan + h_plan - aux.second
								<< " by " << *(aux.first) << endl;
					}
				}
				checkExploration(exp);
				return false;
			} else {
				cout << "#State " << id << " closed with good g_value: "
						<< g_plan << endl;
			}
		} else {
			int parentValue = -1;
			SymExploration * parent = exp->getParent();
			while (parent) {
				if (g_values.count(parent)) {
					parentValue = g_values[parent];
				}
				parent = parent->getParent();
			}
			cout << "#Abstract state " << id << " closed ("
					<< *(exp->get_mgr()->getAbstraction()) << ")";
			if (parentValue >= 0) {
				cout << " (previously closed with value " << parentValue
						<< ") ";
			}

			//Coincidence with abstracted
			if (g_val == g_plan) {
				cout << " with good g: " << g_val << endl;
			} else if ((fw && g_val < g_plan) || (!fw && g_val > g_plan)) {
				cout << " with admissible g: " << g_val << " instead of "
						<< g_plan << endl;
			} else {
				cout << " with inadmissible g: " << g_val << " instead of "
						<< g_plan << endl;
				if (parentValue == -1) {
					cout
							<< "#ERROR: inadmissible value and is not closed by any parent!: "
							<< *this << endl;
					parent = exp->getParent();
					while (parent) {
						BDD closedByParent = coincidence
								* (parent->getClosed()->getClosed());
						if (closedByParent.IsZero()) {
							cout << "#Not closed by: " << *parent << endl;
						} else {
							cout << "#Closed by: " << *parent << endl;
						}
						parent = parent->getParent();
					}
					return false;
				}
			}
		}
	}
	return true;
}

void GSTPlan::checkOpen(BDD openStates, int g, SymExploration * exp) {
	bool fw = exp->isFW();
	int g_val = fw ? g : f - g;

	for (auto & step : plan) {
		step.checkOpen(openStates, g_val, exp);
	}

}

void GSTPlan::checkExploration(SymExploration * exp) {
	cout << "#Check " << *exp << " ";
	for (auto & step : plan) {
		step.checkExploration(exp);
	}
	cout << endl;
}

void GSTPlan::checkClose(BDD closedStates, SymExploration * exp) {
	bool fw = exp->isFW();
	int g = fw ? exp->getG() : f - exp->getG();
	for (auto & step : plan) {
		if (!step.checkClose(closedStates, g, fw, exp)) {
			checkExploration(exp);
			exit(-1);
		}
	}
}

void GSTPlan::loadPlan(string filename, const SymVariables & vars) {
	vector<Operator *> plan_ops;
	loadPlan(filename, plan_ops);
	if (!plan_ops.empty()) {
		f = 0;
		for (auto op : plan_ops) {
			f += op->get_cost();
		}
		//cout << "#Total plan cost: " << f << endl;

		State s(g_initial_state());
		int id = 0;
		int g = 0;
		int h = f;
		BDD bdd = vars.getStateBDD(s);
		plan.push_back(GSTPlanStep(id++, g, h, bdd));

		//Get state
		for (auto op : plan_ops) {
			if (!op->is_applicable(s)) {
				cout << "#ERROR: bad plan reconstruction" << endl;
				exit(-1);
			}
			s = g_state_registry->get_successor_state(s, *op);
//			s = State(s, *op);
			g += op->get_cost();
			h -= op->get_cost();
			bdd = vars.getStateBDD(s);

			plan.push_back(GSTPlanStep(id++, g, h, bdd));
		}

		if (!test_goal(s)) {
			cout << "#ERROR: bad plan reconstruction" << endl;
			exit(-1);
		}

		cout << "Test plan loaded: " << *this << endl;
	}
}

void GSTPlan::loadPlan(string filename, vector<Operator *> & plan) {
	ifstream infile(filename);
	cout << "Loading test plan: " << plan.size() << endl;

	if (infile.good()) {
		string line;
		while (getline(infile, line)) {
			line.erase(0, 1);
			line.erase(end(line) - 1);
			for (int i = 0; i < g_operators.size(); ++i) {
				if (g_operators[i].get_name() == line) {
					plan.push_back(&(g_operators[i]));
					break;
				}
			}
		}

	} else {
		cout << "Test plan not found" << endl;
	}
	//cout << "Test plan loaded: " << plan.size() << endl;
}

void GSTPlan::checkBDD(BDD S) const {
	for (auto & step : plan) {

		BDD coincidence = step.getBDD() * S;
		if (!coincidence.IsZero()) {
			cout << "#" << step.getId() << " ";
		}
	}
}

#ifdef DEBUG_GST
GSTPlan gst_plan;
#endif

std::ostream & operator<<(std::ostream &os, const GSTPlanStep & step) {
	os << "GSTPlanStep " << step.id << ", g=" << step.g_plan << ", h="
			<< step.h_plan << ", closed by: ";
	for (auto aux : step.g_values) {
		if (aux.first->isFW()) {
			os << "  g=" << aux.second << " by ";
		} else {
			os << "  h=" << step.g_plan + step.h_plan - aux.second << " by ";
		}
		SymAbstraction * abs = aux.first->get_mgr()->getAbstraction();

		if (abs) {
			os << *abs << ", ";
		} else {
			os << " original state space, ";
		}
	}
	return os;
}

std::ostream & operator<<(std::ostream &os, const GSTPlan & plan) {

	os << "GSTPlan f=" << plan.f << ":" << endl;
	for (auto & step : plan.plan) {
		os << step;
	}
	return os;
}
