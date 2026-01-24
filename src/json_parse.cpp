/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/*
 * json_parse.cpp - Lightweight JSON Parser
 *
 * This module implements a minimal dependency-free JSON parser for
 * parsing HTTP POST request bodies and configuration data, avoiding
 * external JSON library dependencies.
 *
 */

#include "json_parse.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>

bool JsonParser::parse(const std::string& json) {
    json_ = json;
    pos_ = 0;
    values_.clear();
    error_.clear();

    skipWhitespace();
    if (!parseObject()) {
        return false;
    }

    skipWhitespace();
    if (pos_ < json_.length()) {
        setError("Unexpected content after JSON object");
        return false;
    }

    return true;
}

bool JsonParser::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}

JsonParser::JsonValue JsonParser::get(const std::string& key) const {
    return values_.at(key);
}

const std::map<std::string, JsonParser::JsonValue>& JsonParser::getAll() const {
    return values_;
}

std::string JsonParser::getString(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return def;
    }
    if (auto* str = std::get_if<std::string>(&it->second)) {
        return *str;
    }
    if (auto* num = std::get_if<double>(&it->second)) {
        std::ostringstream oss;
        oss << *num;
        return oss.str();
    }
    if (auto* b = std::get_if<bool>(&it->second)) {
        return *b ? "true" : "false";
    }
    return def;
}

double JsonParser::getNumber(const std::string& key, double def) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return def;
    }
    if (auto* num = std::get_if<double>(&it->second)) {
        return *num;
    }
    if (auto* str = std::get_if<std::string>(&it->second)) {
        char* end;
        double val = std::strtod(str->c_str(), &end);
        if (end != str->c_str() && *end == '\0') {
            return val;
        }
    }
    return def;
}

bool JsonParser::getBool(const std::string& key, bool def) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return def;
    }
    if (auto* b = std::get_if<bool>(&it->second)) {
        return *b;
    }
    if (auto* str = std::get_if<std::string>(&it->second)) {
        return *str == "true" || *str == "1";
    }
    if (auto* num = std::get_if<double>(&it->second)) {
        return *num != 0.0;
    }
    return def;
}

void JsonParser::skipWhitespace() {
    while (pos_ < json_.length() && std::isspace(json_[pos_])) {
        pos_++;
    }
}

bool JsonParser::parseObject() {
    if (!expect('{')) {
        return false;
    }

    skipWhitespace();
    if (peek() == '}') {
        pos_++;
        return true; // Empty object
    }

    while (true) {
        if (!parseKeyValue()) {
            return false;
        }

        skipWhitespace();
        char ch = next();
        if (ch == '}') {
            break;
        }
        if (ch != ',') {
            setError("Expected ',' or '}' in object");
            return false;
        }
        skipWhitespace();
    }

    return true;
}

bool JsonParser::parseKeyValue() {
    skipWhitespace();

    std::string key = parseString();
    if (key.empty() && !error_.empty()) {
        return false;
    }

    skipWhitespace();
    if (!expect(':')) {
        return false;
    }

    skipWhitespace();
    JsonValue value = parseValue();
    if (!error_.empty()) {
        return false;
    }

    values_[key] = value;
    return true;
}

std::string JsonParser::parseString() {
    if (!expect('"')) {
        return "";
    }

    std::string result;
    while (pos_ < json_.length()) {
        char ch = json_[pos_++];

        if (ch == '"') {
            return result;
        }

        if (ch == '\\') {
            if (pos_ >= json_.length()) {
                setError("Unterminated escape sequence");
                return "";
            }
            ch = json_[pos_++];
            switch (ch) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:
                    setError("Invalid escape sequence");
                    return "";
            }
        } else {
            result += ch;
        }
    }

    setError("Unterminated string");
    return "";
}

JsonParser::JsonValue JsonParser::parseValue() {
    skipWhitespace();

    if (pos_ >= json_.length()) {
        setError("Unexpected end of input");
        return nullptr;
    }

    char ch = peek();

    if (ch == '"') {
        return parseString();
    }

    if (ch == 't' || ch == 'f') {
        return parseBool();
    }

    if (ch == '-' || std::isdigit(ch)) {
        return parseNumber();
    }

    setError("Unexpected character in value");
    return nullptr;
}

double JsonParser::parseNumber() {
    size_t start = pos_;

    if (peek() == '-') {
        pos_++;
    }

    if (pos_ >= json_.length() || !std::isdigit(json_[pos_])) {
        setError("Invalid number format");
        return 0.0;
    }

    while (pos_ < json_.length() && std::isdigit(json_[pos_])) {
        pos_++;
    }

    // Handle decimal point
    if (pos_ < json_.length() && json_[pos_] == '.') {
        pos_++;
        if (pos_ >= json_.length() || !std::isdigit(json_[pos_])) {
            setError("Invalid number format after decimal point");
            return 0.0;
        }
        while (pos_ < json_.length() && std::isdigit(json_[pos_])) {
            pos_++;
        }
    }

    std::string numStr = json_.substr(start, pos_ - start);
    char* end;
    double value = std::strtod(numStr.c_str(), &end);

    if (end == numStr.c_str()) {
        setError("Failed to parse number");
        return 0.0;
    }

    return value;
}

bool JsonParser::parseBool() {
    if (pos_ + 4 <= json_.length() && json_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return true;
    }

    if (pos_ + 5 <= json_.length() && json_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return false;
    }

    setError("Invalid boolean value");
    return false;
}

void JsonParser::setError(const std::string& msg) {
    if (error_.empty()) {  // Only set first error
        error_ = msg + " at position " + std::to_string(pos_);
    }
}

bool JsonParser::expect(char ch) {
    skipWhitespace();
    if (pos_ >= json_.length() || json_[pos_] != ch) {
        setError(std::string("Expected '") + ch + "'");
        return false;
    }
    pos_++;
    return true;
}

char JsonParser::peek() const {
    if (pos_ >= json_.length()) {
        return '\0';
    }
    return json_[pos_];
}

char JsonParser::next() {
    if (pos_ >= json_.length()) {
        return '\0';
    }
    return json_[pos_++];
}
