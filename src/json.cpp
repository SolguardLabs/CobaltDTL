#include "json.hpp"

#include <cmath>
#include <iomanip>

namespace cobaltdtl {

JsonValue::JsonValue() = default;
JsonValue::JsonValue(std::nullptr_t) : type_(Type::Null), data_(nullptr) {}
JsonValue::JsonValue(bool value) : type_(Type::Bool), data_(value) {}
JsonValue::JsonValue(long double value) : type_(Type::Number), data_(value) {}
JsonValue::JsonValue(std::string value) : type_(Type::String), data_(std::move(value)) {}
JsonValue::JsonValue(Array value) : type_(Type::Array), data_(std::move(value)) {}
JsonValue::JsonValue(Object value) : type_(Type::Object), data_(std::move(value)) {}

JsonValue::Type JsonValue::type() const {
    return type_;
}

bool JsonValue::isNull() const {
    return type_ == Type::Null;
}

bool JsonValue::isBool() const {
    return type_ == Type::Bool;
}

bool JsonValue::isNumber() const {
    return type_ == Type::Number;
}

bool JsonValue::isString() const {
    return type_ == Type::String;
}

bool JsonValue::isArray() const {
    return type_ == Type::Array;
}

bool JsonValue::isObject() const {
    return type_ == Type::Object;
}

bool JsonValue::asBool(const FieldPath& path) const {
    if (!isBool()) {
        fail(ErrorCode::Validation, path.str() + " must be a boolean");
    }
    return std::get<bool>(data_);
}

long double JsonValue::asNumber(const FieldPath& path) const {
    if (!isNumber()) {
        fail(ErrorCode::Validation, path.str() + " must be a number");
    }
    return std::get<long double>(data_);
}

Amount JsonValue::asAmount(const FieldPath& path) const {
    if (isString()) {
        return parseIntegerStrict(asString(path), path.str());
    }
    const long double number = asNumber(path);
    if (!std::isfinite(number) || std::floor(number) != number) {
        fail(ErrorCode::Validation, path.str() + " must be an integer amount");
    }
    if (number > static_cast<long double>(std::numeric_limits<Amount>::max()) ||
        number < static_cast<long double>(std::numeric_limits<Amount>::min())) {
        fail(ErrorCode::Validation, path.str() + " is outside amount range");
    }
    return static_cast<Amount>(number);
}

const std::string& JsonValue::asString(const FieldPath& path) const {
    if (!isString()) {
        fail(ErrorCode::Validation, path.str() + " must be a string");
    }
    return std::get<std::string>(data_);
}

const JsonValue::Array& JsonValue::asArray(const FieldPath& path) const {
    if (!isArray()) {
        fail(ErrorCode::Validation, path.str() + " must be an array");
    }
    return std::get<Array>(data_);
}

const JsonValue::Object& JsonValue::asObject(const FieldPath& path) const {
    if (!isObject()) {
        fail(ErrorCode::Validation, path.str() + " must be an object");
    }
    return std::get<Object>(data_);
}

bool JsonValue::has(std::string_view key) const {
    if (!isObject()) {
        return false;
    }
    const auto& object = std::get<Object>(data_);
    return object.find(std::string(key)) != object.end();
}

const JsonValue& JsonValue::at(std::string_view key, const FieldPath& path) const {
    if (!isObject()) {
        fail(ErrorCode::Validation, path.str() + " must be an object");
    }
    const auto& object = std::get<Object>(data_);
    const auto it = object.find(std::string(key));
    if (it == object.end()) {
        fail(ErrorCode::MissingField, path.child(std::string(key)).str() + " is required");
    }
    return it->second;
}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (!isObject()) {
        return nullptr;
    }
    const auto& object = std::get<Object>(data_);
    const auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

class JsonParser {
  public:
    JsonParser(std::string_view text, std::string sourceName) : text_(text), sourceName_(std::move(sourceName)) {}

    JsonValue parse() {
        skipWhitespace();
        JsonValue value = parseValue(FieldPath(sourceName_));
        skipWhitespace();
        if (!eof()) {
            error("unexpected trailing content");
        }
        return value;
    }

  private:
    bool eof() const {
        return pos_ >= text_.size();
    }

    char peek() const {
        return eof() ? '\0' : text_[pos_];
    }

    char get() {
        if (eof()) {
            error("unexpected end of input");
        }
        const char ch = text_[pos_++];
        if (ch == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return ch;
    }

    bool consume(char expected) {
        if (peek() != expected) {
            return false;
        }
        (void)get();
        return true;
    }

    void expect(char expected, std::string_view message) {
        if (!consume(expected)) {
            error(std::string(message));
        }
    }

    void skipWhitespace() {
        while (!eof()) {
            const char ch = peek();
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                (void)get();
                continue;
            }
            break;
        }
    }

    JsonValue parseValue(const FieldPath& path) {
        skipWhitespace();
        switch (peek()) {
            case 'n':
                parseLiteral("null");
                return JsonValue(nullptr);
            case 't':
                parseLiteral("true");
                return JsonValue(true);
            case 'f':
                parseLiteral("false");
                return JsonValue(false);
            case '"':
                return JsonValue(parseString());
            case '[':
                return parseArray(path);
            case '{':
                return parseObject(path);
            default:
                if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                    return JsonValue(parseNumber());
                }
                error("unexpected token while parsing value");
        }
    }

    void parseLiteral(std::string_view literal) {
        for (char expected : literal) {
            if (get() != expected) {
                error(std::string("expected literal ") + std::string(literal));
            }
        }
    }

    std::string parseString() {
        expect('"', "expected string");
        std::string out;
        while (!eof()) {
            const char ch = get();
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (eof()) {
                error("unterminated escape sequence");
            }
            const char esc = get();
            switch (esc) {
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
                case 'u':
                    appendUnicodeEscape(out);
                    break;
                default:
                    error("invalid string escape");
            }
        }
        error("unterminated string");
    }

    int hexValue(char ch) {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return 10 + ch - 'a';
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + ch - 'A';
        }
        error("invalid unicode escape");
    }

    void appendUtf8(std::string& out, std::uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    void appendUnicodeEscape(std::string& out) {
        std::uint32_t codepoint = 0;
        for (int i = 0; i < 4; ++i) {
            codepoint = (codepoint << 4) | static_cast<std::uint32_t>(hexValue(get()));
        }
        appendUtf8(out, codepoint);
    }

    long double parseNumber() {
        const std::size_t start = pos_;
        if (peek() == '-') {
            (void)get();
        }
        if (peek() == '0') {
            (void)get();
        } else {
            if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
                error("invalid number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                (void)get();
            }
        }
        if (peek() == '.') {
            (void)get();
            if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
                error("invalid fraction");
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                (void)get();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            (void)get();
            if (peek() == '+' || peek() == '-') {
                (void)get();
            }
            if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
                error("invalid exponent");
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                (void)get();
            }
        }
        const std::string token(text_.substr(start, pos_ - start));
        try {
            std::size_t consumed = 0;
            const long double value = std::stold(token, &consumed);
            if (consumed != token.size() || !std::isfinite(value)) {
                error("invalid number literal");
            }
            return value;
        } catch (const std::exception&) {
            error("invalid number literal");
        }
    }

    JsonValue parseArray(const FieldPath& path) {
        expect('[', "expected array");
        JsonValue::Array items;
        skipWhitespace();
        if (consume(']')) {
            return JsonValue(std::move(items));
        }
        std::size_t index = 0;
        while (true) {
            items.push_back(parseValue(path.child(std::to_string(index))));
            ++index;
            skipWhitespace();
            if (consume(']')) {
                break;
            }
            expect(',', "expected comma in array");
            skipWhitespace();
        }
        return JsonValue(std::move(items));
    }

    JsonValue parseObject(const FieldPath& path) {
        expect('{', "expected object");
        JsonValue::Object object;
        skipWhitespace();
        if (consume('}')) {
            return JsonValue(std::move(object));
        }
        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                error("expected object key");
            }
            std::string key = parseString();
            skipWhitespace();
            expect(':', "expected colon after key");
            if (object.find(key) != object.end()) {
                fail(ErrorCode::Validation, path.child(key).str() + " is duplicated");
            }
            object.emplace(key, parseValue(path.child(key)));
            skipWhitespace();
            if (consume('}')) {
                break;
            }
            expect(',', "expected comma in object");
        }
        return JsonValue(std::move(object));
    }

    [[noreturn]] void error(const std::string& message) const {
        std::ostringstream out;
        out << sourceName_ << ":" << line_ << ":" << column_ << ": " << message;
        fail(ErrorCode::Parse, out.str());
    }

    std::string_view text_;
    std::string sourceName_;
    std::size_t pos_{0};
    std::size_t line_{1};
    std::size_t column_{1};
};

JsonValue parseJson(const std::string& text, std::string sourceName) {
    return JsonParser(text, std::move(sourceName)).parse();
}

std::string escapeJsonString(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec;
                } else {
                    out << ch;
                }
        }
    }
    out << '"';
    return out.str();
}

static void stringifyInto(const JsonValue& value, JsonWriter& writer) {
    switch (value.type()) {
        case JsonValue::Type::Null:
            writer.nullValue();
            break;
        case JsonValue::Type::Bool:
            writer.value(value.asBool(FieldPath("value")));
            break;
        case JsonValue::Type::Number:
            writer.value(value.asNumber(FieldPath("value")));
            break;
        case JsonValue::Type::String:
            writer.value(value.asString(FieldPath("value")));
            break;
        case JsonValue::Type::Array:
            writer.beginArray();
            for (const auto& item : value.asArray(FieldPath("value"))) {
                stringifyInto(item, writer);
            }
            writer.endArray();
            break;
        case JsonValue::Type::Object:
            writer.beginObject();
            for (const auto& item : value.asObject(FieldPath("value"))) {
                writer.key(item.first);
                stringifyInto(item.second, writer);
            }
            writer.endObject();
            break;
    }
}

std::string stringifyJson(const JsonValue& value, bool pretty) {
    JsonWriter writer(pretty);
    stringifyInto(value, writer);
    return writer.str();
}

JsonWriter::JsonWriter(bool pretty) : pretty_(pretty) {}

void JsonWriter::beginObject() {
    beforeValue();
    out_ << '{';
    stack_.push_back(Frame{FrameKind::Object, true, false});
}

void JsonWriter::endObject() {
    if (stack_.empty() || stack_.back().kind != FrameKind::Object || stack_.back().afterKey) {
        fail(ErrorCode::Internal, "invalid JSON writer object close");
    }
    const bool hadItems = !stack_.back().first;
    stack_.pop_back();
    if (pretty_ && hadItems) {
        out_ << '\n';
        writeIndent();
    }
    out_ << '}';
    afterContainerValue();
}

void JsonWriter::beginArray() {
    beforeValue();
    out_ << '[';
    stack_.push_back(Frame{FrameKind::Array, true, false});
}

void JsonWriter::endArray() {
    if (stack_.empty() || stack_.back().kind != FrameKind::Array) {
        fail(ErrorCode::Internal, "invalid JSON writer array close");
    }
    const bool hadItems = !stack_.back().first;
    stack_.pop_back();
    if (pretty_ && hadItems) {
        out_ << '\n';
        writeIndent();
    }
    out_ << ']';
    afterContainerValue();
}

void JsonWriter::key(std::string_view keyName) {
    if (stack_.empty() || stack_.back().kind != FrameKind::Object || stack_.back().afterKey) {
        fail(ErrorCode::Internal, "invalid JSON writer key");
    }
    Frame& frame = stack_.back();
    if (!frame.first) {
        out_ << ',';
    }
    frame.first = false;
    newlineIfPretty();
    out_ << escapeJsonString(keyName);
    out_ << (pretty_ ? ": " : ":");
    frame.afterKey = true;
}

void JsonWriter::value(std::string_view valueText) {
    beforeValue();
    out_ << escapeJsonString(valueText);
    afterContainerValue();
}

void JsonWriter::value(const char* valueText) {
    value(std::string_view(valueText));
}

void JsonWriter::value(Amount valueNumber) {
    beforeValue();
    out_ << valueNumber;
    afterContainerValue();
}

void JsonWriter::value(long double valueNumber) {
    beforeValue();
    out_ << std::setprecision(16) << static_cast<double>(valueNumber);
    afterContainerValue();
}

void JsonWriter::value(bool valueBool) {
    beforeValue();
    out_ << (valueBool ? "true" : "false");
    afterContainerValue();
}

void JsonWriter::nullValue() {
    beforeValue();
    out_ << "null";
    afterContainerValue();
}

std::string JsonWriter::str() const {
    if (!stack_.empty()) {
        fail(ErrorCode::Internal, "JSON writer has unclosed containers");
    }
    return out_.str();
}

void JsonWriter::beforeValue() {
    if (stack_.empty()) {
        return;
    }
    Frame& frame = stack_.back();
    if (frame.kind == FrameKind::Object) {
        if (!frame.afterKey) {
            fail(ErrorCode::Internal, "object value without key");
        }
        frame.afterKey = false;
        return;
    }
    if (!frame.first) {
        out_ << ',';
    }
    frame.first = false;
    newlineIfPretty();
}

void JsonWriter::afterContainerValue() {
}

void JsonWriter::newlineIfPretty() {
    if (!pretty_) {
        return;
    }
    out_ << '\n';
    writeIndent();
}

void JsonWriter::writeIndent() {
    if (!pretty_) {
        return;
    }
    out_ << repeat(' ', stack_.size() * 2);
}

}  // namespace cobaltdtl

