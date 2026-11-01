// Small JSON document type for Radiance's settings and profile files.
// Objects keep insertion order so saved files stay readable and diff cleanly.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace radiance::json
{
    class Value
    {
    public:
        enum class Type { Null, Bool, Number, String, Array, Object };

        Value() = default;
        Value(std::nullptr_t) {}
        Value(bool b) : type_(Type::Bool), bool_(b) {}
        Value(int n) : type_(Type::Number), number_(n) {}
        Value(int64_t n) : type_(Type::Number), number_(static_cast<double>(n)) {}
        Value(uint32_t n) : type_(Type::Number), number_(n) {}
        Value(double n) : type_(Type::Number), number_(n) {}
        Value(const char* s) : type_(Type::String), string_(s ? s : "") {}
        Value(std::string s) : type_(Type::String), string_(std::move(s)) {}

        static Value MakeArray();
        static Value MakeObject();

        Type GetType() const { return type_; }
        bool IsNull() const { return type_ == Type::Null; }
        bool IsArray() const { return type_ == Type::Array; }
        bool IsObject() const { return type_ == Type::Object; }

        bool AsBool(bool fallback = false) const;
        double AsNumber(double fallback = 0.0) const;
        int AsInt(int fallback = 0) const;
        int64_t AsInt64(int64_t fallback = 0) const;
        std::string AsString(std::string_view fallback = {}) const;

        // Object access. A missing key reads as null, so lookups chain safely.
        const Value& operator[](std::string_view key) const;
        bool Has(std::string_view key) const;
        Value& Set(std::string key, Value value);
        size_t MemberCount() const { return keys_.size(); }
        const std::string& KeyAt(size_t index) const { return keys_[index]; }
        const Value& ValueAt(size_t index) const { return values_[index]; }

        // Array access.
        size_t Size() const;
        const Value& At(size_t index) const;
        Value& Push(Value value);
        const std::vector<Value>& Elements() const { return array_; }

    private:
        Type type_ = Type::Null;
        bool bool_ = false;
        double number_ = 0.0;
        std::string string_;
        std::vector<Value> array_;
        std::vector<std::string> keys_;   // object members, in insertion order
        std::vector<Value> values_;
    };

    std::optional<Value> Parse(std::string_view text);
    std::string Dump(const Value& value, int indent = 2);
}
