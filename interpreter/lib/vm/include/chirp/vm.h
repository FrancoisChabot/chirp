#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace chirp {

class Nature;
class Trait;
class Value;
class compute_unit;
class bindings_table;
class traits_table;

using nature_key = std::string;
using NatureRef = const Nature*;
using TraitRef = const Trait*;

class vm {
public:
    vm();
    ~vm();

    // Disable copy constructors/assignment.
    vm(const vm&) = delete;
    vm& operator=(const vm&) = delete;

    // Enable move constructors/assignment.
    vm(vm&&) noexcept;
    vm& operator=(vm&&) noexcept;

    NatureRef get_int_nature() const { return int_nature_cache_; }
    NatureRef get_bool_nature() const { return bool_nature_cache_; }
    NatureRef get_string_nature() const { return string_nature_cache_; }
    NatureRef get_intrinsic_nature() const { return intrinsic_nature_cache_; }
    NatureRef get_nature_nature() const { return nature_nature_cache_; }
    NatureRef get_trait_nature() const { return trait_nature_cache_; }

    TraitRef create_trait(const Value& interface);
    void register_trait_implementation(TraitRef trait, NatureRef on, const Value& implementation);
    std::optional<Value> lookup_trait_implementation(TraitRef trait, NatureRef on) const;

    class compute_unit& get_compute_unit();
    const class compute_unit& get_compute_unit() const;

    class bindings_table& get_bindings_table();
    const class bindings_table& get_bindings_table() const;

    class traits_table& get_traits_table();
    const class traits_table& get_traits_table() const;

private:
    std::unordered_map<nature_key, std::unique_ptr<Nature>> natures_;

    NatureRef int_nature_cache_ = nullptr;
    NatureRef bool_nature_cache_ = nullptr;
    NatureRef string_nature_cache_ = nullptr;
    NatureRef intrinsic_nature_cache_ = nullptr;
    NatureRef nature_nature_cache_ = nullptr;
    NatureRef trait_nature_cache_ = nullptr;

    std::unique_ptr<class compute_unit> compute_unit_;
    std::unique_ptr<class bindings_table> bindings_table_;
    std::unique_ptr<class traits_table> traits_table_;
};

} // namespace chirp
