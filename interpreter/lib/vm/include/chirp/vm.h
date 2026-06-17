#pragma once

#include <memory>

namespace chirp {

class vm_Impl;

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

private:
    friend class vm_accessor;

    std::unique_ptr<vm_Impl> impl_;
};

} // namespace chirp
