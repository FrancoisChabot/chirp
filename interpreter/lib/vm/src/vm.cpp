#include "chirp/vm.h"
#include "Value.h"
#include "nature.h"
#include "compute_unit.h"
#include "bindings_table.h"
#include <stdexcept>

namespace chirp {

vm::vm()
    : compute_unit_(std::make_unique<class compute_unit>()),
      bindings_table_(std::make_unique<class bindings_table>()) {
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

vm::~vm() = default;

vm::vm(vm&&) noexcept = default;
vm& vm::operator=(vm&&) noexcept = default;

compute_unit& vm::get_compute_unit() {
    return *compute_unit_;
}

const compute_unit& vm::get_compute_unit() const {
    return *compute_unit_;
}

bindings_table& vm::get_bindings_table() {
    return *bindings_table_;
}

const bindings_table& vm::get_bindings_table() const {
    return *bindings_table_;
}

} // namespace chirp
