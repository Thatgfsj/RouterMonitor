// JsonParser.h
// Minimal read-only JSON parser. ~300 LOC.
// Supports object/array/string/number/bool/null navigation.
// All access is safe on missing keys (returns Null/empty).
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <cstdint>

namespace rm {

struct JsonValue;
using JsonObject = std::vector<std::pair<std::string, std::unique_ptr<JsonValue>>>;
using JsonArray = std::vector<std::unique_ptr<JsonValue>>;

enum class JsonType {
    Null,
    Bool,
    Number,    // stored as double
    String,
    Array,
    Object
};

struct JsonValue {
    JsonType type = JsonType::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    JsonArray a;
    JsonObject o;

    bool IsNull() const { return type == JsonType::Null; }
    bool IsObject() const { return type == JsonType::Object; }
    bool IsArray() const { return type == JsonType::Array; }
    bool IsString() const { return type == JsonType::String; }
    bool IsNumber() const { return type == JsonType::Number; }
    bool IsBool() const { return type == JsonType::Bool; }

    // Lookup by key. Returns nullptr if missing or not an object.
    const JsonValue* Find(const std::string& key) const;
    JsonValue* Find(const std::string& key);

    // Index array. Returns nullptr if out of range or not an array.
    const JsonValue* At(size_t i) const;
    JsonValue* At(size_t i);

    // Convenience: Get string with default if missing.
    std::string Str(const std::string& key, const std::string& def = "") const;
    double Num(const std::string& key, double def = 0.0) const;
    int64_t Int(const std::string& key, int64_t def = 0) const;
    bool Bool(const std::string& key, bool def = false) const;
};

// Parse a UTF-8 JSON string. Returns nullptr on parse error (with optional err msg).
std::unique_ptr<JsonValue> JsonParse(const std::string& text, std::string* err = nullptr);

} // namespace rm