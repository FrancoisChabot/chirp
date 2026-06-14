#include <gtest/gtest.h>

#include "chirp/backend.h"
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

chirp::backend::SessionExpectations run_vm_expectations(std::string_view source) {
    std::ostringstream out;
    auto session = chirp::vm::createSession(out, true);
    session->execute_source(std::string(source), "<test>");
    return session->getExpectations();
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

TEST(VmTest, NegativeIntegerLiteralsWorkInOperandContexts) {
    EXPECT_EQ(run_vm("-128;\n"), "-128\n");
    EXPECT_EQ(run_vm("-128..=127;\n"), "-128..=127\n");
}

TEST(VmTest, BootIoHooksWork) {
    EXPECT_EQ(
        run_vm("let write = `import(\"io.write\", \"__chirp_boot\"); write(\"abc\", 1);\n"),
        "abc");
}

TEST(VmTest, BootInputHookUsesInjectedStdin) {
    EXPECT_EQ(
        run_vm("let inject_stdin = `import(\"testing.inject_stdin\", \"__chirp_boot\"); "
               "let input = `import(\"io.input\", \"__chirp_boot\"); "
               "let same = `import(\"values.same\", \"__chirp_boot\"); "
               "inject_stdin(\"hello\\n\"); if (same(input(), \"hello\")) 1 else 0;\n"),
        "1\n");
}

TEST(VmTest, BootTestingHooksPopulateExpectations) {
    auto expectations = run_vm_expectations(
        "let expect = `import(\"testing.expect\", \"__chirp_boot\"); "
        "let expect_stdout = `import(\"testing.expect_stdout\", \"__chirp_boot\"); "
        "let expect_exit = `import(\"testing.expect_exit\", \"__chirp_boot\"); "
        "expect(true); expect_stdout(\"abc\"); expect_exit(7);\n");
    EXPECT_TRUE(expectations.has_expectations);
    EXPECT_EQ(expectations.expectation_checks, 1);
    ASSERT_TRUE(expectations.expected_stdout.has_value());
    EXPECT_EQ(*expectations.expected_stdout, "abc");
    ASSERT_TRUE(expectations.expected_exit.has_value());
    EXPECT_EQ(*expectations.expected_exit, 7);
}

TEST(VmTest, BootExitHookThrowsScriptExit) {
    std::ostringstream out;
    auto session = chirp::vm::createSession(out, false);
    EXPECT_THROW(
        session->execute_source(
            "let exit = `import(\"system.exit\", \"__chirp_boot\"); exit(7);\n",
            "<test>"),
        chirp::backend::ScriptExit);
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

TEST(VmTest, PrimitiveStructFieldConstraintIsEnforced) {
    EXPECT_EQ(run_vm("let Point = struct { x: int, y: int }; let p = Point(x=1, y=2); p.x + p.y;\n"), "3\n");
    EXPECT_THROW(run_vm("let Point = struct { x: int, y: int }; Point(x=true, y=2);\n"), std::runtime_error);
}

TEST(VmTest, NestedStructConstraintCoercesAnonymousLiteral) {
    EXPECT_EQ(
        run_vm("let Point = struct { x: int, y: int }; let Rect = struct { top_left: Point, bottom_right: Point }; "
               "let r = Rect(top_left={x=1, y=2}, bottom_right={x=3, y=4}); r.top_left.y;\n"),
        "2\n");
}

TEST(VmTest, NestedStructConstraintRejectsInvalidAnonymousLiteral) {
    EXPECT_THROW(
        run_vm("let Point = struct { x: int, y: int }; let Rect = struct { top_left: Point, bottom_right: Point }; "
               "Rect(top_left={x=true, y=2}, bottom_right={x=3, y=4});\n"),
        std::runtime_error);
}

TEST(VmTest, StructDefaultConstraintIsEnforced) {
    EXPECT_THROW(run_vm("let Point = struct { x: int = true, y: int = 2 }; Point();\n"), std::runtime_error);
}

TEST(VmTest, IsStructTypeHostHookWorks) {
    EXPECT_EQ(
        run_vm("let is_struct_type = `import(\"types.is_struct_type\", \"__chirp_boot\"); let Point = struct { x: int, y: int }; "
               "if (is_struct_type(Point)) 1 else 0;\n"),
        "1\n");
}

TEST(VmTest, ConstructionArgsHostHookReturnsStructType) {
    EXPECT_EQ(
        run_vm("let construction_args = `import(\"types.construction_args\", \"__chirp_boot\"); "
               "let is_struct_type = `import(\"types.is_struct_type\", \"__chirp_boot\"); "
               "let Point = struct { x: int, y: int = 4 }; "
               "if (is_struct_type(construction_args(Point))) 1 else 0;\n"),
        "1\n");
}

TEST(VmTest, ConstructHostHookBuildsStructValue) {
    EXPECT_EQ(
        run_vm("let construct = `import(\"types.construct\", \"__chirp_boot\"); "
               "let Point = struct { x: int, y: int = 4 }; "
               "let p = construct(Point, {x=3}); p.x + p.y;\n"),
        "7\n");
}

TEST(VmTest, ConstructHostHookRejectsInvalidBundle) {
    EXPECT_THROW(
        run_vm("let construct = `import(\"types.construct\", \"__chirp_boot\"); "
               "let Point = struct { x: int, y: int }; "
               "construct(Point, {x=true, y=2});\n"),
        std::runtime_error);
}

TEST(VmTest, EnumeratedSetMembershipWorks) {
    EXPECT_EQ(run_vm("if (2 ∈ {1, 2, 3}) 1 else 0;\n"), "1\n");
}

TEST(VmTest, ConstructedSetMembershipWorks) {
    EXPECT_EQ(run_vm("if (2 ∈ {x: int | x < 3}) 1 else 0;\n"), "1\n");
}

TEST(VmTest, ForLoopCanMutateOuterLocal) {
    EXPECT_EQ(run_vm("let mut sum = 0; for (x ∈ {1, 2, 3}) do { sum = sum + x; }; sum;\n"), "6\n");
}

TEST(VmTest, MintFiniteProducesTypedValues) {
    EXPECT_EQ(
        run_vm("let mint_finite = `import(\"types.mint_finite\", \"__chirp_boot\"); "
               "let type_of = `import(\"types.type_of\", \"__chirp_boot\"); "
               "let same = `import(\"values.same\", \"__chirp_boot\"); "
               "let minted = mint_finite(2); "
               "if (same(type_of(minted.values[0]), minted.type)) 1 else 0;\n"),
        "1\n");
}

TEST(VmTest, TraitRegistryHooksWork) {
    EXPECT_EQ(
        run_vm("let make_trait = `import(\"traits.make\", \"__chirp_boot\"); "
               "let implement = `import(\"traits.implement\", \"__chirp_boot\"); "
               "let implements = `import(\"traits.implements\", \"__chirp_boot\"); "
               "let type_of = `import(\"types.type_of\", \"__chirp_boot\"); "
               "let MyTrait = make_trait(struct { invoke: () -> int }); "
               "implement(trait=MyTrait, on=type_of(1), impl={invoke=() => 1}); "
               "if (implements(MyTrait, type_of(1))) 1 else 0;\n"),
        "1\n");
}

TEST(VmTest, HeapCreateAndDerefWork) {
    EXPECT_EQ(
        run_vm("let heap_create = `import(\"memory.heap_create\", \"__chirp_boot\"); "
               "let p = heap_create(3); *p;\n"),
        "3\n");
}

TEST(VmTest, HeapDerefAssignmentWorks) {
    EXPECT_EQ(
        run_vm("let heap_create = `import(\"memory.heap_create\", \"__chirp_boot\"); "
               "let p = heap_create(3); do { *p = 7; }; *p;\n"),
        "7\n");
}

TEST(VmTest, SignatureFieldConstraintAcceptsMatchingLambda) {
    EXPECT_EQ(
        run_vm("let Iface = struct { deref: (self) -> any }; let x = Iface(deref=(self) => self); 1;\n"),
        "1\n");
}

TEST(VmTest, CompositeCallableConstraintAcceptsLambda) {
    EXPECT_EQ(
        run_vm("let is_pure = `import(\"compute.is_pure\", \"__chirp_boot\"); "
               "let pure_fn = { v | is_pure(v) }; "
               "let Iface = struct { deref: pure_fn ∩ (self) -> any }; "
               "let x = Iface(deref=(self) => self); 1;\n"),
        "1\n");
}
