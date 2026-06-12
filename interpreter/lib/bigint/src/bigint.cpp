#include "chirp/bigint.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace chirp::interpreter {
namespace {

constexpr uint32_t LIMB_BITS = 31;
constexpr uint32_t LIMB_BASE = uint32_t{1} << LIMB_BITS;
constexpr uint32_t LIMB_MASK = LIMB_BASE - 1;
constexpr uint32_t DECIMAL_BASE = 1'000'000'000;

using Magnitude = std::vector<uint32_t>;

void trim_magnitude(Magnitude& magnitude) {
    while (!magnitude.empty() && magnitude.back() == 0) {
        magnitude.pop_back();
    }
}

bool is_zero(const Magnitude& magnitude) {
    return magnitude.empty();
}

Magnitude magnitude_from_uint64(uint64_t value) {
    Magnitude magnitude;
    while (value != 0) {
        magnitude.push_back(static_cast<uint32_t>(value & LIMB_MASK));
        value >>= LIMB_BITS;
    }
    return magnitude;
}

Magnitude magnitude_from_int64(int64_t value) {
    if (value >= 0) {
        return magnitude_from_uint64(static_cast<uint64_t>(value));
    }

    uint64_t magnitude = static_cast<uint64_t>(-(value + 1));
    magnitude += 1;
    return magnitude_from_uint64(magnitude);
}

int compare_magnitude(const Magnitude& left, const Magnitude& right) {
    if (left.size() < right.size()) {
        return -1;
    }
    if (left.size() > right.size()) {
        return 1;
    }

    for (size_t i = left.size(); i > 0; --i) {
        if (left[i - 1] < right[i - 1]) {
            return -1;
        }
        if (left[i - 1] > right[i - 1]) {
            return 1;
        }
    }
    return 0;
}

Magnitude add_magnitude(const Magnitude& left, const Magnitude& right) {
    Magnitude result;
    result.reserve(std::max(left.size(), right.size()) + 1);

    uint64_t carry = 0;
    size_t count = std::max(left.size(), right.size());
    for (size_t i = 0; i < count; ++i) {
        uint64_t sum = carry;
        if (i < left.size()) {
            sum += left[i];
        }
        if (i < right.size()) {
            sum += right[i];
        }
        result.push_back(static_cast<uint32_t>(sum & LIMB_MASK));
        carry = sum >> LIMB_BITS;
    }

    if (carry != 0) {
        result.push_back(static_cast<uint32_t>(carry));
    }

    trim_magnitude(result);
    return result;
}

Magnitude subtract_magnitude(const Magnitude& left, const Magnitude& right) {
    Magnitude result;
    result.reserve(left.size());

    int64_t borrow = 0;
    for (size_t i = 0; i < left.size(); ++i) {
        int64_t diff = static_cast<int64_t>(left[i]) - borrow;
        if (i < right.size()) {
            diff -= static_cast<int64_t>(right[i]);
        }
        if (diff < 0) {
            diff += static_cast<int64_t>(LIMB_BASE);
            borrow = 1;
        } else {
            borrow = 0;
        }
        result.push_back(static_cast<uint32_t>(diff));
    }

    trim_magnitude(result);
    return result;
}

Magnitude multiply_magnitude(const Magnitude& left, const Magnitude& right) {
    if (is_zero(left) || is_zero(right)) {
        return {};
    }

    Magnitude result(left.size() + right.size(), 0);
    for (size_t i = 0; i < left.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < right.size(); ++j) {
            uint64_t product = static_cast<uint64_t>(left[i]) * right[j];
            uint64_t acc = static_cast<uint64_t>(result[i + j]) + product + carry;
            result[i + j] = static_cast<uint32_t>(acc & LIMB_MASK);
            carry = acc >> LIMB_BITS;
        }

        size_t index = i + right.size();
        while (carry != 0) {
            if (index >= result.size()) {
                result.push_back(0);
            }
            uint64_t acc = static_cast<uint64_t>(result[index]) + carry;
            result[index] = static_cast<uint32_t>(acc & LIMB_MASK);
            carry = acc >> LIMB_BITS;
            ++index;
        }
    }

    trim_magnitude(result);
    return result;
}

void multiply_small_in_place(Magnitude& magnitude, uint32_t factor) {
    if (is_zero(magnitude) || factor == 1) {
        return;
    }
    if (factor == 0) {
        magnitude.clear();
        return;
    }

    uint64_t carry = 0;
    for (uint32_t& limb : magnitude) {
        uint64_t product = static_cast<uint64_t>(limb) * factor + carry;
        limb = static_cast<uint32_t>(product & LIMB_MASK);
        carry = product >> LIMB_BITS;
    }

    while (carry != 0) {
        magnitude.push_back(static_cast<uint32_t>(carry & LIMB_MASK));
        carry >>= LIMB_BITS;
    }
}

void add_small_in_place(Magnitude& magnitude, uint32_t addend) {
    if (addend == 0) {
        return;
    }
    if (magnitude.empty()) {
        magnitude.push_back(addend);
        trim_magnitude(magnitude);
        return;
    }

    uint64_t carry = addend;
    size_t index = 0;
    while (carry != 0) {
        if (index >= magnitude.size()) {
            magnitude.push_back(0);
        }
        uint64_t sum = static_cast<uint64_t>(magnitude[index]) + carry;
        magnitude[index] = static_cast<uint32_t>(sum & LIMB_MASK);
        carry = sum >> LIMB_BITS;
        ++index;
    }
}

std::pair<Magnitude, uint32_t> divide_magnitude_by_small(const Magnitude& magnitude, uint32_t divisor) {
    if (divisor == 0) {
        throw std::domain_error("Division by zero");
    }

    Magnitude quotient(magnitude.size(), 0);
    uint64_t remainder = 0;
    for (size_t i = magnitude.size(); i > 0; --i) {
        uint64_t current = remainder * LIMB_BASE + magnitude[i - 1];
        quotient[i - 1] = static_cast<uint32_t>(current / divisor);
        remainder = current % divisor;
    }

    trim_magnitude(quotient);
    return {std::move(quotient), static_cast<uint32_t>(remainder)};
}

size_t bit_length(const Magnitude& magnitude) {
    if (magnitude.empty()) {
        return 0;
    }

    uint32_t top = magnitude.back();
    size_t bits = 0;
    while (top != 0) {
        ++bits;
        top >>= 1;
    }
    return (magnitude.size() - 1) * LIMB_BITS + bits;
}

Magnitude shift_left(const Magnitude& magnitude, size_t bits) {
    if (magnitude.empty() || bits == 0) {
        return magnitude;
    }

    size_t limb_shift = bits / LIMB_BITS;
    uint32_t bit_shift = static_cast<uint32_t>(bits % LIMB_BITS);
    Magnitude result(limb_shift, 0);
    result.reserve(magnitude.size() + limb_shift + 1);

    uint64_t carry = 0;
    for (uint32_t limb : magnitude) {
        uint64_t shifted = (static_cast<uint64_t>(limb) << bit_shift) | carry;
        result.push_back(static_cast<uint32_t>(shifted & LIMB_MASK));
        carry = shifted >> LIMB_BITS;
    }

    if (carry != 0) {
        result.push_back(static_cast<uint32_t>(carry));
    }

    trim_magnitude(result);
    return result;
}

void shift_right_one(Magnitude& magnitude) {
    if (magnitude.empty()) {
        return;
    }

    uint32_t carry = 0;
    for (size_t i = magnitude.size(); i > 0; --i) {
        uint32_t next_carry = magnitude[i - 1] & 1u;
        magnitude[i - 1] = (magnitude[i - 1] >> 1) | (carry << (LIMB_BITS - 1));
        carry = next_carry;
    }

    trim_magnitude(magnitude);
}

void set_bit(Magnitude& magnitude, size_t bit_index) {
    size_t limb_index = bit_index / LIMB_BITS;
    uint32_t bit = static_cast<uint32_t>(bit_index % LIMB_BITS);
    if (magnitude.size() <= limb_index) {
        magnitude.resize(limb_index + 1, 0);
    }
    magnitude[limb_index] |= (uint32_t{1} << bit);
}

std::pair<Magnitude, Magnitude> divide_magnitude(const Magnitude& dividend, const Magnitude& divisor) {
    if (divisor.empty()) {
        throw std::domain_error("Division by zero");
    }

    int order = compare_magnitude(dividend, divisor);
    if (order < 0) {
        return {Magnitude{}, dividend};
    }
    if (order == 0) {
        return {Magnitude{1}, Magnitude{}};
    }
    if (divisor.size() == 1) {
        auto [quotient, remainder] = divide_magnitude_by_small(dividend, divisor[0]);
        Magnitude rem_mag;
        if (remainder != 0) {
            rem_mag.push_back(remainder);
        }
        return {std::move(quotient), std::move(rem_mag)};
    }

    size_t shift = bit_length(dividend) - bit_length(divisor);
    Magnitude shifted_divisor = shift_left(divisor, shift);
    Magnitude remainder = dividend;
    Magnitude quotient;

    while (true) {
        if (compare_magnitude(remainder, shifted_divisor) >= 0) {
            remainder = subtract_magnitude(remainder, shifted_divisor);
            set_bit(quotient, shift);
        }

        if (shift == 0) {
            break;
        }

        --shift;
        shift_right_one(shifted_divisor);
    }

    trim_magnitude(quotient);
    trim_magnitude(remainder);
    return {std::move(quotient), std::move(remainder)};
}

uint64_t magnitude_to_uint64(const Magnitude& magnitude) {
    uint64_t value = 0;
    for (size_t i = magnitude.size(); i > 0; --i) {
        value = (value << LIMB_BITS) | magnitude[i - 1];
    }
    return value;
}

bool magnitude_fits_uint64(const Magnitude& magnitude) {
    return bit_length(magnitude) <= 64;
}

bool magnitude_fits_signed_small(const Magnitude& magnitude, bool negative) {
    if (!magnitude_fits_uint64(magnitude)) {
        return false;
    }

    uint64_t value = magnitude_to_uint64(magnitude);
    if (negative) {
        return value <= (uint64_t{1} << 63);
    }
    return value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
}

std::strong_ordering compare_signed_magnitude(bool left_negative, const Magnitude& left, bool right_negative, const Magnitude& right) {
    if (left_negative != right_negative) {
        return left_negative ? std::strong_ordering::less : std::strong_ordering::greater;
    }

    int magnitude_order = compare_magnitude(left, right);
    if (magnitude_order == 0) {
        return std::strong_ordering::equal;
    }
    if (!left_negative) {
        return magnitude_order < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    return magnitude_order < 0 ? std::strong_ordering::greater : std::strong_ordering::less;
}

} // namespace

BigInt::BigInt() : storage_(int64_t{0}) {}

BigInt::BigInt(int64_t value) : storage_(value) {}

BigInt::BigInt(Storage storage) : storage_(std::move(storage)) {}

bool BigInt::is_small() const {
    return std::holds_alternative<int64_t>(storage_);
}

int64_t BigInt::small_value() const {
    return std::get<int64_t>(storage_);
}

const BigInt::LargeValue& BigInt::large_value() const {
    return std::get<LargeValue>(storage_);
}

std::pair<bool, std::vector<uint32_t>> BigInt::signed_magnitude() const {
    if (is_small()) {
        int64_t small = small_value();
        return {small < 0, magnitude_from_int64(small)};
    }

    const auto& large = large_value();
    return {large.negative, large.limbs};
}

BigInt BigInt::from_signed_magnitude(bool negative, Magnitude limbs) {
    trim_magnitude(limbs);
    if (limbs.empty()) {
        return BigInt(0);
    }

    if (magnitude_fits_signed_small(limbs, negative)) {
        uint64_t magnitude = magnitude_to_uint64(limbs);
        if (negative) {
            if (magnitude == (uint64_t{1} << 63)) {
                return BigInt(std::numeric_limits<int64_t>::min());
            }
            return BigInt(-static_cast<int64_t>(magnitude));
        }
        return BigInt(static_cast<int64_t>(magnitude));
    }

    return BigInt(LargeValue{negative, std::move(limbs)});
}

BigInt::BigInt(std::string_view str) {
    if (str.empty()) {
        throw std::invalid_argument("Empty string is not a valid BigInt");
    }

    size_t index = 0;
    bool negative = false;
    if (str[index] == '-') {
        negative = true;
        ++index;
    } else if (str[index] == '+') {
        ++index;
    }

    if (index == str.size()) {
        throw std::invalid_argument("Invalid BigInt: sign without digits in \"" + std::string(str) + "\"");
    }

    Magnitude magnitude;
    for (; index < str.size(); ++index) {
        char c = str[index];
        if (c < '0' || c > '9') {
            throw std::invalid_argument("Invalid character in BigInt: \"" + std::string(str) + "\"");
        }
        multiply_small_in_place(magnitude, 10);
        add_small_in_place(magnitude, static_cast<uint32_t>(c - '0'));
    }

    *this = from_signed_magnitude(negative, std::move(magnitude));
}

std::string BigInt::to_string() const {
    if (is_small()) {
        return std::to_string(small_value());
    }

    const auto& large = large_value();
    Magnitude magnitude = large.limbs;
    std::vector<uint32_t> parts;
    while (!magnitude.empty()) {
        auto [quotient, remainder] = divide_magnitude_by_small(magnitude, DECIMAL_BASE);
        parts.push_back(remainder);
        magnitude = std::move(quotient);
    }

    std::ostringstream out;
    if (large.negative) {
        out << '-';
    }
    out << parts.back();
    for (size_t i = parts.size() - 1; i > 0; --i) {
        out << std::setw(9) << std::setfill('0') << parts[i - 1];
    }
    return out.str();
}

BigInt BigInt::operator-() const {
    if (is_small()) {
        int64_t value = small_value();
        if (value != std::numeric_limits<int64_t>::min()) {
            return BigInt(-value);
        }
        return from_signed_magnitude(false, magnitude_from_int64(value));
    }

    LargeValue flipped = large_value();
    flipped.negative = !flipped.negative;
    return from_signed_magnitude(flipped.negative, std::move(flipped.limbs));
}

BigInt BigInt::operator+() const {
    return *this;
}

BigInt BigInt::operator+(const BigInt& other) const {
    auto [left_negative, left_magnitude] = signed_magnitude();
    auto [right_negative, right_magnitude] = other.signed_magnitude();

    if (left_negative == right_negative) {
        return from_signed_magnitude(left_negative, add_magnitude(left_magnitude, right_magnitude));
    }

    int order = compare_magnitude(left_magnitude, right_magnitude);
    if (order == 0) {
        return BigInt(0);
    }
    if (order > 0) {
        return from_signed_magnitude(left_negative, subtract_magnitude(left_magnitude, right_magnitude));
    }
    return from_signed_magnitude(right_negative, subtract_magnitude(right_magnitude, left_magnitude));
}

BigInt BigInt::operator-(const BigInt& other) const {
    return *this + (-other);
}

BigInt BigInt::operator*(const BigInt& other) const {
    auto [left_negative, left_magnitude] = signed_magnitude();
    auto [right_negative, right_magnitude] = other.signed_magnitude();
    bool negative = left_negative != right_negative;
    return from_signed_magnitude(negative, multiply_magnitude(left_magnitude, right_magnitude));
}

BigInt BigInt::operator/(const BigInt& other) const {
    auto [dividend_negative, dividend_magnitude] = signed_magnitude();
    auto [divisor_negative, divisor_magnitude] = other.signed_magnitude();

    if (divisor_magnitude.empty()) {
        throw std::domain_error("Division by zero");
    }

    auto [quotient, _] = divide_magnitude(dividend_magnitude, divisor_magnitude);
    bool negative = !quotient.empty() && (dividend_negative != divisor_negative);
    return from_signed_magnitude(negative, std::move(quotient));
}

BigInt BigInt::operator%(const BigInt& other) const {
    auto [dividend_negative, dividend_magnitude] = signed_magnitude();
    auto [_, divisor_magnitude] = other.signed_magnitude();

    if (divisor_magnitude.empty()) {
        throw std::domain_error("Division by zero");
    }

    auto [__, remainder] = divide_magnitude(dividend_magnitude, divisor_magnitude);
    bool negative = !remainder.empty() && dividend_negative;
    return from_signed_magnitude(negative, std::move(remainder));
}

BigInt& BigInt::operator+=(const BigInt& other) {
    *this = *this + other;
    return *this;
}

BigInt& BigInt::operator-=(const BigInt& other) {
    *this = *this - other;
    return *this;
}

BigInt& BigInt::operator*=(const BigInt& other) {
    *this = *this * other;
    return *this;
}

BigInt& BigInt::operator/=(const BigInt& other) {
    *this = *this / other;
    return *this;
}

BigInt& BigInt::operator%=(const BigInt& other) {
    *this = *this % other;
    return *this;
}

bool BigInt::operator==(const BigInt& other) const {
    return (*this <=> other) == std::strong_ordering::equal;
}

std::strong_ordering BigInt::operator<=>(const BigInt& other) const {
    if (is_small() && other.is_small()) {
        if (small_value() < other.small_value()) {
            return std::strong_ordering::less;
        }
        if (small_value() > other.small_value()) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    auto [left_negative, left_magnitude] = signed_magnitude();
    auto [right_negative, right_magnitude] = other.signed_magnitude();
    return compare_signed_magnitude(left_negative, left_magnitude, right_negative, right_magnitude);
}

std::ostream& operator<<(std::ostream& os, const BigInt& val) {
    os << val.to_string();
    return os;
}

bool BigInt::fits_int64() const {
    return is_small();
}

int64_t BigInt::to_int64() const {
    if (!fits_int64()) {
        throw std::out_of_range("BigInt value does not fit in int64_t");
    }
    return small_value();
}

bool BigInt::fits_uint64() const {
    if (is_small()) {
        return small_value() >= 0;
    }

    const auto& large = large_value();
    return !large.negative && magnitude_fits_uint64(large.limbs);
}

uint64_t BigInt::to_uint64() const {
    if (!fits_uint64()) {
        throw std::out_of_range("BigInt value does not fit in uint64_t");
    }

    if (is_small()) {
        return static_cast<uint64_t>(small_value());
    }
    return magnitude_to_uint64(large_value().limbs);
}

} // namespace chirp::interpreter
