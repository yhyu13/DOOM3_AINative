// Minimal strict JSON reader used by the shader manifest loader and the tests.
// Port-owned, dependency free, no engine headers.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace d3::shader::json {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    bool integer = false;
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    bool IsNull() const { return type == Type::Null; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsArray() const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    // Returns nullptr when this value is not an object or the key is absent.
    const Value* Find(const std::string& key) const;
};

// Parses exactly one JSON document. Comments, trailing commas and duplicate
// object members are rejected. On failure `error` is non-empty.
bool Parse(const std::string& text, Value& out, std::string& error);

// Serializes a value back to compact JSON. Used by the tests to rebuild the
// manifest document embedded in the vectors fixture.
void Write(const Value& value, std::string& out);

} // namespace d3::shader::json
