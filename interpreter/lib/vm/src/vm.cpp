#include "chirp/vm.h"
#include "vm_impl.h"
#include "Value.h"
#include "nature.h"
#include <stdexcept>

namespace chirp {

vm_Impl::vm_Impl() {
    auto int_n = std::make_unique<IntNature>();
    auto bool_n = std::make_unique<BoolNature>();
    auto string_n = std::make_unique<StringNature>();
    auto nature_n = std::make_unique<NatureNature>();
    auto intrinsic_n = std::make_unique<IntrinsicFunctionNature>();
    
    int_nature_cache_ = int_n.get();
    bool_nature_cache_ = bool_n.get();
    string_nature_cache_ = string_n.get();
    nature_nature_cache_ = nature_n.get();
    intrinsic_nature_cache_ = intrinsic_n.get();

    natures_["int"] = std::move(int_n);
    natures_["bool"] = std::move(bool_n);
    natures_["string"] = std::move(string_n);
    natures_["nature"] = std::move(nature_n);
    natures_["IntrinsicFunction"] = std::move(intrinsic_n);
}

vm::vm() : impl_(std::make_unique<vm_Impl>()) {}
vm::~vm() = default;

vm::vm(vm&&) noexcept = default;
vm& vm::operator=(vm&&) noexcept = default;

} // namespace chirp
