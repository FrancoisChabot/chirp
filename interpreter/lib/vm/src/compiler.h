#pragma once

#include "chirp/frontend.h"
#include "program_unit.h"

namespace chirp::vm {

class Compiler {
public:
    ProgramUnit compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts);
};

} // namespace chirp::vm
