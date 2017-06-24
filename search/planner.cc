#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "ext/tree_util.hh"
#include "timer.h"
#include "utilities.h"
#include "search_engine.h"
#include "wtimer.h"


#include <iostream>
#include <fstream>
#include <new>
using namespace std;

int main(int argc, const char **argv) {
    register_event_handlers();

    if (argc < 2) {
        cout << OptionParser::usage(argv[0]) << endl;
        exit_with(EXIT_INPUT_ERROR);
    }

//    if (string(argv[1]).compare("--help") != 0)
//        read_everything(cin);

    std::ifstream is(argv[1], std::ifstream::in);
    if (is) {
    	read_everything(is);
    } else {
    	cout << "output file not in place" << endl;
    	exit_with(EXIT_INPUT_ERROR);
    }

    argc--;
    argv++;

    SearchEngine *engine = 0;

    //the input will be parsed twice:
    //once in dry-run mode, to check for simple input errors,
    //then in normal mode
    try {
        OptionParser::parse_cmd_line(argc, argv, true);
        engine = OptionParser::parse_cmd_line(argc, argv, false);
    } catch (ParseError &pe) {
        cerr << pe << endl;
        exit_with(EXIT_INPUT_ERROR);
    }

    Timer search_timer;
    WTimer wall_timer;
    engine->search();
    wall_timer.stop();
    search_timer.stop();
    g_timer.stop();
    cout << "done search\n";
    engine->save_plan_if_necessary();
    engine->statistics();
    engine->heuristic_statistics();
    cout << "Search walltime: " << wall_timer << endl;
    cout << "Search time: " << search_timer << endl;
    cout << "Total time: " << g_timer << endl;

    if (engine->found_solution()) {
        exit_with(EXIT_PLAN_FOUND);
    } else {
        exit_with(EXIT_UNSOLVED_INCOMPLETE);
    }
}
