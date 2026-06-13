#pragma once
#include <iosfwd>
#include <memory>
#include "chirp/backend.h"

namespace chirp::vm {

std::unique_ptr<chirp::backend::Session> createSession(std::ostream& out, bool testing_enabled = false);

} // namespace chirp::vm
