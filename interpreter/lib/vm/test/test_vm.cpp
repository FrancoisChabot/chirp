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
