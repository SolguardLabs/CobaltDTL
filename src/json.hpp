#pragma once

#include "common.hpp"

#include <variant>

namespace cobaltdtl {

class JsonValue {
  public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool value);
    JsonValue(long double value);
    JsonValue(std::string value);
    JsonValue(Array value);
    JsonValue(Object value);

    Type type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool asBool(const FieldPath& path) const;
    long double asNumber(const FieldPath& path) const;
    Amount asAmount(const FieldPath& path) const;
    const std::string& asString(const FieldPath& path) const;
    const Array& asArray(const FieldPath& path) const;
    const Object& asObject(const FieldPath& path) const;

    bool has(std::string_view key) const;
    const JsonValue& at(std::string_view key, const FieldPath& path) const;
    const JsonValue* find(std::string_view key) const;

  private:
    Type type_{Type::Null};
    std::variant<std::nullptr_t, bool, long double, std::string, Array, Object> data_{nullptr};
};

JsonValue parseJson(const std::string& text, std::string sourceName = "<input>");
std::string stringifyJson(const JsonValue& value, bool pretty = false);
std::string escapeJsonString(std::string_view value);

class JsonWriter {
  public:
    explicit JsonWriter(bool pretty = false);

    void beginObject();
    void endObject();
    void beginArray();
    void endArray();
    void key(std::string_view key);
    void value(std::string_view value);
    void value(const char* value);
    void value(Amount value);
    void value(long double value);
    void value(bool value);
    void nullValue();
    std::string str() const;

  private:
    enum class FrameKind { Object, Array };

    struct Frame {
        FrameKind kind;
        bool first{true};
        bool afterKey{false};
    };

    void beforeValue();
    void afterContainerValue();
    void newlineIfPretty();
    void writeIndent();

    bool pretty_{false};
    std::ostringstream out_;
    std::vector<Frame> stack_;
};

}  // namespace cobaltdtl

