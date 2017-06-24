#include "sym_engine.h"

#include "sym_bdexp.h"
#include "sym_solution.h"
#include "sym_enums.h"
#include "sym_hnode.h"
#include "../debug.h" 
#include "../globals.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "sym_ph.h"
#include "test/sym_test.h"

#include <set>
using namespace std;

SymEngine::SymEngine(const Options &opts) :
		SearchEngine(opts), SymController(opts), searchDir(
				Dir(opts.get_enum("search_dir"))), phs(
				opts.get_list<SymPH *>("ph")), lower_bound(0), originalStateSpace(
				nullptr), originalSearch(nullptr) {

}

void SymEngine::initialize() {
	cout << "Conducting symbolic search" << " Operator cost: " << cost_type
			<< " (real) bound = " << bound << endl;

#ifdef DEBUG_GST
	gst_plan.loadPlan("plan.test", *(vars.get()));
#endif

	cout << "Initialize abstraction hierarchy" << endl;
	originalStateSpace = new SymHNode(this, mgrParams);
	nodes.push_back(unique_ptr<SymHNode>(originalStateSpace));
	printf("SymBDExp\n");
	originalSearch = new SymBDExp(this, searchParams, searchDir);
	unique_ptr<SymBDExp> refExp(originalSearch);

	printf("SymPH\n");

	vector<SymPH *> tmp;
	tmp.swap(phs);
	for (auto ph : tmp) {
		if (ph->init(this, vars.get(), originalStateSpace->getManager())) {
			phs.push_back(ph);
		} else {
			delete ph;
		}
	}

	printf("initFrontier\n");

	if (refExp->initFrontier(originalStateSpace, numeric_limits<int>::max(),
			numeric_limits<int>::max())
			&& refExp->initAll(numeric_limits<int>::max(),
					numeric_limits<int>::max())) {
		originalStateSpace->add_exploration(std::move(refExp));
	} else {
		cout << "Init exploration failed" << endl;
		exit(-1);
	}

	//hierarchy->init(this, policyHierarchy);
	//hierarchy->init_exploration(searchDir);
}

void SymEngine::statistics() const {
	search_progress.print_statistics();
	search_space.statistics();
}

int SymEngine::stepReturn() const {
	if (!solved()) {
		return IN_PROGRESS;
	} else if (found_solution()) {
		return SOLVED;
	} else {
		return FAILED;
	}
}

void SymEngine::dump_search_space() {
	cout << *originalSearch << endl;
}

void SymEngine::new_solution(const SymSolution & sol) {
	if (sol.getCost() < bound) {
		bound = sol.getCost();
		vector<const Operator *> plan;
		sol.getPlan(plan);
		set_plan(plan);
		cout << "BOUND: " << lower_bound << " < " << bound << ", total time: "
				<< g_timer << endl;
	}
}

void SymEngine::add_options_to_parser(OptionParser &parser) {
	SearchEngine::add_options_to_parser(parser);
	SymController::add_options_to_parser(parser, 45e3, 1e7);

	parser.add_enum_option("search_dir", DirValues, "search direction",
			"BIDIR");
	parser.add_list_option<SymPH *>("ph",
			"policies to generate abstractions. None by default.", "[]");
//	parser.add_list_option<SymPH *>("ph", vector<SymPH *>(),
//			"policies to generate abstractions. None by default.");
}

void SymEngine::set_default_options(Options & opts) {
	opts.set<int>("search_dir", 2);
}

SymBDExp * SymEngine::relax(SymBDExp * exp) const {
	for (auto ph : phs) {
		SymBDExp * res = ph->relax(exp);
		if (res)
			return res;
	}
	return nullptr;
}
