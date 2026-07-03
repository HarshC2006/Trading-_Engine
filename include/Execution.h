#pragma once

#include "Event.h"

class ExecutionHandler {
public:
    ExecutionHandler(double bps = 0.0005, double fee = 1.0);

    Fill fill(const Order& order, double raw_price, long volume) const;

private:
    double bps_;
    double fee_;
};
