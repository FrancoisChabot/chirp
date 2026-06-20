#pragma once

#include "Value.h"
#include <utility>

namespace chirp {

class Binding {
public:
    Binding(Value fc, Value cv)
        : fc_(std::move(fc)), cv_(std::move(cv)) {}

    const Value& get_fc() const { return fc_; }
    const Value& get_cv() const { return cv_; }

    void set_cv(Value cv) { cv_ = std::move(cv); }

private:
    Value fc_;
    Value cv_;
};

} // namespace chirp
