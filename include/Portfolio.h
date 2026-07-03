#pragma once

#include "Event.h"
#include "Execution.h"

#include <string>
#include <unordered_map>
#include <vector>

struct Position {
    int qty = 0;
    double avg_price = 0.0;
    double stop = 0.0;
    double target = 0.0;
    double atr = 0.0;
    std::string entry_date;
    double entry_commission = 0.0;
};

struct EquityPoint {
    std::string date;
    double equity = 0.0;
    double cash = 0.0;
    double gross = 0.0;
    double exposure = 0.0;
    double drawdown = 0.0;
};

struct RiskConfig {
    double risk_per_trade = 0.01;
    double stop_atr = 2.0;
    double target_atr = 3.0;
    double max_gross = 1.0;
    double max_drawdown = 0.20;
    bool vol_scale = true;
    double target_vol = 0.02;
    double min_scale = 0.50;
    double max_scale = 1.50;
};

class Portfolio {
public:
    explicit Portfolio(double capital = 100000.0, RiskConfig risk = {});

    Order make_order(const Signal& signal) const;
    void apply_fill(const Fill& fill);
    std::vector<Fill> check_exits(const Bar& bar, const ExecutionHandler& exec);
    std::vector<Fill> close_all(const std::vector<Bar>& bars, const ExecutionHandler& exec, ExitReason reason);
    void mark(const std::string& date, const std::vector<Bar>& bars);

    bool has_position(const std::string& symbol) const;
    bool halted() const;
    void halt();
    double equity() const;
    double current_drawdown() const;
    double get_initial_capital() const;
    double get_cash() const;
    const std::unordered_map<std::string, Position>& positions() const;
    const std::vector<EquityPoint>& curve() const;
    const std::vector<Trade>& trades() const;

private:
    double position_scale(double vol) const;
    double gross_value() const;

    double initial_capital_;
    double cash_;
    double high_water_;
    RiskConfig risk_;
    bool halted_ = false;
    std::unordered_map<std::string, Position> positions_;
    std::unordered_map<std::string, double> last_close_;
    std::vector<Trade> trades_;
    std::vector<EquityPoint> curve_;
};
