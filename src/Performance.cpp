#include "../include/Performance.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

double mean(const std::vector<double>& v) {
    if (v.empty()) {
        return 0.0;
    }
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

double stdev(const std::vector<double>& v, double m) {
    if (v.size() < 2) {
        return 0.0;
    }
    double acc = 0.0;
    for (double x : v) {
        const double d = x - m;
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(v.size() - 1));
}

}

PerformanceReport PerformanceAnalyzer::analyze(const Portfolio& portfolio) {
    PerformanceReport report;
    const auto& curve = portfolio.curve();
    if (curve.size() < 2) {
        return report;
    }

    report.total_return = curve.back().equity / portfolio.get_initial_capital() - 1.0;

    const double years = static_cast<double>(curve.size() - 1) / 252.0;
    if (years > 0.0 && curve.back().equity > 0.0) {
        report.cagr = std::pow(curve.back().equity / portfolio.get_initial_capital(), 1.0 / years) - 1.0;
    }

    std::vector<double> rets;
    std::vector<double> downside;
    rets.reserve(curve.size() - 1);

    double exp_total = 0.0;
    for (std::size_t i = 1; i < curve.size(); ++i) {
        const double r = curve[i].equity / curve[i - 1].equity - 1.0;
        rets.push_back(r);
        if (r < 0.0) {
            downside.push_back(r);
        }
        exp_total += curve[i].exposure;
        report.max_drawdown = std::max(report.max_drawdown, curve[i].drawdown);
    }

    const double m = mean(rets);
    const double sd = stdev(rets, m);
    const double downside_sd = std::sqrt([&]() {
        if (downside.empty()) {
            return 0.0;
        }
        double acc = 0.0;
        for (double x : downside) {
            acc += x * x;
        }
        return acc / static_cast<double>(downside.size());
    }());

    if (sd > 0.0) {
        report.sharpe = (m * 252.0) / (sd * std::sqrt(252.0));
    }
    if (downside_sd > 0.0) {
        report.sortino = (m * 252.0) / (downside_sd * std::sqrt(252.0));
    }

    const auto& trades = portfolio.trades();
    report.trades = static_cast<int>(trades.size());
    report.exposure = exp_total / static_cast<double>(std::max<std::size_t>(1, curve.size() - 1));

    double gross_win = 0.0;
    double gross_loss = 0.0;
    double win_sum = 0.0;
    double loss_sum = 0.0;
    int wins = 0;
    int losses = 0;

    for (const auto& trade : trades) {
        if (trade.pnl > 0.0) {
            gross_win += trade.pnl;
            win_sum += trade.pnl;
            ++wins;
        } else if (trade.pnl < 0.0) {
            gross_loss += -trade.pnl;
            loss_sum += trade.pnl;
            ++losses;
        }
    }

    if (wins + losses > 0) {
        report.win_rate = static_cast<double>(wins) / static_cast<double>(wins + losses);
    }
    if (gross_loss > 0.0) {
        report.profit_factor = gross_win / gross_loss;
    }
    if (wins > 0) {
        report.avg_win = win_sum / static_cast<double>(wins);
    }
    if (losses > 0) {
        report.avg_loss = loss_sum / static_cast<double>(losses);
    }

    return report;
}

void PerformanceAnalyzer::print(const Portfolio& portfolio, const PerformanceReport& report, int folds, int oos_days) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nWalk-forward report\n";
    std::cout << "folds: " << folds << "  oos_days: " << oos_days << "\n";
    std::cout << "start: $" << portfolio.get_initial_capital() << "  end: $" << portfolio.equity() << "\n";
    std::cout << "total_return: " << report.total_return * 100.0 << "%\n";
    std::cout << "cagr: " << report.cagr * 100.0 << "%\n";
    std::cout << "sharpe: " << report.sharpe << "\n";
    std::cout << "sortino: " << report.sortino << "\n";
    std::cout << "max_drawdown: " << report.max_drawdown * 100.0 << "%\n";
    std::cout << "win_rate: " << report.win_rate * 100.0 << "%\n";
    std::cout << "profit_factor: " << report.profit_factor << "\n";
    std::cout << "avg_win: $" << report.avg_win << "\n";
    std::cout << "avg_loss: $" << report.avg_loss << "\n";
    std::cout << "exposure: " << report.exposure * 100.0 << "%\n";
    std::cout << "trades: " << report.trades << "\n\n";
}
