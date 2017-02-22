#include "sym_ph_pdbs.h"

#include "sym_engine.h" 
#include "sym_hnode.h"
#include "sym_bdexp.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../globals.h"
#include "../rng.h"
#include "sym_pdb.h"
#include "../debug.h"

#ifdef __CDT_PARSER__
#undef DEBUG_MSG
#define DEBUG_MSG(a)  ;
#endif

SymPHPDBs::SymPHPDBs(const Options &opts) :
		SymPH(opts), strategy(LinearPDBStrategy(opts.get_enum("st"))), strategy_abstract(
				LinearPDBStrategy(opts.get_enum("st_abs"))), var_strategy(
				MergeStrategy(opts.get_enum("sel_var"))) {
}

bool SymPHPDBs::init() {
	return true;
}

SymBDExp * SymPHPDBs::relax(SymBDExp * bdExp, SymHNode * iniHNode, Dir dir,
		int num_relaxations) {
	cout << ">> Abstract " << *bdExp << " total time: " << g_timer << endl;
	vector<SymHNode *> nodes;
	SymHNode * hNode = iniHNode;
	if (iniHNode->isAbstracted()) {
		bool added = true;
		while (hNode && added) {
			added = false;
			for (auto child : hNode->getChildren(this)) {
				if (child->empty()) {
					nodes.push_back(child);
					hNode = child;
					added = true;
					break;
				}
			}
		}
	}
	getListAbstraction(hNode, nodes);

	//Failed to generate any more abstract state space
	if (nodes.empty())
		return nullptr;

	DEBUG_MSG(
			cout << "List of abstractions: " << endl; for(auto node : nodes) cout << *node << endl;);

	unique_ptr<SymBDExp> newBDExp;
	switch ((iniHNode->isAbstracted() ? strategy_abstract : strategy)) {
	case LinearPDBStrategy::LEVEL:
		newBDExp = select_linear(nodes, bdExp, dir, num_relaxations);
		break;
	case LinearPDBStrategy::REVERSE:
		newBDExp = select_reverse(nodes, bdExp, dir, num_relaxations);
		break;
	case LinearPDBStrategy::BINARY:
		newBDExp = select_binary_search(nodes, bdExp, dir, num_relaxations);
		break;
	}
	SymBDExp * res = addHeuristicExploration(bdExp, move(newBDExp));

	if (res) {
		cout << ">> Abstracted exploration: " << *res << " total time: "
				<< g_timer << endl;
		DEBUG_MSG(res->getHNode()->getAbstraction()->print(cout, true););
	} else {
		//TODO: mark exploration as not relaxable ??
		cout << ">> Abstracted not possible. total time: " << g_timer << endl;
	}
	return res;
}

void SymPHPDBs::getListAbstraction(SymHNode * hNode, vector<SymHNode *> & res) {
	SymHNode * current = hNode;
	vector<int> remainingVars;
	if (current->getAbstraction()) {
		const auto & full = current->getAbstraction()->getFullVars();
		remainingVars.insert(end(remainingVars), begin(full), end(full));
	} else {
		for (int i = 0; i < g_variable_name.size(); i++) {
			remainingVars.push_back(i);
		}
	}DEBUG_MSG(
			cout << "Remaining: "; for(auto i : remainingVars){ cout << i << " "; } cout << endl;);
//	bool is_first = generatedSets.empty();
//  VariableOrderFinder vof (var_strategy, is_first, remainingVars);
	VariableOrderFinder vof(var_strategy);
	printf("VariableOrderFinder Constructor may have error.\n");

	vector<int> var_ordering;
	while (!vof.done()) {
		int var = vof.next();
		var_ordering.push_back(var);
		DEBUG_MSG(cout << "Selected var:"<< var << endl;);
		if (vof.done())
			break;
	}

	set<int> currentVars(begin(remainingVars), end(remainingVars));
	for (auto vit = var_ordering.rbegin(); vit != var_ordering.rend(); ++vit) {
		currentVars.erase(*vit);
		//for(auto var : var_ordering){
		//currentVars.erase(var);
		if (generatedSets.count(currentVars)) {
			if (generatedSets[currentVars]->empty()) {
				current = generatedSets[currentVars];
			}
		} else {
			SymPDB * newPDB = new SymPDB(vars, absTRsStrategy, currentVars);
			current = engine->createHNode(current,
					unique_ptr<SymAbstraction>(newPDB), this);
			generatedSets[currentVars] = current;
		}
		res.push_back(current);
	}
}

unique_ptr<SymBDExp> SymPHPDBs::select_linear(const vector<SymHNode *> & nodes,
		SymBDExp * bdExp, Dir dir, int num_relaxations) {
	Timer time_select;
	DEBUG_MSG(cout << "Relax one by one: " << nodes.size() << endl;);
	unique_ptr<SymBDExp> newBDExp = createBDExp(dir, bdExp);
	for (SymHNode * hNode : nodes) {
		if (relax_in(bdExp, newBDExp, hNode, num_relaxations)) {
			return newBDExp;
		}
		if (time_select() > phTime || vars->totalMemory() > phMemory
				|| !hNode->isUsefulFor(bdExp)) {
			break;
		}
	}
	return nullptr;
}

unique_ptr<SymBDExp> SymPHPDBs::select_reverse(const vector<SymHNode *> & nodes,
		SymBDExp * bdExp, Dir dir, int num_relaxations) {
	cout << "Relax one by one" << endl;
	unique_ptr<SymBDExp> newBDExp;
	for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
		SymHNode * hNode = *it;
		if (!hNode->isUsefulFor(bdExp) || hNode->hasExpFor(bdExp)) {
			continue;
		}

		if (!newBDExp) {
			newBDExp = createBDExp(dir, bdExp);
		}

		//I have a potential hNode
		if (relax_in(bdExp, newBDExp, hNode, num_relaxations)) {
			return newBDExp;
		} else {
			newBDExp.reset(nullptr);
			if (hNode->isUsefulFor(bdExp)) {
				break; //and return nullptr
			}
		}
	}
	newBDExp.reset(nullptr);
	return newBDExp;
}

unique_ptr<SymBDExp> SymPHPDBs::select_binary_search(
		const vector<SymHNode *> & nodes, SymBDExp * bdExp, Dir dir,
		int num_relaxations) {
	Timer time_select;
	int imin = 0; // Most informed abstraction
	int imax = nodes.size() - 1; //Most relaxed abstraction
	unique_ptr<SymBDExp> newExp;
	unique_ptr<SymBDExp> best;
	int trials = 0;
	while (imax >= imin && time_select() <= phTime
			&& vars->totalMemory() <= phMemory && trials++ < 15) { //TODO: HACK: this 15 should be a parameter
		int imid = (imax + imin) / 2;
		DEBUG_MSG(
				cout << "imax: " << imax << " imin: " << imin << " imid: " << imid << endl;);
		if (!nodes[imid]->isUsefulFor(bdExp)) {
			//Not successful because I need to relax less
			imax = imid - 1;
			continue;
		} else if (nodes[imid]->hasExpFor(bdExp)) {
			//Not successful because I need to relax more
			imin = imid + 1;
			continue;
		}
		if (!newExp) {
			newExp = createBDExp(dir, bdExp);
		}
		bool success = relax_in(bdExp, newExp, nodes[imid], num_relaxations);

		if (success) {
			//Sucessful, I should try to relax less
			best = move(newExp);
			newExp = createBDExp(dir, bdExp);
			imax = imid - 1;
		} else if (!nodes[imid]->isUsefulFor(bdExp)) {
			//Not successful because I need to relax less
			newExp = createBDExp(dir, bdExp);
			imax = imid - 1;
		} else if (nodes[imid]->hasExpFor(bdExp)) {
			//Not successful because I need to relax more
			imin = imid + 1;
		} else {
			// exit(-1);
		}
	}

	if (!best) {
		//TODO: implement this for multiple abstractions
		//I have not found any best, that means that imin is not useful and imax is not relaxable/searchable.
		//Try to relax from imax in other path
		/*   do {

		 //If still not a search is found: mark imax as notuseful and return
		 }while(maxNode != bdExp->getHNode());*/
		//If we reach this point with best = nullptr, no relaxation is possible
	}

	return best;
}

static SymPH *_parse(OptionParser &parser) {
	//maximum of 100 PDBs by default
	SymPH::add_options_to_parser(parser, "IND_TR_SHRINK", 100);

	vector<string> merge_strategies;
	//TODO: it's a bit dangerous that the merge strategies here
	// have to be specified exactly in the same order
	// as in the enum definition. Try to find a way around this,
	// or at least raise an error when the order is wrong.
	merge_strategies.push_back("MERGE_LINEAR_CG_GOAL_LEVEL");
	merge_strategies.push_back("MERGE_LINEAR_CG_GOAL_RANDOM");
	merge_strategies.push_back("MERGE_LINEAR_GOAL_CG_LEVEL");
	merge_strategies.push_back("MERGE_LINEAR_RANDOM");
	merge_strategies.push_back("MERGE_DFP");
	merge_strategies.push_back("MERGE_LINEAR_LEVEL");
	merge_strategies.push_back("MERGE_LINEAR_REVERSE_LEVEL");
	parser.add_enum_option("sel_var", merge_strategies,
			"merge strategy to select which vars to abstract",
			"MERGE_LINEAR_GOAL_CG_LEVEL");

	parser.add_enum_option("st", LinearPDBStrategyValues,
			"relax strategy", "binary");
	parser.add_enum_option("st_abs", LinearPDBStrategyValues,
			"relax strategy for abstractions", "level");

	Options opts = parser.parse();

	SymPH *policy = 0;
	if (!parser.dry_run()) {
		policy = new SymPHPDBs(opts);
	}

	return policy;
}

void SymPHPDBs::dump_options() const {
	SymPH::dump_options();
	cout << "   Relax Strategy: " << strategy << ", " << strategy_abstract
			<< endl;
	cout << "Selection of Variables: " << var_strategy << endl;
}

static Plugin<SymPH> _plugin("pdbs", _parse);

// std::unique_ptr<SymAbstraction> SymPHPDBs:noneAbstraction(){
//   set<int> selectedVars;
//   for(int i = 0; i < g_variable_name.size(); i++){
//     selectedVars.insert(i);
//   }
//   //  generatedSets[selectedVars] = nullptr;
//   return unique_ptr<SymAbstraction> (new SymPDB(vars, absTRsStrategy, selectedVars));
// }
