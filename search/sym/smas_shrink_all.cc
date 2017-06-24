#include "smas_shrink_all.h"

#include "sym_smas.h"

#include "../option_parser.h"
#include "../plugin.h"

#include <cassert>
#include <iostream>
#include <vector>
using namespace std;


SMASShrinkAll::SMASShrinkAll(const Options &opts)
    : SMASShrinkStrategy(opts) {
}

SMASShrinkAll::~SMASShrinkAll() {
}

bool SMASShrinkAll::reduce_labels_before_shrinking() const {
    return false;
}

bool SMASShrinkAll::shrink_before_merge(SymSMAS &abs1, SymSMAS &/*abs2*/){
  return shrink(abs1, 1);
}

bool SMASShrinkAll::shrink(SymSMAS &abs,
		       int threshold, bool force) {
    if (must_shrink(abs, threshold, force)) {
	EquivalenceClass group;
	for(int i = 0; i < abs.size(); i++) {
	  group.push_front(i);
	}
        EquivalenceRelation equiv_relation(1, group);

        apply(abs, equiv_relation, threshold);
	return true;
    }
    return false;
}

string SMASShrinkAll::name() const{
  return "shrink_all";
}

static SMASShrinkStrategy *_parse(OptionParser &parser) {
    SMASShrinkStrategy::add_options_to_parser(parser);
    Options opts = parser.parse();
    SMASShrinkStrategy::handle_option_defaults(opts);

    if (!parser.dry_run())
        return new SMASShrinkAll(opts);
    else
        return 0;
}

static Plugin<SMASShrinkStrategy> _plugin("shrink_all", _parse);

