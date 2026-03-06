#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <algorithm>
#include <functional>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <locale>
#include <cctype>
#include <cstdlib>
#include <charconv>
#include <system_error>
#include <limits>
#include <utility>
#include <type_traits>

namespace fa {

    using StateId = std::uint32_t;

    // -------------------------
    // Utilities
    // -------------------------

    struct Error {
        bool ok{true};
        std::string message{};
        static Error success() { return {true, ""}; }
        static Error fail(std::string msg) { return {false, std::move(msg)}; }
    };

    // UI-friendly state metadata
    struct Position {
        float x{0.0f};
        float y{0.0f};
    };

    struct NodeSnapshot {
        StateId id{0};
        bool start{false};
        bool accepting{false};
        std::optional<std::string> label;
        std::optional<Position> position;
    };

    template <typename Symbol>
    struct EdgeBundle {
        StateId from{0};
        StateId to{0};
        std::vector<Symbol> symbols; // empty for epsilon edges in NFA snapshot
        bool epsilon{false};
    };

template <typename Symbol>
struct GraphSnapshot {
    std::vector<NodeSnapshot> nodes;
    std::vector<EdgeBundle<Symbol>> edges;
};

// -------------------------
// JSON wire format (GraphSnapshot)
// -------------------------

namespace json {

constexpr std::uint32_t kGraphSnapshotVersion = 1;

namespace detail {

template <typename T, typename = void>
struct has_ostream_op : std::false_type {};

template <typename T>
struct has_ostream_op<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_istream_op : std::false_type {};

template <typename T>
struct has_istream_op<T, std::void_t<decltype(std::declval<std::istream&>() >> std::declval<T&>())>>
    : std::true_type {};

template <typename T>
Error parse_integral(std::string_view in, T& out) {
    if (in.empty()) return Error::fail("SymbolCodec: expected integer");
    T value{};
    const char* first = in.data();
    const char* last = in.data() + in.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc() || res.ptr != last)
        return Error::fail("SymbolCodec: invalid integer");
    out = value;
    return Error::success();
}

template <typename T>
Error parse_floating(std::string_view in, T& out) {
    if (in.empty()) return Error::fail("SymbolCodec: expected number");
    std::string tmp(in);
    std::istringstream iss(tmp);
    iss.imbue(std::locale::classic());
    iss >> out;
    if (!iss) return Error::fail("SymbolCodec: invalid number");
    iss >> std::ws;
    if (!iss.eof()) return Error::fail("SymbolCodec: trailing characters");
    return Error::success();
}

template <typename T>
Error parse_streamable(std::string_view in, T& out) {
    std::string tmp(in);
    std::istringstream iss(tmp);
    iss.imbue(std::locale::classic());
    iss >> out;
    if (!iss) return Error::fail("SymbolCodec: failed to parse streamable type");
    iss >> std::ws;
    if (!iss.eof()) return Error::fail("SymbolCodec: trailing characters");
    return Error::success();
}

} // namespace detail

template <typename Symbol>
struct SymbolCodec {
    static Error to_string(const Symbol& s, std::string& out) {
        if constexpr (std::is_same_v<Symbol, bool>) {
            out = s ? "true" : "false";
            return Error::success();
        } else if constexpr (std::is_integral_v<Symbol> && !std::is_same_v<Symbol, bool>) {
            out = std::to_string(s);
            return Error::success();
        } else if constexpr (std::is_floating_point_v<Symbol>) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
            oss << std::setprecision(std::numeric_limits<Symbol>::max_digits10) << s;
            if (!oss) return Error::fail("SymbolCodec: failed to format floating point");
            out = oss.str();
            return Error::success();
        } else if constexpr (std::is_enum_v<Symbol>) {
            using U = std::underlying_type_t<Symbol>;
            out = std::to_string(static_cast<U>(s));
            return Error::success();
        } else if constexpr (detail::has_ostream_op<Symbol>::value) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << s;
            if (!oss) return Error::fail("SymbolCodec: failed to stream Symbol");
            out = oss.str();
            return Error::success();
        } else {
            return Error::fail("SymbolCodec: unsupported Symbol type; provide a specialization");
        }
    }

    static Error from_string(std::string_view in, Symbol& out) {
        if constexpr (std::is_same_v<Symbol, bool>) {
            if (in == "true" || in == "1") { out = true; return Error::success(); }
            if (in == "false" || in == "0") { out = false; return Error::success(); }
            return Error::fail("SymbolCodec: invalid boolean");
        } else if constexpr (std::is_integral_v<Symbol> && !std::is_same_v<Symbol, bool>) {
            return detail::parse_integral(in, out);
        } else if constexpr (std::is_floating_point_v<Symbol>) {
            return detail::parse_floating(in, out);
        } else if constexpr (std::is_enum_v<Symbol>) {
            using U = std::underlying_type_t<Symbol>;
            U v{};
            auto err = detail::parse_integral(in, v);
            if (!err.ok) return err;
            out = static_cast<Symbol>(v);
            return Error::success();
        } else if constexpr (detail::has_istream_op<Symbol>::value) {
            return detail::parse_streamable(in, out);
        } else {
            return Error::fail("SymbolCodec: unsupported Symbol type; provide a specialization");
        }
    }
};

template <>
struct SymbolCodec<char> {
    static Error to_string(const char& s, std::string& out) {
        out.assign(1, s);
        return Error::success();
    }
    static Error from_string(std::string_view in, char& out) {
        if (in.size() != 1) return Error::fail("SymbolCodec<char>: expected single-character string");
        out = in[0];
        return Error::success();
    }
};

template <>
struct SymbolCodec<std::string> {
    static Error to_string(const std::string& s, std::string& out) {
        out = s;
        return Error::success();
    }
    static Error from_string(std::string_view in, std::string& out) {
        out.assign(in.begin(), in.end());
        return Error::success();
    }
};

namespace detail {

inline void append_hex4(std::string& out, std::uint16_t v) {
    const char* hex = "0123456789abcdef";
    out.push_back(hex[(v >> 12) & 0xF]);
    out.push_back(hex[(v >> 8) & 0xF]);
    out.push_back(hex[(v >> 4) & 0xF]);
    out.push_back(hex[v & 0xF]);
}

inline void append_json_string(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += "\\u";
                append_hex4(out, static_cast<std::uint16_t>(c));
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

inline void append_float(std::string& out, double v) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    oss << std::setprecision(9) << v;
    out += oss.str();
}

inline void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class Reader {
public:
    explicit Reader(std::string_view s) : s_(s) {}

    void skip_ws() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
    }

    bool eof() const { return i_ >= s_.size(); }

    char peek() const { return eof() ? '\0' : s_[i_]; }

    bool consume(char c) {
        skip_ws();
        if (peek() == c) { ++i_; return true; }
        return false;
    }

    Error expect(char c, const char* msg) {
        skip_ws();
        if (peek() != c) return Error::fail(msg);
        ++i_;
        return Error::success();
    }

    Error parse_string(std::string& out) {
        skip_ws();
        if (peek() != '"') return Error::fail("JSON: expected string");
        ++i_;
        out.clear();
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return Error::success();
            if (static_cast<unsigned char>(c) < 0x20) return Error::fail("JSON: invalid control char in string");
            if (c == '\\') {
                if (i_ >= s_.size()) return Error::fail("JSON: invalid escape");
                char e = s_[i_++];
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
                    auto hex = [&](char h, std::uint32_t& v) -> bool {
                        if (h >= '0' && h <= '9') { v = h - '0'; return true; }
                        if (h >= 'a' && h <= 'f') { v = 10 + (h - 'a'); return true; }
                        if (h >= 'A' && h <= 'F') { v = 10 + (h - 'A'); return true; }
                        return false;
                    };
                    if (i_ + 4 > s_.size()) return Error::fail("JSON: invalid unicode escape");
                    std::uint32_t cp = 0;
                    for (int k = 0; k < 4; ++k) {
                        std::uint32_t v = 0;
                        if (!hex(s_[i_++], v)) return Error::fail("JSON: invalid unicode escape");
                        cp = (cp << 4) | v;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (i_ + 6 > s_.size() || s_[i_] != '\\' || s_[i_ + 1] != 'u')
                            return Error::fail("JSON: invalid surrogate pair");
                        i_ += 2;
                        std::uint32_t cp2 = 0;
                        for (int k = 0; k < 4; ++k) {
                            std::uint32_t v = 0;
                            if (!hex(s_[i_++], v)) return Error::fail("JSON: invalid unicode escape");
                            cp2 = (cp2 << 4) | v;
                        }
                        if (cp2 < 0xDC00 || cp2 > 0xDFFF) return Error::fail("JSON: invalid surrogate pair");
                        cp = 0x10000 + (((cp - 0xD800) << 10) | (cp2 - 0xDC00));
                    }
                    append_utf8(out, cp);
                    break;
                }
                default:
                    return Error::fail("JSON: invalid escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return Error::fail("JSON: unterminated string");
    }

    Error parse_bool(bool& out) {
        skip_ws();
        if (s_.substr(i_, 4) == "true") { i_ += 4; out = true; return Error::success(); }
        if (s_.substr(i_, 5) == "false") { i_ += 5; out = false; return Error::success(); }
        return Error::fail("JSON: expected boolean");
    }

    Error parse_null() {
        skip_ws();
        if (s_.substr(i_, 4) == "null") { i_ += 4; return Error::success(); }
        return Error::fail("JSON: expected null");
    }

    Error parse_uint64(std::uint64_t& out) {
        skip_ws();
        std::size_t start = i_;
        if (peek() == '-') return Error::fail("JSON: expected unsigned integer");
        if (peek() == '+') return Error::fail("JSON: invalid number");
        if (!std::isdigit(static_cast<unsigned char>(peek())))
            return Error::fail("JSON: expected number");
        std::uint64_t value = 0;
        while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
            std::uint64_t digit = static_cast<std::uint64_t>(s_[i_] - '0');
            if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
                return Error::fail("JSON: integer overflow");
            value = value * 10 + digit;
            ++i_;
        }
        if (i_ == start) return Error::fail("JSON: expected number");
        out = value;
        return Error::success();
    }

    Error parse_number(double& out) {
        skip_ws();
        std::size_t start = i_;
        if (peek() == '-') ++i_;
        if (peek() == '0') {
            ++i_;
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        } else {
            return Error::fail("JSON: expected number");
        }
        if (peek() == '.') {
            ++i_;
            if (!std::isdigit(static_cast<unsigned char>(peek())))
                return Error::fail("JSON: expected number");
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++i_;
            if (peek() == '+' || peek() == '-') ++i_;
            if (!std::isdigit(static_cast<unsigned char>(peek())))
                return Error::fail("JSON: expected number");
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        std::string tmp(s_.substr(start, i_ - start));
        char* endptr = nullptr;
        out = std::strtod(tmp.c_str(), &endptr);
        if (!endptr || *endptr != '\0') return Error::fail("JSON: invalid number");
        return Error::success();
    }

    Error skip_value() {
        skip_ws();
        char c = peek();
        if (c == '{') {
            ++i_;
            skip_ws();
            if (consume('}')) return Error::success();
            while (true) {
                std::string key;
                auto e = parse_string(key);
                if (!e.ok) return e;
                e = expect(':', "JSON: expected ':'");
                if (!e.ok) return e;
                e = skip_value();
                if (!e.ok) return e;
                skip_ws();
                if (consume('}')) return Error::success();
                e = expect(',', "JSON: expected ','");
                if (!e.ok) return e;
            }
        } else if (c == '[') {
            ++i_;
            skip_ws();
            if (consume(']')) return Error::success();
            while (true) {
                auto e = skip_value();
                if (!e.ok) return e;
                skip_ws();
                if (consume(']')) return Error::success();
                e = expect(',', "JSON: expected ','");
                if (!e.ok) return e;
            }
        } else if (c == '"') {
            std::string tmp;
            return parse_string(tmp);
        } else if (c == 't' || c == 'f') {
            bool tmp = false;
            return parse_bool(tmp);
        } else if (c == 'n') {
            return parse_null();
        } else {
            double tmp = 0.0;
            return parse_number(tmp);
        }
    }

private:
    std::string_view s_;
    std::size_t i_{0};
};

} // namespace detail

template <typename Symbol>
Error serialize(const GraphSnapshot<Symbol>& snap, std::string& out) {
    out.clear();
    out += "{";
    out += "\"version\":";
    out += std::to_string(kGraphSnapshotVersion);
    out += ",\"nodes\":[";

    for (std::size_t i = 0; i < snap.nodes.size(); ++i) {
        const auto& n = snap.nodes[i];
        if (i > 0) out += ",";
        out += "{";
        out += "\"id\":";
        out += std::to_string(n.id);
        out += ",\"start\":";
        out += (n.start ? "true" : "false");
        out += ",\"accepting\":";
        out += (n.accepting ? "true" : "false");
        if (n.label.has_value()) {
            out += ",\"label\":";
            detail::append_json_string(out, *n.label);
        }
        if (n.position.has_value()) {
            out += ",\"position\":{";
            out += "\"x\":";
            detail::append_float(out, n.position->x);
            out += ",\"y\":";
            detail::append_float(out, n.position->y);
            out += "}";
        }
        out += "}";
    }

    out += "],\"edges\":[";
    for (std::size_t i = 0; i < snap.edges.size(); ++i) {
        const auto& e = snap.edges[i];
        if (i > 0) out += ",";
        out += "{";
        out += "\"from\":";
        out += std::to_string(e.from);
        out += ",\"to\":";
        out += std::to_string(e.to);
        out += ",\"epsilon\":";
        out += (e.epsilon ? "true" : "false");
        out += ",\"symbols\":[";
        for (std::size_t j = 0; j < e.symbols.size(); ++j) {
            if (j > 0) out += ",";
            std::string sym_str;
            auto err = SymbolCodec<Symbol>::to_string(e.symbols[j], sym_str);
            if (!err.ok) return err;
            detail::append_json_string(out, sym_str);
        }
        out += "]";
        out += "}";
    }
    out += "]}";
    return Error::success();
}

template <typename Symbol>
Error deserialize(std::string_view json_text, GraphSnapshot<Symbol>& out) {
    out = GraphSnapshot<Symbol>{};
    detail::Reader r(json_text);
    auto err = r.expect('{', "JSON: expected '{'");
    if (!err.ok) return err;

    bool seen_nodes = false;
    bool seen_edges = false;
    bool seen_version = false;

    while (true) {
        r.skip_ws();
        if (r.consume('}')) break;
        std::string key;
        err = r.parse_string(key);
        if (!err.ok) return err;
        err = r.expect(':', "JSON: expected ':'");
        if (!err.ok) return err;

        if (key == "version") {
            std::uint64_t v = 0;
            err = r.parse_uint64(v);
            if (!err.ok) return err;
            if (v != kGraphSnapshotVersion)
                return Error::fail("JSON: unsupported GraphSnapshot version");
            seen_version = true;
        } else if (key == "nodes") {
            err = r.expect('[', "JSON: expected '['");
            if (!err.ok) return err;
            r.skip_ws();
            if (!r.consume(']')) {
                while (true) {
                    err = r.expect('{', "JSON: expected '{' for node");
                    if (!err.ok) return err;
                    NodeSnapshot n;
                    bool seen_id = false;
                    while (true) {
                        r.skip_ws();
                        if (r.consume('}')) break;
                        std::string nk;
                        err = r.parse_string(nk);
                        if (!err.ok) return err;
                        err = r.expect(':', "JSON: expected ':'");
                        if (!err.ok) return err;
                        if (nk == "id") {
                            std::uint64_t id = 0;
                            err = r.parse_uint64(id);
                            if (!err.ok) return err;
                            if (id > std::numeric_limits<StateId>::max())
                                return Error::fail("JSON: node id out of range");
                            n.id = static_cast<StateId>(id);
                            seen_id = true;
                        } else if (nk == "start") {
                            err = r.parse_bool(n.start);
                            if (!err.ok) return err;
                        } else if (nk == "accepting") {
                            err = r.parse_bool(n.accepting);
                            if (!err.ok) return err;
                        } else if (nk == "label") {
                            std::string label;
                            err = r.parse_string(label);
                            if (!err.ok) return err;
                            n.label = label;
                        } else if (nk == "position") {
                            err = r.expect('{', "JSON: expected '{' for position");
                            if (!err.ok) return err;
                            bool has_x = false;
                            bool has_y = false;
                            Position p;
                            while (true) {
                                r.skip_ws();
                                if (r.consume('}')) break;
                                std::string pk;
                                err = r.parse_string(pk);
                                if (!err.ok) return err;
                                err = r.expect(':', "JSON: expected ':'");
                                if (!err.ok) return err;
                                if (pk == "x") {
                                    double v = 0.0;
                                    err = r.parse_number(v);
                                    if (!err.ok) return err;
                                    p.x = static_cast<float>(v);
                                    has_x = true;
                                } else if (pk == "y") {
                                    double v = 0.0;
                                    err = r.parse_number(v);
                                    if (!err.ok) return err;
                                    p.y = static_cast<float>(v);
                                    has_y = true;
                                } else {
                                    err = r.skip_value();
                                    if (!err.ok) return err;
                                }
                                r.skip_ws();
                                if (r.consume('}')) break;
                                err = r.expect(',', "JSON: expected ','");
                                if (!err.ok) return err;
                            }
                            if (!has_x || !has_y)
                                return Error::fail("JSON: position requires x and y");
                            n.position = p;
                        } else {
                            err = r.skip_value();
                            if (!err.ok) return err;
                        }
                        r.skip_ws();
                        if (r.consume('}')) break;
                        err = r.expect(',', "JSON: expected ','");
                        if (!err.ok) return err;
                    }
                    if (!seen_id) return Error::fail("JSON: node missing id");
                    out.nodes.push_back(std::move(n));
                    r.skip_ws();
                    if (r.consume(']')) break;
                    err = r.expect(',', "JSON: expected ','");
                    if (!err.ok) return err;
                }
            }
            seen_nodes = true;
        } else if (key == "edges") {
            err = r.expect('[', "JSON: expected '['");
            if (!err.ok) return err;
            r.skip_ws();
            if (!r.consume(']')) {
                while (true) {
                    err = r.expect('{', "JSON: expected '{' for edge");
                    if (!err.ok) return err;
                    EdgeBundle<Symbol> e;
                    bool seen_from = false;
                    bool seen_to = false;
                    while (true) {
                        r.skip_ws();
                        if (r.consume('}')) break;
                        std::string ek;
                        err = r.parse_string(ek);
                        if (!err.ok) return err;
                        err = r.expect(':', "JSON: expected ':'");
                        if (!err.ok) return err;
                        if (ek == "from") {
                            std::uint64_t id = 0;
                            err = r.parse_uint64(id);
                            if (!err.ok) return err;
                            if (id > std::numeric_limits<StateId>::max())
                                return Error::fail("JSON: edge from out of range");
                            e.from = static_cast<StateId>(id);
                            seen_from = true;
                        } else if (ek == "to") {
                            std::uint64_t id = 0;
                            err = r.parse_uint64(id);
                            if (!err.ok) return err;
                            if (id > std::numeric_limits<StateId>::max())
                                return Error::fail("JSON: edge to out of range");
                            e.to = static_cast<StateId>(id);
                            seen_to = true;
                        } else if (ek == "epsilon") {
                            err = r.parse_bool(e.epsilon);
                            if (!err.ok) return err;
                        } else if (ek == "symbols") {
                            err = r.expect('[', "JSON: expected '['");
                            if (!err.ok) return err;
                            r.skip_ws();
                            if (!r.consume(']')) {
                                while (true) {
                                    std::string sym;
                                    err = r.parse_string(sym);
                                    if (!err.ok) return err;
                                    Symbol decoded{};
                                    err = SymbolCodec<Symbol>::from_string(sym, decoded);
                                    if (!err.ok) return err;
                                    e.symbols.push_back(std::move(decoded));
                                    r.skip_ws();
                                    if (r.consume(']')) break;
                                    err = r.expect(',', "JSON: expected ','");
                                    if (!err.ok) return err;
                                }
                            }
                        } else {
                            err = r.skip_value();
                            if (!err.ok) return err;
                        }
                        r.skip_ws();
                        if (r.consume('}')) break;
                        err = r.expect(',', "JSON: expected ','");
                        if (!err.ok) return err;
                    }
                    if (!seen_from || !seen_to) return Error::fail("JSON: edge missing from/to");
                    out.edges.push_back(std::move(e));
                    r.skip_ws();
                    if (r.consume(']')) break;
                    err = r.expect(',', "JSON: expected ','");
                    if (!err.ok) return err;
                }
            }
            seen_edges = true;
        } else {
            err = r.skip_value();
            if (!err.ok) return err;
        }

        r.skip_ws();
        if (r.consume('}')) break;
        err = r.expect(',', "JSON: expected ','");
        if (!err.ok) return err;
    }

    if (!seen_nodes || !seen_edges)
        return Error::fail("JSON: missing nodes or edges");
    if (!seen_version) {
        // Allow missing version for backward compatibility with draft exports
    }
    return Error::success();
}

} // namespace json

// A stable, hashable set of states (sorted unique vector).
// Good for subset construction keys and deterministic behavior.
struct StateSet {
        std::vector<StateId> v;

        StateSet() = default;
        explicit StateSet(std::vector<StateId> states) : v(std::move(states)) {
            normalize();
        }

        void normalize() {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        }

        bool empty() const { return v.empty(); }
        std::size_t size() const { return v.size(); }

        bool operator==(const StateSet& other) const { return v == other.v; }
    };

    struct StateSetHash {
        std::size_t operator()(const StateSet& s) const noexcept {
            // FNV-1a-ish
            std::size_t h = 1469598103934665603ull;
            for (auto x : s.v) {
                h ^= static_cast<std::size_t>(x) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
            }
            return h;
        }
    };

    template <typename T>
    inline void append_unique_sorted(std::vector<T>& out, const std::vector<T>& add) {
        out.insert(out.end(), add.begin(), add.end());
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    template <typename T, typename = void>
    struct is_less_comparable : std::false_type {};

    template <typename T>
    struct is_less_comparable<T, std::void_t<decltype(std::declval<T>() < std::declval<T>())>>
        : std::true_type {};

    template <typename T>
    inline void sort_symbols(std::vector<T>& v) {
        if (v.empty()) return;
        if constexpr (is_less_comparable<T>::value) {
            std::sort(v.begin(), v.end());
        } else {
            std::sort(v.begin(), v.end(), [](const T& a, const T& b) {
                return std::hash<T>{}(a) < std::hash<T>{}(b);
            });
        }
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    // -------------------------
    // DFA
    // -------------------------

    template <typename Symbol>
    class DFA {
    public:
        using symbol_type = Symbol;

        struct RunResult {
            bool accepted{false};
            bool complete{true};               // false if transition missing (dead)
            std::size_t consumed{0};           // number of symbols consumed
            std::optional<StateId> end_state;  // empty if dead
        };

        struct TraceStep {
            std::size_t index;
            Symbol symbol;
            StateId from;
            std::optional<StateId> to;
            bool took;
            bool accepting_after;
        };

        struct Trace {
            bool accepted{false};
            bool complete{true};
            std::size_t consumed{0};
            std::optional<StateId> end_state;
            std::vector<TraceStep> steps;
        };

        // ---- Definition ----
        Error set_start(StateId s) {
            states_.insert(s);
            start_ = s;
            has_start_ = true;
            return Error::success();
        }

        void clear_start() {
            has_start_ = false;
            start_.reset();
        }

        void add_state(StateId s) { states_.insert(s); }

        void set_accepting(StateId s, bool accepting = true) {
            states_.insert(s);
            if (accepting) accepting_.insert(s);
            else accepting_.erase(s);
        }

        bool has_state(StateId s) const { return states_.find(s) != states_.end(); }
        bool is_accepting(StateId s) const { return accepting_.find(s) != accepting_.end(); }

        // delta(from, sym) = to
        void set_transition(StateId from, const Symbol& sym, StateId to) {
            states_.insert(from);
            states_.insert(to);
            delta_[Key{from, sym}] = to;
        }

        // Remove transition if present
        void clear_transition(StateId from, const Symbol& sym) {
            delta_.erase(Key{from, sym});
        }

        bool remove_transition(StateId from, const Symbol& sym) {
            return delta_.erase(Key{from, sym}) > 0;
        }

        bool has_transition(StateId from, const Symbol& sym) const {
            return delta_.find(Key{from, sym}) != delta_.end();
        }

        std::optional<StateId> transition(StateId from, const Symbol& sym) const {
            auto it = delta_.find(Key{from, sym});
            if (it == delta_.end()) return std::nullopt;
            return it->second;
        }

        bool remove_state(StateId s) {
            if (states_.erase(s) == 0) return false;
            accepting_.erase(s);
            labels_.erase(s);
            positions_.erase(s);
            if (has_start_ && start_ && *start_ == s) {
                has_start_ = false;
                start_.reset();
            }
            for (auto it = delta_.begin(); it != delta_.end(); ) {
                if (it->first.from == s || it->second == s) it = delta_.erase(it);
                else ++it;
            }
            return true;
        }

        void clear() {
            states_.clear();
            accepting_.clear();
            delta_.clear();
            labels_.clear();
            positions_.clear();
            has_start_ = false;
            start_.reset();
        }

        // Ensure total DFA over an alphabet by adding/filling missing transitions to sink.
        // Returns error if no start state set.
        Error make_total(StateId sink, const std::vector<Symbol>& alphabet) {
            if (!has_start_) return Error::fail("DFA: start state not set");
            add_state(sink);
            // sink loops
            for (const auto& a : alphabet) set_transition(sink, a, sink);

            // Copy states to iterate stable snapshot
            std::vector<StateId> all(states_.begin(), states_.end());
            for (auto s : all) {
                for (const auto& a : alphabet) {
                    if (!has_transition(s, a)) set_transition(s, a, sink);
                }
            }
            return Error::success();
        }

        // Validate: start set, all accepting exist, transitions point to known states
        Error validate() const {
            if (!has_start_) return Error::fail("DFA: start state not set");
            if (states_.find(*start_) == states_.end())
                return Error::fail("DFA: start state missing in states set (internal inconsistency)");

            for (auto acc : accepting_) {
                if (states_.find(acc) == states_.end())
                    return Error::fail("DFA: accepting state not present in states set");
            }

            for (const auto& kv : delta_) {
                auto from = kv.first.from;
                auto to = kv.second;
                if (states_.find(from) == states_.end())
                    return Error::fail("DFA: transition from unknown state");
                if (states_.find(to) == states_.end())
                    return Error::fail("DFA: transition to unknown state");
            }

            return Error::success();
        }

        // ---- Execution ----
        template <typename It>
        RunResult run(It begin, It end) const {
            RunResult r{};
            if (!has_start_) {
                r.complete = false;
                r.accepted = false;
                r.end_state.reset();
                return r;
            }

            StateId cur = *start_;
            std::size_t i = 0;

            for (auto it = begin; it != end; ++it, ++i) {
                auto nxt = transition(cur, *it);
                if (!nxt.has_value()) {
                    r.complete = false;
                    r.consumed = i;
                    r.end_state.reset();
                    r.accepted = false;
                    return r;
                }
                cur = *nxt;
            }

            r.complete = true;
            r.consumed = i;
            r.end_state = cur;
            r.accepted = (accepting_.find(cur) != accepting_.end());
            return r;
        }

        RunResult run(const std::basic_string<Symbol>& input) const {
            return run(input.begin(), input.end());
        }

        template <typename It>
        Trace run_trace(It begin, It end) const {
            Trace t{};
            if (!has_start_) {
                t.complete = false;
                t.accepted = false;
                t.end_state.reset();
                return t;
            }

            StateId cur = *start_;
            std::size_t i = 0;

            for (auto it = begin; it != end; ++it, ++i) {
                auto nxt = transition(cur, *it);
                TraceStep step{i, *it, cur, nxt, nxt.has_value(), false};

                if (!nxt.has_value()) {
                    t.complete = false;
                    t.consumed = i;
                    t.end_state.reset();
                    t.accepted = false;
                    t.steps.push_back(std::move(step));
                    return t;
                }

                cur = *nxt;
                step.accepting_after = (accepting_.find(cur) != accepting_.end());
                t.steps.push_back(std::move(step));
            }

            t.complete = true;
            t.consumed = i;
            t.end_state = cur;
            t.accepted = (accepting_.find(cur) != accepting_.end());
            return t;
        }

        Trace run_trace(const std::basic_string<Symbol>& input) const {
            return run_trace(input.begin(), input.end());
        }

        // ---- Queries / metadata ----
        std::optional<StateId> start_state() const { return start_; }
        const std::unordered_set<StateId>& states() const { return states_; }
        const std::unordered_set<StateId>& accepting_states() const { return accepting_; }

        std::vector<StateId> states_sorted() const {
            std::vector<StateId> out(states_.begin(), states_.end());
            if (has_start_ && start_ && !states_.count(*start_)) out.push_back(*start_);
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return out;
        }

        std::vector<Symbol> alphabet() const {
            std::vector<Symbol> out;
            out.reserve(delta_.size());
            for (const auto& kv : delta_) out.push_back(kv.first.sym);
            sort_symbols(out);
            return out;
        }

        std::size_t transition_count() const { return delta_.size(); }

        std::optional<StateId> next_state_id() const {
            bool has = false;
            StateId max_id = 0;
            for (auto s : states_) {
                if (!has || s > max_id) max_id = s;
                has = true;
            }
            if (has_start_ && start_) {
                if (!has || *start_ > max_id) max_id = *start_;
                has = true;
            }
            if (!has) return StateId{0};
            if (max_id == std::numeric_limits<StateId>::max()) return std::nullopt;
            return max_id + 1;
        }

        // Optional labels (production convenience)
        void set_label(StateId s, std::string label) {
            states_.insert(s);
            labels_[s] = std::move(label);
        }
        std::optional<std::string> label(StateId s) const {
            auto it = labels_.find(s);
            if (it == labels_.end()) return std::nullopt;
            return it->second;
        }

        void set_position(StateId s, Position p) {
            states_.insert(s);
            positions_[s] = p;
        }

        void clear_position(StateId s) { positions_.erase(s); }

        std::optional<Position> position(StateId s) const {
            auto it = positions_.find(s);
            if (it == positions_.end()) return std::nullopt;
            return it->second;
        }

        GraphSnapshot<Symbol> snapshot() const {
            GraphSnapshot<Symbol> snap;
            auto ids = states_sorted();
            snap.nodes.reserve(ids.size());
            for (auto id : ids) {
                NodeSnapshot n;
                n.id = id;
                n.start = (has_start_ && start_ && *start_ == id);
                n.accepting = (accepting_.find(id) != accepting_.end());
                auto it_label = labels_.find(id);
                if (it_label != labels_.end()) n.label = it_label->second;
                auto it_pos = positions_.find(id);
                if (it_pos != positions_.end()) n.position = it_pos->second;
                snap.nodes.push_back(std::move(n));
            }

            struct EdgeKey {
                StateId from;
                StateId to;
                bool operator==(const EdgeKey& o) const { return from == o.from && to == o.to; }
            };
            struct EdgeKeyHash {
                std::size_t operator()(const EdgeKey& k) const noexcept {
                    std::size_t h1 = std::hash<StateId>{}(k.from);
                    std::size_t h2 = std::hash<StateId>{}(k.to);
                    return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1<<6) + (h1>>2));
                }
            };

            std::unordered_map<EdgeKey, std::vector<Symbol>, EdgeKeyHash> grouped;
            grouped.reserve(delta_.size());
            for (const auto& kv : delta_) {
                grouped[EdgeKey{kv.first.from, kv.second}].push_back(kv.first.sym);
            }

            snap.edges.reserve(grouped.size());
            for (auto& kv : grouped) {
                EdgeBundle<Symbol> e;
                e.from = kv.first.from;
                e.to = kv.first.to;
                e.symbols = std::move(kv.second);
                sort_symbols(e.symbols);
                e.epsilon = false;
                snap.edges.push_back(std::move(e));
            }

            std::sort(snap.edges.begin(), snap.edges.end(),
                    [](const EdgeBundle<Symbol>& a, const EdgeBundle<Symbol>& b) {
                        if (a.from != b.from) return a.from < b.from;
                        return a.to < b.to;
                    });

            return snap;
        }

        // Hopcroft minimization (requires alphabet; recommended to call make_total first).
        // Returns a minimized DFA with remapped state IDs starting at 0.
        // If DFA is not total, behavior is still defined but may reduce less effectively.
        DFA minimize(const std::vector<Symbol>& alphabet) const {
            // Collect reachable states from start (avoid minimizing unreachable)
            std::unordered_set<StateId> reachable;
            if (has_start_) {
                std::vector<StateId> stack{*start_};
                reachable.insert(*start_);
                while (!stack.empty()) {
                    StateId s = stack.back();
                    stack.pop_back();
                    for (const auto& a : alphabet) {
                        auto t = transition(s, a);
                        if (t && reachable.insert(*t).second) stack.push_back(*t);
                    }
                }
            }

            std::vector<StateId> R(reachable.begin(), reachable.end());
            std::sort(R.begin(), R.end());
            if (R.empty()) return DFA{};

            // Initial partition: accepting vs non-accepting
            std::vector<StateId> A, N;
            A.reserve(R.size()); N.reserve(R.size());
            for (auto s : R) {
                if (accepting_.count(s)) A.push_back(s);
                else N.push_back(s);
            }

            // Partitions stored as vectors (sorted)
            std::vector<std::vector<StateId>> P;
            if (!A.empty()) P.push_back(A);
            if (!N.empty()) P.push_back(N);

            // Worklist: indices into P
            std::vector<std::size_t> W;
            if (!A.empty()) W.push_back(0);
            else if (!N.empty()) W.push_back(0);

            // Precompute inverse transitions for each symbol: inv[a][t] = { s | delta(s,a)=t }
            // Use unordered_map per symbol for sparsity
            std::vector<std::unordered_map<StateId, std::vector<StateId>>> inv(alphabet.size());
            for (std::size_t ai = 0; ai < alphabet.size(); ++ai) {
                const auto& a = alphabet[ai];
                for (auto s : R) {
                    auto t = transition(s, a);
                    if (!t) continue;
                    inv[ai][*t].push_back(s);
                }
                for (auto& kv : inv[ai]) {
                    std::sort(kv.second.begin(), kv.second.end());
                }
            }

            while (!W.empty()) {
                std::size_t Aidx = W.back();
                W.pop_back();
                if (Aidx >= P.size()) continue;

                const auto& splitter = P[Aidx];

                for (std::size_t ai = 0; ai < alphabet.size(); ++ai) {
                    // X = preimage of splitter under symbol a
                    std::vector<StateId> X;
                    for (auto t : splitter) {
                        auto it = inv[ai].find(t);
                        if (it != inv[ai].end()) append_unique_sorted(X, it->second);
                    }
                    if (X.empty()) continue;

                    // Refine each Y in P into (Y intersect X) and (Y \\ X) if both non-empty
                    std::size_t psize = P.size();
                    for (std::size_t Yidx = 0; Yidx < psize; ++Yidx) {
                        const auto& Y = P[Yidx];
                        std::vector<StateId> Y1, Y2;
                        Y1.reserve(Y.size());
                        Y2.reserve(Y.size());
                        for (auto s : Y) {
                            if (std::binary_search(X.begin(), X.end(), s)) Y1.push_back(s);
                            else Y2.push_back(s);
                        }
                        if (Y1.empty() || Y2.empty()) continue;

                        // Replace Y with Y1, append Y2
                        P[Yidx] = std::move(Y1);
                        P.push_back(std::move(Y2));

                        // Update worklist: if Y was in W, replace with both; else add smaller
                        bool was_in_W = false;
                        for (auto w : W) {
                            if (w == Yidx) { was_in_W = true; break; }
                        }
                        if (was_in_W) {
                            W.push_back(P.size() - 1);
                        } else {
                            // add smaller partition
                            if (P[Yidx].size() <= P.back().size()) W.push_back(Yidx);
                            else W.push_back(P.size() - 1);
                        }
                    }
                }
            }

            // Build minimized DFA: map old state -> class id
            std::unordered_map<StateId, StateId> cls;
            cls.reserve(R.size());
            for (StateId cid = 0; cid < static_cast<StateId>(P.size()); ++cid) {
                auto part = P[cid];
                std::sort(part.begin(), part.end());
                for (auto s : part) cls[s] = cid;
            }

            DFA out;
            // Start
            out.set_start(cls.at(*start_));

            // States + accepting
            for (StateId cid = 0; cid < static_cast<StateId>(P.size()); ++cid) {
                out.add_state(cid);
                bool acc = false;
                for (auto s : P[cid]) { if (accepting_.count(s)) { acc = true; break; } }
                out.set_accepting(cid, acc);
            }

            // Transitions: representative per class (take first)
            for (StateId cid = 0; cid < static_cast<StateId>(P.size()); ++cid) {
                StateId rep = P[cid].front();
                for (const auto& a : alphabet) {
                    auto t = transition(rep, a);
                    if (t && cls.find(*t) != cls.end()) {
                        out.set_transition(cid, a, cls.at(*t));
                    }
                }
            }

            return out;
        }

    private:
        struct Key {
            StateId from;
            Symbol sym;
            bool operator==(const Key& o) const { return from == o.from && sym == o.sym; }
        };
        struct KeyHash {
            std::size_t operator()(const Key& k) const noexcept {
                std::size_t h1 = std::hash<StateId>{}(k.from);
                std::size_t h2 = std::hash<Symbol>{}(k.sym);
                return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1<<6) + (h1>>2));
            }
        };

        std::unordered_set<StateId> states_;
        std::unordered_set<StateId> accepting_;
        std::unordered_map<Key, StateId, KeyHash> delta_;

        std::unordered_map<StateId, std::string> labels_;
        std::unordered_map<StateId, Position> positions_;

        bool has_start_{false};
        std::optional<StateId> start_;
    };

    // -------------------------
    // NFA with epsilon transitions (epsilon-NFA)
    // -------------------------

    template <typename Symbol>
    class NFA {
    public:
        using symbol_type = Symbol;

        struct RunResult {
            bool accepted{false};
            std::size_t consumed{0};
            StateSet end_states; // all possible states after processing
        };

        struct TraceStep {
            std::size_t index;
            Symbol symbol;
            StateSet before;
            StateSet after;
        };

        struct Trace {
            bool accepted{false};
            std::size_t consumed{0};
            StateSet end_states;
            std::vector<TraceStep> steps;
        };

        Error set_start(StateId s) {
            states_.insert(s);
            start_ = s;
            has_start_ = true;
            return Error::success();
        }

        void add_state(StateId s) { states_.insert(s); }

        void set_accepting(StateId s, bool accepting = true) {
            states_.insert(s);
            if (accepting) accepting_.insert(s);
            else accepting_.erase(s);
        }

        // Add transition on symbol: from --sym--> to
        void add_transition(StateId from, const Symbol& sym, StateId to) {
            states_.insert(from);
            states_.insert(to);
            auto& vec = delta_[Key{from, sym}];
            if (std::find(vec.begin(), vec.end(), to) == vec.end()) vec.push_back(to);
        }

        // Add epsilon transition: from --epsilon--> to
        void add_epsilon(StateId from, StateId to) {
            states_.insert(from);
            states_.insert(to);
            auto& vec = eps_[from];
            if (std::find(vec.begin(), vec.end(), to) == vec.end()) vec.push_back(to);
        }

        bool remove_transition(StateId from, const Symbol& sym, StateId to) {
            auto it = delta_.find(Key{from, sym});
            if (it == delta_.end()) return false;
            auto& vec = it->second;
            auto vit = std::remove(vec.begin(), vec.end(), to);
            if (vit == vec.end()) return false;
            vec.erase(vit, vec.end());
            if (vec.empty()) delta_.erase(it);
            return true;
        }

        bool remove_epsilon(StateId from, StateId to) {
            auto it = eps_.find(from);
            if (it == eps_.end()) return false;
            auto& vec = it->second;
            auto vit = std::remove(vec.begin(), vec.end(), to);
            if (vit == vec.end()) return false;
            vec.erase(vit, vec.end());
            if (vec.empty()) eps_.erase(it);
            return true;
        }

        bool remove_state(StateId s) {
            if (states_.erase(s) == 0) return false;
            accepting_.erase(s);
            labels_.erase(s);
            positions_.erase(s);
            if (has_start_ && start_ && *start_ == s) {
                has_start_ = false;
                start_.reset();
            }

            for (auto it = delta_.begin(); it != delta_.end(); ) {
                if (it->first.from == s) {
                    it = delta_.erase(it);
                    continue;
                }
                auto& vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), s), vec.end());
                if (vec.empty()) it = delta_.erase(it);
                else ++it;
            }

            for (auto it = eps_.begin(); it != eps_.end(); ) {
                if (it->first == s) {
                    it = eps_.erase(it);
                    continue;
                }
                auto& vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), s), vec.end());
                if (vec.empty()) it = eps_.erase(it);
                else ++it;
            }

            return true;
        }

        void clear() {
            states_.clear();
            accepting_.clear();
            delta_.clear();
            eps_.clear();
            labels_.clear();
            positions_.clear();
            has_start_ = false;
            start_.reset();
        }

        Error validate() const {
            if (!has_start_) return Error::fail("NFA: start state not set");
            if (states_.find(*start_) == states_.end())
                return Error::fail("NFA: start state missing in states set (internal inconsistency)");
            for (auto acc : accepting_) {
                if (states_.find(acc) == states_.end())
                    return Error::fail("NFA: accepting state not present in states set");
            }
            // transitions point to known states
            for (const auto& kv : delta_) {
                if (!states_.count(kv.first.from)) return Error::fail("NFA: transition from unknown state");
                for (auto t : kv.second) if (!states_.count(t)) return Error::fail("NFA: transition to unknown state");
            }
            for (const auto& kv : eps_) {
                if (!states_.count(kv.first)) return Error::fail("NFA: epsilon from unknown state");
                for (auto t : kv.second) if (!states_.count(t)) return Error::fail("NFA: epsilon to unknown state");
            }
            return Error::success();
        }

        // ---- Execution ----
        template <typename It>
        RunResult run(It begin, It end) const {
            RunResult r{};
            if (!has_start_) return r;

            StateSet cur = epsilon_closure(StateSet{{*start_}});

            std::size_t i = 0;
            for (auto it = begin; it != end; ++it, ++i) {
                StateSet next = move(cur, *it);
                next = epsilon_closure(next);
                cur = std::move(next);
            }

            r.consumed = i;
            r.end_states = cur;
            r.accepted = any_accepting(cur);
            return r;
        }

        RunResult run(const std::basic_string<Symbol>& input) const {
            return run(input.begin(), input.end());
        }

        template <typename It>
        Trace run_trace(It begin, It end) const {
            Trace t{};
            if (!has_start_) return t;

            StateSet cur = epsilon_closure(StateSet{{*start_}});

            std::size_t i = 0;
            for (auto it = begin; it != end; ++it, ++i) {
                StateSet next = move(cur, *it);
                next = epsilon_closure(next);
                t.steps.push_back(TraceStep{i, *it, cur, next});
                cur = std::move(next);
            }

            t.consumed = i;
            t.end_states = cur;
            t.accepted = any_accepting(cur);
            return t;
        }

        Trace run_trace(const std::basic_string<Symbol>& input) const {
            return run_trace(input.begin(), input.end());
        }

        // ---- Queries / metadata ----
        std::optional<StateId> start_state() const { return start_; }
        const std::unordered_set<StateId>& states() const { return states_; }
        const std::unordered_set<StateId>& accepting_states() const { return accepting_; }

        std::vector<StateId> states_sorted() const {
            std::vector<StateId> out(states_.begin(), states_.end());
            if (has_start_ && start_ && !states_.count(*start_)) out.push_back(*start_);
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return out;
        }

        std::vector<Symbol> alphabet() const {
            std::vector<Symbol> out;
            out.reserve(delta_.size());
            for (const auto& kv : delta_) out.push_back(kv.first.sym);
            sort_symbols(out);
            return out;
        }

        std::size_t symbol_transition_count() const {
            std::size_t count = 0;
            for (const auto& kv : delta_) count += kv.second.size();
            return count;
        }

        std::size_t epsilon_transition_count() const {
            std::size_t count = 0;
            for (const auto& kv : eps_) count += kv.second.size();
            return count;
        }

        std::size_t transition_count() const {
            return symbol_transition_count() + epsilon_transition_count();
        }

        std::optional<StateId> next_state_id() const {
            bool has = false;
            StateId max_id = 0;
            for (auto s : states_) {
                if (!has || s > max_id) max_id = s;
                has = true;
            }
            if (has_start_ && start_) {
                if (!has || *start_ > max_id) max_id = *start_;
                has = true;
            }
            if (!has) return StateId{0};
            if (max_id == std::numeric_limits<StateId>::max()) return std::nullopt;
            return max_id + 1;
        }

        void set_label(StateId s, std::string label) {
            states_.insert(s);
            labels_[s] = std::move(label);
        }

        std::optional<std::string> label(StateId s) const {
            auto it = labels_.find(s);
            if (it == labels_.end()) return std::nullopt;
            return it->second;
        }

        void set_position(StateId s, Position p) {
            states_.insert(s);
            positions_[s] = p;
        }

        void clear_position(StateId s) { positions_.erase(s); }

        std::optional<Position> position(StateId s) const {
            auto it = positions_.find(s);
            if (it == positions_.end()) return std::nullopt;
            return it->second;
        }

        GraphSnapshot<Symbol> snapshot() const {
            GraphSnapshot<Symbol> snap;
            auto ids = states_sorted();
            snap.nodes.reserve(ids.size());
            for (auto id : ids) {
                NodeSnapshot n;
                n.id = id;
                n.start = (has_start_ && start_ && *start_ == id);
                n.accepting = (accepting_.find(id) != accepting_.end());
                auto it_label = labels_.find(id);
                if (it_label != labels_.end()) n.label = it_label->second;
                auto it_pos = positions_.find(id);
                if (it_pos != positions_.end()) n.position = it_pos->second;
                snap.nodes.push_back(std::move(n));
            }

            struct EdgeKey {
                StateId from;
                StateId to;
                bool operator==(const EdgeKey& o) const { return from == o.from && to == o.to; }
            };
            struct EdgeKeyHash {
                std::size_t operator()(const EdgeKey& k) const noexcept {
                    std::size_t h1 = std::hash<StateId>{}(k.from);
                    std::size_t h2 = std::hash<StateId>{}(k.to);
                    return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1<<6) + (h1>>2));
                }
            };

            std::unordered_map<EdgeKey, std::vector<Symbol>, EdgeKeyHash> grouped;
            for (const auto& kv : delta_) {
                for (auto to : kv.second) {
                    grouped[EdgeKey{kv.first.from, to}].push_back(kv.first.sym);
                }
            }

            std::unordered_map<EdgeKey, bool, EdgeKeyHash> eps_grouped;
            for (const auto& kv : eps_) {
                for (auto to : kv.second) {
                    eps_grouped[EdgeKey{kv.first, to}] = true;
                }
            }

            snap.edges.reserve(grouped.size() + eps_grouped.size());
            for (auto& kv : grouped) {
                EdgeBundle<Symbol> e;
                e.from = kv.first.from;
                e.to = kv.first.to;
                e.symbols = std::move(kv.second);
                sort_symbols(e.symbols);
                e.epsilon = false;
                snap.edges.push_back(std::move(e));
            }

            for (const auto& kv : eps_grouped) {
                EdgeBundle<Symbol> e;
                e.from = kv.first.from;
                e.to = kv.first.to;
                e.epsilon = true;
                snap.edges.push_back(std::move(e));
            }

            std::sort(snap.edges.begin(), snap.edges.end(),
                    [](const EdgeBundle<Symbol>& a, const EdgeBundle<Symbol>& b) {
                        if (a.from != b.from) return a.from < b.from;
                        if (a.to != b.to) return a.to < b.to;
                        return a.epsilon < b.epsilon;
                    });

            return snap;
        }

        // ---- Conversion: epsilon-NFA -> DFA (subset construction) ----
        // Requires an explicit alphabet (symbols to consider).
        DFA<Symbol> to_dfa(const std::vector<Symbol>& alphabet) const {
            DFA<Symbol> dfa;
            if (!has_start_) return dfa;

            auto start_set = epsilon_closure(StateSet{{*start_}});

            std::unordered_map<StateSet, StateId, StateSetHash> id;
            std::vector<StateSet> sets; // id -> stateset
            sets.reserve(64);

            auto get_id = [&](const StateSet& s) -> std::pair<StateId, bool> {
                auto it = id.find(s);
                if (it != id.end()) return {it->second, false};
                StateId nid = static_cast<StateId>(sets.size());
                sets.push_back(s);
                id.emplace(s, nid);
                dfa.add_state(nid);
                if (any_accepting(s)) dfa.set_accepting(nid, true);
                return {nid, true};
            };

            auto dfa_start = get_id(start_set).first;
            dfa.set_start(dfa_start);

            std::vector<StateId> work;
            work.push_back(dfa_start);

            while (!work.empty()) {
                StateId sid = work.back();
                work.pop_back();
                const StateSet& S = sets[sid];

                for (const auto& a : alphabet) {
                    StateSet T = epsilon_closure(move(S, a));
                    if (T.empty()) continue; // partial DFA; call make_total if desired
                    auto [tid, inserted] = get_id(T);
                    dfa.set_transition(sid, a, tid);
                    if (inserted) work.push_back(tid);
                }
            }
            return dfa;
        }

    private:
        struct Key {
            StateId from;
            Symbol sym;
            bool operator==(const Key& o) const { return from == o.from && sym == o.sym; }
        };
        struct KeyHash {
            std::size_t operator()(const Key& k) const noexcept {
                std::size_t h1 = std::hash<StateId>{}(k.from);
                std::size_t h2 = std::hash<Symbol>{}(k.sym);
                return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1<<6) + (h1>>2));
            }
        };

        bool any_accepting(const StateSet& s) const {
            for (auto q : s.v) if (accepting_.count(q)) return true;
            return false;
        }

        StateSet epsilon_closure(const StateSet& input) const {
            // DFS over epsilon edges
            std::vector<StateId> stack = input.v;
            std::unordered_set<StateId> seen(input.v.begin(), input.v.end());

            while (!stack.empty()) {
                StateId s = stack.back();
                stack.pop_back();
                auto it = eps_.find(s);
                if (it == eps_.end()) continue;
                for (auto t : it->second) {
                    if (seen.insert(t).second) stack.push_back(t);
                }
            }

            std::vector<StateId> out(seen.begin(), seen.end());
            std::sort(out.begin(), out.end());
            return StateSet{std::move(out)};
        }

        StateSet move(const StateSet& from_set, const Symbol& sym) const {
            std::vector<StateId> out;
            for (auto s : from_set.v) {
                auto it = delta_.find(Key{s, sym});
                if (it == delta_.end()) continue;
                out.insert(out.end(), it->second.begin(), it->second.end());
            }
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return StateSet{std::move(out)};
        }

        std::unordered_set<StateId> states_;
        std::unordered_set<StateId> accepting_;
        std::unordered_map<Key, std::vector<StateId>, KeyHash> delta_;
        std::unordered_map<StateId, std::vector<StateId>> eps_;
        std::unordered_map<StateId, std::string> labels_;
        std::unordered_map<StateId, Position> positions_;

        bool has_start_{false};
        std::optional<StateId> start_;
    };

} // namespace fa
