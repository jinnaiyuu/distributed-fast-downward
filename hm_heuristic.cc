#include "hm_heuristic.h"

#include "option_parser.h"
#include "plugin.h"

#include <cassert>
#include <limits>
#include <set>

using namespace std;

HMHeuristic::HMHeuristic(const Options &opts)
    : Heuristic(opts),
      m(opts.get<int>("m")) {
}

HMHeuristic::~HMHeuristic() {
}

void HMHeuristic::initialize() {
    cout << "Using h^" << m << endl;
    cout << "The implementation of the h^m heuristic is preliminary" << endl
         << "It is SLOOOOOOOOOOOW" << endl
         << "Please do not use this for comparison" << endl;
    generate_all_tuples();
}

int HMHeuristic::compute_heuristic(const State &state) {
    if (test_goal(state)) {
        return 0;
    } else {
        ttuple s_tup;
        state_to_tuple(state, s_tup);

        init_hm_table(s_tup);
        update_hm_table();
        //dump_table();

        int h = eval(g_goal);

        if (h == numeric_limits<int>::max())
            return DEAD_END;
        return h;
    }
}


void HMHeuristic::init_hm_table(ttuple &t) {
    map<ttuple, int>::iterator it;
    for (it = hm_table.begin(); it != hm_table.end(); it++) {
        pair<ttuple, int> hm_ent = *it;
        ttuple &tup = hm_ent.first;
        int h_val = check_tuple_in_tuple(tup, t);
        hm_table[tup] = h_val;
    }
}


void HMHeuristic::update_hm_table() {
    int round = 0;
    do {
        round++;
        was_updated = false;

        for (int op_id = 0; op_id < g_operators.size(); op_id++) {
            const Operator &op = g_operators[op_id];
            ttuple pre;
            get_operator_pre(op, pre);

            int c1 = eval(pre);
            if (c1 != numeric_limits<int>::max()) {
                ttuple eff;
                vector<ttuple> partial_eff;
                get_operator_eff(op, eff);
                generate_all_partial_tuple(eff, partial_eff);
                for (int i = 0; i < partial_eff.size(); i++) {
                    update_hm_entry(partial_eff[i], c1 + get_adjusted_cost(op));

                    if (partial_eff[i].size() < m) {
                        extend_tuple(partial_eff[i], op);
                    }
                }
            }
        }
    } while (was_updated);
}

void HMHeuristic::extend_tuple(ttuple &t, const Operator &op) {
    map<ttuple, int>::const_iterator it;
    for (it = hm_table.begin(); it != hm_table.end(); it++) {
        pair<ttuple, int> hm_ent = *it;
        ttuple &entry = hm_ent.first;
        bool contradict = false;
        for (int i = 0; i < entry.size(); i++) {
            if (contradict_effect_of(op, entry[i].first, entry[i].second)) {
                contradict = true;
                break;
            }
        }
        if (!contradict && (entry.size() > t.size()) && (check_tuple_in_tuple(t, entry) == 0)) {
            ttuple pre;
            get_operator_pre(op, pre);

            ttuple others;
            for (int i = 0; i < entry.size(); i++) {
                pair<int, int> fact = entry[i];
                if (find(t.begin(), t.end(), fact) == t.end()) {
                    others.push_back(fact);
                    if (find(pre.begin(), pre.end(), fact) == pre.end()) {
                        pre.push_back(fact);
                    }
                }
            }

            sort(pre.begin(), pre.end());


            set<int> vars;
            bool is_valid = true;
            for (int i = 0; i < pre.size(); i++) {
                if (vars.count(pre[i].first) != 0) {
                    is_valid = false;
                    break;
                }
                vars.insert(pre[i].first);
            }

            if (is_valid) {
                int c2 = eval(pre);
                if (c2 != numeric_limits<int>::max()) {
                    update_hm_entry(entry, c2 + get_adjusted_cost(op));
                }
            }
        }
    }
}

int HMHeuristic::eval(ttuple &t) {
    vector<ttuple> partial;
    generate_all_partial_tuple(t, partial);
    int max = 0;
    for (int i = 0; i < partial.size(); i++) {
        assert(hm_table.count(partial[i]) == 1);

        int h = hm_table[partial[i]];
        if (h > max) {
            max = h;
        }
    }
    return max;
}

int HMHeuristic::update_hm_entry(ttuple &t, int val) {
    assert(hm_table.count(t) == 1);
    if (hm_table[t] > val) {
        hm_table[t] = val;
        was_updated = true;
    }
    return val;
}

void HMHeuristic::generate_all_tuples(int var, int sz, ttuple &base) {
    if (sz == 1) {
        for (int i = var; i < g_variable_domain.size(); i++) {
            for (int j = 0; j < g_variable_domain[i]; j++) {
                ttuple tup(base);
                tup.push_back(make_pair(i, j));
                hm_table[tup] = 0;
            }
        }
    } else {
        for (int i = var; i < g_variable_domain.size(); i++) {
            for (int j = 0; j < g_variable_domain[i]; j++) {
                ttuple tup(base);
                tup.push_back(make_pair(i, j));
                hm_table[tup] = 0;
                generate_all_tuples(i + 1, sz - 1, tup);
            }
        }
    }
}

void HMHeuristic::generate_all_partial_tuple(ttuple &base_tuple, ttuple &t,
                                             int index, int sz, vector<ttuple> &res) {
    if (sz == 1) {
        for (int i = index; i < base_tuple.size(); i++) {
            ttuple tup(t);
            tup.push_back(base_tuple[i]);
            res.push_back(tup);
        }
    } else {
        for (int i = index; i < base_tuple.size(); i++) {
            ttuple tup(t);
            tup.push_back(base_tuple[i]);
            res.push_back(tup);
            generate_all_partial_tuple(base_tuple, tup, i + 1, sz - 1, res);
        }
    }
}


int HMHeuristic::check_tuple_in_tuple(const ttuple &tup, const ttuple &big_tuple) {
    for (int i = 0; i < tup.size(); i++) {
        bool found = false;
        for (int j = 0; j < big_tuple.size(); j++) {
            if (tup[i] == big_tuple[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return numeric_limits<int>::max();
        }
    }
    return 0;
}

static Heuristic *_parse(OptionParser &parser) {
    parser.document_synopsis("h^m heuristic", "");
    parser.document_language_support("action costs", "supported");
    parser.document_language_support("conditional_effects", "ignored");
    parser.document_language_support("axioms", "ignored");
    parser.document_property("admissible",
                             "yes for tasks without conditional "
                             "effects or axioms");
    parser.document_property("consistent",
                             "yes for tasks without conditional "
                             "effects or axioms");

    parser.document_property("safe",
                             "yes for tasks without conditional "
                             "effects or axioms");
    parser.document_property("preferred operators", "no");

    parser.add_option<int>("m", "subset size", "2");
    Heuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new HMHeuristic(opts);
}


static Plugin<Heuristic> _plugin("hm", _parse);
