#include "../include/Backtest.h"

#include <algorithm>
#include <map>
#include <unordered_map>

namespace {

using DayMap = std::map<std::string, std::vector<Bar>>;
using DateIndex = std::unordered_map<std::string, std::size_t>;
using SampleMap = std::unordered_map<std::string, Sample>;
using OrderMap = std::unordered_map<std::string, Order>;

std::string key(const std::string& date, const std::string& symbol) {
    return date + '\x1f' + symbol;
}

void build_book(const std::vector<Bar>& bars, DayMap& bars_by_date, std::vector<std::string>& dates, DateIndex& date_idx) {
    for (const auto& bar : bars) {
        bars_by_date[bar.date].push_back(bar);
    }

    dates.reserve(bars_by_date.size());
    for (const auto& item : bars_by_date) {
        date_idx[item.first] = dates.size();
        dates.push_back(item.first);
    }
}

void build_samples(const std::vector<Bar>& bars, std::size_t fft_window, SampleMap& sample_map) {
    const auto rows = FeatureEngine::build(bars, fft_window);
    sample_map.reserve(rows.size());
    for (const auto& row : rows) {
        sample_map[key(row.date, row.symbol)] = row;
    }
}

void fill_pending_on_open(const std::string& date, const std::vector<Bar>& day_bars, Portfolio& portfolio, ExecutionHandler& exec, OrderMap& pending) {
    for (const auto& bar : day_bars) {
        const auto it = pending.find(bar.symbol);
        if (it == pending.end()) {
            continue;
        }
        if (!portfolio.halted() && it->second.qty > 0) {
            Order order = it->second;
            order.date = date;
            portfolio.apply_fill(exec.fill(order, bar.open, bar.volume));
        }
        pending.erase(it);
    }
}

void manage_exits(const std::vector<Bar>& day_bars, Portfolio& portfolio, ExecutionHandler& exec) {
    for (const auto& bar : day_bars) {
        const auto fills = portfolio.check_exits(bar, exec);
        for (const auto& fill : fills) {
            portfolio.apply_fill(fill);
        }
    }
}

void flatten_positions(const std::string& date, const std::vector<Bar>& day_bars, Portfolio& portfolio, ExecutionHandler& exec) {
    for (const auto& bar : day_bars) {
        if (!portfolio.has_position(bar.symbol)) {
            continue;
        }
        const int qty = portfolio.positions().at(bar.symbol).qty;
        portfolio.apply_fill(exec.fill({date, bar.symbol, Side::Sell, qty, bar.open, 0.0, 0.0, ExitReason::Drawdown}, bar.open, bar.volume));
    }
}

BacktestResult run_stream_backtest(
    const std::vector<Bar>& bars,
    const BacktestConfig& cfg,
    const Strategy* strategy
) {
    Portfolio portfolio(cfg.capital, cfg.risk);
    ExecutionHandler exec;
    DayMap bars_by_date;
    std::vector<std::string> dates;
    DateIndex date_idx;
    SampleMap sample_map;

    build_book(bars, bars_by_date, dates, date_idx);
    build_samples(bars, cfg.fft_window, sample_map);

    OrderMap pending;
    bool flatten_next_open = false;
    int folds = 0;
    int oos_days = 0;
    const std::size_t last_day = dates.empty() ? 0 : dates.size() - 1;

    for (std::size_t day = 0; day <= last_day; ++day) {
        const auto& cur_date = dates[day];
        const auto& day_bars = bars_by_date[cur_date];

        if (flatten_next_open) {
            flatten_positions(cur_date, day_bars, portfolio, exec);
            flatten_next_open = false;
        }

        fill_pending_on_open(cur_date, day_bars, portfolio, exec, pending);
        manage_exits(day_bars, portfolio, exec);

        const bool can_score = !portfolio.halted();
        if (can_score && strategy != nullptr) {
            for (const auto& bar : day_bars) {
                const auto it = sample_map.find(key(cur_date, bar.symbol));
                if (it == sample_map.end()) {
                    continue;
                }
                if (!it->second.next_date.empty()) {
                    const auto next_day = date_idx.find(it->second.next_date);
                    if (next_day == date_idx.end()) {
                        continue;
                    }
                } else {
                    continue;
                }

                const Signal signal = strategy->score(it->second).signal;
                if (signal.side == Side::Hold) {
                    continue;
                }
                Order order = portfolio.make_order(signal);
                if (order.qty > 0) {
                    pending[bar.symbol] = order;
                }
            }
        }

        portfolio.mark(cur_date, day_bars);
        ++oos_days;

        if (!portfolio.halted() && portfolio.current_drawdown() >= cfg.risk.max_drawdown) {
            portfolio.halt();
            pending.clear();
            flatten_next_open = true;
        }
    }

    BacktestResult result{portfolio, PerformanceAnalyzer::analyze(portfolio), folds, oos_days};
    return result;
}

}

WalkForwardBacktester::WalkForwardBacktester(BacktestConfig cfg) : cfg_(cfg) {}

BacktestResult WalkForwardBacktester::run(const std::vector<Bar>& bars) const {
    Portfolio portfolio(cfg_.capital, cfg_.risk);
    ExecutionHandler exec;

    std::map<std::string, std::vector<Bar>> bars_by_date;
    for (const auto& bar : bars) {
        bars_by_date[bar.date].push_back(bar);
    }

    std::vector<std::string> dates;
    dates.reserve(bars_by_date.size());
    std::unordered_map<std::string, std::size_t> date_idx;
    for (const auto& item : bars_by_date) {
        date_idx[item.first] = dates.size();
        dates.push_back(item.first);
    }

    const auto samples = FeatureEngine::build(bars, cfg_.fft_window);
    std::unordered_map<std::string, Sample> sample_map;
    sample_map.reserve(samples.size());
    for (const auto& row : samples) {
        sample_map[key(row.date, row.symbol)] = row;
    }

    std::unordered_map<std::string, Order> pending;
    bool flatten_next_open = false;
    int folds = 0;

    for (std::size_t start = 0; start + cfg_.train_days + cfg_.test_days <= dates.size(); start += cfg_.test_days) {
        const std::size_t train_end = start + cfg_.train_days - 1;
        const std::size_t test_start = train_end + 1;
        const std::size_t test_end = test_start + cfg_.test_days - 1;

        std::vector<Sample> train_rows;
        for (const auto& row : samples) {
            const auto d0 = date_idx.find(row.date);
            const auto d1 = date_idx.find(row.next_date);
            if (d0 == date_idx.end() || d1 == date_idx.end()) {
                continue;
            }
            if (d0->second >= start && d1->second <= train_end) {
                train_rows.push_back(row);
            }
        }
        if (train_rows.size() < 30) {
            continue;
        }

        ++folds;
        Strategy strategy(train_models(train_rows));

        for (std::size_t day = test_start; day <= test_end; ++day) {
            const auto& cur_date = dates[day];
            const auto& day_bars = bars_by_date[cur_date];

            if (flatten_next_open) {
                flatten_positions(cur_date, day_bars, portfolio, exec);
                flatten_next_open = false;
            }

            fill_pending_on_open(cur_date, day_bars, portfolio, exec, pending);
            manage_exits(day_bars, portfolio, exec);

            const bool last_test_day = day == test_end;
            if (!portfolio.halted() && !last_test_day) {
                for (const auto& bar : day_bars) {
                    const auto it = sample_map.find(key(cur_date, bar.symbol));
                    if (it == sample_map.end()) {
                        continue;
                    }
                    const auto next_day = date_idx.find(it->second.next_date);
                    if (next_day == date_idx.end() || next_day->second > test_end) {
                        continue;
                    }
                    const Signal signal = strategy.score(it->second).signal;
                    if (signal.side == Side::Hold) {
                        continue;
                    }
                    Order order = portfolio.make_order(signal);
                    if (order.qty > 0) {
                        pending[bar.symbol] = order;
                    }
                }
            } else {
                pending.clear();
                const auto fills = portfolio.close_all(day_bars, exec, ExitReason::EndOfTest);
                for (const auto& fill : fills) {
                    portfolio.apply_fill(fill);
                }
            }

            portfolio.mark(cur_date, day_bars);

            if (!portfolio.halted() && portfolio.current_drawdown() >= cfg_.risk.max_drawdown) {
                portfolio.halt();
                pending.clear();
                flatten_next_open = true;
            }
        }
    }

    BacktestResult result{portfolio, PerformanceAnalyzer::analyze(portfolio), folds, static_cast<int>(portfolio.curve().size())};
    return result;
}

BacktestResult run_model_backtest(const std::vector<Bar>& bars, const ModelPack& models, const BacktestConfig& cfg) {
    Strategy strategy(models);
    BacktestConfig fixed_cfg = cfg;
    fixed_cfg.fft_window = models.fft_window;
    return run_stream_backtest(bars, fixed_cfg, &strategy);
}
