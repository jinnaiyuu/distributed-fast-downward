#include "symba.h"

#include "sym_ph.h" 
#include "sym_exploration.h"
#include "sym_bdexp.h"
#include "sym_hnode.h" 
#include "../debug.h"
#include "../option_parser.h"
#include "../plugin.h"

SymBA::SymBA(const Options &opts) :
		SymEngine(opts), t_orig(opts.get<double>("t_orig")), currentPH(0) {
	printf("SymBA::SymBA\n");
}

void SymBA::initialize() {
	print_options();
	SymEngine::initialize();
}

int SymBA::step() {
	SymExploration * currentSearch = selectExploration();
	if (currentSearch) {
		currentSearch->stepImage();
		if (!currentSearch->isAbstracted()) {
			for (auto ph : phs) {
				ph->operate(originalSearch);
			}
		}
	}
	return stepReturn();
}

SymExploration * SymBA::selectExploration() {
	if (forceOriginal()) {
		// We are forced to search in the original state space because no
		// more time should be spent on abstractions
		return originalSearch->selectBestDirection(true);
	}

	//I cannot explore the original state space. I must select a
	// relaxed search that is useful and explorable.
	vector<SymExploration *> potentialExplorations;
	potentialExplorations.push_back(originalSearch->getFw());
	potentialExplorations.push_back(originalSearch->getBw());
	originalSearch->getFw()->getPossiblyUsefulExplorations(
			potentialExplorations);
	originalSearch->getBw()->getPossiblyUsefulExplorations(
			potentialExplorations);

	//1) Look in already generated explorations => get the easiest one
	//(gives preference to shouldSearch abstractions)
	std::sort(begin(potentialExplorations), end(potentialExplorations),
			[this] (const SymExploration * e1, const SymExploration * e2) {
				return e1->isBetter (*e2);
			});
	for (auto exp : potentialExplorations) {
		if (exp->isSearchable() && exp->isUseful()) {
			return exp;
		}
	}

	//2) Select a hierarchy policy and generate a new exploration
	// for(SymExploration * exp : potentialExplorations){
	//   //Check if it is useful (because if the other direction was deemed
	//   //as no useful), then we should not try to relax it again
	//   if(!exp->isAbstracted() || !exp->getBDExp()->isRelaxable() || !exp->isUseful()) continue;

	//   SymBDExp * newBDExp = exp->getBDExp()->relax();
	//   if(newBDExp){
	//     if(newBDExp->isSearchable()){
	// 	DEBUG_MSG(cout << "Select best direction of: " << *newBDExp << endl;);
	// 	return newBDExp->selectBestDirection();
	//     }else{
	// 	//Add explicit heuristic
	//     }
	//   }
	// }

	//3) Ask hierarchy policies to generate new heuristics/explorations
	for (int i = 0; i < phs.size(); i++) { //Once per heuristic
		bool didSomething = phs[currentPH]->askHeuristic(originalSearch,
				t_orig - g_timer());
		currentPH++;
		if (currentPH >= phs.size()) {
			currentPH = 0;
		}
		if (didSomething)
			return nullptr;
	}

	//4) We cannot search anything, just keep trying original search
	//¿continue easiest instead?
	return originalSearch->selectBestDirection(true);
}

void SymBA::print_options() const {
	cout << "SymBA* " << endl;
	cout << "   Search dir: " << searchDir << cout << endl;
	cout << "   Time force original: " << t_orig << " seconds" << endl;
}

static SearchEngine *_parse(OptionParser &parser) {
	SymEngine::add_options_to_parser(parser);
	parser.add_option<double>("t_orig",
			"After t_orig seconds, only search on the original state space.",
			"1500.0");
	Options opts = parser.parse();

	SearchEngine *policy = 0;
	if (!parser.dry_run()) {
		policy = new SymBA(opts);
	}
	return policy;
}

static Plugin<SearchEngine> _plugin("symba", _parse);
