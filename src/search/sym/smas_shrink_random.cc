#include "smas_shrink_random.h"

#include "sym_smas.h"

#include "../option_parser.h"
#include "../plugin.h"

#include <cassert>

using namespace std;


SMASShrinkRandom::SMASShrinkRandom(const Options &opts)
    : SMASShrinkBucketBased(opts) {
}

SMASShrinkRandom::~SMASShrinkRandom() {
}

string SMASShrinkRandom::name() const {
    return "random";
}

void SMASShrinkRandom::partition_into_buckets(
    const SymSMAS &abs, vector<Bucket> &buckets) const {
    assert(buckets.empty());
    buckets.resize(1);
    Bucket &big_bucket = buckets.back();
    big_bucket.reserve(abs.size());
    for (AbstractStateRef state = 0; state < abs.size(); ++state)
        big_bucket.push_back(state);
    assert(!big_bucket.empty());
}

static SMASShrinkStrategy *_parse(OptionParser &parser) {
    SMASShrinkStrategy::add_options_to_parser(parser);
    Options opts = parser.parse();
    SMASShrinkStrategy::handle_option_defaults(opts);

    if (!parser.dry_run())
        return new SMASShrinkRandom(opts);
    else
        return 0;
}

static Plugin<SMASShrinkStrategy> _plugin("shrink_random", _parse);
