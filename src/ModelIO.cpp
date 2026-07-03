#include "../include/ModelIO.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string read_text(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open model file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == '\\' || ch == '"') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

std::size_t find_key(const std::string& text, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const std::size_t pos = text.find(token);
    if (pos == std::string::npos) {
        throw std::runtime_error("missing key in model file: " + key);
    }
    return pos + token.size();
}

std::size_t skip_to_value(const std::string& text, std::size_t pos) {
    pos = text.find(':', pos);
    if (pos == std::string::npos) {
        throw std::runtime_error("invalid model file");
    }
    ++pos;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return pos;
}

double parse_number(const std::string& text, const std::string& key) {
    std::size_t pos = skip_to_value(text, find_key(text, key));
    std::size_t end = pos;
    while (end < text.size()) {
        const char ch = text[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) {
            break;
        }
        ++end;
    }
    return std::stod(text.substr(pos, end - pos));
}

std::vector<double> parse_array(const std::string& text, const std::string& key) {
    std::size_t pos = skip_to_value(text, find_key(text, key));
    if (pos >= text.size() || text[pos] != '[') {
        throw std::runtime_error("expected array for key: " + key);
    }
    ++pos;
    std::vector<double> values;
    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            break;
        }
        std::size_t end = pos;
        while (end < text.size()) {
            const char ch = text[end];
            if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) {
                break;
            }
            ++end;
        }
        values.push_back(std::stod(text.substr(pos, end - pos)));
        pos = end;
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
        }
    }
    return values;
}

void assign_array(std::array<double, kFeatureCount>& dst, const std::vector<double>& src, const std::string& key) {
    if (src.size() != kFeatureCount) {
        throw std::runtime_error("wrong array size for key: " + key);
    }
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        dst[i] = src[i];
    }
}

}

void save_model(const ModelPack& pack, const std::string& path) {
    std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("cannot write model file: " + path);
    }

    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"feature_count\": " << kFeatureCount << ",\n";
    out << "  \"fft_window\": " << pack.fft_window << ",\n";
    out << "  \"buy_level\": " << pack.buy_level << ",\n";
    out << "  \"sell_level\": " << pack.sell_level << ",\n";
    out << "  \"min_move\": " << pack.min_move << ",\n";
    out << "  \"feature_names\": [";
    const auto& names = feature_names();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << "\"" << escape_json(names[i]) << "\"";
    }
    out << "],\n";
    out << "  \"logistic\": {\n";
    out << "    \"bias\": " << pack.logistic.b << ",\n";
    out << "    \"mean\": [";
    for (std::size_t i = 0; i < pack.logistic.scaler.mean.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << pack.logistic.scaler.mean[i];
    }
    out << "],\n";
    out << "    \"scale\": [";
    for (std::size_t i = 0; i < pack.logistic.scaler.scale.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << pack.logistic.scaler.scale[i];
    }
    out << "],\n";
    out << "    \"weights\": [";
    for (std::size_t i = 0; i < pack.logistic.w.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << pack.logistic.w[i];
    }
    out << "]\n";
    out << "  },\n";
    out << "  \"ridge\": {\n";
    out << "    \"bias\": " << pack.ridge.b << ",\n";
    out << "    \"mean\": [";
    for (std::size_t i = 0; i < pack.ridge.scaler.mean.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << pack.ridge.scaler.mean[i];
    }
    out << "],\n";
    out << "    \"scale\": [";
    for (std::size_t i = 0; i < pack.ridge.scaler.scale.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << pack.ridge.scaler.scale[i];
    }
    out << "],\n";
    out << "    \"weights\": [";
    for (std::size_t i = 0; i < pack.ridge.w.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << pack.ridge.w[i];
    }
    out << "]\n";
    out << "  }\n";
    out << "}\n";
}

ModelPack load_model(const std::string& path) {
    const std::string text = read_text(path);

    ModelPack pack;
    pack.fft_window = static_cast<std::size_t>(parse_number(text, "fft_window"));
    pack.buy_level = parse_number(text, "buy_level");
    pack.sell_level = parse_number(text, "sell_level");
    pack.min_move = parse_number(text, "min_move");

    pack.logistic.b = parse_number(text, "bias");
    assign_array(pack.logistic.scaler.mean, parse_array(text, "mean"), "logistic.mean");
    assign_array(pack.logistic.scaler.scale, parse_array(text, "scale"), "logistic.scale");
    assign_array(pack.logistic.w, parse_array(text, "weights"), "logistic.weights");

    const std::size_t ridge_pos = find_key(text, "ridge");
    const std::string ridge_text = text.substr(ridge_pos);
    pack.ridge.b = parse_number(ridge_text, "bias");
    assign_array(pack.ridge.scaler.mean, parse_array(ridge_text, "mean"), "ridge.mean");
    assign_array(pack.ridge.scaler.scale, parse_array(ridge_text, "scale"), "ridge.scale");
    assign_array(pack.ridge.w, parse_array(ridge_text, "weights"), "ridge.weights");

    return pack;
}
