#pragma once

#include "Features.h"

#include <array>
#include <string>
#include <vector>

struct Standardizer {
    std::array<double, kFeatureCount> mean{};
    std::array<double, kFeatureCount> scale{};

    std::array<double, kFeatureCount> transform(const std::array<double, kFeatureCount>& x) const;
    static Standardizer fit(const std::vector<Sample>& rows);
};

struct LogisticModel {
    Standardizer scaler;
    std::array<double, kFeatureCount> w{};
    double b = 0.0;

    double predict(const std::array<double, kFeatureCount>& x) const;
};

struct RidgeModel {
    Standardizer scaler;
    std::array<double, kFeatureCount> w{};
    double b = 0.0;

    double predict(const std::array<double, kFeatureCount>& x) const;
};

struct ModelPack {
    LogisticModel logistic;
    RidgeModel ridge;
    double buy_level = 0.55;
    double sell_level = 0.45;
    double min_move = 0.0010;
    std::size_t fft_window = 16;
};

ModelPack train_models(const std::vector<Sample>& rows);
ModelPack default_models();
const std::array<std::string, kFeatureCount>& feature_names();
