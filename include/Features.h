#pragma once

#include "Event.h"

#include <array>
#include <cstddef>
#include <vector>

constexpr std::size_t kFeatureCount = 16;

struct Sample {
    std::string date;
    std::string symbol;
    std::string next_date;
    std::array<double, kFeatureCount> x{};
    double close = 0.0;
    double next_open = 0.0;
    double next_ret = 0.0;
    double atr = 0.0;
    double vol = 0.0;
};

class FeatureEngine {
public:
    static std::vector<Sample> build(const std::vector<Bar>& bars, std::size_t fft_window = 16, bool include_latest = false);
};
