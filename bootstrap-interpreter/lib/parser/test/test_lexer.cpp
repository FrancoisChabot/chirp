#include <gtest/gtest.h>
#include "chirp/parser.h"

using namespace chirp::parser;

TEST(LexerTest, BasicTokens) {
    auto tokens = tokenize("let mut x = 0..3;");
    ASSERT_EQ(tokens.size(), 9); 
    EXPECT_EQ(tokens[0].type, token_type::kw_let);
    EXPECT_EQ(tokens[1].type, token_type::kw_mut);
    EXPECT_EQ(tokens[2].type, token_type::identifier);
    EXPECT_EQ(tokens[3].type, token_type::equal);
    EXPECT_EQ(tokens[4].type, token_type::number);
    EXPECT_EQ(tokens[5].type, token_type::dot_dot);
    EXPECT_EQ(tokens[6].type, token_type::number);
    EXPECT_EQ(tokens[7].type, token_type::semicolon);
    EXPECT_EQ(tokens[8].type, token_type::eof);
}

TEST(LexerTest, UnicodeOperatorsAndColumns) {
    auto tokens = tokenize("x ∈ int && y ⊆ z");
    ASSERT_EQ(tokens.size(), 8); 
    
    EXPECT_EQ(tokens[0].lexeme, "x");
    EXPECT_EQ(tokens[0].column, 1);

    EXPECT_EQ(tokens[1].type, token_type::in_op);
    EXPECT_EQ(tokens[1].column, 3); // x(1), space(2), ∈(3)

    EXPECT_EQ(tokens[2].lexeme, "int");
    EXPECT_EQ(tokens[2].column, 5); // ∈ is 1 char wide, so space at 4, int at 5

    EXPECT_EQ(tokens[3].type, token_type::and_and);
    EXPECT_EQ(tokens[5].type, token_type::subset_op);
}

TEST(LexerTest, Intrinsics) {
    auto tokens = tokenize("`any `type");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, token_type::intrinsic);
    EXPECT_EQ(tokens[0].lexeme, "`any");
    EXPECT_EQ(tokens[1].type, token_type::intrinsic);
    EXPECT_EQ(tokens[1].lexeme, "`type");
}

TEST(LexerTest, RangeOperators) {
    auto tokens = tokenize("0..1 0..=1");
    ASSERT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[1].type, token_type::dot_dot);
    EXPECT_EQ(tokens[4].type, token_type::dot_dot_equal);
}

TEST(LexerTest, DecimalNumbersRequireFractionDigits) {
    auto tokens = tokenize("1.2 1.foo 1.;");
    ASSERT_EQ(tokens.size(), 8);

    EXPECT_EQ(tokens[0].type, token_type::number);
    EXPECT_EQ(tokens[0].lexeme, "1.2");

    EXPECT_EQ(tokens[1].type, token_type::number);
    EXPECT_EQ(tokens[1].lexeme, "1");
    EXPECT_EQ(tokens[2].type, token_type::dot);
    EXPECT_EQ(tokens[3].type, token_type::identifier);

    EXPECT_EQ(tokens[4].type, token_type::number);
    EXPECT_EQ(tokens[4].lexeme, "1");
    EXPECT_EQ(tokens[5].type, token_type::dot);
    EXPECT_EQ(tokens[6].type, token_type::semicolon);
}

TEST(LexerTest, CharacterLiteralsAreSingleUnicodeCodepoints) {
    auto tokens = tokenize("'a' '«' '😀' '\\n' '\\'' '\\\\' '\\u00AB'");
    ASSERT_EQ(tokens.size(), 8);

    for (size_t i = 0; i < 7; i++) {
        EXPECT_EQ(tokens[i].type, token_type::character);
    }

    EXPECT_EQ(tokens[0].lexeme, "'a'");
    EXPECT_EQ(tokens[1].lexeme, "'«'");
    EXPECT_EQ(tokens[2].lexeme, "'😀'");
    EXPECT_EQ(tokens[3].lexeme, "'\\n'");
    EXPECT_EQ(tokens[4].lexeme, "'\\''");
    EXPECT_EQ(tokens[5].lexeme, "'\\\\'");
    EXPECT_EQ(tokens[6].lexeme, "'\\u00AB'");
}

TEST(LexerTest, CharacterLiteralsRejectMalformedBodies) {
    auto tokens = tokenize("'' 'ab' '«»' '\\u00G1' '\\uD800' '\\x' 'a");
    ASSERT_EQ(tokens.size(), 8);

    for (size_t i = 0; i < 7; i++) {
        EXPECT_EQ(tokens[i].type, token_type::error);
    }
    EXPECT_EQ(tokens[7].type, token_type::eof);
}

TEST(LexerTest, Keywords) {
    auto tokens = tokenize("do let mut struct if else while for true false undecided break match");
    ASSERT_EQ(tokens.size(), 14);
    EXPECT_EQ(tokens[0].type, token_type::kw_do);
    EXPECT_EQ(tokens[1].type, token_type::kw_let);
    EXPECT_EQ(tokens[2].type, token_type::kw_mut);
    EXPECT_EQ(tokens[3].type, token_type::kw_struct);
    EXPECT_EQ(tokens[4].type, token_type::kw_if);
    EXPECT_EQ(tokens[5].type, token_type::kw_else);
    EXPECT_EQ(tokens[6].type, token_type::kw_while);
    EXPECT_EQ(tokens[7].type, token_type::kw_for);
    EXPECT_EQ(tokens[8].type, token_type::kw_true);
    EXPECT_EQ(tokens[9].type, token_type::kw_false);
    EXPECT_EQ(tokens[10].type, token_type::kw_undecided);
    EXPECT_EQ(tokens[11].type, token_type::kw_break);
    EXPECT_EQ(tokens[12].type, token_type::kw_match);
    EXPECT_EQ(tokens[13].type, token_type::eof);
}

TEST(LexerTest, LocationsAndComments) {
    auto tokens = tokenize("let x = 1;\n// comment\nx = 2;");
    ASSERT_EQ(tokens[0].line, 1);
    
    // x = 2; should be on line 3
    // tokens: let, x, =, 1, ;, x, =, 2, ;, eof
    ASSERT_EQ(tokens.size(), 10);
    EXPECT_EQ(tokens[5].lexeme, "x");
    EXPECT_EQ(tokens[5].line, 3);
    EXPECT_EQ(tokens[5].column, 1);
}
