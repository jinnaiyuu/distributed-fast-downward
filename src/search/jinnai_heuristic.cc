#include "jinnai_heuristic.h"

#include "globals.h"
#include "option_parser.h"
#include "plugin.h"
#include "state.h"


// For debugging.
#include <iostream>



JinnaiHeuristic::JinnaiHeuristic(const Options &opts)
  : Heuristic(opts), offset(opts.get<int>("offset")) {
}

JinnaiHeuristic::~JinnaiHeuristic() {
}

void JinnaiHeuristic::initialize() {
    cout << "Initializing jinnai heuristic..." << endl;
    dump_everything();
    robby = 0;
    left  = 1;
    right = 2;    
    free = g_variable_domain.size() - 3;
    cout << "g_variable_domain.size() = " << g_variable_domain.size() << endl;
}

// Domain specifit heuristic for gripper.
int JinnaiHeuristic::compute_heuristic(const State &state) {
    int unsatisfied_goal_count = 0;
    // For now its not perfect.
    //    int perfect_heuristic = 0;


    for (int i = 0; i < g_goal.size(); i++) {
        int var = g_goal[i].first, value = g_goal[i].second;
        if (state[var] != value) {
            unsatisfied_goal_count++;
	}
    }
    

    /*
    vars[0] is about at-robby. if in rooma ->0, in roomb -> 1.
    vars[1] is what the left arm is holding.
    If its free, value would be = g_variable_domain.size() - 3.
    vars[2] is that of right arm.

    So there are six possible states for these variables.

    1. robby holds nothing at rooma.
       code: vars[0] == 0 && vars[1] == vars[2] == free;
       even unsat -> unsat*3-1 
       odd unsat  -> unsat*3 
    2. robby holds a single ball at rooma.
       code: vars[0] == 0 && vars[1] == free && vars[2] != free ||
             vars[0] == 0 && vars[1] != free && vars[2] == free
       even unsat -> unsat*3-2
       odd unsat  -> unsat*3-1
    3. robby holds two balls at rooma.
       code: vars[0] == 0 && vars[1] != free && vars[2] != free
       even unsat -> unsat*3-3
       odd unsat  -> unsat*3-2
    4. robby holds two balls at roomb.
       -> vars[1] == 1 && vars[1] != free && vars[2] != free
       even unsat -> unsat*3-4
       odd unsat  -> unsat*3-3
    5. robby holds a single ball at roomb.
       -> vars[0] == 1 && vars[1] == free && vars[2] != free ||
          vars[0] == 1 && vars[1] != free && vars[2] == free
       even unsat -> unsat*3-1
       odd unsat  -> unsat*3-2    As one of unsat has solved, even & odd flips.
    6. robby holds nothing at roomb.
       -> vars[0] == 0 && vars[1] == vars[2] == free
       even unsat -> unsat*3
       odd unsat  -> unsat*3+1

       Might be able to make it more simple.
    */

    

    bool is_even = (unsatisfied_goal_count % 2 == 0);

    /*
    state.dump_fdr();
    cout << "robby, left, right, is_even = ";
    cout << state[robby] << ", " << state[left] 
	 << ", " << state[right] << ", " << is_even << endl;
    */

    int base_heuristic = unsatisfied_goal_count*3 + offset;
    int h = 0;

    if (state[robby] == 0) {
      if (state[left] == free && state[right] == free) {
	h = base_heuristic - is_even;
      } else if ((state[left] == free && state[right] != free) || 
		 (state[left] != free && state[right] == free)){
	h = base_heuristic - is_even - 1;
      } else {
	h = base_heuristic - is_even - 2;
      }
    } else { // state[0] == 1
      if (state[left] == free && state[right] == free) {
	h = base_heuristic - is_even + 1;
      } else if ((state[left] == free && state[right] != free) || 
		 (state[left] != free && state[right] == free)){
	h = base_heuristic + (is_even - 1) - 1; // even & odd flips here.
      } else {
	h = base_heuristic - is_even - 3;
      }
    }
    
    if (h < 0) {
      return 0;
    }
    return h;
    
    /*
    std::cout << "Facts:" << std::endl;
    for (vector<vector<string> >::iterator iter = g_fact_names.begin();
	 iter != g_fact_names.end(); ++iter) {
      for (vector<string>::iterator i = iter->begin();
	   i != iter->end(); ++i) {
	std::cout << *i << ", ";
       }      
    }
    std::cout << std::endl << std::endl;
    */
    // If even amount of unsat goal
    /*
    if (unsatisfied_goal_count % 2 == 0) {
      perfect_heuristic += base_heuristic;
    } else {
      perfect_heuristic += (unsatisfied_goal_count-1)*3 + 4;
    }
    */

    //    return perfect_heuristic;
}

static Heuristic *_parse(OptionParser &parser) {
    parser.document_synopsis("Jinnai heuristic", "");
    parser.document_language_support("action costs", "ignored by design");
    parser.document_language_support("conditional_effects", "supported");
    parser.document_language_support("axioms", "supported");
    parser.document_property("admissible", "no");
    parser.document_property("consistent", "no");
    parser.document_property("safe", "yes");
    parser.document_property("preferred operators", "no");

    // TODO: not sure correct.
    parser.add_option<int>("offset");
    Heuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new JinnaiHeuristic(opts);
}


static Plugin<Heuristic> _plugin("jinnai", _parse);
