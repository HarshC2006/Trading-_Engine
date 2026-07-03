#include "../include/ModelIO.h"
#include "../include/Performance.h"
#include "../include/Strategy.h"

#include <cmath>
#include <cstdio>
#include <iostream>

namespace {

int failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

void test_strategy() {
    ModelPack pack;
    pack.buy_level = 0.60;
    pack.sell_level = 0.40;
    pack.min_move = 0.10;
    pack.logistic.scaler.scale.fill(1.0);
    pack.ridge.scaler.scale.fill(1.0);
    pack.logistic.w[0] = 2.0;
    pack.ridge.w[0] = 1.0;

    Strategy strategy(pack);

    Sample row;
    row.date = "2024-01-01";
    row.symbol = "AAPL";
    row.next_open = 100.0;
    row.atr = 0.02;
    row.vol = 0.01;

    row.x.fill(0.0);
    row.x[0] = 1.0;
    check(strategy.on_sample(row).side == Side::Buy, "strategy should emit buy");

    row.x[0] = -1.0;
    check(strategy.on_sample(row).side == Side::Sell, "strategy should emit sell");

    row.x[0] = 0.0;
    check(strategy.on_sample(row).side == Side::Hold, "strategy should hold when edge is weak");
}

void test_portfolio() {
    Portfolio portfolio(100000.0);

    portfolio.apply_fill({"2024-01-02", "AAPL", Side::Buy, 10, 100.0, 0.0, 0.0, 95.0, 110.0, ExitReason::Signal});
    portfolio.apply_fill({"2024-01-02", "MSFT", Side::Buy, 5, 200.0, 0.0, 0.0, 190.0, 220.0, ExitReason::Signal});

    check(portfolio.has_position("AAPL"), "portfolio should hold AAPL");
    check(portfolio.has_position("MSFT"), "portfolio should hold MSFT");

    portfolio.mark("2024-01-02", {
        {"2024-01-02", "AAPL", 100.0, 101.0, 99.0, 101.0, 1000},
        {"2024-01-02", "MSFT", 200.0, 205.0, 199.0, 205.0, 1000},
    });

    portfolio.apply_fill({"2024-01-03", "AAPL", Side::Sell, 10, 105.0, 0.0, 0.0, 0.0, 0.0, ExitReason::Signal});

    check(!portfolio.has_position("AAPL"), "AAPL should be closed");
    check(portfolio.has_position("MSFT"), "MSFT should stay open");
    check(portfolio.trades().size() == 1, "portfolio should record one trade");
}

void test_performance() {
    Portfolio portfolio(1000.0);
    portfolio.mark("2024-01-01", {});
    portfolio.apply_fill({"2024-01-02", "AAPL", Side::Buy, 5, 100.0, 0.0, 0.0, 95.0, 110.0, ExitReason::Signal});
    portfolio.mark("2024-01-02", {
        {"2024-01-02", "AAPL", 100.0, 110.0, 100.0, 110.0, 1000},
    });
    portfolio.apply_fill({"2024-01-03", "AAPL", Side::Sell, 5, 110.0, 0.0, 0.0, 0.0, 0.0, ExitReason::Signal});
    portfolio.mark("2024-01-03", {
        {"2024-01-03", "AAPL", 110.0, 110.0, 110.0, 110.0, 1000},
    });

    const auto report = PerformanceAnalyzer::analyze(portfolio);
    check(report.trades == 1, "report should include one trade");
    check(std::fabs(report.win_rate - 1.0) < 1e-9, "win rate should be 100%");
    check(report.total_return > 0.0, "total return should be positive");
    check(report.profit_factor == 0.0, "profit factor is zero when there are no losses");
}

void test_model_io() {
    ModelPack pack;
    pack.fft_window = 32;
    pack.buy_level = 0.61;
    pack.sell_level = 0.39;
    pack.min_move = 0.0025;
    pack.logistic.b = 0.12;
    pack.logistic.scaler.mean[0] = 1.0;
    pack.logistic.scaler.scale[0] = 2.0;
    pack.logistic.w[0] = 3.0;
    pack.ridge.b = -0.07;
    pack.ridge.scaler.mean[1] = 4.0;
    pack.ridge.scaler.scale[1] = 5.0;
    pack.ridge.w[1] = 6.0;

    const char* path = "model_test.json";
    save_model(pack, path);
    const ModelPack loaded = load_model(path);
    std::remove(path);

    check(loaded.fft_window == 32, "fft window should round-trip");
    check(std::fabs(loaded.buy_level - 0.61) < 1e-9, "buy level should round-trip");
    check(std::fabs(loaded.logistic.b - 0.12) < 1e-9, "logistic bias should round-trip");
    check(std::fabs(loaded.logistic.w[0] - 3.0) < 1e-9, "logistic weight should round-trip");
    check(std::fabs(loaded.ridge.w[1] - 6.0) < 1e-9, "ridge weight should round-trip");
}

}

int main() {
    test_strategy();
    test_portfolio();
    test_performance();
    test_model_io();

    if (failures > 0) {
        return 1;
    }

    std::cout << "all tests passed\n";
    return 0;
}
