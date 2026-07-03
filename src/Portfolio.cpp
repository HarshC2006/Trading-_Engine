#include "../include/Portfolio.h"

#include <algorithm>
#include <cmath>

namespace {

double clamp(double x, double lo, double hi) {
    return std::max(lo, std::min(hi, x));
}

}

Portfolio::Portfolio(double capital, RiskConfig risk)
    : initial_capital_(capital), cash_(capital), high_water_(capital), risk_(risk) {}

double Portfolio::position_scale(double vol) const {
    if (!risk_.vol_scale || vol <= 0.0) {
        return 1.0;
    }
    return clamp(risk_.target_vol / vol, risk_.min_scale, risk_.max_scale);
}

double Portfolio::gross_value() const {
    double gross = 0.0;
    for (const auto& item : positions_) {
        const auto it = last_close_.find(item.first);
        if (it != last_close_.end()) {
            gross += static_cast<double>(item.second.qty) * it->second;
        }
    }
    return gross;
}

Order Portfolio::make_order(const Signal& signal) const {
    if (halted_) {
        return {};
    }

    const auto it = positions_.find(signal.symbol);
    if (signal.side == Side::Buy) {
        if (it != positions_.end() && it->second.qty > 0) {
            return {};
        }

        const double stop_dist = std::max(signal.atr * risk_.stop_atr * signal.price, signal.price * 0.005);
        if (stop_dist <= 0.0 || signal.price <= 0.0) {
            return {};
        }

        const double risk_cash = equity() * risk_.risk_per_trade * position_scale(signal.vol);
        int qty = static_cast<int>(std::floor(risk_cash / stop_dist));
        const int cash_cap = static_cast<int>(std::floor(cash_ / signal.price));
        const int gross_cap = static_cast<int>(std::floor((equity() * risk_.max_gross) / signal.price));
        qty = std::min(qty, std::min(cash_cap, gross_cap));

        if (qty <= 0) {
            return {};
        }

        return {
            signal.date,
            signal.symbol,
            Side::Buy,
            qty,
            signal.price,
            signal.price - signal.atr * risk_.stop_atr * signal.price,
            signal.price + signal.atr * risk_.target_atr * signal.price,
            ExitReason::Signal
        };
    }

    if (signal.side == Side::Sell && it != positions_.end() && it->second.qty > 0) {
        return {
            signal.date,
            signal.symbol,
            Side::Sell,
            it->second.qty,
            signal.price,
            0.0,
            0.0,
            ExitReason::Signal
        };
    }

    return {};
}

void Portfolio::apply_fill(const Fill& fill) {
    if (fill.qty <= 0) {
        return;
    }

    auto it = positions_.find(fill.symbol);

    if (fill.side == Side::Buy) {
        cash_ -= (fill.price * static_cast<double>(fill.qty)) + fill.commission;

        Position pos;
        pos.qty = fill.qty;
        pos.avg_price = fill.price;
        pos.stop = fill.stop;
        pos.target = fill.target;
        pos.atr = 0.0;
        pos.entry_date = fill.date;
        pos.entry_commission = fill.commission;
        positions_[fill.symbol] = pos;
        last_close_[fill.symbol] = fill.price;
        return;
    }

    if (fill.side == Side::Sell && it != positions_.end()) {
        cash_ += (fill.price * static_cast<double>(fill.qty)) - fill.commission;
        const Position pos = it->second;
        const double gross = (fill.price - pos.avg_price) * static_cast<double>(fill.qty);
        const double pnl = gross - fill.commission - pos.entry_commission;
        trades_.push_back({
            fill.symbol,
            pos.entry_date,
            fill.date,
            fill.qty,
            pos.avg_price,
            fill.price,
            pnl,
            gross,
            fill.reason
        });
        positions_.erase(it);
    }
}

std::vector<Fill> Portfolio::check_exits(const Bar& bar, const ExecutionHandler& exec) {
    std::vector<Fill> out;
    const auto it = positions_.find(bar.symbol);
    if (it == positions_.end() || it->second.qty <= 0) {
        return out;
    }

    const Position& pos = it->second;
    if (bar.low <= pos.stop) {
        const double raw = bar.open < pos.stop ? bar.open : pos.stop;
        out.push_back(exec.fill({bar.date, bar.symbol, Side::Sell, pos.qty, raw, 0.0, 0.0, ExitReason::Stop}, raw, bar.volume));
        return out;
    }

    if (bar.high >= pos.target) {
        const double raw = bar.open > pos.target ? bar.open : pos.target;
        out.push_back(exec.fill({bar.date, bar.symbol, Side::Sell, pos.qty, raw, 0.0, 0.0, ExitReason::Target}, raw, bar.volume));
    }

    return out;
}

std::vector<Fill> Portfolio::close_all(const std::vector<Bar>& bars, const ExecutionHandler& exec, ExitReason reason) {
    std::vector<Fill> out;
    for (const auto& bar : bars) {
        const auto it = positions_.find(bar.symbol);
        if (it == positions_.end() || it->second.qty <= 0) {
            continue;
        }
        out.push_back(exec.fill({bar.date, bar.symbol, Side::Sell, it->second.qty, bar.close, 0.0, 0.0, reason}, bar.close, bar.volume));
    }
    return out;
}

void Portfolio::mark(const std::string& date, const std::vector<Bar>& bars) {
    for (const auto& bar : bars) {
        last_close_[bar.symbol] = bar.close;
    }

    const double gross = gross_value();
    const double eq = cash_ + gross;
    high_water_ = std::max(high_water_, eq);
    const double dd = high_water_ > 0.0 ? (high_water_ - eq) / high_water_ : 0.0;
    const double exp = eq > 0.0 ? gross / eq : 0.0;
    curve_.push_back({date, eq, cash_, gross, exp, dd});
}

bool Portfolio::has_position(const std::string& symbol) const {
    const auto it = positions_.find(symbol);
    return it != positions_.end() && it->second.qty > 0;
}

bool Portfolio::halted() const {
    return halted_;
}

void Portfolio::halt() {
    halted_ = true;
}

double Portfolio::equity() const {
    return cash_ + gross_value();
}

double Portfolio::current_drawdown() const {
    return curve_.empty() ? 0.0 : curve_.back().drawdown;
}

double Portfolio::get_initial_capital() const {
    return initial_capital_;
}

double Portfolio::get_cash() const {
    return cash_;
}

const std::unordered_map<std::string, Position>& Portfolio::positions() const {
    return positions_;
}

const std::vector<EquityPoint>& Portfolio::curve() const {
    return curve_;
}

const std::vector<Trade>& Portfolio::trades() const {
    return trades_;
}
