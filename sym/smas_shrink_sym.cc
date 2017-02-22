#include "smas_shrink_sym.h"

#include "sym_smas.h"

#include "../option_parser.h"
#include "../plugin.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <ext/hash_map>

using namespace std;
using namespace __gnu_cxx;

static const int infinity = numeric_limits<int>::max();

/* TODO: The following class should probably be renamed. It encodes
 all we need to know about a state for shrink: its h value,
 which equivalence class ("group") it currently belongs to, its
 signature (see above), and what the original state is. */

struct Signature {
	int group;
	BDD bdd; //bdd describing the states (wrt. non relevant variables) reachable
	int state;

	Signature(int group_, int state_) :
			group(group_), succ_signature(succ_signature_), state(state_) {
		//bdd is NULL by default
	}

	bool operator<(const Signature &other) const {
		if (group != other.group)
			return group < other.group;
		/* Comparing the BDD pointers may cause arbitrary orders
		 different in each execution. Thus, either we split the whole group or
		 we do not split the group: the order should not matter!*/
		if (bdd != other.bdd)
			return bdd.getNode() < other.bdd.getNode();
		return state < other.state;
	}

	void dump() const {
		cout << "Signature(group = " << group << ", state = " << state
				<< ", succ_sig = [";
		if (bdd.getNode() == 0) {
			cout << "NULL";
		} else {
			bdd.print(1, 1);
		}
		cout << "])" << endl;
	}
};

SMASShrinkSymbolic::SMASShrinkSymbolic(const Options &opts) :
		SMASShrinkStrategy(opts), group_by_h(opts.get<bool>("group_by_h")) {
}

SMASShrinkSymbolic::~SMASShrinkSymbolic() {
}

string SMASShrinkSymbolic::name() const {
	return "symbolic";
}

void SMASShrinkSymbolic::dump_strategy_specific_options() const {
	cout << "Group by h: " << (group_by_h ? "yes" : "no") << endl;
}

bool SMASShrinkSymbolic::reduce_labels_before_shrinking() const {
	return true;
}

void SMASShrinkSymbolic::shrink(SymSMAS &abs, int target, bool force) {

	if (must_shrink(abs, std::min(target, threshold), force)) {
		EquivalenceRelation equivalence_relation;
		compute_abstraction(abs, target, equivalence_relation);
		apply(abs, equivalence_relation, target);
	}
}

void SMASShrinkSymbolic::compute_abstraction(SymSMAS &abs, int target_size,
		EquivalenceRelation &equivalence_relation) {
	int num_states = abs.size();
	SymExploration * absExploration = abs.get_exploration();
	SymManager * absManager = absExploration.get_manager();

	vector<int> var_subset = select_vars(abs);
	SymExploration exploration(*absExploration, var_subset);

	//Initialize state to group by assigning all the states to 0
	int num_groups = 1;
	vector<int> state_to_group(num_states, 0);

	/*     cout << "number of initial groups: " << num_groups << endl
	 << "target_size: " << target_size << endl;*/

	vector<Signature> signatures;
	signatures.reserve(num_states);
	for (int state = 0; state < num_states; ++state) {
		signatures.push_back(Signature(state_to_group[state], state));
	}
	assert(num_groups <= target_size);

	//Break when num_groups exceeds the threshold
	while (true) {
		if (exploration.finished()) {
			var_subset = select_vars();
			exploration = SymExploration(*absExploration, var_subset);
		}
		BDD S = exploration.step();

		//Assign BDD to signature
		for (int i = 0; i < signatures.size(); i++) {
			int state = signatures[i].state;
			signatures[i].bdd = S * abs.absStatePreBDD(state);
		}

		::sort(signatures.begin(), signatures.end());

		// Verify size of signatures and presence of sentinels.
		assert(signatures.size() == num_states + 2);
		assert(signatures[0].h_and_goal == -2);
		assert(signatures[num_states + 1].h_and_goal == infinity);

		//Recompute signature groups
		int new_group = 0; // Compute the number of groups needed after splitting.
		int sig = 0;
		while (sig < signatures.size()) {
			int old_group = signatures[sig].group;
			BDD bdd = signatures[sig].bdd;
			while (signatures[sig].group = old_group
					&& signatures[sig].bdd == bdd) {
				signatures[sig].group = new_group;
				sig++;
			}
			new_group++;
		}

		if (new_group > target_size) {
			break;
		} else {
			//Copy signatures group into state_to_group
			for (int i = 0; i < signatures.size(); i++) {
				state_to_group[signatures[i].state] = signatures[i].group;
			}
		}
	}

	//    cout << "Num groups: " << num_groups << endl;

	/* Reduce memory pressure before generating the equivalence
	 relation since this is one of the code parts relevant to peak
	 memory. */
	vector<Signature>().swap(signatures);

	// Generate final result.
	assert(equivalence_relation.empty());
	equivalence_relation.resize(num_groups);
	for (int state = 0; state < num_states; ++state) {
		int group = state_to_group[state];
		if (group != -1) {
			assert(group >= 0 && group < num_groups);
			equivalence_relation[group].push_front(state);
		}
	}
}

static SMASShrinkStrategy *_parse(OptionParser &parser) {
	SMASShrinkStrategy::add_options_to_parser(parser);
	parser.add_option<bool>("group_by_h", false);

	Options opts = parser.parse();
	SMASShrinkStrategy::handle_option_defaults(opts);

	if (!parser.dry_run())
		return new SMASShrinkSymbolic(opts);
	else
		return 0;
}

static Plugin<SMASShrinkStrategy> _plugin("shrink_symbolic", _parse);
