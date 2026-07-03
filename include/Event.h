#pragma once

#include <string>

enum class Side {
    Buy,
    Sell,
    Hold
};

enum class ExitReason {
    Signal,
    Stop,
    Target,
    Drawdown,
    EndOfTest
};

struct Bar {
    std::string date;
    std::string symbol;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    long volume = 0;
};

struct Signal {
    std::string date;
    std::string symbol;
    Side side = Side::Hold;
    double price = 0.0;
    double score = 0.0;
    double atr = 0.0;
    double vol = 0.0;
};

struct Order {
    std::string date;
    std::string symbol;
    Side side = Side::Hold;
    int qty = 0;
    double ref_price = 0.0;
    double stop = 0.0;
    double target = 0.0;
    ExitReason reason = ExitReason::Signal;
};

struct Fill {
    std::string date;
    std::string symbol;
    Side side = Side::Hold;
    int qty = 0;
    double price = 0.0;
    double commission = 0.0;
    double slippage = 0.0;
    double stop = 0.0;
    double target = 0.0;
    ExitReason reason = ExitReason::Signal;
};

struct Trade {
    std::string symbol;
    std::string entry_date;
    std::string exit_date;
    int qty = 0;
    double entry_price = 0.0;
    double exit_price = 0.0;
    double pnl = 0.0;
    double gross = 0.0;
    ExitReason reason = ExitReason::Signal;
};
