#include <gtest/gtest.h>
#include "chirp/vm.h"
#include "compute_unit.h"
#include "Value.h"
#include "nature.h"
#include "intrinsics.h"
#include <span>
#include <vector>

using namespace chirp;

TEST(NatureTest, BasicNatures) {
    IntNature int_nature;
    BoolNature bool_nature;
    StringNature string_nature;
    NatureNature nature_nature;
    IntrinsicFunctionNature intrinsic_nature;

    EXPECT_EQ(int_nature.name(), "int");
    EXPECT_EQ(bool_nature.name(), "bool");
    EXPECT_EQ(string_nature.name(), "string");
    EXPECT_EQ(nature_nature.name(), "nature");
    EXPECT_EQ(intrinsic_nature.name(), "IntrinsicFunction");
}

TEST(ValueTest, IntValue) {
    IntNature int_nature;
    NatureRef int_ref = &int_nature;

    Value v(42, int_ref);
    EXPECT_TRUE(v.isInt());
    EXPECT_FALSE(v.isBool());
    EXPECT_EQ(v.asInt(), 42);
    EXPECT_THROW(v.asBool(), std::runtime_error);
    EXPECT_EQ(v.getNature(), int_ref);
}

TEST(ValueTest, BoolValue) {
    BoolNature bool_nature;
    NatureRef bool_ref = &bool_nature;

    Value v(true, bool_ref);
    EXPECT_FALSE(v.isInt());
    EXPECT_TRUE(v.isBool());
    EXPECT_TRUE(v.asBool());
    EXPECT_THROW(v.asInt(), std::runtime_error);
    EXPECT_EQ(v.getNature(), bool_ref);
}

TEST(ValueTest, StringValue) {
    StringNature string_nature;
    NatureRef string_ref = &string_nature;

    Value v("hello", string_ref);
    EXPECT_TRUE(v.isString());
    EXPECT_FALSE(v.isInt());
    EXPECT_EQ(v.asString(), "hello");
    EXPECT_THROW(v.asInt(), std::runtime_error);
    EXPECT_EQ(v.getNature(), string_ref);
}

TEST(ValueTest, NatureAsValue) {
    NatureNature nature_nature;
    IntNature int_nature;
    NatureRef nature_ref = &nature_nature;
    NatureRef int_ref = &int_nature;

    Value v(int_ref, nature_ref);
    EXPECT_TRUE(v.isNature());
    EXPECT_FALSE(v.isBool());
    EXPECT_EQ(v.asNature(), int_ref);
    EXPECT_EQ(v.getNature(), nature_ref);
}

TEST(ValueTest, IntrinsicFunctionAsValue) {
    IntrinsicFunctionNature intrinsic_nature;
    NatureRef intrinsic_ref = &intrinsic_nature;

    Value v(intrinsic_nature_of, intrinsic_ref);
    EXPECT_TRUE(v.isIntrinsicFunction());
    EXPECT_EQ(v.asIntrinsicFunction(), intrinsic_nature_of);
    EXPECT_EQ(v.getNature(), intrinsic_ref);
}

TEST(ValueTest, Equality) {
    IntNature int_nature;
    BoolNature bool_nature;
    NatureRef int_ref = &int_nature;
    NatureRef bool_ref = &bool_nature;

    Value v1(42, int_ref);
    Value v2(42, int_ref);
    Value v3(43, int_ref);
    Value v4(true, bool_ref);
    Value v5(false, bool_ref);

    EXPECT_EQ(v1, v2);
    EXPECT_NE(v1, v3);
    EXPECT_NE(v1, v4);
    EXPECT_EQ(v4, Value(true, bool_ref));
    EXPECT_NE(v4, v5);
}


TEST(VMTest, PreallocatedNaturesAndCaches) {
    vm virtual_machine;

    EXPECT_NE(virtual_machine.get_int_nature(), nullptr);
    EXPECT_NE(virtual_machine.get_bool_nature(), nullptr);
    EXPECT_NE(virtual_machine.get_string_nature(), nullptr);
    EXPECT_NE(virtual_machine.get_nature_nature(), nullptr);
}

TEST(VMTest, ComputeUnitInstructions) {
    vm virtual_machine;

    auto& cu = virtual_machine.get_compute_unit();
    EXPECT_TRUE(cu.instructions.empty());

    instruction inst;
    inst.op = opcode::noop;
    cu.instructions.push_back(inst);

    EXPECT_EQ(cu.instructions.size(), 1);
    EXPECT_EQ(cu.instructions[0].op, opcode::noop);
}

TEST(IntrinsicTest, NatureOfIntrinsic) {
    vm virtual_machine;

    // Arg to nature_of (uses cached int nature)
    Value arg(42, virtual_machine.get_int_nature());
    std::vector<Value> args = {arg};
    std::span<const Value> args_span(args);

    Value result = intrinsic_nature_of(virtual_machine, args_span);
    EXPECT_TRUE(result.isNature());
    EXPECT_EQ(result.asNature(), virtual_machine.get_int_nature());
    EXPECT_EQ(result.getNature(), virtual_machine.get_nature_nature());
}

TEST(IntrinsicTest, ImportIntrinsic) {
    vm virtual_machine;

    Value key("nature.nature_of", virtual_machine.get_string_nature());
    Value format("__chirp_boot", virtual_machine.get_string_nature());

    std::vector<Value> args = {key, format};
    std::span<const Value> args_span(args);

    Value imported = intrinsic_import(virtual_machine, args_span);
    EXPECT_TRUE(imported.isIntrinsicFunction());
    EXPECT_EQ(imported.asIntrinsicFunction(), intrinsic_nature_of);
    EXPECT_EQ(imported.getNature(), virtual_machine.get_intrinsic_nature());
}
