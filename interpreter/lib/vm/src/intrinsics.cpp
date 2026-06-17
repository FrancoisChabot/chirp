#include "intrinsics.h"
#include "chirp/vm.h"
#include <cassert>
#include <stdexcept>

namespace chirp {

Value intrinsic_register(vm& machine, std::span<const Value> args) {
    return Value(); // Return a default void Value (or whatever register returns)
}

Value intrinsic_same(vm& machine, std::span<const Value> args) {
    return Value(false, machine.get_bool_nature());
}

Value intrinsic_nature_of(vm& machine, std::span<const Value> args) {
    assert(args.size() == 1);
    
    // Get the nature of the passed argument
    NatureRef arg_nature = args[0].getNature();
    
    // Retrieve the "nature" NatureRef from the VM registry using the cached accessor
    NatureRef nature_nature = machine.get_nature_nature();
    
    return Value(arg_nature, nature_nature);
}

Value intrinsic_import(vm& machine, std::span<const Value> args) {
    assert(args.size() == 2);

    const Value& key_v = args[0];
    const Value& format_v = args[1];
    
    if (format_v.asString() == "__chirp_boot") {
        std::string_view key = key_v.asString();
        NatureRef intrinsic_nature = machine.get_intrinsic_nature();
        if (key == "nature.nature_of") {return Value(&intrinsic_nature_of, intrinsic_nature); }
        else if (key == "values.same") {return Value(&intrinsic_same, intrinsic_nature); }
        else if (key == "system.register") {return Value(&intrinsic_register, intrinsic_nature); }
    }
    
    throw std::runtime_error("unknown import format");
}

} // namespace chirp
