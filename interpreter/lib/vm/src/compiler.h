#pragma once

#include "chirp/frontend.h"
#include "program_unit.h"
#include <memory>

namespace chirp::vm {

class Compiler {
public:
    std::shared_ptr<ProgramUnit> compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts);
};

} // namespace chirp::vm
