#include "traits_table.h"
#include "Value.h"
#include <functional>
#include <stdexcept>

namespace chirp {

size_t traits_table::implementation_key_hash::operator()(const implementation_key& key) const {
    size_t trait_hash = std::hash<TraitRef>{}(key.trait);
    size_t nature_hash = std::hash<NatureRef>{}(key.on);
    return trait_hash ^ (nature_hash << 1);
}

TraitRef traits_table::create_trait(const Value& interface) {
    auto trait = std::make_unique<Trait>(next_trait_id_++, interface);
    TraitRef ref = trait.get();
    traits_.push_back(std::move(trait));
    return ref;
}

void traits_table::register_implementation(TraitRef trait, NatureRef on, const Value& implementation) {
    if (trait == nullptr) {
        throw std::runtime_error("Cannot register implementation for null trait");
    }
    if (on == nullptr) {
        throw std::runtime_error("Cannot register implementation on null nature");
    }

    implementations_[implementation_key{trait, on}] = implementation;
}

std::optional<Value> traits_table::lookup_implementation(TraitRef trait, NatureRef on) const {
    auto it = implementations_.find(implementation_key{trait, on});
    if (it == implementations_.end()) {
        return std::nullopt;
    }

    return it->second;
}

} // namespace chirp
