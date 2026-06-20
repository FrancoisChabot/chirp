#pragma once

#include <memory>
#include <unordered_map>
#include <string>

namespace chirp {

class Nature;
class Value;
class compute_unit;
class bindings_table;

using nature_key = std::string;
using NatureRef = const Nature*;

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

    class compute_unit& get_compute_unit();
    const class compute_unit& get_compute_unit() const;

    class bindings_table& get_bindings_table();
    const class bindings_table& get_bindings_table() const;

private:
    std::unordered_map<nature_key, std::unique_ptr<Nature>> natures_;

    NatureRef int_nature_cache_ = nullptr;
    NatureRef bool_nature_cache_ = nullptr;
    NatureRef string_nature_cache_ = nullptr;
    NatureRef intrinsic_nature_cache_ = nullptr;
    NatureRef nature_nature_cache_ = nullptr;

    std::unique_ptr<class compute_unit> compute_unit_;
    std::unique_ptr<class bindings_table> bindings_table_;
};

} // namespace chirp
