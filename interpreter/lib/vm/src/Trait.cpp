#include "Trait.h"
#include "Value.h"

namespace chirp {

Trait::Trait(size_t id, const Value& interface)
    : id_(id), interface_(std::make_shared<Value>(interface)) {}

Trait::~Trait() = default;
Trait::Trait(Trait&&) noexcept = default;
Trait& Trait::operator=(Trait&&) noexcept = default;

size_t Trait::id() const {
    return id_;
}

const Value& Trait::interface() const {
    return *interface_;
}

} // namespace chirp
