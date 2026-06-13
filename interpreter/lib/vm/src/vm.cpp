#include "chirp/vm.h"
#include <stdexcept>

namespace chirp::vm {

std::unique_ptr<chirp::backend::Session> createSession(std::ostream& out, bool testing_enabled) {
    throw std::runtime_error("VM backend is not implemented yet");
}

} // namespace chirp::vm
