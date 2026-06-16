#pragma once

#include "chirp/vm.h"
#include "Value.h"
#include "nature.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace chirp {

class vm_Impl {
public:
    vm_Impl();

    NatureRef get_int_nature() const { return int_nature_cache_; }
    NatureRef get_bool_nature() const { return bool_nature_cache_; }
    NatureRef get_string_nature() const { return string_nature_cache_; }
    NatureRef get_intrinsic_nature() const { return intrinsic_nature_cache_; }
    NatureRef get_nature_nature() const { return nature_nature_cache_; }

private:
    std::unordered_map<nature_key, std::unique_ptr<Nature>> natures_;

    NatureRef int_nature_cache_ = nullptr;
    NatureRef bool_nature_cache_ = nullptr;
    NatureRef string_nature_cache_ = nullptr;
    NatureRef intrinsic_nature_cache_ = nullptr;
    NatureRef nature_nature_cache_ = nullptr;

    std::vector<Value> stack_;
};

} // namespace chirp
