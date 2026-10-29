#include "shader_json.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace d3::shader::json {
namespace {

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void AppendUtf8(unsigned int cp, std::string& out) {
    if (cp < 0x80u) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text), pos_(0), depth_(0) {}

    bool ParseDocument(Value& out) {
        SkipWhitespace();
        if (!ParseValue(out)) {
            return false;
        }
        SkipWhitespace();
        if (pos_ != text_.size()) {
            return Fail("trailing characters after JSON document");
        }
        return true;
    }

    const std::string& Error() const { return error_; }

private:
    static const int kMaxDepth = 256;

    bool Fail(const std::string& message) {
        if (error_.empty()) {
            error_ = message + " at offset " + std::to_string(pos_);
        }
        return false;
    }

    void SkipWhitespace() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool ParseValue(Value& out) {
        if (depth_ > kMaxDepth) {
            return Fail("maximum nesting depth exceeded");
        }
        SkipWhitespace();
        if (pos_ >= text_.size()) {
            return Fail("unexpected end of input");
        }
        const char c = text_[pos_];
        switch (c) {
        case '{':
            return ParseObject(out);
        case '[':
            return ParseArray(out);
        case '"':
            out.type = Type::String;
            return ParseString(out.string);
        case 't':
            return ParseBoolLiteral(out, "true", true);
        case 'f':
            return ParseBoolLiteral(out, "false", false);
        case 'n':
            return ParseNullLiteral(out);
        default:
            if (c == '-' || IsDigit(c)) {
                return ParseNumber(out);
            }
            return Fail("unexpected character");
        }
    }

    bool ParseBoolLiteral(Value& out, const char* literal, bool value) {
        const std::size_t length = std::strlen(literal);
        if (text_.compare(pos_, length, literal) != 0) {
            return Fail("invalid literal");
        }
        pos_ += length;
        out.type = Type::Bool;
        out.boolean = value;
        return true;
    }

    bool ParseNullLiteral(Value& out) {
        if (text_.compare(pos_, 4, "null") != 0) {
            return Fail("invalid literal");
        }
        pos_ += 4;
        out.type = Type::Null;
        return true;
    }

    bool ParseNumber(Value& out) {
        const std::size_t start = pos_;
        bool integral = true;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= text_.size() || !IsDigit(text_[pos_])) {
            return Fail("invalid number");
        }
        if (text_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < text_.size() && IsDigit(text_[pos_])) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            integral = false;
            ++pos_;
            if (pos_ >= text_.size() || !IsDigit(text_[pos_])) {
                return Fail("invalid number fraction");
            }
            while (pos_ < text_.size() && IsDigit(text_[pos_])) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            integral = false;
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            if (pos_ >= text_.size() || !IsDigit(text_[pos_])) {
                return Fail("invalid number exponent");
            }
            while (pos_ < text_.size() && IsDigit(text_[pos_])) {
                ++pos_;
            }
        }
        const std::string token = text_.substr(start, pos_ - start);
        char* end = nullptr;
        const double parsed = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            return Fail("invalid number");
        }
        out.type = Type::Number;
        out.number = parsed;
        out.integer = integral;
        return true;
    }

    bool ParseHex4(unsigned int& out) {
        unsigned int value = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos_ >= text_.size()) {
                return Fail("truncated unicode escape");
            }
            const int digit = HexValue(text_[pos_]);
            if (digit < 0) {
                return Fail("invalid unicode escape");
            }
            ++pos_;
            value = (value << 4) | static_cast<unsigned int>(digit);
        }
        out = value;
        return true;
    }

    bool ParseCodePoint(unsigned int& out) {
        unsigned int cp = 0;
        if (!ParseHex4(cp)) {
            return false;
        }
        if (cp >= 0xD800u && cp <= 0xDBFFu) {
            if (pos_ + 1 >= text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') {
                return Fail("unpaired high surrogate");
            }
            pos_ += 2;
            unsigned int low = 0;
            if (!ParseHex4(low)) {
                return false;
            }
            if (low < 0xDC00u || low > 0xDFFFu) {
                return Fail("invalid low surrogate");
            }
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
            return Fail("unpaired low surrogate");
        }
        out = cp;
        return true;
    }

    bool ParseString(std::string& out) {
        ++pos_; // opening quote
        out.clear();
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_]);
            if (c == '"') {
                ++pos_;
                return true;
            }
            if (c == '\\') {
                ++pos_;
                if (pos_ >= text_.size()) {
                    return Fail("unterminated escape");
                }
                const char escape = text_[pos_++];
                switch (escape) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    unsigned int cp = 0;
                    if (!ParseCodePoint(cp)) {
                        return false;
                    }
                    AppendUtf8(cp, out);
                    break;
                }
                default:
                    return Fail("invalid escape sequence");
                }
                continue;
            }
            if (c < 0x20u) {
                return Fail("unescaped control character in string");
            }
            out.push_back(static_cast<char>(c));
            ++pos_;
        }
        return Fail("unterminated string");
    }

    bool ParseArray(Value& out) {
        ++pos_; // '['
        ++depth_;
        out.type = Type::Array;
        out.array.clear();
        SkipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            --depth_;
            return true;
        }
        for (;;) {
            Value element;
            if (!ParseValue(element)) {
                return false;
            }
            out.array.push_back(std::move(element));
            SkipWhitespace();
            if (pos_ >= text_.size()) {
                return Fail("unterminated array");
            }
            if (text_[pos_] == ',') {
                ++pos_;
                SkipWhitespace();
                continue;
            }
            if (text_[pos_] == ']') {
                ++pos_;
                --depth_;
                return true;
            }
            return Fail("expected ',' or ']' in array");
        }
    }

    bool ParseObject(Value& out) {
        ++pos_; // '{'
        ++depth_;
        out.type = Type::Object;
        out.object.clear();
        SkipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            --depth_;
            return true;
        }
        for (;;) {
            SkipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return Fail("expected object member name");
            }
            std::string name;
            if (!ParseString(name)) {
                return false;
            }
            for (const std::pair<std::string, Value>& member : out.object) {
                if (member.first == name) {
                    return Fail("duplicate object member '" + name + "'");
                }
            }
            SkipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                return Fail("expected ':' after object member name");
            }
            ++pos_;
            Value member;
            if (!ParseValue(member)) {
                return false;
            }
            out.object.emplace_back(std::move(name), std::move(member));
            SkipWhitespace();
            if (pos_ >= text_.size()) {
                return Fail("unterminated object");
            }
            if (text_[pos_] == ',') {
                ++pos_;
                SkipWhitespace();
                continue;
            }
            if (text_[pos_] == '}') {
                ++pos_;
                --depth_;
                return true;
            }
            return Fail("expected ',' or '}' in object");
        }
    }

    const std::string& text_;
    std::size_t pos_;
    int depth_;
    std::string error_;
};

void WriteString(const std::string& text, std::string& out) {
    out.push_back('"');
    for (const char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20u) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(c));
                out += buffer;
            } else {
                out.push_back(raw);
            }
            break;
        }
    }
    out.push_back('"');
}

void WriteValue(const Value& value, std::string& out) {
    switch (value.type) {
    case Type::Null:
        out += "null";
        break;
    case Type::Bool:
        out += value.boolean ? "true" : "false";
        break;
    case Type::Number: {
        char buffer[64];
        if (value.integer) {
            std::snprintf(buffer, sizeof(buffer), "%.0f", value.number);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.17g", value.number);
        }
        out += buffer;
        break;
    }
    case Type::String:
        WriteString(value.string, out);
        break;
    case Type::Array: {
        out.push_back('[');
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            if (i != 0) {
                out.push_back(',');
            }
            WriteValue(value.array[i], out);
        }
        out.push_back(']');
        break;
    }
    case Type::Object: {
        out.push_back('{');
        for (std::size_t i = 0; i < value.object.size(); ++i) {
            if (i != 0) {
                out.push_back(',');
            }
            WriteString(value.object[i].first, out);
            out.push_back(':');
            WriteValue(value.object[i].second, out);
        }
        out.push_back('}');
        break;
    }
    }
}

} // namespace

const Value* Value::Find(const std::string& key) const {
    if (type != Type::Object) {
        return nullptr;
    }
    for (const std::pair<std::string, Value>& member : object) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

bool Parse(const std::string& text, Value& out, std::string& error) {
    error.clear();
    Parser parser(text);
    out = Value();
    if (!parser.ParseDocument(out)) {
        error = parser.Error();
        out = Value();
        return false;
    }
    return true;
}

void Write(const Value& value, std::string& out) {
    out.clear();
    WriteValue(value, out);
}

} // namespace d3::shader::json
