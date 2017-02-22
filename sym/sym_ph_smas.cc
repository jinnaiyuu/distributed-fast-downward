#include "sym_ph_smas.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../debug.h"
#include "../timer.h"
#include "sym_smas.h"
#include "sym_controller.h"
#include "sym_bdexp.h"

#include "smas_shrink_fh.h"
#include "smas_shrink_bisimulation.h"

#ifdef __CDT_PARSER__
#undef DEBUG_MSG
#define DEBUG_MSG(a)  ;
#endif

SymPHSMAS::SymPHSMAS(const Options &opts) :
		SymPH(opts), merge_strategy(
				MergeStrategy(opts.get_enum("merge_strategy"))), shrink_strategy(
				opts.get<SMASShrinkStrategy *>("shrink_strategy")), use_label_reduction(
				opts.get<bool>("reduce_labels")), use_expensive_statistics(
				opts.get<bool>("expensive_statistics")), cost_type(
				OperatorCost(opts.get_enum("cost_type"))), is_unit_cost_problem(
				true), finalMASAbstraction(nullptr)
/*,write_to_disk(opts.get<bool>("write_to_disk"))*/
{
	for (int i = 0; i < g_operators.size(); i++) {
		if (g_operators[i].get_cost() != 1) {
			is_unit_cost_problem = false;
			break;
		}
	}
}

SymBDExp * SymPHSMAS::relax(SymBDExp * bdExp, SymHNode * iniHNode, Dir dir,
		int num_relaxations) {
	//Only allow a single relaxation with ph_smas
	if (bdExp->getHNode()->getAbstraction() != finalMASAbstraction)
		return nullptr;
	cout << ">> Abstract " << *bdExp << " total time: " << g_timer << endl;
	unique_ptr<SymBDExp> newBDExp = relax_one_by_one(bdExp, iniHNode, dir,
			num_relaxations);
	SymBDExp * res = addHeuristicExploration(bdExp, move(newBDExp));
	if (res) {
		cout << ">> Abstracted exploration: " << *res << " total time: "
				<< g_timer << endl;
		res->getHNode()->getAbstraction()->print(cout, true);
	} else {
		//TODO: mark exploration as not relaxable ??
		cout << ">> Abstracted not possible. total time: " << g_timer << endl;
		if (finalMASAbstraction) {
			DEBUG_MSG(cout << "Adding explicit heuristic" << endl ;);
			map<int, BDD> h_abs_fw, h_abs_bw;
			//heuristic for fw search obtained by an explicit bw search
			// on the abstraction and viceversa
			finalMASAbstraction->getExplicitHeuristicBDD(false, h_abs_fw);
			DEBUG_MSG(cout << "Got explicit heuristic for fw search" << endl ;);
			finalMASAbstraction->getExplicitHeuristicBDD(true, h_abs_bw);
			DEBUG_MSG(cout << "Got explicit heuristic for bw search" << endl ;);
			intermediate_heuristics_fw.push_back(std::move(h_abs_fw));
			intermediate_heuristics_bw.push_back(std::move(h_abs_bw));
		}
	}

	return res;
}

unique_ptr<SymBDExp> SymPHSMAS::relax_one_by_one(SymBDExp * bdExp,
		SymHNode * iniHNode, Dir dir, int num_relaxations) {
	unique_ptr<SymBDExp> newBDExp = createBDExp(dir, bdExp);

	Timer time;
	SymHNode * hNode = iniHNode;

	while (hNode && time() < phTime && vars->totalMemory() < phMemory) {
		bool created = false;
		//I still do not have the new exploration I want
		//Select a more abstracted hNode and make recursive call
		for (auto child : hNode->getChildren(this)) {
			if (!child->hasExpFor(bdExp)) {
				hNode = child;
				created = true;
				break;
			}
		}

		if (!created) {
			unique_ptr<SymAbstraction> abs = abstract(hNode->getAbstraction(),
					phTime - time());
			if (abs) {
				hNode = hNode->getEngine()->createHNode(hNode, move(abs), this);
			} else {
				hNode = nullptr;
			}
		}

		if (!hNode || hNode->hasExpFor(bdExp)) {
			break;
		}DEBUG_MSG(cout << "We have a potential hNode" << endl ;);
		//I have a potential hNode
		if (relax_in(bdExp, newBDExp, hNode, num_relaxations)) {
			return newBDExp;
		}

		if (!hNode || !hNode->isUsefulFor(bdExp)) {
			break;
		}
	}

	return nullptr;
}

bool SymPHSMAS::init() {
	Timer timer;
	cout << "Initializing symbolic merge-and-shrink heuristic..." << endl;
	dump_options();
	//warn_on_unusual_options();
	if (domain_has_axioms() || domain_has_cond_effects()) {
		return false;
	}

	//int peak_memory = 0;
	//cout << "Done initializing merge-and-shrink heuristic [" << timer << "]" << endl;
	//cout << "Estimated peak memory for abstraction: " << peak_memory << " bytes" << endl;

	SymSMAS::build_atomic_abstractions(vars, is_unit_cost_problem, cost_type,
			atomic_abstractions);

	SMASShrinkStrategy * hpreserv_shrink =
			SMASShrinkBisimulation::create_default();
	cout << "Shrinking atomic abstractions..." << endl;
	for (size_t i = 0; i < atomic_abstractions.size(); ++i) {
		atomic_abstractions[i]->compute_distances();
		if (!atomic_abstractions[i]->is_solvable()) {
			cerr << "Atomic abstraction " << i << " ";
			copy(g_fact_names[i].begin(), g_fact_names[i].end(),
					ostream_iterator<string>(cerr, " "));
			cerr << "is unsolvable" << endl;
			//TODO: notify this to someone
		}

		hpreserv_shrink->shrink_atomic(*atomic_abstractions[i]);
	}
	delete hpreserv_shrink;
	return true;
}

std::unique_ptr<SymAbstraction> SymPHSMAS::noneAbstraction() {
	return unique_ptr<SymAbstraction>(
			new SymSMAS(vars, false, OperatorCost::NORMAL));
}

unique_ptr<SymAbstraction> SymPHSMAS::abstract(SymAbstraction * abs,
		double allotedTime) {
	Timer time;
	unique_ptr<SymAbstraction> tmp;
	if (!abs) {
		tmp = noneAbstraction();
		abs = tmp.get();
	}
	SymSMAS * abstraction = static_cast<SymSMAS *>(abs);
	DEBUG_MSG(cout << "SM&S abstract: " << *abstraction << endl ;);
	//cout << "Merging abstractions..." << endl;

	//  VariableOrderFinder order(merge_strategy, is_first);
	//int var_no = order.next();
	// cout << "First variable: #" << var_no << endl;
	// abstraction->statistics(use_expensive_statistics);
	bool abstracted = false;
	unique_ptr<SymSMAS> new_abstraction;
	do {
		//MERGE VARIABLE
		const set<int> & remainingVars = abstraction->getFullVars();
		if (remainingVars.empty()) {
			break;
		}
		//TODO: use merge strategies to select next variable (right now is
		//fixed to a single merge_strategy
		int var_no = *(remainingVars.begin());
		//int var_no = order.next();
		//cout << "Remaining vars: "; for(auto v : remainingVars) cout << v << " "; cout << endl;
		cout << "Next variable: #" << var_no << endl;
		SymSMAS *other_abstraction = atomic_abstractions[var_no];

		//Merge two variables
		if (!abstraction->isAbstracted()) {
			DEBUG_MSG(
					cout << "Abstraction" << *abstraction << " is not abstracted" << endl ;);
			abstraction = other_abstraction;
			continue;
		}

		DEBUG_MSG(cout << "Normalize" << endl ;);
		// TODO: When using nonlinear merge strategies, make sure not
		// to normalize multiple parts of a composite. See issue68.
		if (shrink_strategy->reduce_labels_before_shrinking()) {
			abstraction->normalize(use_label_reduction);
			other_abstraction->normalize(false);
		}DEBUG_MSG(cout << "Compute distances" << endl ;);
		abstraction->compute_distances();
		if (!abstraction->is_solvable()) {
			//TODO: Notify unsolvable problem
			return nullptr;
		}
		other_abstraction->compute_distances();

		// map<int, BDD> h_abs_fw, h_abs_bw;
		// //heuristic for fw search obtained by an explicit bw search on the abstraction and viceversa
		// abstraction->getExplicitHeuristicBDD(false, h_abs_fw);
		// abstraction->getExplicitHeuristicBDD(true, h_abs_bw);
		// map<int, BDD> h_other_abs_fw, h_other_abs_bw;
		// other_abstraction->getExplicitHeuristicBDD(false, h_other_abs_fw);
		// other_abstraction->getExplicitHeuristicBDD(true, h_other_abs_bw);

		DEBUG_MSG(cout << "Shrink before merge" << endl ;);
		abstracted = shrink_strategy->shrink_before_merge(*abstraction,
				*other_abstraction);
		//Store heuristics if necessary
		// if(!abstraction->is_f_preserved()){
		//   if(h_abs_fw.size() > 2)
		// 	intermediate_heuristics_fw.push_back(std::move(h_abs_fw));
		//   if(h_abs_bw.size() > 2)
		// 	intermediate_heuristics_bw.push_back(std::move(h_abs_bw));
		// }

		// if(!other_abstraction->is_f_preserved()){
		//   if(h_other_abs_fw.size() > 2)
		// 	intermediate_heuristics_fw.push_back(std::move(h_other_abs_fw));
		//   if(h_other_abs_bw.size() > 2)
		// 	intermediate_heuristics_bw.push_back(std::move(h_other_abs_bw));
		// }

		// TODO: Make shrink_before_merge return a pair of bools
		//       that tells us whether they have actually changed,
		//       and use that to decide whether to dump statistics?
		// (The old code would print statistics on abstraction iff it was
		// shrunk. This is not so easy any more since this method is not
		// in control, and the shrink strategy doesn't know whether we want
		// expensive statistics. As a temporary aid, we just print the
		// statistics always now, whether or not we shrunk.)
		abstraction->statistics(use_expensive_statistics);
		other_abstraction->statistics(use_expensive_statistics);

		abstraction->normalize(use_label_reduction);
		abstraction->statistics(use_expensive_statistics);

		// Don't label-reduce the atomic abstraction -- see issue68.
		other_abstraction->normalize(false);
		other_abstraction->statistics(use_expensive_statistics);

		DEBUG_MSG(cout << "Merge Symbolic Abstractions" << endl ;);
		//abstraction->printBDDs();
		//other_abstraction->printBDDs();
		//Merge

		auto tmpAbs = new SymSMAS(abstraction, other_abstraction,
				absTRsStrategy, notMutexBDDs);

		abstraction->release_memory();
		other_abstraction->release_memory();
		abstraction = tmpAbs;

		new_abstraction.reset(abstraction); //Ensures that memory is released
		new_abstraction->compute_distances();
		if (!new_abstraction->is_solvable()) {
			//TODO: Notify unsolvable problem
			return nullptr;
		}

		DEBUG_MSG(cout << "Merged." << endl ;);
	} while (!abstracted && time() < allotedTime
			&& vars->totalMemory() < phMemory);

	DEBUG_MSG(
			cout << "Return ph smas: time spent: " << time() << " of " << allotedTime << ", memory used: " << vars->totalMemory() << " of " << phMemory << endl ;);
	if (new_abstraction && abstracted) {
		DEBUG_MSG(cout << "Still not finished with this PH" << endl ;);
		finalMASAbstraction = new_abstraction.get();
		return move(new_abstraction);
	}
	return nullptr;

}

void SymPHSMAS::dump_options() const {
	SymPH::dump_options();
	cout << "Merge strategy: ";
	switch (merge_strategy) {
	case MERGE_LINEAR_CG_GOAL_LEVEL:
		cout << "linear CG/GOAL, tie breaking on level (main)";
		break;
	case MERGE_LINEAR_CG_GOAL_RANDOM:
		cout << "linear CG/GOAL, tie breaking random";
		break;
	case MERGE_LINEAR_GOAL_CG_LEVEL:
		cout << "linear GOAL/CG, tie breaking on level";
		break;
	case MERGE_LINEAR_RANDOM:
		cout << "linear random";
		break;
	case MERGE_DFP:
		cout << "Draeger/Finkbeiner/Podelski" << endl;
		cerr << "DFP merge strategy not implemented." << endl;
		exit(2);
	case MERGE_LINEAR_LEVEL:
		cout << "linear by level";
		break;
	case MERGE_LINEAR_REVERSE_LEVEL:
		cout << "linear by reverse level";
		break;
	default:
		abort();
	}
	cout << endl;

	shrink_strategy->dump_options();
//cout << "Write to disk: " << write_to_disk << endl;
	cout << "Label reduction: "
			<< (use_label_reduction ? "enabled" : "disabled") << endl
			<< "Expensive statistics: "
			<< (use_expensive_statistics ? "enabled" : "disabled") << endl;

	cout << "Operator cost: " << cost_type << endl;
}

static SymPH *_parse(OptionParser &parser) {
	SymPH::add_options_to_parser(parser, "SHRINK_AFTER_IMG"/*"REBUILD_TRS"*/,
			1);

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
	parser.add_enum_option("merge_strategy", merge_strategies,
			"merge strategy", "MERGE_LINEAR_CG_GOAL_LEVEL");

// TODO: Default shrink strategy should only be created
// when it's actually used.
//	SMASShrinkStrategy *def_shrink = SMASShrinkFH::create_default(50000);

	parser.add_option<SMASShrinkStrategy *>("shrink_strategy",
			"shrink strategy: TODO: may contain error", "50000");

	parser.add_option<bool>("reduce_labels", "enable label reduction", "true");
	parser.add_option<bool>("expensive_statistics",
			"show statistics on \"unique unlabeled edges\" (WARNING: "
					"these are *very* slow -- check the warning in the output)",
			"false");

	vector<string> cost_types;
	cost_types.push_back("NORMAL");
	cost_types.push_back("ONE");
	cost_types.push_back("PLUSONE");
	parser.add_enum_option("cost_type", cost_types, "NORMAL",
			"operator cost adjustment type");

//parser.add_option<bool>("write_to_disk", false, "write BDDs to disk and exit");

	Options opts = parser.parse();
	if (parser.help_mode())
		return 0;

	MergeStrategy merge_strategy = MergeStrategy(
			opts.get_enum("merge_strategy"));
	if (merge_strategy < 0 || merge_strategy >= MAX_MERGE_STRATEGY) {
		cerr << "error: unknown merge strategy: " << merge_strategy << endl;
		exit(2);
	}

	SymPH *policy = 0;

	if (!parser.dry_run()) {
		policy = new SymPHSMAS(opts);
	}

	return policy;
}

static Plugin<SymPH> _plugin("smas", _parse);
