#pragma once

#include "chirp/frontend.h"
#include "program_unit.h"
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <string>

namespace chirp::vm {

class Compiler {
    std::unordered_set<std::string>* global_final_bindings_;
    std::unordered_map<std::string, bool>* global_purity_;
public:
    Compiler(std::unordered_set<std::string>* global_final_bindings = nullptr,
             std::unordered_map<std::string, bool>* global_purity = nullptr) 
        : global_final_bindings_(global_final_bindings), global_purity_(global_purity) {}
    std::shared_ptr<ProgramUnit> compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, bool is_module = false);
};

} // namespace chirp::vm
