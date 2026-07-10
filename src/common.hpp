#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cobaltdtl {

using Amount = std::int64_t;
using Epoch = std::int64_t;

constexpr Amount kScale = 1'000'000;
constexpr Amount kBps = 10'000;
constexpr Amount kNoLimit = std::numeric_limits<Amount>::max() / 8;

enum class ErrorCode {
    Parse,
    Validation,
    MissingField,
    UnknownReference,
    InsufficientBalance,
    Policy,
    Internal
};

class DtlError : public std::runtime_error {
  public:
    DtlError(ErrorCode code, std::string message);

    ErrorCode code() const noexcept;

  private:
    ErrorCode code_;
};

std::string errorCodeName(ErrorCode code);

[[noreturn]] void fail(ErrorCode code, const std::string& message);

std::string trim(std::string_view input);
std::string toLower(std::string_view input);
std::string toUpper(std::string_view input);
bool startsWith(std::string_view input, std::string_view prefix);
bool endsWith(std::string_view input, std::string_view suffix);
std::vector<std::string> split(std::string_view input, char delimiter);
std::string join(const std::vector<std::string>& items, std::string_view delimiter);
std::string repeat(char ch, std::size_t count);

bool isIdentifier(std::string_view value);
std::string requireIdentifier(std::string_view field, std::string value);

Amount checkedAdd(Amount left, Amount right, std::string_view context);
Amount checkedSub(Amount left, Amount right, std::string_view context);
Amount checkedMul(Amount left, Amount right, std::string_view context);
Amount clampAmount(Amount value, Amount low, Amount high);
Amount minAmount(Amount left, Amount right);
Amount maxAmount(Amount left, Amount right);
Amount absAmount(Amount value);

Amount mulDivFloor(Amount value, Amount numerator, Amount denominator, std::string_view context);
Amount mulDivCeil(Amount value, Amount numerator, Amount denominator, std::string_view context);
Amount applyBpsFloor(Amount value, Amount bps, std::string_view context);
Amount applyBpsCeil(Amount value, Amount bps, std::string_view context);
Amount subtractBpsFloor(Amount value, Amount bps, std::string_view context);
Amount addBpsFloor(Amount value, Amount bps, std::string_view context);

std::string amountToString(Amount value);
Amount parseIntegerStrict(std::string_view value, std::string_view context);
std::string scaledToDecimal(Amount value, int decimals = 6);
std::string bpsToString(Amount bps);

template <typename Map>
bool containsKey(const Map& map, const typename Map::key_type& key) {
    return map.find(key) != map.end();
}

template <typename Map>
typename Map::mapped_type& requireMutable(Map& map, const typename Map::key_type& key, std::string_view label) {
    auto it = map.find(key);
    if (it == map.end()) {
        fail(ErrorCode::UnknownReference, std::string("unknown ") + std::string(label) + ": " + key);
    }
    return it->second;
}

template <typename Map>
const typename Map::mapped_type& requireConst(const Map& map, const typename Map::key_type& key, std::string_view label) {
    auto it = map.find(key);
    if (it == map.end()) {
        fail(ErrorCode::UnknownReference, std::string("unknown ") + std::string(label) + ": " + key);
    }
    return it->second;
}

template <typename T>
std::vector<T> sortedValues(const std::map<std::string, T>& map) {
    std::vector<T> values;
    values.reserve(map.size());
    for (const auto& item : map) {
        values.push_back(item.second);
    }
    return values;
}

struct FieldPath {
    std::vector<std::string> parts;

    FieldPath() = default;
    explicit FieldPath(std::string root);

    FieldPath child(std::string segment) const;
    std::string str() const;
};

class IdAllocator {
  public:
    explicit IdAllocator(std::string prefix);

    std::string next();
    std::string nextWith(std::string_view hint);
    void observe(std::string_view id);

  private:
    std::string prefix_;
    std::uint64_t counter_{0};
};

struct RunningTotal {
    Amount value{0};

    void add(Amount amount, std::string_view context);
    void sub(Amount amount, std::string_view context);
};

}  // namespace cobaltdtl

