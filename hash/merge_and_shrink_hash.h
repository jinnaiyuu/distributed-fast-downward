#ifndef MERGE_AND_SHRINK_MERGE_AND_SHRINK_HASH_H
#define MERGE_AND_SHRINK_MERGE_AND_SHRINK_HASH_H

#include "../merge_and_shrink/shrink_strategy.h"
#include "../merge_and_shrink/merge_and_shrink_heuristic.h"

#include "../heuristic.h"
#include "distribution_hash.h"

class Abstraction;



class MergeAndShrinkHash : public Heuristic, public DistributionHash {

	const MergeStrategy merge_strategy;
    ShrinkStrategy *const shrink_strategy;
    const bool use_label_reduction;
    const bool use_expensive_statistics;

    Abstraction *final_abstraction;
    Abstraction *build_abstraction();

    void dump_options() const;
    void warn_on_unusual_options() const;
protected:
    virtual void initialize();
    virtual int compute_heuristic(const State &state);
public:
    MergeAndShrinkHash(const Options &opts);
    ~MergeAndShrinkHash();


    // methods drived from Distributino Hash
    unsigned int hash(const State& state);
	unsigned int hash(const state_var_t* state);
	unsigned int hash_incremental(const State& state,
			const unsigned int parent_d_hash, const Operator* op);
	std::string hash_name();
};

#endif
