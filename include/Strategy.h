#pragma once

#include "Models.h"

struct ScoredSignal {
    Signal signal;
    double prob = 0.0;
    double ret = 0.0;
};

class Strategy {
public:
    explicit Strategy(ModelPack models);

    ScoredSignal score(const Sample& row) const;
    Signal on_sample(const Sample& row) const;

private:
    ModelPack models_;
};
