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
 * json_parse.hpp - Lightweight JSON Parser Interface
 *
 * Header file defining a minimal dependency-free JSON parser for HTTP
 * POST request bodies. Handles flat objects with string/number/boolean
 * values. No arrays, nested objects, or null values.
 *
 */

#ifndef _INCLUDE_JSON_PARSE_HPP_
#define _INCLUDE_JSON_PARSE_HPP_

#include <string>
#include <map>
#include <variant>

class JsonParser {
public:
    using JsonValue = std::variant<std::string, double, bool, std::nullptr_t>;

    /**
     * Parse JSON string into internal map
     * Returns true if parsing succeeded, false otherwise
     */
    bool parse(const std::string& json);

    /**
     * Check if a key exists in the parsed JSON
     */
    bool has(const std::string& key) const;

    /**
     * Get value by key (may throw std::out_of_range)
     */
    JsonValue get(const std::string& key) const;

    /**
     * Get all parsed key-value pairs
     */
    const std::map<std::string, JsonValue>& getAll() const;

    /**
     * Get string value with default fallback
     */
    std::string getString(const std::string& key, const std::string& def = "") const;

    /**
     * Get double value with default fallback
     */
    double getNumber(const std::string& key, double def = 0.0) const;

    /**
     * Get boolean value with default fallback
     */
    bool getBool(const std::string& key, bool def = false) const;

    /**
     * Get last parse error message
     */
    const std::string& getError() const { return error_; }

private:
    std::map<std::string, JsonValue> values_;
    std::string json_;
    size_t pos_ = 0;
    std::string error_;

    void skipWhitespace();
    bool parseObject();
    bool parseKeyValue();
    std::string parseString();
    JsonValue parseValue();
    double parseNumber();
    bool parseBool();

    void setError(const std::string& msg);
    bool expect(char ch);
    char peek() const;
    char next();
};

#endif /* _INCLUDE_JSON_PARSE_HPP_ */
