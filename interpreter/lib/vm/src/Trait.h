#pragma once

#include <cstddef>
#include <memory>

namespace chirp {

class Value;

class Trait {
public:
    Trait(size_t id, const Value& interface);
    ~Trait();

    Trait(const Trait&) = delete;
    Trait& operator=(const Trait&) = delete;
    Trait(Trait&&) noexcept;
    Trait& operator=(Trait&&) noexcept;

    size_t id() const;
    const Value& interface() const;

private:
    size_t id_;
    std::shared_ptr<Value> interface_;
};

using TraitRef = const Trait*;

} // namespace chirp
