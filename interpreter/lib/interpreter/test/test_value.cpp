#include <gtest/gtest.h>
#include "chirp/interpreter.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace chirp::interpreter;

namespace {

namespace fs = std::filesystem;

struct TempDir {
    fs::path path;

    TempDir() {
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("chirp_module_test_" + std::to_string(stamp));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("could not write test file: " + path.string());
    }
    out << contents;
}

} // namespace

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

    Value trait = Value::make_trait(1, Set());
    EXPECT_EQ(trait.getType(), getTraitType());
    EXPECT_TRUE(trait.isTrait());
    EXPECT_EQ(trait.asTraitId(), 1);
    EXPECT_EQ(trait.asTraitInterface(), Set());
    EXPECT_EQ(belongsTo(Set(), trait), Value::make_bool(true));
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

TEST(InterpreterTest, BootPublicBindingsArePublishedAndPrivateBindingsAreHidden) {
    std::ostringstream out;
    Session session(out);

    session.execute_boot_source(
        "let pub final exposed = 41;\n"
        "let final hidden = 1;\n",
        "boot-one");
    session.execute_boot_source(
        "let pub final from_hidden = hidden + 1;\n",
        "boot-two");

    EXPECT_NO_THROW(session.execute_source(
        "let x : {41} = exposed;\n"
        "let y : {2} = from_hidden;\n",
        "script"));
    EXPECT_THROW(session.execute_source("hidden;\n", "script-hidden"), std::runtime_error);
}

TEST(InterpreterTest, ModulePrivateExportsAreHidden) {
    TempDir temp;
    write_file(temp.path / "mod.chirp",
        "let hidden = 1;\n"
        "let pub visible = 2;\n");

    std::ostringstream out;
    Session session(out);

    EXPECT_THROW(session.execute_source(
        "let m = `import(\"./mod\");\n"
        "let visible : {2} = m.visible;\n"
        "m.hidden;\n",
        (temp.path / "main.chirp").string()), std::runtime_error);
}

TEST(InterpreterTest, ModuleImportCycleIsDetected) {
    TempDir temp;
    write_file(temp.path / "a.chirp",
        "let b = `import(\"./b\");\n"
        "let pub value = 1;\n");
    write_file(temp.path / "b.chirp",
        "let a = `import(\"./a\");\n"
        "let pub value = 2;\n");

    std::ostringstream out;
    Session session(out);

    EXPECT_THROW(session.execute_source(
        "let a = `import(\"./a\");\n",
        (temp.path / "main.chirp").string()), std::runtime_error);
}

TEST(InterpreterTest, ModuleFailureIsCached) {
    TempDir temp;
    fs::path broken = temp.path / "broken.chirp";
    write_file(broken, "missing;\n");

    std::ostringstream out;
    Session session(out);

    EXPECT_THROW(session.execute_source(
        "let broken = `import(\"./broken\");\n",
        (temp.path / "first.chirp").string()), std::runtime_error);

    write_file(broken, "let pub x = 1;\n");

    EXPECT_THROW(session.execute_source(
        "let broken = `import(\"./broken\");\n"
        "let x : {1} = broken.x;\n",
        (temp.path / "second.chirp").string()), std::runtime_error);
}

TEST(InterpreterTest, StdImportsUseConfiguredChirpRoot) {
    TempDir temp;
    write_file(temp.path / "std" / "sample.chirp",
        "let private = 40;\n"
        "let pub answer = private + 2;\n");

    std::ostringstream out;
    Session session(out);
    session.set_chirp_root(temp.path.string());

    EXPECT_NO_THROW(session.execute_source(
        "let sample = `import(\"std/sample\");\n"
        "let answer : {42} = sample.answer;\n",
        (temp.path / "main.chirp").string()));
}

TEST(InterpreterTest, NonStdLogicalImportsAreRejectedInV1) {
    std::ostringstream out;
    Session session(out);

    EXPECT_THROW(session.execute_source(
        "let sample = `import(\"mylib/sample\");\n",
        "main.chirp"), std::runtime_error);
}
