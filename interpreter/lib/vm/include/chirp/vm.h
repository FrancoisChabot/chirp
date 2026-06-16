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

    // MVP API for pushing and popping standard type values on the stack.
    void push_int(int val);
    void push_bool(bool val);

    int pop_int();
    bool pop_bool();

    size_t stack_size() const;

private:
    friend class vm_accessor;

    std::unique_ptr<vm_Impl> impl_;
};

} // namespace chirp
