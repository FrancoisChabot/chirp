#pragma once

#include "Value.h"
#include "nature.h"
#include <span>

namespace chirp {

class vm_Impl;

Value intrinsic_import(vm_Impl& machine, std::span<const Value> args);

Value intrinsic_register(vm_Impl& machine, std::span<const Value> args);
Value intrinsic_nature_of(vm_Impl& machine, std::span<const Value> args);
Value intrinsic_same(vm_Impl& machine, std::span<const Value> args);


} // namespace chirp
