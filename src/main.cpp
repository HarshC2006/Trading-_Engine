#include "../include/Backtest.h"
#include "../include/Datahandler.h"
#include "../include/Features.h"
#include "../include/ModelIO.h"
#include "../include/Strategy.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

std::string side_name(Side side) {
    switch (side) {
    case Side::Buy: return "BUY";
    case Side::Sell: return "SELL";
    default: return "HOLD";
    }
}

void print_usage() {
    std::cout
        << "usage:\n"
        << "  quant_engine.exe --mode backtest --file market_data.csv --train 252 --test 63 --capital 100000\n"
        << "  quant_engine.exe --mode train --file market_data.csv --model model.json\n"
        << "  quant_engine.exe --mode predict --file latest.csv --model model.json\n";
}

std::string arg_value(int& i, int argc, char* argv[]) {
    if (i + 1 >= argc) {
        throw std::runtime_error(std::string("missing value for ") + argv[i]);
    }
    return argv[++i];
}

void run_train(const std::vector<Bar>& bars, std::size_t fft_window, const std::string& model_path) {
    const auto rows = FeatureEngine::build(bars, fft_window);
    if (rows.empty()) {
        throw std::runtime_error("not enough bars to train a model");
    }

    ModelPack pack = train_models(rows);
    pack.fft_window = fft_window;
    save_model(pack, model_path);

    std::cout << "trained on " << rows.size() << " samples\n";
    std::cout << "saved model to " << model_path << "\n";
}

void run_predict(const std::vector<Bar>& bars, const std::string& model_path) {
    const ModelPack pack = load_model(model_path);
    const Strategy strategy(pack);
    const auto rows = FeatureEngine::build(bars, pack.fft_window, true);
    if (rows.empty()) {
        throw std::runtime_error("not enough bars to score");
    }

    std::map<std::string, ScoredSignal> latest;
    for (const auto& row : rows) {
        latest[row.symbol] = strategy.score(row);
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "date,symbol,prob_up,pred_ret,signal\n";
    for (const auto& item : latest) {
        const auto& out = item.second;
        std::cout << out.signal.date << ','
                  << out.signal.symbol << ','
                  << out.prob << ','
                  << out.ret << ','
                  << side_name(out.signal.side) << '\n';
    }
}

void run_backtest(const std::vector<Bar>& bars, const std::string& model_path, const BacktestConfig& cfg) {
    if (!model_path.empty()) {
        const auto models = load_model(model_path);
        const auto result = run_model_backtest(bars, models, cfg);
        PerformanceAnalyzer::print(result.portfolio, result.report, result.folds, result.oos_days);
        return;
    }

    WalkForwardBacktester backtest(cfg);
    const auto result = backtest.run(bars);
    PerformanceAnalyzer::print(result.portfolio, result.report, result.folds, result.oos_days);
}

}

int main(int argc, char* argv[]) {
    std::string mode = "backtest";
    std::string path = "market_data.csv";
    std::string model_path;
    double capital = 100000.0;
    std::size_t train_days = 252;
    std::size_t test_days = 63;
    std::size_t fft_window = 16;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                print_usage();
                return 0;
            } else if (arg == "--mode") {
                mode = arg_value(i, argc, argv);
            } else if (arg == "--file") {
                path = arg_value(i, argc, argv);
            } else if (arg == "--model") {
                model_path = arg_value(i, argc, argv);
            } else if (arg == "--capital") {
                capital = std::stod(arg_value(i, argc, argv));
            } else if (arg == "--train") {
                train_days = static_cast<std::size_t>(std::stoul(arg_value(i, argc, argv)));
            } else if (arg == "--test") {
                test_days = static_cast<std::size_t>(std::stoul(arg_value(i, argc, argv)));
            } else if (arg == "--fft-window") {
                fft_window = static_cast<std::size_t>(std::stoul(arg_value(i, argc, argv)));
            } else {
                throw std::runtime_error("unknown flag: " + arg);
            }
        }

        DataHandler loader(path);
        const auto bars = loader.load();

        if (mode == "train") {
            if (model_path.empty()) {
                model_path = "model.json";
            }
            run_train(bars, fft_window, model_path);
            return 0;
        }

        if (mode == "predict") {
            if (model_path.empty()) {
                throw std::runtime_error("--model is required in predict mode");
            }
            run_predict(bars, model_path);
            return 0;
        }

        if (mode == "backtest") {
            BacktestConfig cfg;
            cfg.capital = capital;
            cfg.train_days = train_days;
            cfg.test_days = test_days;
            cfg.fft_window = fft_window;
            run_backtest(bars, model_path, cfg);
            return 0;
        }

        throw std::runtime_error("unknown mode: " + mode);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        print_usage();
        return 1;
    }
}
