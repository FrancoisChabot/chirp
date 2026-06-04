#pragma once

#include "value.h"

namespace chirp::interpreter {

class Binding {
public:
    Binding(Value fc, Value lc, Value cv, bool is_final = false)
        : fc_(std::move(fc)), lc_(std::move(lc)), cv_(std::move(cv)), is_final_(is_final) {}

    const Value& getFC() const { return fc_; }
    const Value& getLC() const { return lc_; }
    const Value& getCV() const { return cv_; }
    bool isFinal() const { return is_final_; }

    void setCV(Value cv) { cv_ = std::move(cv); }
    void setLC(Value lc) { lc_ = std::move(lc); }

private:
    Value fc_;
    Value lc_;
    Value cv_;
    bool is_final_;
};

} // namespace chirp::interpreter
