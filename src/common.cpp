#include "common.hpp"

#include <cmath>
#include <iomanip>

namespace cobaltdtl {

DtlError::DtlError(ErrorCode code, std::string message) : std::runtime_error(std::move(message)), code_(code) {}

ErrorCode DtlError::code() const noexcept {
    return code_;
}

std::string errorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::Parse:
            return "parse";
        case ErrorCode::Validation:
            return "validation";
        case ErrorCode::MissingField:
            return "missing_field";
        case ErrorCode::UnknownReference:
            return "unknown_reference";
        case ErrorCode::InsufficientBalance:
            return "insufficient_balance";
        case ErrorCode::Policy:
            return "policy";
        case ErrorCode::Internal:
            return "internal";
    }
    return "unknown";
}

[[noreturn]] void fail(ErrorCode code, const std::string& message) {
    throw DtlError(code, message);
}

std::string trim(std::string_view input) {
    std::size_t begin = 0;
    std::size_t end = input.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return std::string(input.substr(begin, end - begin));
}

std::string toLower(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

std::string toUpper(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return out;
}

bool startsWith(std::string_view input, std::string_view prefix) {
    return input.size() >= prefix.size() && input.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view input, std::string_view suffix) {
    return input.size() >= suffix.size() && input.substr(input.size() - suffix.size()) == suffix;
}

std::vector<std::string> split(std::string_view input, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= input.size()) {
        std::size_t found = input.find(delimiter, start);
        if (found == std::string_view::npos) {
            parts.emplace_back(input.substr(start));
            break;
        }
        parts.emplace_back(input.substr(start, found - start));
        start = found + 1;
    }
    return parts;
}

std::string join(const std::vector<std::string>& items, std::string_view delimiter) {
    std::ostringstream out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            out << delimiter;
        }
        out << items[i];
    }
    return out.str();
}

std::string repeat(char ch, std::size_t count) {
    return std::string(count, ch);
}

bool isIdentifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    for (char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) == 0 && ch != '-' && ch != '_' && ch != '.' && ch != ':') {
            return false;
        }
    }
    return true;
}

std::string requireIdentifier(std::string_view field, std::string value) {
    value = trim(value);
    if (!isIdentifier(value)) {
        fail(ErrorCode::Validation, std::string("invalid identifier in ") + std::string(field) + ": " + value);
    }
    return value;
}

Amount checkedAdd(Amount left, Amount right, std::string_view context) {
    if (right > 0 && left > std::numeric_limits<Amount>::max() - right) {
        fail(ErrorCode::Validation, std::string("amount overflow while adding ") + std::string(context));
    }
    if (right < 0 && left < std::numeric_limits<Amount>::min() - right) {
        fail(ErrorCode::Validation, std::string("amount underflow while adding ") + std::string(context));
    }
    return left + right;
}

Amount checkedSub(Amount left, Amount right, std::string_view context) {
    if (right == std::numeric_limits<Amount>::min()) {
        fail(ErrorCode::Validation, std::string("amount underflow while subtracting ") + std::string(context));
    }
    return checkedAdd(left, -right, context);
}

Amount checkedMul(Amount left, Amount right, std::string_view context) {
    const long double product = static_cast<long double>(left) * static_cast<long double>(right);
    if (product > static_cast<long double>(std::numeric_limits<Amount>::max()) ||
        product < static_cast<long double>(std::numeric_limits<Amount>::min())) {
        fail(ErrorCode::Validation, std::string("amount overflow while multiplying ") + std::string(context));
    }
    return static_cast<Amount>(left * right);
}

Amount clampAmount(Amount value, Amount low, Amount high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

Amount minAmount(Amount left, Amount right) {
    return std::min(left, right);
}

Amount maxAmount(Amount left, Amount right) {
    return std::max(left, right);
}

Amount absAmount(Amount value) {
    if (value == std::numeric_limits<Amount>::min()) {
        fail(ErrorCode::Validation, "cannot represent absolute value");
    }
    return value < 0 ? -value : value;
}

Amount mulDivFloor(Amount value, Amount numerator, Amount denominator, std::string_view context) {
    if (denominator <= 0) {
        fail(ErrorCode::Validation, std::string("invalid denominator in ") + std::string(context));
    }
    const long double product = static_cast<long double>(value) * static_cast<long double>(numerator);
    const long double result = std::floor(product / static_cast<long double>(denominator));
    if (!std::isfinite(result) || result > static_cast<long double>(std::numeric_limits<Amount>::max()) ||
        result < static_cast<long double>(std::numeric_limits<Amount>::min())) {
        fail(ErrorCode::Validation, std::string("amount overflow in ") + std::string(context));
    }
    return static_cast<Amount>(result);
}

Amount mulDivCeil(Amount value, Amount numerator, Amount denominator, std::string_view context) {
    if (denominator <= 0) {
        fail(ErrorCode::Validation, std::string("invalid denominator in ") + std::string(context));
    }
    const long double product = static_cast<long double>(value) * static_cast<long double>(numerator);
    const long double result = std::ceil(product / static_cast<long double>(denominator));
    if (!std::isfinite(result) || result > static_cast<long double>(std::numeric_limits<Amount>::max()) ||
        result < static_cast<long double>(std::numeric_limits<Amount>::min())) {
        fail(ErrorCode::Validation, std::string("amount overflow in ") + std::string(context));
    }
    return static_cast<Amount>(result);
}

Amount applyBpsFloor(Amount value, Amount bps, std::string_view context) {
    if (bps < 0) {
        fail(ErrorCode::Validation, std::string("negative bps in ") + std::string(context));
    }
    return mulDivFloor(value, bps, kBps, context);
}

Amount applyBpsCeil(Amount value, Amount bps, std::string_view context) {
    if (bps < 0) {
        fail(ErrorCode::Validation, std::string("negative bps in ") + std::string(context));
    }
    return mulDivCeil(value, bps, kBps, context);
}

Amount subtractBpsFloor(Amount value, Amount bps, std::string_view context) {
    return checkedSub(value, applyBpsFloor(value, bps, context), context);
}

Amount addBpsFloor(Amount value, Amount bps, std::string_view context) {
    return checkedAdd(value, applyBpsFloor(value, bps, context), context);
}

std::string amountToString(Amount value) {
    return std::to_string(value);
}

Amount parseIntegerStrict(std::string_view value, std::string_view context) {
    const std::string clean = trim(value);
    if (clean.empty()) {
        fail(ErrorCode::Parse, std::string("empty integer in ") + std::string(context));
    }
    std::size_t i = 0;
    bool negative = false;
    if (clean[i] == '-' || clean[i] == '+') {
        negative = clean[i] == '-';
        ++i;
    }
    if (i == clean.size()) {
        fail(ErrorCode::Parse, std::string("invalid integer in ") + std::string(context));
    }
    Amount result = 0;
    for (; i < clean.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(clean[i])) == 0) {
            fail(ErrorCode::Parse, std::string("invalid integer in ") + std::string(context) + ": " + clean);
        }
        const Amount digit = static_cast<Amount>(clean[i] - '0');
        if (result > (std::numeric_limits<Amount>::max() - digit) / 10) {
            fail(ErrorCode::Parse, std::string("integer overflow in ") + std::string(context));
        }
        result = result * 10 + digit;
    }
    return negative ? -result : result;
}

std::string scaledToDecimal(Amount value, int decimals) {
    if (decimals <= 0) {
        return std::to_string(value);
    }
    const bool negative = value < 0;
    Amount abs = absAmount(value);
    Amount divisor = 1;
    for (int i = 0; i < decimals; ++i) {
        divisor *= 10;
    }
    const Amount whole = abs / divisor;
    Amount fraction = abs % divisor;
    std::ostringstream out;
    if (negative) {
        out << '-';
    }
    out << whole << '.';
    out << std::setw(decimals) << std::setfill('0') << fraction;
    return out.str();
}

std::string bpsToString(Amount bps) {
    std::ostringstream out;
    out << (bps / 100) << '.';
    const Amount fraction = absAmount(bps % 100);
    out << std::setw(2) << std::setfill('0') << fraction << "%";
    return out.str();
}

FieldPath::FieldPath(std::string root) {
    parts.push_back(std::move(root));
}

FieldPath FieldPath::child(std::string segment) const {
    FieldPath copy = *this;
    copy.parts.push_back(std::move(segment));
    return copy;
}

std::string FieldPath::str() const {
    return join(parts, ".");
}

IdAllocator::IdAllocator(std::string prefix) : prefix_(std::move(prefix)) {}

std::string IdAllocator::next() {
    ++counter_;
    std::ostringstream out;
    out << prefix_ << "-" << counter_;
    return out.str();
}

std::string IdAllocator::nextWith(std::string_view hint) {
    ++counter_;
    std::ostringstream out;
    out << prefix_ << "-" << hint << "-" << counter_;
    return out.str();
}

void IdAllocator::observe(std::string_view id) {
    if (!startsWith(id, prefix_)) {
        return;
    }
    const std::size_t dash = id.find_last_of('-');
    if (dash == std::string_view::npos || dash + 1 >= id.size()) {
        return;
    }
    try {
        const Amount parsed = parseIntegerStrict(id.substr(dash + 1), "observed id");
        if (parsed > 0 && static_cast<std::uint64_t>(parsed) > counter_) {
            counter_ = static_cast<std::uint64_t>(parsed);
        }
    } catch (const DtlError&) {
    }
}

void RunningTotal::add(Amount amount, std::string_view context) {
    value = checkedAdd(value, amount, context);
}

void RunningTotal::sub(Amount amount, std::string_view context) {
    value = checkedSub(value, amount, context);
}

}  // namespace cobaltdtl

