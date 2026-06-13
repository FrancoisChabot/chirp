#pragma once

#include "program_unit.h"
#include "value.h"
#include <unordered_map>
#include <string>

namespace chirp::vm {

class Evaluator {
public:
    Value evaluate(std::shared_ptr<ProgramUnit> unit, std::unordered_map<std::string, Value>& globals, std::ostream& out);
};

} // namespace chirp::vm
