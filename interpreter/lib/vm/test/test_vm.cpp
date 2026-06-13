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

TEST(VmTest, StructPositionalConstructionWorks) {
    EXPECT_EQ(run_vm("let Point = struct { x: int, y: int }; let p = Point(1, 2); p.x + p.y;\n"), "3\n");
}

TEST(VmTest, StructNamedConstructionCanBeReordered) {
    EXPECT_EQ(run_vm("let Point = struct { x: int, y: int }; let p = Point(y=2, x=1); p.x + p.y;\n"), "3\n");
}

TEST(VmTest, StructDefaultsAreApplied) {
    EXPECT_EQ(run_vm("let Point3D = struct { x: int, y: int, z: int = 4 }; let p = Point3D(1, 2); p.z;\n"), "4\n");
}

TEST(VmTest, StructDefaultCanCaptureLexicalValues) {
    EXPECT_EQ(run_vm("let base = 7; let Point = struct { x: int = base, y: int = 2 }; let p = Point(); p.x + p.y;\n"), "9\n");
}

TEST(VmTest, AnonymousStructLiteralProducesRuntimeStruct) {
    EXPECT_EQ(run_vm("let p = {x=1, y=2}; p.x + p.y;\n"), "3\n");
}

TEST(VmTest, NestedStructFieldAccessWorks) {
    EXPECT_EQ(
        run_vm("let Point = struct { x: int, y: int }; let Rect = struct { top_left: Point, bottom_right: Point }; "
               "let r = Rect(top_left={x=1, y=10}, bottom_right={x=3, y=4}); r.top_left.x + r.bottom_right.y;\n"),
        "5\n");
}

TEST(VmTest, StructUnknownFieldIsRejected) {
    EXPECT_THROW(run_vm("let Point = struct { x: int, y: int }; Point(x=1, z=2);\n"), std::runtime_error);
}

TEST(VmTest, StructMissingFieldIsRejected) {
    EXPECT_THROW(run_vm("let Point = struct { x: int, y: int }; Point(x=1);\n"), std::runtime_error);
}

TEST(VmTest, StructDuplicateNamedFieldIsRejected) {
    EXPECT_THROW(run_vm("let Point = struct { x: int, y: int }; Point(x=1, x=2);\n"), std::runtime_error);
}

TEST(VmTest, AnonymousStructDuplicateFieldIsRejected) {
    EXPECT_THROW(run_vm("let p = {x=1, x=2}; p.x;\n"), std::runtime_error);
}
