#include "../include/Datahandler.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        out.push_back(item);
    }
    return out;
}

std::string lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

double to_double(const std::string& s) {
    return std::stod(s);
}

long to_long(const std::string& s) {
    return std::stol(s);
}

}

DataHandler::DataHandler(std::string path) : path_(std::move(path)) {}

std::vector<Bar> DataHandler::load() const {
    std::ifstream file(path_);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open data file: " + path_);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    if (lines.empty()) {
        throw std::runtime_error("data file is empty: " + path_);
    }

    std::vector<Bar> bars;

    if (starts_with(lines.front(), "Price,") && lines.size() >= 4 && starts_with(lines[1], "Ticker,")) {
        const auto meta = split_csv(lines[1]);
        const std::string symbol = meta.size() > 1 && !meta[1].empty() ? meta[1] : "ASSET";

        for (std::size_t i = 3; i < lines.size(); ++i) {
            const auto cols = split_csv(lines[i]);
            if (cols.size() < 6) {
                continue;
            }

            try {
                bars.push_back({
                    cols[0],
                    symbol,
                    to_double(cols[4]),
                    to_double(cols[2]),
                    to_double(cols[3]),
                    to_double(cols[1]),
                    to_long(cols[5])
                });
            } catch (...) {
                continue;
            }
        }
    } else {
        const auto header = split_csv(lines.front());
        int date_col = -1;
        int symbol_col = -1;
        int open_col = -1;
        int high_col = -1;
        int low_col = -1;
        int close_col = -1;
        int volume_col = -1;

        for (std::size_t i = 0; i < header.size(); ++i) {
            const auto name = lower_copy(header[i]);
            if (name == "date") {
                date_col = static_cast<int>(i);
            } else if (name == "symbol" || name == "ticker") {
                symbol_col = static_cast<int>(i);
            } else if (name == "open") {
                open_col = static_cast<int>(i);
            } else if (name == "high") {
                high_col = static_cast<int>(i);
            } else if (name == "low") {
                low_col = static_cast<int>(i);
            } else if (name == "close" || name == "adj close") {
                close_col = static_cast<int>(i);
            } else if (name == "volume") {
                volume_col = static_cast<int>(i);
            }
        }

        if (date_col < 0 || open_col < 0 || high_col < 0 || low_col < 0 || close_col < 0 || volume_col < 0) {
            throw std::runtime_error("unsupported csv format in " + path_);
        }

        for (std::size_t i = 1; i < lines.size(); ++i) {
            const auto cols = split_csv(lines[i]);
            if (static_cast<int>(cols.size()) <= std::max(volume_col, close_col)) {
                continue;
            }

            try {
                bars.push_back({
                    cols[date_col],
                    symbol_col >= 0 ? cols[symbol_col] : "ASSET",
                    to_double(cols[open_col]),
                    to_double(cols[high_col]),
                    to_double(cols[low_col]),
                    to_double(cols[close_col]),
                    to_long(cols[volume_col])
                });
            } catch (...) {
                continue;
            }
        }
    }

    std::sort(bars.begin(), bars.end(), [](const Bar& a, const Bar& b) {
        if (a.date == b.date) {
            return a.symbol < b.symbol;
        }
        return a.date < b.date;
    });

    if (bars.empty()) {
        throw std::runtime_error("no usable rows found in " + path_);
    }

    return bars;
}
