#include "chirp/vm.h"
#include "Value.h"
#include "bindings_table.h"
#include "compute_unit.h"
#include "nature.h"
#include "traits_table.h"

namespace chirp {

vm::vm()
    : compute_unit_(std::make_unique<class compute_unit>()),
      bindings_table_(std::make_unique<class bindings_table>()),
      traits_table_(std::make_unique<class traits_table>()) {
    auto int_n = std::make_unique<IntNature>();
    auto bool_n = std::make_unique<BoolNature>();
    auto string_n = std::make_unique<StringNature>();
    auto nature_n = std::make_unique<NatureNature>();
    auto trait_n = std::make_unique<TraitNature>();
    auto intrinsic_n = std::make_unique<IntrinsicFunctionNature>();
    
    int_nature_cache_ = int_n.get();
    bool_nature_cache_ = bool_n.get();
    string_nature_cache_ = string_n.get();
    nature_nature_cache_ = nature_n.get();
    trait_nature_cache_ = trait_n.get();
    intrinsic_nature_cache_ = intrinsic_n.get();

    natures_["int"] = std::move(int_n);
    natures_["bool"] = std::move(bool_n);
    natures_["string"] = std::move(string_n);
    natures_["nature"] = std::move(nature_n);
    natures_["trait"] = std::move(trait_n);
    natures_["IntrinsicFunction"] = std::move(intrinsic_n);
}

vm::~vm() = default;

vm::vm(vm&&) noexcept = default;
vm& vm::operator=(vm&&) noexcept = default;

TraitRef vm::create_trait(const Value& interface) {
    return traits_table_->create_trait(interface);
}

void vm::register_trait_implementation(TraitRef trait, NatureRef on, const Value& implementation) {
    traits_table_->register_implementation(trait, on, implementation);
}

std::optional<Value> vm::lookup_trait_implementation(TraitRef trait, NatureRef on) const {
    return traits_table_->lookup_implementation(trait, on);
}

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

traits_table& vm::get_traits_table() {
    return *traits_table_;
}

const traits_table& vm::get_traits_table() const {
    return *traits_table_;
}

} // namespace chirp
