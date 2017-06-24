#include "spmas_heuristic.h"

#include "sym_enums.h"
#include "sym_bdexp.h"
#include "sym_variables.h"
#include "sym_hnode.h"
#include "sym_ph.h"
#include "../option_parser.h"
#include "../plugin.h"

#include <cassert>
#include <limits>
#include <memory>
#include <vector>

using namespace std;

SPMASHeuristic::SPMASHeuristic(const Options &opts) :
		Heuristic(opts), SymController(opts), phs(opts.get_list<SymPH *>("ph")), generationTime(
				opts.get<int>("generation_time")), generationMemory(
				opts.get<double>("generation_memory")) {
}

void SPMASHeuristic::initialize() {
	Timer timer;
	cout << "Initializing smas heuristic..." << endl;
	dump_options();

	cout << "Initialize original search" << endl;
	SymHNode * originalStateSpace = new SymHNode(this, mgrParams);
	nodes.push_back(unique_ptr<SymHNode>(originalStateSpace));
	SymBDExp * originalSearch = new SymBDExp(this, searchParams, Dir::BW);
	unique_ptr<SymBDExp> refExp(originalSearch);

	if (refExp->initFrontier(originalStateSpace, numeric_limits<int>::max(),
			numeric_limits<int>::max())
			&& refExp->initAll(numeric_limits<int>::max(),
					numeric_limits<int>::max())) {
		originalStateSpace->add_exploration(std::move(refExp));
	} else {
		cout << "Init exploration failed" << endl;
		exit(-1);
	}

	//Get mutex fw BDDs to detect spurious states as dead ends
	const vector<BDD> & nmBDD = originalSearch->getManager()->getNotMutexBDDs(
			true);
	notMutexBDDs.insert(end(notMutexBDDs), begin(nmBDD), end(nmBDD));

	SymBDExp * currentBDExp = originalSearch;
	SymExploration * currentSearch = originalSearch->getBw();
	while (!currentSearch->finished() && g_timer() < generationTime
			&& vars->totalMemory() < generationMemory
			&& currentSearch->isSearchable()
			&& (!solution.solved()
					|| originalSearch->getF() < solution.getCost())) {
		currentSearch->stepImage();
	}
	bool perimeter_considered = false;
	if (!currentSearch->finished()
			&& (!solution.solved()
					|| originalSearch->getF() < solution.getCost())
			&& g_timer() < generationTime
			&& vars->totalMemory() < generationMemory) {
		vector<SymPH *> tmp;
		tmp.swap(phs);
		for (auto ph : tmp) {
			if (ph->init(this, vars.get(), originalStateSpace->getManager())) {
				ph->setIgnoreIfUseful();
				phs.push_back(ph);
			} else {
				delete ph;
			}
		}

		SPHeuristic origSearchSp;
		originalSearch->getBw()->getClosed()->getHeuristic(
				origSearchSp.heuristicADDs, origSearchSp.max_heuristic_value);
		for (auto & ph : phs) {
			if (g_timer() > generationTime
					|| vars->totalMemory() > generationMemory) {
				break;
			}
			SPHeuristic sp(origSearchSp);

			currentBDExp = ph->relax(originalSearch, Dir::BW);
			if (currentBDExp) {
				currentBDExp->desactivate(); //Desactivates the search, so
				//that no notifications are
				//performed
				currentSearch = currentBDExp->getBw();
			}

			while (currentBDExp && !currentSearch->finished()
					&& g_timer() < generationTime
					&& vars->totalMemory() < generationMemory) {
				cout << "CHECKED " << g_timer() << endl;
				if (!currentSearch->stepImage()
						|| !currentSearch->isSearchable()) {
					if (!currentSearch->finished()) {

						currentSearch->getClosed()->getHeuristic(
								sp.heuristicADDs, sp.max_heuristic_value);
						currentBDExp = ph->relax(currentBDExp, Dir::BW);
						if (currentBDExp) {
							currentBDExp->desactivate();
							currentSearch = currentBDExp->getBw();
						}
					}
				}
			}

			cout << "Finished with explorations of PH: " << endl;
			if (currentBDExp) {
				currentSearch->getClosed()->getHeuristic(sp.heuristicADDs,
						sp.max_heuristic_value);
			}

			for (const auto & heur : ph->get_intermediate_heuristics_fw()) {
				if (!heur.empty()) {
					auto newH = vars->getADD(heur);
					cout << "Added heuristic: " << newH.nodeCount()
							<< " with max value: " << heur.rbegin()->first
							<< endl;
					explicitHeuristics.push_back(std::move(newH));
				}
			}
			perimeter_considered = true;
			spHeuristics.push_back(sp);
			if (nodes.size() >= 1) {
				nodes.erase(begin(nodes) + 1, end(nodes));
			}
		}
	} else if (solution.solved()) {
		//Finished backward search
		explicitHeuristics.push_back(solution.getADD());
		perimeter_considered = true;
	}

	if (!perimeter_considered) {
		explicitHeuristics.push_back(
				originalSearch->getBw()->getClosed()->getHeuristic());
	}
	std::vector<std::unique_ptr<SymHNode>>().swap(nodes);

	cout << "Done initializing symbolic perimeter merge-and-shrink heuristic ["
			<< timer << "] total memory: " << vars->totalMemory() << endl
			<< "initial h value: " << compute_heuristic(g_initial_state())
			<< endl;
	//cout << "Estimated peak memory for abstraction: " << peak_memory << " bytes" << endl;
}

int SPHeuristic::compute_heuristic(int * inputs) const {
	int cost = 0;
	for (int i = 0; i < heuristicADDs.size(); ++i) {
		ADD evalNode = heuristicADDs[i].Eval(inputs);
		int abs_cost = Cudd_V(evalNode.getRegularNode());
		if (abs_cost == -1) {
			//cout << "Detected dead end by abstraction " << i << endl;
			return abs_cost;
		}

		cost = max(cost, abs_cost);

		if (abs_cost < max_heuristic_value[i]) {
			//The value in the rest of heuristics should be zero so we can stop
			break;
		}
	}
	return cost;
}

int SPMASHeuristic::compute_heuristic(const State &state) {
	int * inputs = vars->getBinaryDescription(state);
	for (const BDD & bdd : notMutexBDDs) {
		if (bdd.Eval(inputs).IsZero()) {
			return DEAD_END;
		}
	}

	int h = 0;
	for (const auto & explicitHeuristic : explicitHeuristics) {
		ADD evalNode = explicitHeuristic.Eval(inputs);
		int eval = Cudd_V(evalNode.getRegularNode());
		if (eval == -1) {
			return DEAD_END;
		}
		h = max(h, eval);
	}

	for (const auto & sph : spHeuristics) {
		int eval = sph.compute_heuristic(inputs);
		if (eval == -1) {
			return DEAD_END;
		}
		h = max(h, eval);
	}
	return h;
}

void SPMASHeuristic::dump_options() const {
	for (const auto & ph : phs)
		ph->dump_options();

	cout << "Generation time: " << generationTime << endl;
	cout << "Generation memory: " << generationMemory << endl;
}

static ScalarEvaluator *_parse(OptionParser &parser) {
	Heuristic::add_options_to_parser(parser);
	SymController::add_options_to_parser(parser, 30e3, 1e7);
	parser.add_list_option<SymPH *>("ph",
			"policies to generate abstractions. None by default.", "");
//	vector<SymPH *>()

	parser.add_option<int>("generation_time",
			"maximum time used in heuristic generation", "1200");

	parser.add_option<double>("generation_memory",
			"maximum memory used in heuristic generation", "3000000000");

	Options opts = parser.parse();
	if (parser.help_mode())
		return 0;

	if (parser.dry_run()) {
		return 0;
	} else {
		SPMASHeuristic *result = new SPMASHeuristic(opts);
		return result;
	}
}

static Plugin<ScalarEvaluator> _plugin("spmas", _parse);
