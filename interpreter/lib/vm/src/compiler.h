#pragma once

#include "chirp/frontend.h"
#include "program_unit.h"
#include <memory>
#include <unordered_set>
#include <string>

namespace chirp::vm {

class Compiler {
    std::unordered_set<std::string>* global_final_bindings_;
public:
    Compiler(std::unordered_set<std::string>* global_final_bindings = nullptr) 
        : global_final_bindings_(global_final_bindings) {}
    std::shared_ptr<ProgramUnit> compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts);
};

} // namespace chirp::vm
