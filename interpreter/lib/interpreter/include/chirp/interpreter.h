#pragma once

#include "value.h"
#include "type.h"
#include "binding.h"

#include <iosfwd>
#include <memory>
#include <vector>

namespace chirp::frontend {
class Stmt;
}

namespace chirp::interpreter {

// Getters for the 12 core predefined interpreter values and types

// Core Types (as std::shared_ptr<const Type>)
std::shared_ptr<const Type> getBoolType();
std::shared_ptr<const Type> getMetaType(); // represents the "Type" type
std::shared_ptr<const Type> getAnyType();
std::shared_ptr<const Type> getEmptyType();
std::shared_ptr<const Type> getSetType();
std::shared_ptr<const Type> getVoidType();
std::shared_ptr<const Type> getEnumeratedSetType();
std::shared_ptr<const Type> getRangeType();
std::shared_ptr<const Type> getConstructedSetType();
std::shared_ptr<const Type> getBindingType();
std::shared_ptr<const Type> getFunctionType();

// Convenient types for literals
std::shared_ptr<const Type> getIntType();
std::shared_ptr<const Type> getStringType();

// Core Values (as Value objects)
const Value& Bool();
const Value& True();
const Value& False();
const Value& TypeVal();
const Value& Any();
const Value& AnyTypeVal();
const Value& Empty();
const Value& EmptyTypeVal();
const Value& Set();
const Value& SetTypeVal();
const Value& Void();
const Value& VoidVal();

// Set belonging helper: typeof(S).bp(S, v)
Value belongsTo(const Value& S, const Value& v);

// Set range helper: typeof(S).br(S, lc)
Value belongsRange(const Value& S, const Value& lc);

// Execute a parsed Chirp script.
void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::ostream& out);

} // namespace chirp::interpreter
