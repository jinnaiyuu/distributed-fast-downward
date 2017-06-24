#include "smas_shrink_strategy.h"

#include "sym_smas.h"

#include "../option_parser.h"
#include "../debug.h"


#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using namespace std;
using namespace __gnu_cxx;


SMASShrinkStrategy::SMASShrinkStrategy(const Options &opts)
    : max_states(opts.get<int>("max_states")),
      max_states_before_merge(opts.get<int>("max_states_before_merge")), 
      max_trs(opts.get<int>("max_trs")) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
}


SMASShrinkStrategy::~SMASShrinkStrategy() {
}

void SMASShrinkStrategy::dump_options() const {
    cout << "Shrink strategy: " << name() << endl;
    cout << "Abstraction size limit: " << max_states << endl
         << "Abstraction size limit right before merge: "
         << max_states_before_merge << endl;
    dump_strategy_specific_options();
}

void SMASShrinkStrategy::dump_strategy_specific_options() const {
    // Default implementation does nothing.
}

bool SMASShrinkStrategy::must_shrink(
    const SymSMAS &abs, int threshold, bool force) const {
    assert(threshold >= 1);
    assert(abs.is_solvable());
    if (abs.size() > threshold) {
        cout << abs.tag() << "shrink from size " << abs.size()
             << " (threshold: " << threshold << ")" << endl;
        return true;
    }
    if (force) {
        cout << abs.tag()
             << "shrink forced to prune unreachable/irrelevant states" << endl;
        return true;
    }
    return false;
}

pair<int, int> SMASShrinkStrategy::
compute_shrink_sizes(int size1, int size2, 
		     int trs1, int trs2) const {
  int max_st = max_states;
  //TODO: If we are close to max_trs we may reduce max states. Maybe
  //we should parametrize this
  if(max_trs > 0 && trs1 > max_trs/2){
    max_st = min<int>(max_st, size1*((double)max_trs/trs1));
  }
  if(max_trs > 0 && trs2 > max_trs/2){
    max_st = min<int>(max_st, size2*((double)max_trs/trs2));
  }
  max_st = max<int> (1, max_st);

  DEBUG_MSG(cout << "size: " << size1 << " " << size2 << 
	    " trs: " << trs1 << " " << trs2 
	    << "   Selected max_st: " << max_st << endl;);
  
  // Bound both sizes by max allowed size before merge.
  int new_size1 = min(size1, max_states_before_merge);
  int new_size2 = min(size2, max_states_before_merge);
  
    // Check if product would exceed max allowed size.
    // Use division instead of multiplication to avoid overflow.
    if (max_st / new_size1 < new_size2) {
        int balanced_size = int(sqrt(max_st));

        // Shrink size2 (which in the linear strategies is the size
        // for the atomic abstraction) down to balanced_size if larger.
        new_size2 = min(new_size2, balanced_size);

        // Use whatever is left for size1.
        new_size1 = min(new_size1, max_st / new_size2);
    }
    assert(new_size1 <= size1 && new_size2 <= size2);
    assert(new_size1 <= max_states_before_merge);
    assert(new_size2 <= max_states_before_merge);
    assert(new_size1 * new_size2 <= max_st);
    return make_pair(new_size1, new_size2);
}

void SMASShrinkStrategy::shrink_atomic(SymSMAS & /*abs*/) {
    // Default implemention does nothing.
}

bool SMASShrinkStrategy::shrink_before_merge(SymSMAS &abs1, SymSMAS &abs2) {
  pair<int, int> new_sizes = compute_shrink_sizes(abs1.size(), abs2.size(), 
						  abs1.total_transitions(), 
						  abs2.total_transitions());
    int new_size1 = new_sizes.first;
    int new_size2 = new_sizes.second;

    // HACK: The output is based on the assumptions of a linear merge
    //       strategy. It would be better (and quite possible) to
    //       treat both abstractions exactly the same here by amending
    //       the output a bit.
    
    if (new_size2 != abs2.size()) {
        cout << abs2.tag() << "atomic abstraction too big; must shrink" << endl;
        shrink(abs2, new_size2);
    }

    if (new_size1 != abs1.size()) {
        shrink(abs1, new_size1);
	return true;
    }
    return false;
}

/*
  TODO: I think we could get a nicer division of responsibilities if
  this method were part of the abstraction class. The shrink
  strategies would then return generate an equivalence class
  ("collapsed_groups") and not modify the abstraction, which would be
  passed as const.
 */

void SMASShrinkStrategy::apply(
    SymSMAS &abs,
    EquivalenceRelation &equivalence_relation,
    int target) const {
    assert(equivalence_relation.size() <= target);
    abs.apply_abstraction(equivalence_relation);
    cout << abs.tag() << "size after shrink " << abs.size()
         << ", target " << target << endl;
    assert(abs.size() <= target);
}

void SMASShrinkStrategy::add_options_to_parser(OptionParser &parser) {
    // TODO: better documentation what each parameter does
    parser.add_option<int>(
        "max_states",
        "maximum abstraction size", "-1");
    parser.add_option<int>(
        "max_states_before_merge",
        "maximum abstraction size for factors of synchronized product", "-1");
    parser.add_option<int>("max_trs",
			   "maximum number of transitions", "-1");

}

void SMASShrinkStrategy::handle_option_defaults(Options &opts) {
    int max_states = opts.get<int>("max_states");
    int max_states_before_merge = opts.get<int>("max_states_before_merge");
    if (max_states == -1 && max_states_before_merge == -1) {
        // None of the two options specified: set default limit.
        max_states = 50000;
    }

    // If exactly one of the max_states options has been set, set the other
    // so that it imposes no further limits.
    if (max_states_before_merge == -1) {
        max_states_before_merge = max_states;
    } else if (max_states == -1) {
        int n = max_states_before_merge;
        max_states = n * n;
        if (max_states < 0 || max_states / n != n)         // overflow
            max_states = numeric_limits<int>::max();
    }

    if (max_states_before_merge > max_states) {
        cerr << "warning: max_states_before_merge exceeds max_states, "
             << "correcting." << endl;
        max_states_before_merge = max_states;
    }

    if (max_states < 1) {
        cerr << "error: abstraction size must be at least 1" << endl;
        exit(2);
    }

    if (max_states_before_merge < 1) {
        cerr << "error: abstraction size before merge must be at least 1"
             << endl;
        exit(2);
    }

    opts.set<int>("max_states", max_states);
    opts.set<int>("max_states_before_merge", max_states_before_merge);
}


