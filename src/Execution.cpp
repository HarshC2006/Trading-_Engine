#include "../include/Execution.h"

#include <algorithm>
#include <cmath>

ExecutionHandler::ExecutionHandler(double bps, double fee) : bps_(bps), fee_(fee) {}

Fill ExecutionHandler::fill(const Order& order, double raw_price, long volume) const {
    const double part = static_cast<double>(order.qty) / static_cast<double>(std::max<long>(1, volume));
    const double slip_pct = std::min(0.01, 0.0001 + part * part);
    const double slip = raw_price * slip_pct;

    double price = raw_price;
    if (order.side == Side::Buy) {
        price += slip;
    } else if (order.side == Side::Sell) {
        price -= slip;
    }

    const double commission = (price * static_cast<double>(order.qty) * bps_) + fee_;
    return {
        order.date,
        order.symbol,
        order.side,
        order.qty,
        price,
        commission,
        slip,
        order.stop,
        order.target,
        order.reason
    };
}
