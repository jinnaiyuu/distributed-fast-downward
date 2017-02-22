#include "state.h"

#include "globals.h"
#include "utilities.h"
#include "state_registry.h"

#include <algorithm>
#include <iostream>
#include <cassert>
using namespace std;

State::State(const state_var_t *buffer, const StateRegistry &registry_,
		StateID id_) :
		vars(buffer), registry(&registry_), id(id_) {
	assert(vars);
	assert(id != StateID::no_state);
}

//// For Symbolic search
//State::State(const vector<int> & val) {
//	_allocate();
//	for (int i = 0; i < val.size(); i++) {
//		vars[i] = val[i];
//	}
//}

//void State::_allocate() {
//    borrowed_buffer = false;
//    vars = new state_var_t[g_variable_domain.size()];
//}
//
//void State::_deallocate() {
//    if (!borrowed_buffer)
//        delete[] vars;
//}
//
//void State::_copy_buffer_from_state(const State &state) {
//    // TODO: Profile if memcpy could speed this up significantly,
//    //       e.g. if we do blind A* search.
//    for (int i = 0; i < g_variable_domain.size(); i++)
//        vars[i] = state.vars[i];
//}

State::~State() {
}

void State::dump_pddl() const {
	for (int i = 0; i < g_variable_domain.size(); i++) {
		const string &fact_name = g_fact_names[i][vars[i]];
		if (fact_name != "<none of those>")
			cout << fact_name << endl;
	}
}

void State::dump_fdr() const {
	// We cast the values to int since we'd get bad output otherwise
	// if state_var_t == char.
	for (size_t i = 0; i < g_variable_domain.size(); ++i)
		cout << "  #" << i << " [" << g_variable_name[i] << "] -> "
				<< static_cast<int>(vars[i]) << endl;
}

void State::dump_raw() const {
	// We cast the values to int since we'd get bad output otherwise
	// if state_var_t == char.
	cout << "# ";
	for (size_t i = 0; i < g_variable_domain.size(); ++i) {
		cout << static_cast<int>(vars[i]) << " ";
	}
	cout << endl;
}
