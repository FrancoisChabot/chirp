#include <gtest/gtest.h>
#include "chirp/vm.h"
#include "Binding.h"
#include "Trait.h"
#include "compute_unit.h"
#include "bindings_table.h"
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
    TraitNature trait_nature;
    IntrinsicFunctionNature intrinsic_nature;

    EXPECT_EQ(int_nature.name(), "int");
    EXPECT_EQ(bool_nature.name(), "bool");
    EXPECT_EQ(string_nature.name(), "string");
    EXPECT_EQ(nature_nature.name(), "nature");
    EXPECT_EQ(trait_nature.name(), "trait");
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

TEST(ValueTest, TraitAsValue) {
    TraitNature trait_nature;
    IntNature int_nature;

    Value interface_value(42, &int_nature);
    Trait trait(9, interface_value);
    Value v(&trait, &trait_nature);

    EXPECT_TRUE(v.isTrait());
    EXPECT_FALSE(v.isNature());
    EXPECT_EQ(v.asTrait(), &trait);
    EXPECT_EQ(v.getNature(), &trait_nature);
    EXPECT_EQ(v.asTrait()->id(), 9u);
    EXPECT_EQ(v.asTrait()->interface(), interface_value);
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
    EXPECT_NE(virtual_machine.get_trait_nature(), nullptr);
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

TEST(VMTest, BindingsTable) {
    vm virtual_machine;

    auto& table = virtual_machine.get_bindings_table();
    EXPECT_EQ(table.size(), 0);

    Value fc1(42, virtual_machine.get_int_nature());
    Value cv1(7, virtual_machine.get_int_nature());
    Value fc2(true, virtual_machine.get_bool_nature());
    Value cv2(false, virtual_machine.get_bool_nature());

    Binding b1(fc1, cv1);
    Binding b2(fc2, cv2);

    size_t idx1 = table.register_binding("hello", b1);
    size_t idx2 = table.register_binding("world", b2);
    size_t idx3 = table.register_binding("hello", b2);

    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(idx3, 0);
    EXPECT_EQ(table.size(), 2);

    EXPECT_EQ(table.get_binding(0).get_fc(), fc1);
    EXPECT_EQ(table.get_binding(0).get_cv(), cv1);
    EXPECT_EQ(table.get_binding(1).get_fc(), fc2);
    EXPECT_EQ(table.get_binding(1).get_cv(), cv2);
    EXPECT_THROW(table.get_binding(2), std::out_of_range);

    // Update binding
    table.set_binding(0, b2);
    EXPECT_EQ(table.get_binding(0).get_fc(), fc2);
    EXPECT_EQ(table.get_binding(0).get_cv(), cv2);

    auto opt1 = table.lookup_index("hello");
    auto opt2 = table.lookup_index("foo");

    EXPECT_TRUE(opt1.has_value());
    EXPECT_EQ(*opt1, 0);
    EXPECT_FALSE(opt2.has_value());
}

TEST(VMTest, TraitsAndImplementations) {
    vm virtual_machine;

    Value interface_value(42, virtual_machine.get_int_nature());
    TraitRef trait = virtual_machine.create_trait(interface_value);
    ASSERT_NE(trait, nullptr);

    Value trait_value(trait, virtual_machine.get_trait_nature());
    EXPECT_TRUE(trait_value.isTrait());
    EXPECT_EQ(trait_value.asTrait(), trait);
    EXPECT_EQ(trait->interface(), interface_value);

    Value implementation_value(true, virtual_machine.get_bool_nature());
    virtual_machine.register_trait_implementation(trait, virtual_machine.get_int_nature(), implementation_value);

    auto implementation = virtual_machine.lookup_trait_implementation(trait, virtual_machine.get_int_nature());
    ASSERT_TRUE(implementation.has_value());
    EXPECT_EQ(*implementation, implementation_value);

    auto missing = virtual_machine.lookup_trait_implementation(trait, virtual_machine.get_string_nature());
    EXPECT_FALSE(missing.has_value());
}
