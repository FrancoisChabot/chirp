#pragma once

#include "chirp/vm.h"
#include "vm_impl.h"

namespace chirp {

class vm_accessor {
public:
    static vm_Impl& get_impl(vm& machine) {
        return *machine.impl_;
    }
};

} // namespace chirp
