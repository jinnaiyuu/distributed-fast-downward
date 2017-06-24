#ifndef SMAS_SHRINK_RANDOM_H
#define SMAS_SHRINK_RANDOM_H

#include "smas_shrink_bucket_based.h"

class Options;

class SMASShrinkRandom : public SMASShrinkBucketBased {
protected:
    virtual void partition_into_buckets(
        const SymSMAS &abs, std::vector<Bucket> &buckets) const;

    virtual std::string name() const;
public:
    SMASShrinkRandom(const Options &opts);
    virtual ~SMASShrinkRandom();
};

#endif
