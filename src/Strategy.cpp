#include "../include/Strategy.h"

Strategy::Strategy(ModelPack models) : models_(std::move(models)) {}

ScoredSignal Strategy::score(const Sample& row) const {
    const double prob = models_.logistic.predict(row.x);
    const double ret = models_.ridge.predict(row.x);
    Signal signal{row.date, row.symbol, Side::Hold, row.next_open, ret, row.atr, row.vol};

    if (prob >= models_.buy_level && ret >= models_.min_move) {
        signal.side = Side::Buy;
    } else if (prob <= models_.sell_level && ret <= -models_.min_move) {
        signal.side = Side::Sell;
    }

    return {signal, prob, ret};
}

Signal Strategy::on_sample(const Sample& row) const {
    return score(row).signal;
}
