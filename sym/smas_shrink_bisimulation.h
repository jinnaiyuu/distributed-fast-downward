#ifndef SMAS_SHRINK_BISIMULATION_H
#define SMAS_SHRINK_BISIMULATION_H

#include "smas_shrink_strategy.h"

class Options;
class Signature;

class SMASShrinkBisimulation : public SMASShrinkStrategy {
    enum AtLimit {
        RETURN,
        USE_UP
    };

    /*
      threshold: Shrink the abstraction iff it is larger than this
      size. Note that this is set independently from max_states, which
      is the number of states to which the abstraction is shrunk.
    */

    const bool greedy;
    const int threshold;
    const bool group_by_h;
    const AtLimit at_limit;

    //Returns true if the new abstraction supposes a relaxation
    bool compute_abstraction(
        SymSMAS &abs,
        int target_size,
        EquivalenceRelation &equivalence_relation);

    int initialize_groups(const SymSMAS &abs,
                          std::vector<int> &state_to_group);
    void compute_signatures(
        const SymSMAS &abs,
        std::vector<Signature> &signatures,
        std::vector<int> &state_to_group);
public:
    SMASShrinkBisimulation(const Options &opts);
    virtual ~SMASShrinkBisimulation();

    virtual std::string name() const;
    virtual void dump_strategy_specific_options() const;

    virtual bool reduce_labels_before_shrinking() const;

    //Returns true if the abstraction have been relaxed
    virtual bool shrink(SymSMAS &abs, int target, bool force = false);
    virtual void shrink_atomic(SymSMAS &abs);
    //Returns true if abs1 have been relaxed
    virtual bool shrink_before_merge(SymSMAS &abs1, SymSMAS &abs2);
    static SMASShrinkStrategy *create_default();
};

#endif
