#pragma once

#include "Value.h"
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

    // Register a binding name with an initial Value. If it already exists, it returns the existing index.
    size_t register_binding(const std::string& name, const Value& val) {
        auto it = index_.find(name);
        if (it != index_.end()) {
            return it->second;
        }
        size_t idx = values_.size();
        values_.push_back(val);
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

    // Retrieve a value by its index.
    const Value& get_value(size_t index) const {
        return values_.at(index);
    }

    // Update the value at a given index.
    void set_value(size_t index, const Value& val) {
        values_.at(index) = val;
    }

    // Get the total number of registered bindings.
    size_t size() const {
        return values_.size();
    }

private:
    std::vector<Value> values_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace chirp
