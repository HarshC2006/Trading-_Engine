#include "../include/Models.h"
#include "../include/GeneratedModels.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kEps = 1e-9;
constexpr double kLogisticL2 = 0.05;
constexpr double kRidgeL2 = 5.0;
constexpr int kLogisticSteps = 800;
constexpr double kLogisticLr = 0.05;

const std::array<std::string, kFeatureCount> kFeatureNames = {{
    "ret_1",
    "mean_3",
    "mean_5",
    "mean_10",
    "mean_20",
    "vol_5",
    "vol_10",
    "vol_20",
    "atr_14",
    "range_1",
    "band_low",
    "band_mid",
    "band_high",
    "band_low_mid",
    "band_high_low",
    "vol_z"
}};

double sigmoid(double x) {
    if (x >= 0.0) {
        const double z = std::exp(-x);
        return 1.0 / (1.0 + z);
    }
    const double z = std::exp(x);
    return z / (1.0 + z);
}

std::vector<double> solve_linear(std::vector<std::vector<double>> a, std::vector<double> b) {
    const std::size_t n = a.size();
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t pivot = i;
        for (std::size_t r = i + 1; r < n; ++r) {
            if (std::fabs(a[r][i]) > std::fabs(a[pivot][i])) {
                pivot = r;
            }
        }
        std::swap(a[i], a[pivot]);
        std::swap(b[i], b[pivot]);

        const double diag = std::fabs(a[i][i]) < kEps ? (a[i][i] < 0.0 ? -kEps : kEps) : a[i][i];
        for (std::size_t c = i; c < n; ++c) {
            a[i][c] /= diag;
        }
        b[i] /= diag;

        for (std::size_t r = 0; r < n; ++r) {
            if (r == i) {
                continue;
            }
            const double factor = a[r][i];
            if (std::fabs(factor) < kEps) {
                continue;
            }
            for (std::size_t c = i; c < n; ++c) {
                a[r][c] -= factor * a[i][c];
            }
            b[r] -= factor * b[i];
        }
    }
    return b;
}

}

std::array<double, kFeatureCount> Standardizer::transform(const std::array<double, kFeatureCount>& x) const {
    std::array<double, kFeatureCount> out{};
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        out[i] = (x[i] - mean[i]) / scale[i];
    }
    return out;
}

Standardizer Standardizer::fit(const std::vector<Sample>& rows) {
    Standardizer scaler;
    if (rows.empty()) {
        scaler.scale.fill(1.0);
        return scaler;
    }

    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        double total = 0.0;
        for (const auto& row : rows) {
            total += row.x[i];
        }
        scaler.mean[i] = total / static_cast<double>(rows.size());
    }

    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        double acc = 0.0;
        for (const auto& row : rows) {
            const double d = row.x[i] - scaler.mean[i];
            acc += d * d;
        }
        scaler.scale[i] = std::sqrt(acc / std::max<double>(1.0, static_cast<double>(rows.size() - 1)));
        if (scaler.scale[i] < kEps) {
            scaler.scale[i] = 1.0;
        }
    }

    return scaler;
}

double LogisticModel::predict(const std::array<double, kFeatureCount>& x) const {
    const auto z = scaler.transform(x);
    double score = b;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        score += z[i] * w[i];
    }
    return sigmoid(score);
}

double RidgeModel::predict(const std::array<double, kFeatureCount>& x) const {
    const auto z = scaler.transform(x);
    double score = b;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        score += z[i] * w[i];
    }
    return score;
}

ModelPack train_models(const std::vector<Sample>& rows) {
    if (rows.empty()) {
        return default_models();
    }

    ModelPack pack;
    pack.logistic.scaler = Standardizer::fit(rows);
    pack.ridge.scaler = Standardizer::fit(rows);

    const std::size_t n = rows.size();
    std::vector<std::array<double, kFeatureCount>> x(n);
    std::vector<double> y_cls(n, 0.0);
    std::vector<double> y_reg(n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        x[i] = pack.logistic.scaler.transform(rows[i].x);
        y_cls[i] = rows[i].next_ret > 0.0 ? 1.0 : 0.0;
        y_reg[i] = rows[i].next_ret;
    }

    for (int step = 0; step < kLogisticSteps; ++step) {
        std::array<double, kFeatureCount> grad{};
        double grad_b = 0.0;

        for (std::size_t i = 0; i < n; ++i) {
            double score = pack.logistic.b;
            for (std::size_t j = 0; j < kFeatureCount; ++j) {
                score += pack.logistic.w[j] * x[i][j];
            }
            const double p = sigmoid(score);
            const double err = p - y_cls[i];
            grad_b += err;
            for (std::size_t j = 0; j < kFeatureCount; ++j) {
                grad[j] += err * x[i][j];
            }
        }

        for (std::size_t j = 0; j < kFeatureCount; ++j) {
            grad[j] = (grad[j] / static_cast<double>(n)) + (kLogisticL2 * pack.logistic.w[j]);
            pack.logistic.w[j] -= kLogisticLr * grad[j];
        }
        pack.logistic.b -= kLogisticLr * grad_b / static_cast<double>(n);
    }

    const std::size_t p = kFeatureCount + 1;
    std::vector<std::vector<double>> xtx(p, std::vector<double>(p, 0.0));
    std::vector<double> xty(p, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        std::array<double, kFeatureCount> z = pack.ridge.scaler.transform(rows[i].x);
        xtx[0][0] += 1.0;
        xty[0] += y_reg[i];

        for (std::size_t j = 0; j < kFeatureCount; ++j) {
            xtx[0][j + 1] += z[j];
            xtx[j + 1][0] += z[j];
            xty[j + 1] += z[j] * y_reg[i];
            for (std::size_t k = 0; k < kFeatureCount; ++k) {
                xtx[j + 1][k + 1] += z[j] * z[k];
            }
        }
    }

    for (std::size_t i = 1; i < p; ++i) {
        xtx[i][i] += kRidgeL2;
    }

    const auto beta = solve_linear(xtx, xty);
    pack.ridge.b = beta[0];
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        pack.ridge.w[i] = beta[i + 1];
    }

    return pack;
}

ModelPack default_models() {
    ModelPack pack;
    pack.logistic.scaler.mean = generated::logistic_mean;
    pack.logistic.scaler.scale = generated::logistic_scale;
    pack.logistic.w = generated::logistic_weights;
    pack.logistic.b = generated::logistic_bias;

    pack.ridge.scaler.mean = generated::ridge_mean;
    pack.ridge.scaler.scale = generated::ridge_scale;
    pack.ridge.w = generated::ridge_weights;
    pack.ridge.b = generated::ridge_bias;

    pack.buy_level = generated::buy_level;
    pack.sell_level = generated::sell_level;
    pack.min_move = generated::min_move;
    return pack;
}

const std::array<std::string, kFeatureCount>& feature_names() {
    return kFeatureNames;
}
