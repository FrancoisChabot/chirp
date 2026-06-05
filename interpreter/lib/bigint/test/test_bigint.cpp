#include <gtest/gtest.h>
#include "chirp/bigint.h"
#include <limits>
#include <stdexcept>

using namespace chirp::interpreter;

TEST(BigIntTest, DefaultConstructor) {
    BigInt a;
    EXPECT_EQ(a.to_string(), "0");
}

TEST(BigIntTest, Int64Constructor) {
    BigInt a(0);
    EXPECT_EQ(a.to_string(), "0");

    BigInt b(12345);
    EXPECT_EQ(b.to_string(), "12345");

    BigInt c(-98765);
    EXPECT_EQ(c.to_string(), "-98765");

    BigInt d(std::numeric_limits<int64_t>::max());
    EXPECT_EQ(d.to_string(), "9223372036854775807");

    BigInt e(std::numeric_limits<int64_t>::min());
    EXPECT_EQ(e.to_string(), "-9223372036854775808");
}

TEST(BigIntTest, StringConstructorValid) {
    EXPECT_EQ(BigInt("0").to_string(), "0");
    EXPECT_EQ(BigInt("+0").to_string(), "0");
    EXPECT_EQ(BigInt("-0").to_string(), "0");
    EXPECT_EQ(BigInt("12345678901234567890").to_string(), "12345678901234567890");
    EXPECT_EQ(BigInt("-12345678901234567890").to_string(), "-12345678901234567890");
    EXPECT_EQ(BigInt("18446744073709551615").to_string(), "18446744073709551615"); // UINT64_MAX
}

TEST(BigIntTest, StringConstructorInvalid) {
    EXPECT_THROW(BigInt(""), std::invalid_argument);
    EXPECT_THROW(BigInt("-"), std::invalid_argument);
    EXPECT_THROW(BigInt("+"), std::invalid_argument);
    EXPECT_THROW(BigInt("123a4"), std::invalid_argument);
    EXPECT_THROW(BigInt(" 123"), std::invalid_argument);
    EXPECT_THROW(BigInt("123 "), std::invalid_argument);
    EXPECT_THROW(BigInt("1.5"), std::invalid_argument);
}

TEST(BigIntTest, StringConstructorOverflow) {
    // 2^127 - 1 is 170141183460469231731687303715884105727 (39 digits)
    EXPECT_THROW(BigInt("170141183460469231731687303715884105728"), std::out_of_range); // Over positive limit by 1
    EXPECT_THROW(BigInt("-170141183460469231731687303715884105729"), std::out_of_range); // Under negative limit by 1
    EXPECT_NO_THROW(BigInt("-170141183460469231731687303715884105728")); // Exact negative min
    EXPECT_NO_THROW(BigInt("170141183460469231731687303715884105727"));  // Exact positive max
}

TEST(BigIntTest, Comparisons) {
    BigInt a(10);
    BigInt b(20);
    BigInt c(10);
    BigInt d(-5);

    EXPECT_TRUE(a == c);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a <= c);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b >= a);
    EXPECT_TRUE(b >= c);
    EXPECT_TRUE(d < a);
    EXPECT_TRUE(d <= d);
}

TEST(BigIntTest, ArithmeticBasic) {
    BigInt a(100);
    BigInt b(20);

    EXPECT_EQ((a + b).to_string(), "120");
    EXPECT_EQ((a - b).to_string(), "80");
    EXPECT_EQ((a * b).to_string(), "2000");
    EXPECT_EQ((a / b).to_string(), "5");
    EXPECT_EQ((a % b).to_string(), "0");

    BigInt c(3);
    EXPECT_EQ((a % c).to_string(), "1");
}

TEST(BigIntTest, ArithmeticNegative) {
    BigInt a(-15);
    BigInt b(4);

    EXPECT_EQ((a + b).to_string(), "-11");
    EXPECT_EQ((a - b).to_string(), "-19");
    EXPECT_EQ((a * b).to_string(), "-60");
    EXPECT_EQ((a / b).to_string(), "-3");
    EXPECT_EQ((a % b).to_string(), "-3");
}

TEST(BigIntTest, DivisionByZero) {
    BigInt a(10);
    BigInt zero(0);

    EXPECT_THROW(a / zero, std::domain_error);
    EXPECT_THROW(a % zero, std::domain_error);
}

TEST(BigIntTest, UnaryNegation) {
    BigInt a(123);
    EXPECT_EQ((-a).to_string(), "-123");

    BigInt b(-456);
    EXPECT_EQ((-b).to_string(), "456");

    BigInt c(0);
    EXPECT_EQ((-c).to_string(), "0");
}

TEST(BigIntTest, FitsAndConversions) {
    BigInt a(12345);
    EXPECT_TRUE(a.fits_int64());
    EXPECT_EQ(a.to_int64(), 12345);
    EXPECT_TRUE(a.fits_uint64());
    EXPECT_EQ(a.to_uint64(), 12345);

    BigInt b(-12345);
    EXPECT_TRUE(b.fits_int64());
    EXPECT_EQ(b.to_int64(), -12345);
    EXPECT_FALSE(b.fits_uint64());
    EXPECT_THROW(b.to_uint64(), std::out_of_range);

    BigInt c("18446744073709551615"); // UINT64_MAX
    EXPECT_FALSE(c.fits_int64());
    EXPECT_THROW(c.to_int64(), std::out_of_range);
    EXPECT_TRUE(c.fits_uint64());
    EXPECT_EQ(c.to_uint64(), std::numeric_limits<uint64_t>::max());

    BigInt d("-9223372036854775808"); // INT64_MIN
    EXPECT_TRUE(d.fits_int64());
    EXPECT_EQ(d.to_int64(), std::numeric_limits<int64_t>::min());
    EXPECT_FALSE(d.fits_uint64());
}
