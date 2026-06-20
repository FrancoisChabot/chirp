#pragma once

#include "Trait.h"
#include "nature.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace chirp {

class Value;

class traits_table {
public:
    traits_table() = default;
    ~traits_table() = default;

    TraitRef create_trait(const Value& interface);
    void register_implementation(TraitRef trait, NatureRef on, const Value& implementation);
    std::optional<Value> lookup_implementation(TraitRef trait, NatureRef on) const;

private:
    struct implementation_key {
        TraitRef trait = nullptr;
        NatureRef on = nullptr;

        bool operator==(const implementation_key& other) const {
            return trait == other.trait && on == other.on;
        }
    };

    struct implementation_key_hash {
        size_t operator()(const implementation_key& key) const;
    };

    std::vector<std::unique_ptr<Trait>> traits_;
    std::unordered_map<implementation_key, Value, implementation_key_hash> implementations_;
    size_t next_trait_id_ = 0;
};

} // namespace chirp
