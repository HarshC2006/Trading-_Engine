#include "../include/Features.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <unordered_map>

namespace {

constexpr double kEps = 1e-9;

double mean(const std::vector<double>& v, std::size_t from, std::size_t count) {
    double total = 0.0;
    for (std::size_t i = from; i < from + count; ++i) {
        total += v[i];
    }
    return total / static_cast<double>(count);
}

double stdev(const std::vector<double>& v, std::size_t from, std::size_t count) {
    if (count < 2) {
        return 0.0;
    }
    const double m = mean(v, from, count);
    double acc = 0.0;
    for (std::size_t i = from; i < from + count; ++i) {
        const double d = v[i] - m;
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(count - 1));
}

double safe_ratio(double a, double b) {
    return a / (std::fabs(b) + kEps);
}

std::array<double, 5> spectral_stats(const std::vector<double>& rets, std::size_t end_idx, std::size_t window) {
    std::vector<double> frame(window);
    const std::size_t start = end_idx + 1 - window;
    const double m = mean(rets, start, window);

    for (std::size_t i = 0; i < window; ++i) {
        frame[i] = rets[start + i] - m;
    }

    const std::size_t last_bin = window / 2;
    double low = 0.0;
    double mid = 0.0;
    double high = 0.0;

    for (std::size_t k = 1; k <= last_bin; ++k) {
        std::complex<double> acc(0.0, 0.0);
        for (std::size_t n = 0; n < window; ++n) {
            const double angle = -2.0 * 3.14159265358979323846 * static_cast<double>(k * n) / static_cast<double>(window);
            acc += std::polar(frame[n], angle);
        }
        const double energy = std::norm(acc);
        if (k <= 2) {
            low += energy;
        } else if (k <= 5) {
            mid += energy;
        } else {
            high += energy;
        }
    }

    const double total = low + mid + high + kEps;
    return {
        low / total,
        mid / total,
        high / total,
        safe_ratio(low, mid),
        safe_ratio(high, low)
    };
}

double atr(const std::vector<Bar>& bars, std::size_t end_idx, std::size_t len) {
    const std::size_t start = end_idx + 1 - len;
    double total = 0.0;
    for (std::size_t i = start; i <= end_idx; ++i) {
        const double prev_close = i == 0 ? bars[i].close : bars[i - 1].close;
        const double tr1 = bars[i].high - bars[i].low;
        const double tr2 = std::fabs(bars[i].high - prev_close);
        const double tr3 = std::fabs(bars[i].low - prev_close);
        total += std::max(tr1, std::max(tr2, tr3));
    }
    return total / static_cast<double>(len);
}

}

std::vector<Sample> FeatureEngine::build(const std::vector<Bar>& bars, std::size_t fft_window, bool include_latest) {
    std::unordered_map<std::string, std::vector<Bar>> by_symbol;
    for (const auto& bar : bars) {
        by_symbol[bar.symbol].push_back(bar);
    }

    std::vector<Sample> rows;
    const std::size_t lookback = std::max<std::size_t>(20, fft_window);

    for (auto& item : by_symbol) {
        auto& series = item.second;
        std::sort(series.begin(), series.end(), [](const Bar& a, const Bar& b) {
            return a.date < b.date;
        });

        if (series.size() <= lookback + 1) {
            continue;
        }

        std::vector<double> rets(series.size(), 0.0);
        std::vector<double> vols(series.size(), 0.0);

        for (std::size_t i = 1; i < series.size(); ++i) {
            rets[i] = (series[i].close / series[i - 1].close) - 1.0;
            vols[i] = static_cast<double>(series[i].volume);
        }

        const std::size_t limit = include_latest ? series.size() : series.size() - 1;
        for (std::size_t i = lookback; i < limit; ++i) {
            const bool has_next = i + 1 < series.size();
            const auto bands = spectral_stats(rets, i, fft_window);
            const double mean3 = mean(rets, i + 1 - 3, 3);
            const double mean5 = mean(rets, i + 1 - 5, 5);
            const double mean10 = mean(rets, i + 1 - 10, 10);
            const double mean20 = mean(rets, i + 1 - 20, 20);
            const double vol5 = stdev(rets, i + 1 - 5, 5);
            const double vol10 = stdev(rets, i + 1 - 10, 10);
            const double vol20 = stdev(rets, i + 1 - 20, 20);
            const double atr14 = atr(series, i, 14) / std::max(series[i].close, kEps);
            const double range = (series[i].high - series[i].low) / std::max(series[i].close, kEps);
            const double vol_mean = mean(vols, i + 1 - 20, 20);
            const double vol_std = stdev(vols, i + 1 - 20, 20);
            const double vol_z = (vols[i] - vol_mean) / (vol_std + kEps);

            Sample row;
            row.date = series[i].date;
            row.symbol = series[i].symbol;
            row.next_date = has_next ? series[i + 1].date : "";
            row.close = series[i].close;
            row.next_open = has_next ? series[i + 1].open : series[i].close;
            row.next_ret = has_next ? (series[i + 1].close / std::max(series[i + 1].open, kEps)) - 1.0 : 0.0;
            row.atr = atr14;
            row.vol = vol20;
            row.x = {
                rets[i],
                mean3,
                mean5,
                mean10,
                mean20,
                vol5,
                vol10,
                vol20,
                atr14,
                range,
                bands[0],
                bands[1],
                bands[2],
                bands[3],
                bands[4],
                vol_z
            };

            rows.push_back(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Sample& a, const Sample& b) {
        if (a.date == b.date) {
            return a.symbol < b.symbol;
        }
        return a.date < b.date;
    });

    return rows;
}
