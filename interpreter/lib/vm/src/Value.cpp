#include "Value.h"
#include <stdexcept>

namespace chirp {

Value::Value() : storage_(false), nature_(nullptr) {}

Value::Value(int val, NatureRef nature) : storage_(val), nature_(nature) {}

Value::Value(bool val, NatureRef nature) : storage_(val), nature_(nature) {}

Value::Value(std::string val, NatureRef nature) : storage_(std::move(val)), nature_(nature) {}

Value::Value(const char* val, NatureRef nature) : storage_(std::string(val)), nature_(nature) {}

Value::Value(NatureRef nature_val, NatureRef nature_nature)
    : storage_(nature_val), nature_(nature_nature) {}

Value::Value(IntrinsicFunctionPtr func, NatureRef nature_nature)
    : storage_(func), nature_(nature_nature) {}

bool Value::isInt() const { return std::holds_alternative<int>(storage_); }

int Value::asInt() const {
  if (!isInt()) {
    throw std::runtime_error("Value is not an int");
  }
  return std::get<int>(storage_);
}

bool Value::isString() const {
  return std::holds_alternative<std::string>(storage_);
}

std::string_view Value::asString() const {
  if (!isString()) {
    throw std::runtime_error("Value is not a string");
  }
  return std::get<std::string>(storage_);
}

bool Value::isBool() const { return std::holds_alternative<bool>(storage_); }

bool Value::asBool() const {
  if (!isBool()) {
    throw std::runtime_error("Value is not a bool");
  }
  return std::get<bool>(storage_);
}

bool Value::isNature() const {
  return std::holds_alternative<NatureRef>(storage_);
}

NatureRef Value::asNature() const {
  if (!isNature()) {
    throw std::runtime_error("Value is not a nature");
  }
  return std::get<NatureRef>(storage_);
}

bool Value::isIntrinsicFunction() const {
  return std::holds_alternative<IntrinsicFunctionPtr>(storage_);
}

IntrinsicFunctionPtr Value::asIntrinsicFunction() const {
  if (!isIntrinsicFunction()) {
    throw std::runtime_error("Value is not an intrinsic function");
  }
  return std::get<IntrinsicFunctionPtr>(storage_);
}

NatureRef Value::getNature() const { return nature_; }

bool Value::operator==(const Value &other) const {
  return nature_ == other.nature_ && storage_ == other.storage_;
}

} // namespace chirp
