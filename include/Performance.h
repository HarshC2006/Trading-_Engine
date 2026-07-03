#pragma once

#include "Portfolio.h"

struct PerformanceReport {
    double total_return = 0.0;
    double cagr = 0.0;
    double sharpe = 0.0;
    double sortino = 0.0;
    double max_drawdown = 0.0;
    double win_rate = 0.0;
    double profit_factor = 0.0;
    double avg_win = 0.0;
    double avg_loss = 0.0;
    double exposure = 0.0;
    int trades = 0;
};

class PerformanceAnalyzer {
public:
    static PerformanceReport analyze(const Portfolio& portfolio);
    static void print(const Portfolio& portfolio, const PerformanceReport& report, int folds, int oos_days);
};
