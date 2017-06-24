#ifndef SMAS_SHRINK_SYM_H
#define SMAS_SHRINK_SYM_H

#include "smas_shrink_strategy.h"

class Options;
class Signature;

class SMASShrinkSymbolic: public SMASShrinkStrategy {

	const bool group_by_h;

	void compute_abstraction(SymSMAS &abs, int target_size,
			EquivalenceRelation &equivalence_relation);

	int initialize_groups(const SymSMAS &abs, std::vector<int> &state_to_group);
	void compute_signatures(const SymSMAS &abs,
			std::vector<Signature> &signatures,
			std::vector<int> &state_to_group);
public:
	SMASShrinkSymbolic(const Options &opts);
	virtual ~SMASShrinkSymbolic();

	virtual std::string name() const;
	virtual void dump_strategy_specific_options() const;

	virtual bool reduce_labels_before_shrinking() const;

	virtual void shrink(SymSMAS &abs, int target, bool force = false);
	virtual void shrink_before_merge(SymSMAS &abs1, SymSMAS &abs2);

	static SMASShrinkStrategy *create_default();
};

#endif
