#include <gtest/gtest.h>
#include "chirp/frontend.h"



using namespace chirp::frontend;

TEST(ParserTest, BinaryPrecedence) {
    auto tokens = tokenize("x % 2 == 0");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(== (% x 2) 0)");
}

TEST(ParserTest, SymbolicConstant) {
    auto tokens = tokenize("#red == #blue");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(== #red #blue)");
}

TEST(ParserTest, CharacterLiteralUnicodeEscape) {
    auto tokens = tokenize("'\\u00AB'");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "'\\u00AB'");
}

TEST(ParserTest, LogicalAndComparison) {
    auto tokens = tokenize("x ∈ int && y > 3");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(&& (in x int) (> y 3))");
}

TEST(ParserTest, UnaryAndRange) {
    auto tokens = tokenize("!0..3"); 
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(.. (! 0) 3)");
}

TEST(ParserTest, PointerMutabilityOperators) {
    auto address = parse_expression(tokenize("&mut x"));
    auto pointer = parse_expression(tokenize("->mut int"));
    auto mutable_outer = parse_expression(tokenize("->mut ->int"));
    auto mutable_inner = parse_expression(tokenize("-> ->mut int"));
    auto mutable_both = parse_expression(tokenize("->mut ->mut int"));

    ASSERT_NE(address, nullptr);
    ASSERT_NE(pointer, nullptr);
    ASSERT_NE(mutable_outer, nullptr);
    ASSERT_NE(mutable_inner, nullptr);
    ASSERT_NE(mutable_both, nullptr);

    EXPECT_EQ(print_ast(*address), "(&mut x)");
    EXPECT_EQ(print_ast(*pointer), "(->mut int)");
    EXPECT_EQ(print_ast(*mutable_outer), "(->mut (-> int))");
    EXPECT_EQ(print_ast(*mutable_inner), "(-> (->mut int))");
    EXPECT_EQ(print_ast(*mutable_both), "(->mut (->mut int))");
}

TEST(ParserTest, IndirectedMutationStatement) {
    auto tokens = tokenize("let p: ->mut int = &mut x; *p = 1;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 2);
    EXPECT_EQ(print_ast(*stmts[0]), "(let p:(->mut int) = (&mut x))");
    EXPECT_EQ(print_ast(*stmts[1]), "(= (* p) 1)");
}

TEST(ParserTest, RangeOperators) {
    auto exclusive = parse_expression(tokenize("1..10"));
    auto inclusive_end = parse_expression(tokenize("1..=10"));

    ASSERT_NE(exclusive, nullptr);
    ASSERT_NE(inclusive_end, nullptr);

    EXPECT_EQ(print_ast(*exclusive), "(.. 1 10)");
    EXPECT_EQ(print_ast(*inclusive_end), "(..= 1 10)");
}

TEST(ParserTest, ParseExpressionRejectsTrailingTokens) {
    EXPECT_THROW(parse_expression(tokenize("foo bar")), std::runtime_error);
    EXPECT_THROW(parse_expression(tokenize("1 2")), std::runtime_error);
}

TEST(ParserTest, EnumeratedSet) {
    auto tokens = tokenize("{1, 2 + 3, 4}");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(set 1 (+ 2 3) 4)");
}

TEST(ParserTest, ConstructedSetWithBound) {
    auto tokens = tokenize("{x : int | x > 0}");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(c_set x : int | (> x 0))");
}

TEST(ParserTest, WhileExpression) {
    auto tokens = tokenize("while (x > 0) do { x -= 1; }");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(while (> x 0) (block (-= x 1)))");
}

TEST(ParserTest, ForExpression) {
    auto tokens = tokenize("for (x ∈ 1..10) do { x + 1; }");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(for x in (.. 1 10) (block (expr_stmt (+ x 1))))");
}

TEST(ParserTest, IfExpression) {
    auto tokens = tokenize("if (x > 3) 1 else 2");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(if (> x 3) 1 2)");
}

TEST(ParserTest, IfExpressionStatement) {
    auto tokens = tokenize("if (cond) a else b;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(expr_stmt (if cond a b))");
}

TEST(ParserTest, IfExpressionLetInitializer) {
    auto tokens = tokenize("let x = if (cond) a else b;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(let x = (if cond a b))");
}

TEST(ParserTest, IfExpressionRequiresElse) {
    EXPECT_THROW(parse_expression(tokenize("if (x > 3) 1")), std::runtime_error);
}

TEST(ParserTest, IfStatementWithoutElse) {
    auto tokens = tokenize("if (x != 1) do { x = 1; }");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(if_stmt (!= x 1) (expr_stmt (block (= x 1))))");
}

TEST(ParserTest, IfStatementWithElse) {
    auto tokens = tokenize("if (x) x = 1; else x = 2;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(if_stmt x (= x 1) (= x 2))");
}

TEST(ParserTest, LambdaNoBounds) {
    auto tokens = tokenize("(x, y) => x + y");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(lambda (x y) (+ x y))");
}

TEST(ParserTest, LambdaWithBounds) {
    auto tokens = tokenize("(x: int, y: float) : string => foo");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(lambda (x:int y:float):string foo)");
}

TEST(ParserTest, StructExpression) {
    auto tokens = tokenize("struct { x: int, y: int = 0 }");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(struct (field x:int) (field y:int = 0))");
}

TEST(ParserTest, StructExpressionTrailingComma) {
    auto tokens = tokenize("struct { x: int, f(y: int): int = y, }");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(struct (field x:int) (field f = (lambda (y:int):int y)))");
}

TEST(ParserTest, NestedLambda) {
    auto tokens = tokenize("(x) => (y) => x + y");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(lambda (x) (lambda (y) (+ x y)))");
}

TEST(ParserTest, LetStatement) {
    auto tokens = tokenize("let x = 5;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(let x = 5)");
}

TEST(ParserTest, LetStatementWithBound) {
    auto tokens = tokenize("let x: int = 5;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(let x:int = 5)");
}

TEST(ParserTest, MutLetStatement) {
    auto tokens = tokenize("let mut x = 5;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(let mut x = 5)");
}

TEST(ParserTest, LetFunctionSugar) {
    auto tokens = tokenize("let foo(x: int): int = do { break x + 1; };");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(let foo = (lambda (x:int):int (block (break (+ x 1)))))");
}

TEST(ParserTest, BlockExpression) {
    auto tokens = tokenize("let x = do { let y = 1; break y; };");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(let x = (block (let y = 1) (break y)))");
}

TEST(ParserTest, BraceDisambiguationPrefersSets) {
    auto empty = parse_expression(tokenize("{}"));
    auto singleton = parse_expression(tokenize("{x}"));

    ASSERT_NE(empty, nullptr);
    ASSERT_NE(singleton, nullptr);

    EXPECT_EQ(print_ast(*empty), "(set)");
    EXPECT_EQ(print_ast(*singleton), "(set x)");
}

TEST(ParserTest, BlockExpressionWithDo) {
    auto expr_stmt_block = parse_expression(tokenize("do { x; }"));
    auto break_block = parse_expression(tokenize("do { break; }"));

    ASSERT_NE(expr_stmt_block, nullptr);
    ASSERT_NE(break_block, nullptr);

    EXPECT_EQ(print_ast(*expr_stmt_block), "(block (expr_stmt x))");
    EXPECT_EQ(print_ast(*break_block), "(block (break `void))");

    // braces without do prefix is parsed as a set and should fail for statements/semicolons
    EXPECT_THROW(parse_expression(tokenize("{ x; }")), std::runtime_error);
    EXPECT_THROW(parse_expression(tokenize("{ break; }")), std::runtime_error);
}

TEST(ParserTest, NoopFunctionBodies) {
    auto block_tokens = tokenize("let noop_block() = do { break; };");
    auto value_tokens = tokenize("let noop_value() = `void;");

    auto block_stmts = parse(block_tokens);
    auto value_stmts = parse(value_tokens);

    ASSERT_EQ(block_stmts.size(), 1);
    ASSERT_EQ(value_stmts.size(), 1);

    EXPECT_EQ(print_ast(*block_stmts[0]), "(let noop_block = (lambda () (block (break `void))))");
    EXPECT_EQ(print_ast(*value_stmts[0]), "(let noop_value = (lambda () `void))");
}

TEST(ParserTest, ExpressionStatement) {
    auto tokens = tokenize("x + 1;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(expr_stmt (+ x 1))");
}

TEST(ParserTest, AssignmentStatement) {
    auto tokens = tokenize("x += 5;");
    auto stmts = parse(tokens);
    ASSERT_EQ(stmts.size(), 1);
    EXPECT_EQ(print_ast(*stmts[0]), "(+= x 5)");
}

TEST(ParserTest, MemberAccess) {
    auto tokens = tokenize("foo.bar");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(. foo bar)");
}

TEST(ParserTest, NumericMemberAccessAndMalformedDecimal) {
    auto expr = parse_expression(tokenize("1.foo"));
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(. 1 foo)");

    EXPECT_THROW(parse(tokenize("let x = 1.;")), std::runtime_error);
}

TEST(ParserTest, CallExpression) {
    auto tokens = tokenize("foo(1, 2)");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(call foo 1 2)");
}

TEST(ParserTest, CallExpressionNamed) {
    auto tokens = tokenize("foo(x=1, y=2)");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(call foo x=1 y=2)");
}

TEST(ParserTest, ConstructorSyntaxIsNamedCall) {
    auto tokens = tokenize("Point(x=1, y=2)");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(call Point x=1 y=2)");
}

TEST(ParserTest, PostfixStructLiteralSyntaxRejected) {
    EXPECT_THROW(parse_expression(tokenize("Point { x: 1 }")), std::runtime_error);
    EXPECT_THROW(parse(tokenize("let point = Point { x: 1 };")), std::runtime_error);
}

TEST(ParserTest, IndexExpression) {
    auto tokens = tokenize("matrix[x, y]");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(index matrix x y)");
}

TEST(ParserTest, ListExpression) {
    auto tokens = tokenize("[1, 2 + 3, ]");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(list 1 (+ 2 3))");
}

TEST(ParserTest, BraceDisambiguationIfExprInSet) {
    auto tokens = tokenize("{ if (x) a else b }");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(set (if x a b))");
}

TEST(ParserTest, BlockWithIfStmtNoDisambiguationRequired) {
    // Blocks explicitly started with `do` don't need any complex semicolon disambiguation
    auto expr = parse_expression(tokenize("do { if (x) do { break 1; } }"));
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(block (if_stmt x (expr_stmt (block (break 1)))))");

    // Without a semicolon, this if-else is parsed as the tail expression of the block
    auto expr_with_else = parse_expression(tokenize("do { if (x) do { break 1; } else do { break 2; } }"));
    ASSERT_NE(expr_with_else, nullptr);
    EXPECT_EQ(print_ast(*expr_with_else), "(block (break (if x (block (break 1)) (block (break 2)))))");

    // With a semicolon, it is parsed as an ExprStmt wrapping an IfExpr
    auto expr_with_else_stmt = parse_expression(tokenize("do { if (x) do { break 1; } else do { break 2; }; }"));
    ASSERT_NE(expr_with_else_stmt, nullptr);
    EXPECT_EQ(print_ast(*expr_with_else_stmt), "(block (expr_stmt (if x (block (break 1)) (block (break 2)))))");

    // If branches contain statements, it is parsed as a standard IfStmt
    auto expr_stmt_branches = parse_expression(tokenize("do { if (x) a = 1; else a = 2; }"));
    ASSERT_NE(expr_stmt_branches, nullptr);
    EXPECT_EQ(print_ast(*expr_stmt_branches), "(block (if_stmt x (= a 1) (= a 2)))");
}

TEST(ParserTest, BlockTailExpressionDesugaring) {
    auto tokens1 = tokenize("do { let y = 1; y }");
    auto expr1 = parse_expression(tokens1);
    ASSERT_NE(expr1, nullptr);
    EXPECT_EQ(print_ast(*expr1), "(block (let y = 1) (break y))");

    auto tokens2 = tokenize("do { x }");
    auto expr2 = parse_expression(tokens2);
    ASSERT_NE(expr2, nullptr);
    EXPECT_EQ(print_ast(*expr2), "(block (break x))");
}

TEST(ParserTest, MatchExpression) {
    auto tokens = tokenize("match x { 1 => #one, 2 => #two }");
    auto expr = parse_expression(tokens);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(print_ast(*expr), "(match x (=> 1 #one) (=> 2 #two))");
}


