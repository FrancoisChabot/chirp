#pragma once

#include "Binding.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <stdexcept>

namespace chirp {

class bindings_table {
public:
    bindings_table() = default;
    ~bindings_table() = default;

    // Register a binding name with an initial Binding. If it already exists, it returns the existing index.
    size_t register_binding(const std::string& name, const Binding& binding) {
        auto it = index_.find(name);
        if (it != index_.end()) {
            return it->second;
        }
        size_t idx = bindings_.size();
        bindings_.push_back(binding);
        index_[name] = idx;
        return idx;
    }

    // Lookup the index of a binding by name.
    std::optional<size_t> lookup_index(const std::string& name) const {
        auto it = index_.find(name);
        if (it != index_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Retrieve a binding by its index.
    const Binding& get_binding(size_t index) const {
        return bindings_.at(index);
    }

    // Update the binding at a given index.
    void set_binding(size_t index, const Binding& binding) {
        bindings_.at(index) = binding;
    }

    // Get the total number of registered bindings.
    size_t size() const {
        return bindings_.size();
    }

private:
    std::vector<Binding> bindings_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace chirp
