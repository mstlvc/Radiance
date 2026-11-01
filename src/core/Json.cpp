#include "core/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace radiance::json
{
    namespace
    {
        const Value kNull;

        class Parser
        {
        public:
            explicit Parser(std::string_view text) : text_(text) {}

            std::optional<Value> Document()
            {
                Value v;
                if (!ParseValue(v, 0))
                    return std::nullopt;
                SkipSpace();
                if (pos_ != text_.size())
                    return std::nullopt;
                return v;
            }

        private:
            static constexpr int kMaxDepth = 64;

            std::string_view text_;
            size_t pos_ = 0;

            void SkipSpace()
            {
                while (pos_ < text_.size())
                {
                    const char c = text_[pos_];
                    if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                        break;
                    ++pos_;
                }
            }

            bool Consume(char c)
            {
                SkipSpace();
                if (pos_ < text_.size() && text_[pos_] == c)
                {
                    ++pos_;
                    return true;
                }
                return false;
            }

            bool Literal(std::string_view word)
            {
                if (text_.substr(pos_, word.size()) != word)
                    return false;
                pos_ += word.size();
                return true;
            }

            bool ParseValue(Value& out, int depth)
            {
                if (depth > kMaxDepth)
                    return false;
                SkipSpace();
                if (pos_ >= text_.size())
                    return false;
                const char c = text_[pos_];
                if (c == '{') return ParseObject(out, depth);
                if (c == '[') return ParseArray(out, depth);
                if (c == '"')
                {
                    std::string s;
                    if (!ParseString(s))
                        return false;
                    out = Value(std::move(s));
                    return true;
                }
                if (c == 't') { if (!Literal("true")) return false; out = Value(true); return true; }
                if (c == 'f') { if (!Literal("false")) return false; out = Value(false); return true; }
                if (c == 'n') { if (!Literal("null")) return false; out = Value(); return true; }
                return ParseNumber(out);
            }

            bool ParseNumber(Value& out)
            {
                const size_t start = pos_;
                if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+'))
                    ++pos_;
                while (pos_ < text_.size())
                {
                    const char c = text_[pos_];
                    if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+')
                        ++pos_;
                    else
                        break;
                }
                if (pos_ == start)
                    return false;
                const std::string number(text_.substr(start, pos_ - start));
                char* end = nullptr;
                const double d = std::strtod(number.c_str(), &end);
                if (end == number.c_str())
                    return false;
                out = Value(d);
                return true;
            }

            static void AppendUtf8(std::string& s, uint32_t cp)
            {
                if (cp < 0x80) s += static_cast<char>(cp);
                else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            }

            bool Hex4(uint32_t& out)
            {
                if (pos_ + 4 > text_.size())
                    return false;
                out = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char c = text_[pos_++];
                    out <<= 4;
                    if (c >= '0' && c <= '9') out |= static_cast<uint32_t>(c - '0');
                    else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
                    else return false;
                }
                return true;
            }

            bool ParseString(std::string& out)
            {
                if (text_[pos_] != '"')
                    return false;
                ++pos_;
                while (pos_ < text_.size())
                {
                    const char c = text_[pos_++];
                    if (c == '"')
                        return true;
                    if (c != '\\')
                    {
                        out += c;
                        continue;
                    }
                    if (pos_ >= text_.size())
                        return false;
                    const char e = text_[pos_++];
                    switch (e)
                    {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u':
                    {
                        uint32_t cp = 0;
                        if (!Hex4(cp))
                            return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && text_.substr(pos_, 2) == "\\u")
                        {
                            pos_ += 2;
                            uint32_t low = 0;
                            if (!Hex4(low))
                                return false;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        }
                        AppendUtf8(out, cp);
                        break;
                    }
                    default:
                        return false;
                    }
                }
                return false;
            }

            bool ParseArray(Value& out, int depth)
            {
                ++pos_;
                out = Value::MakeArray();
                if (Consume(']'))
                    return true;
                for (;;)
                {
                    Value element;
                    if (!ParseValue(element, depth + 1))
                        return false;
                    out.Push(std::move(element));
                    if (Consume(','))
                        continue;
                    return Consume(']');
                }
            }

            bool ParseObject(Value& out, int depth)
            {
                ++pos_;
                out = Value::MakeObject();
                if (Consume('}'))
                    return true;
                for (;;)
                {
                    SkipSpace();
                    if (pos_ >= text_.size() || text_[pos_] != '"')
                        return false;
                    std::string key;
                    if (!ParseString(key))
                        return false;
                    if (!Consume(':'))
                        return false;
                    Value member;
                    if (!ParseValue(member, depth + 1))
                        return false;
                    out.Set(std::move(key), std::move(member));
                    if (Consume(','))
                        continue;
                    return Consume('}');
                }
            }
        };

        void Escape(std::string& out, const std::string& s)
        {
            out += '"';
            for (const unsigned char c : s)
            {
                switch (c)
                {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    }
                    else
                    {
                        out += static_cast<char>(c);
                    }
                }
            }
            out += '"';
        }

        void Write(std::string& out, const Value& v, int indent, int level)
        {
            const auto newline = [&](int lvl) {
                if (indent <= 0)
                    return;
                out += '\n';
                out.append(static_cast<size_t>(indent * lvl), ' ');
            };
            switch (v.GetType())
            {
            case Value::Type::Null: out += "null"; break;
            case Value::Type::Bool: out += v.AsBool() ? "true" : "false"; break;
            case Value::Type::Number:
            {
                const double d = v.AsNumber();
                char buf[32];
                if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 9.0e15)
                    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(d));
                else if (std::isfinite(d))
                    std::snprintf(buf, sizeof(buf), "%.10g", d);
                else
                    std::snprintf(buf, sizeof(buf), "0");
                out += buf;
                break;
            }
            case Value::Type::String: Escape(out, v.AsString()); break;
            case Value::Type::Array:
            {
                if (v.Size() == 0) { out += "[]"; break; }
                out += '[';
                for (size_t i = 0; i < v.Size(); ++i)
                {
                    if (i) out += ',';
                    newline(level + 1);
                    Write(out, v.At(i), indent, level + 1);
                }
                newline(level);
                out += ']';
                break;
            }
            case Value::Type::Object:
            {
                if (v.MemberCount() == 0) { out += "{}"; break; }
                out += '{';
                for (size_t i = 0; i < v.MemberCount(); ++i)
                {
                    if (i) out += ',';
                    newline(level + 1);
                    Escape(out, v.KeyAt(i));
                    out += indent > 0 ? ": " : ":";
                    Write(out, v.ValueAt(i), indent, level + 1);
                }
                newline(level);
                out += '}';
                break;
            }
            }
        }
    }

    Value Value::MakeArray()
    {
        Value v;
        v.type_ = Type::Array;
        return v;
    }

    Value Value::MakeObject()
    {
        Value v;
        v.type_ = Type::Object;
        return v;
    }

    bool Value::AsBool(bool fallback) const
    {
        return type_ == Type::Bool ? bool_ : fallback;
    }

    double Value::AsNumber(double fallback) const
    {
        return type_ == Type::Number ? number_ : fallback;
    }

    int Value::AsInt(int fallback) const
    {
        return type_ == Type::Number ? static_cast<int>(std::lround(number_)) : fallback;
    }

    int64_t Value::AsInt64(int64_t fallback) const
    {
        return type_ == Type::Number ? static_cast<int64_t>(std::llround(number_)) : fallback;
    }

    std::string Value::AsString(std::string_view fallback) const
    {
        return type_ == Type::String ? string_ : std::string(fallback);
    }

    const Value& Value::operator[](std::string_view key) const
    {
        if (type_ == Type::Object)
        {
            for (size_t i = 0; i < keys_.size(); ++i)
            {
                if (keys_[i] == key)
                    return values_[i];
            }
        }
        return kNull;
    }

    bool Value::Has(std::string_view key) const
    {
        if (type_ != Type::Object)
            return false;
        for (const std::string& k : keys_)
        {
            if (k == key)
                return true;
        }
        return false;
    }

    Value& Value::Set(std::string key, Value value)
    {
        if (type_ != Type::Object)
        {
            *this = MakeObject();
        }
        for (size_t i = 0; i < keys_.size(); ++i)
        {
            if (keys_[i] == key)
            {
                values_[i] = std::move(value);
                return values_[i];
            }
        }
        keys_.push_back(std::move(key));
        values_.push_back(std::move(value));
        return values_.back();
    }

    size_t Value::Size() const
    {
        return type_ == Type::Array ? array_.size() : 0;
    }

    const Value& Value::At(size_t index) const
    {
        return (type_ == Type::Array && index < array_.size()) ? array_[index] : kNull;
    }

    Value& Value::Push(Value value)
    {
        if (type_ != Type::Array)
        {
            *this = MakeArray();
        }
        array_.push_back(std::move(value));
        return array_.back();
    }

    std::optional<Value> Parse(std::string_view text)
    {
        // Tolerate a UTF-8 byte order mark from hand-edited files.
        if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
            text.remove_prefix(3);
        return Parser(text).Document();
    }

    std::string Dump(const Value& value, int indent)
    {
        std::string out;
        Write(out, value, indent, 0);
        if (indent > 0)
            out += '\n';
        return out;
    }
}
