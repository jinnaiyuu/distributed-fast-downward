#include "state_registry.h"

#include "axioms.h"
#include "operator.h"
#include "state_var_t.h"
#include "per_state_information.h"

#include <stdio.h>
using namespace std;

StateRegistry::StateRegistry() :
		state_data_pool(g_variable_domain.size()), registered_states(0,
				StateIDSemanticHash(state_data_pool),
				StateIDSemanticEqual(state_data_pool)), cached_initial_state(0), cached_dummy_state(
				0) {
}

StateRegistry::~StateRegistry() {
	for (set<PerStateInformationBase *>::iterator it = subscribers.begin();
			it != subscribers.end(); ++it) {
		(*it)->remove_state_registry(this);
	}
	delete cached_initial_state;
	delete cached_dummy_state;
}

StateID StateRegistry::insert_id_or_pop_state() {
	/*
	 Attempt to insert a StateID for the last state of state_data_pool
	 if none is present yet. If this fails (another entry for this state
	 is present), we have to remove the duplicate entry from the
	 state data pool.
	 */
	StateID id(state_data_pool.size() - 1);
	pair<StateIDSet::iterator, bool> result = registered_states.insert(id);
	bool is_new_entry = result.second;
	if (!is_new_entry) {
		state_data_pool.pop_back();
	}
	assert(registered_states.size() == state_data_pool.size());
	return *result.first;
}

State StateRegistry::lookup_state(StateID id) const {
	return State(state_data_pool[id.value], *this, id);
}

// for HDA* plan reconstruction
State StateRegistry::lookup_state(int id_int) {
	StateID id(id_int);
	pair<StateIDSet::iterator, bool> result = registered_states.insert(id);
	bool is_new_entry = result.second;
	if (!is_new_entry) {
		assert(false);
	}
	assert(registered_states.size() == state_data_pool.size());
	StateID id_we_needed = *result.first;
	return State(state_data_pool[id_we_needed.value], *this, id_we_needed);
}

const State &StateRegistry::get_initial_state() {
	if (cached_initial_state == 0) {
		state_data_pool.push_back(g_initial_state_buffer);
		state_var_t *vars = state_data_pool[state_data_pool.size() - 1];
		g_axiom_evaluator->evaluate(vars);
		StateID id = insert_id_or_pop_state();
		cached_initial_state = new State(lookup_state(id));
	}
	return *cached_initial_state;
}

/**
 * This function is for distributed algorithm which we need to access
 * the state directly. Don't use it for sequential search.
 */
State &StateRegistry::get_successor_state_by_dummy(const State& parent,
		const Operator &op) {
	if (cached_dummy_state == 0) {
		state_var_t *dummy_buffer = new state_var_t[g_variable_domain.size()];
		copy(cached_initial_state->get_buffer(),
				cached_initial_state->get_buffer() + g_variable_domain.size(),
				dummy_buffer);

		StateID id(-2);
	    cached_dummy_state = new State(dummy_buffer, *this, id);
//		printf("built first dummy\n");
	}
	// HACK! HACK! there tricky this is trying to
	// modify CONST member of the state.
	const state_var_t* p_const = cached_dummy_state->get_buffer();
	state_var_t* p_buff; // CAREFUL this pointer can modify const vars.
	p_buff = (state_var_t *) (p_const);

	// copy parent state to dummy.
	copy(parent.get_buffer(), parent.get_buffer() + g_variable_domain.size(),
			p_buff);
//	*p_buff = parent[0];
//	*(p_buff + 1) = parent[1];

//	printf("parent: ");
//	for (int i = 0; i < g_variable_domain.size(); ++i) {
//		printf("%d ", parent[i]);
//	}
//	printf("\n");
//
//	printf("copied buffer: ");
//	for (int i = 0; i < g_variable_domain.size(); ++i) {
//		printf("%d ", p_buff[i]);
//	}
//	printf("\n");

	// operate op to the dummy.
	// TODO: this thing is causing a huge problem.
	for (size_t i = 0; i < op.get_pre_post().size(); ++i) {
		const PrePost &pre_post = op.get_pre_post()[i];
		if (pre_post.does_fire(p_buff))
			p_buff[pre_post.var] = pre_post.post;
	}

//	printf("child in methods: ");
//	for (int i = 0; i < g_variable_domain.size(); ++i) {
//		printf("%d ", p_buff[i]);
//	}
//	printf("\n");
//
//	printf("dummy in methods: ");
//	for (int i = 0; i < g_variable_domain.size(); ++i) {
//		printf("%d ", cached_dummy_state->get_buffer()[i]);
//	}
//	printf("\n");

	return *cached_dummy_state;
}

void StateRegistry::reset_dummy_state() {
	const state_var_t* p_const = cached_dummy_state->get_buffer();
	state_var_t* p_buff; // CAREFUL this pointer can modify const vars.
	p_buff = (state_var_t *) (&p_const);
	copy(cached_initial_state->get_buffer(),
			cached_initial_state->get_buffer() + g_variable_domain.size(),
			p_buff);
}

//TODO it would be nice to move the actual state creation (and operator application)
//     out of the StateRegistry. This could for example be done by global functions
//     operating on state buffers (state_var_t *).
State StateRegistry::get_successor_state(const State &predecessor,
		const Operator &op) {
	assert(!op.is_axiom());
	state_data_pool.push_back(predecessor.get_buffer());
	state_var_t *vars = state_data_pool[state_data_pool.size() - 1];
	for (size_t i = 0; i < op.get_pre_post().size(); ++i) {
		const PrePost &pre_post = op.get_pre_post()[i];
		if (pre_post.does_fire(predecessor))
			vars[pre_post.var] = pre_post.post;
	}
	g_axiom_evaluator->evaluate(vars);
	StateID id = insert_id_or_pop_state();
	return lookup_state(id);
}

State StateRegistry::build_state(const state_var_t* state) {
	state_data_pool.push_back(state);
	state_var_t *vars = state_data_pool[state_data_pool.size() - 1];

	g_axiom_evaluator->evaluate(vars);
	StateID id = insert_id_or_pop_state();
	return lookup_state(id);
}

void StateRegistry::subscribe(PerStateInformationBase *psi) const {
	subscribers.insert(psi);
}

void StateRegistry::unsubscribe(PerStateInformationBase * const psi) const {
	subscribers.erase(psi);
}
