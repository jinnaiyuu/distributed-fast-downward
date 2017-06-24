#ifndef SMAS_SHRINK_BUCKET_BASED_H
#define SMAS_SHRINK_BUCKET_BASED_H

#include "smas_shrink_strategy.h"

#include <vector>


class Options;

class SMASShrinkAll : public SMASShrinkStrategy {

public:
    SMASShrinkAll(const Options &opts);
    virtual ~SMASShrinkAll();

    bool shrink_before_merge(SymSMAS &abs1, SymSMAS &abs2);

    virtual bool reduce_labels_before_shrinking() const;

    virtual bool shrink(SymSMAS &abs,
			int threshold,
                        bool force = false);

    virtual std::string name() const;

};

#endif
