#pragma once

#include "Performance.h"
#include "Strategy.h"

#include <cstddef>
#include <vector>

struct BacktestConfig {
    std::size_t train_days = 252;
    std::size_t test_days = 63;
    std::size_t fft_window = 16;
    double capital = 100000.0;
    RiskConfig risk{};
};

struct BacktestResult {
    Portfolio portfolio;
    PerformanceReport report;
    int folds = 0;
    int oos_days = 0;
};

class WalkForwardBacktester {
public:
    explicit WalkForwardBacktester(BacktestConfig cfg = {});

    BacktestResult run(const std::vector<Bar>& bars) const;

private:
    BacktestConfig cfg_;
};

BacktestResult run_model_backtest(const std::vector<Bar>& bars, const ModelPack& models, const BacktestConfig& cfg);
