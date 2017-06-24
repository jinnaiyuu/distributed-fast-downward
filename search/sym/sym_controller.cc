#include "sym_controller.h"

#include "sym_manager.h"
#include "sym_bdexp.h"
#include "sym_hnode.h"
#include "../debug.h" 
#include "../globals.h"

using namespace std;

SymController::SymController(const Options &opts) :
		vars(new SymVariables()), mgrParams(opts), searchParams(opts) {
	vector<int> var_order;
	for (int i = 0; i < g_variable_name.size(); i++) {
		var_order.push_back(i);
	}
	vars->init(var_order, mgrParams);
	mgrParams.print_options();
	searchParams.print_options();
}

SymHNode * SymController::createHNode(SymHNode * node,
		unique_ptr<SymAbstraction> && abs, SymPH * ph) {
	SymHNode * newNode = new SymHNode(node, ph, move(abs));
	nodes.push_back(unique_ptr<SymHNode>(newNode));
	node->addChildren(newNode);
	newNode->addParent(node);
	return newNode;
}

void SymController::add_options_to_parser(OptionParser &parser, int maxStepTime,
		int maxStepNodes) {
	SymParamsMgr::add_options_to_parser(parser);
	SymParamsSearch::add_options_to_parser(parser, maxStepTime, maxStepNodes);
}
