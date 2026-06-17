#pragma once

#include "Value.h"
#include "nature.h"
#include <span>

namespace chirp {

class vm;

Value intrinsic_import(vm& machine, std::span<const Value> args);

Value intrinsic_register(vm& machine, std::span<const Value> args);
Value intrinsic_nature_of(vm& machine, std::span<const Value> args);
Value intrinsic_same(vm& machine, std::span<const Value> args);


} // namespace chirp
