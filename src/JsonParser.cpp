// JsonParser.cpp
#include "JsonParser.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace rm {

const JsonValue* JsonValue::Find(const std::string& key) const {
    if (type != JsonType::Object) return nullptr;
    for (const auto& kv : o) if (kv.first == key) return kv.second.get();
    return nullptr;
}
JsonValue* JsonValue::Find(const std::string& key) {
    if (type != JsonType::Object) return nullptr;
    for (auto& kv : o) if (kv.first == key) return kv.second.get();
    return nullptr;
}
const JsonValue* JsonValue::At(size_t i) const {
    if (type != JsonType::Array || i >= a.size()) return nullptr;
    return a[i].get();
}
JsonValue* JsonValue::At(size_t i) {
    if (type != JsonType::Array || i >= a.size()) return nullptr;
    return a[i].get();
}
std::string JsonValue::Str(const std::string& key, const std::string& def) const {
    const JsonValue* v = Find(key);
    if (!v || v->type != JsonType::String) return def;
    return v->s;
}
double JsonValue::Num(const std::string& key, double def) const {
    const JsonValue* v = Find(key);
    if (!v || v->type != JsonType::Number) return def;
    return v->n;
}
int64_t JsonValue::Int(const std::string& key, int64_t def) const {
    const JsonValue* v = Find(key);
    if (!v || v->type != JsonType::Number) return def;
    return (int64_t)v->n;
}
bool JsonValue::Bool(const std::string& key, bool def) const {
    const JsonValue* v = Find(key);
    if (!v || v->type != JsonType::Bool) return def;
    return v->b;
}

namespace {

struct Parser {
    const std::string& src;
    size_t pos = 0;
    std::string err;

    void SkipWs() {
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++pos;
            else break;
        }
    }

    bool Eat(char c) {
        SkipWs();
        if (pos < src.size() && src[pos] == c) { ++pos; return true; }
        return false;
    }

    std::unique_ptr<JsonValue> ParseValue();

    std::unique_ptr<JsonValue> ParseObject() {
        auto v = std::make_unique<JsonValue>();
        v->type = JsonType::Object;
        if (!Eat('{')) { err = "expected '{'"; return nullptr; }
        SkipWs();
        if (Eat('}')) return v;
        while (true) {
            SkipWs();
            // key
            if (pos >= src.size() || src[pos] != '"') { err = "expected string key"; return nullptr; }
            std::string key = ParseStringRaw();
            if (err.empty()) {
                if (!Eat(':')) { err = "expected ':'"; return nullptr; }
                auto val = ParseValue();
                if (!val) return nullptr;
                v->o.emplace_back(std::move(key), std::move(val));
            } else {
                return nullptr;
            }
            SkipWs();
            if (Eat(',')) continue;
            if (Eat('}')) return v;
            err = "expected ',' or '}'";
            return nullptr;
        }
    }

    std::string ParseStringRaw() {
        std::string out;
        if (pos >= src.size() || src[pos] != '"') { err = "expected '\"'"; return out; }
        ++pos;
        while (pos < src.size() && src[pos] != '"') {
            char c = src[pos++];
            if (c == '\\' && pos < src.size()) {
                char e = src[pos++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 > src.size()) { err = "bad \\u escape"; return out; }
                        // For our purposes, just decode to UTF-8 bytes.
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = src[pos + i];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
                            else { err = "bad hex in \\u"; return out; }
                        }
                        pos += 4;
                        if (code < 0x80) {
                            out.push_back((char)code);
                        } else if (code < 0x800) {
                            out.push_back((char)(0xC0 | (code >> 6)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (code >> 12)));
                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default:
                        out.push_back(e);
                        break;
                }
            } else {
                out.push_back(c);
            }
        }
        if (pos >= src.size()) { err = "unterminated string"; return out; }
        ++pos; // closing quote
        return out;
    }

    std::unique_ptr<JsonValue> ParseString() {
        auto v = std::make_unique<JsonValue>();
        v->type = JsonType::String;
        v->s = ParseStringRaw();
        return v;
    }

    std::unique_ptr<JsonValue> ParseNumber() {
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') ++pos;
        while (pos < src.size() && (std::isdigit((unsigned char)src[pos]) ||
               src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
            ++pos;
        }
        std::string n = src.substr(start, pos - start);
        auto v = std::make_unique<JsonValue>();
        v->type = JsonType::Number;
        v->n = std::strtod(n.c_str(), nullptr);
        return v;
    }

    std::unique_ptr<JsonValue> ParseArray() {
        auto v = std::make_unique<JsonValue>();
        v->type = JsonType::Array;
        if (!Eat('[')) { err = "expected '['"; return nullptr; }
        SkipWs();
        if (Eat(']')) return v;
        while (true) {
            auto elem = ParseValue();
            if (!elem) return nullptr;
            v->a.push_back(std::move(elem));
            SkipWs();
            if (Eat(',')) continue;
            if (Eat(']')) return v;
            err = "expected ',' or ']'";
            return nullptr;
        }
    }

    std::unique_ptr<JsonValue> ParseLiteral(const char* lit, JsonType t, bool b) {
        size_t len = std::strlen(lit);
        if (src.compare(pos, len, lit) == 0) {
            pos += len;
            auto v = std::make_unique<JsonValue>();
            v->type = t;
            v->b = b;
            return v;
        }
        err = std::string("expected literal: ") + lit;
        return nullptr;
    }
};

std::unique_ptr<JsonValue> Parser::ParseValue() {
    SkipWs();
    if (pos >= src.size()) { err = "unexpected end"; return nullptr; }
    char c = src[pos];
    if (c == '{') return ParseObject();
    if (c == '[') return ParseArray();
    if (c == '"') return ParseString();
    if (c == 't') return ParseLiteral("true", JsonType::Bool, true);
    if (c == 'f') return ParseLiteral("false", JsonType::Bool, false);
    if (c == 'n') return ParseLiteral("null", JsonType::Null, false);
    if (c == '-' || std::isdigit((unsigned char)c)) return ParseNumber();
    err = "unexpected char";
    return nullptr;
}

} // anonymous namespace

std::unique_ptr<JsonValue> JsonParse(const std::string& text, std::string* err) {
    Parser p{text};
    auto v = p.ParseValue();
    if (err) *err = p.err;
    if (!v) return nullptr;
    p.SkipWs();
    if (p.pos != text.size()) {
        if (err) *err = "trailing data";
        return nullptr;
    }
    return v;
}

} // namespace rm