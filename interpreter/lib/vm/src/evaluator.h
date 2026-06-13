#pragma once

#include "program_unit.h"
#include "value.h"

namespace chirp::vm {

class Evaluator {
public:
    Value evaluate(const ProgramUnit& unit);
};

} // namespace chirp::vm
