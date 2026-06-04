#include <gtest/gtest.h>
#include "chirp/interpreter.h"

using namespace chirp::interpreter;

// 1. Basic type tag identity (typeof(v))
TEST(InterpreterTest, TypeTagIdentity) {
    EXPECT_EQ(True().getType(), getBoolType());
    EXPECT_EQ(False().getType(), getBoolType());
    EXPECT_EQ(UndecidedVal().getType(), getUndecidedType());
    EXPECT_EQ(Value::make_int(42).getType(), getIntType());
    EXPECT_EQ(Value::make_string("hello").getType(), getStringType());
    
    // typeof(Bool) == Type
    EXPECT_EQ(Bool().getType(), getMetaType());

    // typeof(Undecided) == Type
    EXPECT_EQ(Undecided().getType(), getMetaType());
    
    // typeof(Type) == Type
    EXPECT_EQ(TypeVal().getType(), getMetaType());
    
    // typeof(any) == AnyType
    EXPECT_EQ(Any().getType(), getAnyType());
    // typeof(AnyType) == Type
    EXPECT_EQ(AnyTypeVal().getType(), getMetaType());

    // typeof(empty) == EmptyType
    EXPECT_EQ(Empty().getType(), getEmptyType());
    // typeof(EmptyType) == Type
    EXPECT_EQ(EmptyTypeVal().getType(), getMetaType());

    // typeof(set) == SetType
    EXPECT_EQ(Set().getType(), getSetType());
    // typeof(SetType) == Type
    EXPECT_EQ(SetTypeVal().getType(), getMetaType());

    // typeof(`void) == Void
    EXPECT_EQ(VoidVal().getType(), getVoidType());
    // typeof(Void) == Type
    EXPECT_EQ(Void().getType(), getMetaType());

    Value host_print = Value::make_host_function(Value::HostFunction::Print);
    EXPECT_EQ(host_print.getType(), getFunctionType());
    EXPECT_TRUE(host_print.isHostFunction());
    EXPECT_EQ(host_print.asHostFunction(), Value::HostFunction::Print);
}

// 2. Void is equal to itself (reflexive)
TEST(InterpreterTest, VoidEquality) {
    Value v1;
    Value v2;
    
    EXPECT_TRUE(v1.isVoid());
    EXPECT_TRUE(v2.isVoid());
    EXPECT_TRUE(VoidVal().isVoid());

    // void is equal to itself
    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 != v2);
    EXPECT_TRUE(VoidVal() == VoidVal());
    EXPECT_FALSE(VoidVal() != VoidVal());

    // void is not equal to non-void values
    EXPECT_FALSE(v1 == True());
    EXPECT_TRUE(v1 != True());
}

// 3. Set-ness belonging predicate checks
TEST(InterpreterTest, BelongingPredicate) {
    // true ∈ Bool  -->  typeof(Bool).bp(Bool, true) -> Type.bp(Bool, true)
    EXPECT_EQ(belongsTo(Bool(), True()), Value::make_bool(true));
    EXPECT_EQ(belongsTo(Bool(), False()), Value::make_bool(true));
    EXPECT_EQ(belongsTo(Bool(), UndecidedVal()), Value::make_bool(false));
    
    // 42 ∈ Bool -> false
    EXPECT_EQ(belongsTo(Bool(), Value::make_int(42)), Value::make_bool(false));

    // undecided ∈ Undecided -> true
    EXPECT_EQ(belongsTo(Undecided(), UndecidedVal()), Value::make_bool(true));
    EXPECT_EQ(belongsTo(Undecided(), True()), Value::make_bool(false));

    // true ∈ any -> true
    EXPECT_EQ(belongsTo(Any(), True()), Value::make_bool(true));
    EXPECT_EQ(belongsTo(Any(), Value::make_int(42)), Value::make_bool(true));

    // true ∈ empty -> false
    EXPECT_EQ(belongsTo(Empty(), True()), Value::make_bool(false));
    EXPECT_EQ(belongsTo(Empty(), Value::make_int(42)), Value::make_bool(false));

    // Bool ∈ set -> true (since Bool's type is Type, which has setness)
    EXPECT_EQ(belongsTo(Set(), Bool()), Value::make_bool(true));
    EXPECT_EQ(belongsTo(Set(), TypeVal()), Value::make_bool(true));

    // true ∈ set -> false (since True's type is Bool, which does not have setness)
    EXPECT_EQ(belongsTo(Set(), True()), Value::make_bool(false));
    EXPECT_EQ(belongsTo(Set(), UndecidedVal()), Value::make_bool(false));
}

// 4. Enumerated sets
TEST(InterpreterTest, EnumeratedSets) {
    std::vector<Value> elems = { Value::make_int(1), Value::make_int(2), Value::make_int(3) };
    Value my_set = Value::make_enumerated_set(elems);

    EXPECT_TRUE(my_set.isEnumeratedSet());
    EXPECT_EQ(my_set.asEnumeratedSet().size(), 3);
    EXPECT_EQ(my_set.toString(), "{1, 2, 3}");

    // 2 ∈ {1, 2, 3} -> true
    EXPECT_EQ(belongsTo(my_set, Value::make_int(2)), Value::make_bool(true));
    // 4 ∈ {1, 2, 3} -> false
    EXPECT_EQ(belongsTo(my_set, Value::make_int(4)), Value::make_bool(false));
}

// 5. Binding core mechanics
TEST(InterpreterTest, Bindings) {
    // let x : {1, 2, 3} = 2;
    std::vector<Value> elems = { Value::make_int(1), Value::make_int(2), Value::make_int(3) };
    Value fc = Value::make_enumerated_set(elems);
    Value lc = fc; // initially local constraint is the same
    Value cv = Value::make_int(2);

    auto binding = std::make_shared<Binding>(fc, lc, cv);

    EXPECT_EQ(binding->getFC(), fc);
    EXPECT_EQ(binding->getLC(), lc);
    EXPECT_EQ(binding->getCV(), cv);

    // Update cv
    binding->setCV(Value::make_int(3));
    EXPECT_EQ(binding->getCV(), Value::make_int(3));

    // Wrap binding in a Value
    Value val = Value::make_binding(binding);
    EXPECT_TRUE(val.isBinding());
    EXPECT_EQ(val.asBinding(), binding);
}

// 6. Set belonging range checks
TEST(InterpreterTest, BelongingRange) {
    // any's belonging range is {true}
    Value any_br = belongsRange(Any(), Value::make_type(True().getType()));
    EXPECT_TRUE(any_br.isEnumeratedSet());
    EXPECT_EQ(any_br.asEnumeratedSet().size(), 1);
    EXPECT_EQ(any_br.asEnumeratedSet()[0], Value::make_bool(true));

    // empty's belonging range is {false}
    Value empty_br = belongsRange(Empty(), Value::make_type(True().getType()));
    EXPECT_TRUE(empty_br.isEnumeratedSet());
    EXPECT_EQ(empty_br.asEnumeratedSet().size(), 1);
    EXPECT_EQ(empty_br.asEnumeratedSet()[0], Value::make_bool(false));
}
