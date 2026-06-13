#include <gtest/gtest.h>

#include "chirp/vm.h"

#include <sstream>
#include <string_view>

namespace {

std::string run_vm(std::string_view source) {
    std::ostringstream out;
    auto session = chirp::vm::createSession(out, false);
    session->execute_source(std::string(source), "<test>");
    return out.str();
}

} // namespace

TEST(VmTest, BlockShadowingUsesLexicalLocals) {
    EXPECT_EQ(run_vm("let x = 1; let y = do { let x = 2; x }; y;\n"), "2\n");
}

TEST(VmTest, LambdaCapturesBlockLocal) {
    EXPECT_EQ(run_vm("let result = do { let x = 1; let f = () => x; f() }; result;\n"), "1\n");
}

TEST(VmTest, NestedLambdasCaptureThroughIntermediateClosure) {
    EXPECT_EQ(run_vm("let result = do { let x = 1; let mk = () => () => x; let f = mk(); f() }; result;\n"), "1\n");
}

TEST(VmTest, IfConsumesBooleanConditions) {
    EXPECT_EQ(run_vm("if (1 < 2) 3 else 4;\n"), "3\n");
}

TEST(VmTest, TopLevelRecursiveLambdaStillWorks) {
    EXPECT_EQ(run_vm("let fib = (n) => if (n < 2) n else fib(n-1) + fib(n-2); fib(10);\n"), "55\n");
}

TEST(VmTest, NamedArgumentsCanBeReordered) {
    EXPECT_EQ(run_vm("let subtract = (x, y) => x - y; subtract(y=4, x=10);\n"), "6\n");
}

TEST(VmTest, MixingNamedAndPositionalArgumentsIsRejected) {
    EXPECT_THROW(run_vm("let sum = (x, y) => x + y; sum(1, y=2);\n"), std::runtime_error);
}

TEST(VmTest, UnknownNamedArgumentIsRejected) {
    EXPECT_THROW(run_vm("let sum = (x, y) => x + y; sum(x=1, z=2);\n"), std::runtime_error);
}

TEST(VmTest, DuplicateNamedArgumentIsRejected) {
    EXPECT_THROW(run_vm("let sum = (x, y) => x + y; sum(x=1, x=2);\n"), std::runtime_error);
}

TEST(VmTest, MissingNamedArgumentIsRejected) {
    EXPECT_THROW(run_vm("let sum = (x, y) => x + y; sum(x=1);\n"), std::runtime_error);
}
