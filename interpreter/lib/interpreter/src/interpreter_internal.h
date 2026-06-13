#pragma once

#include "value.h"
#include "type.h"
#include "binding.h"

#include <memory>
#include <string>
#include <vector>

namespace chirp::interpreter {

// Getters for the core predefined interpreter values and types

// Core Types (as std::shared_ptr<const Type>)
std::shared_ptr<const Type> getBoolType();

std::shared_ptr<const Type> getMetaType(); // represents the "Type" type

std::shared_ptr<const Type> getSetType();
std::shared_ptr<const Type> getVoidType();
std::shared_ptr<const Type> getEnumeratedSetType();
std::shared_ptr<const Type> getRangeType();
std::shared_ptr<const Type> getConstructedSetType();
std::shared_ptr<const Type> getCompositeSetType();
std::shared_ptr<const Type> getBindingType();
std::shared_ptr<const Type> getFunctionType();

// Convenient types for literals
std::shared_ptr<const Type> getIntType();
std::shared_ptr<const Type> getCharType();
std::shared_ptr<const Type> getStringType();
std::shared_ptr<const Type> getSymbolType();
std::shared_ptr<const Type> getListType();
std::shared_ptr<const Type> getTraitType();
std::shared_ptr<const Type> getModuleType();
std::shared_ptr<const Type> getHeapAllocationType();
std::shared_ptr<const Type> getHeapSharedAllocationType();
std::shared_ptr<const Type> getEnumFamilyType();
std::shared_ptr<const Type> getEnumVariantType();


// Core Values (as Value objects)
const Value& Bool();
const Value& True();
const Value& False();

const Value& TypeVal();

const Value& Set();
const Value& SetTypeVal();
const Value& Void();
const Value& VoidVal();

// Set belonging helper: typeof(S).bp(S, v)
Value belongsTo(const Value& S, const Value& v);

// Set range helper: typeof(S).br(S, lc)
Value belongsRange(const Value& S, const Value& lc);

} // namespace chirp::interpreter
