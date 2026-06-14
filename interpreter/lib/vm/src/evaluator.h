#pragma once

#include "program_unit.h"
#include "value.h"
#include <unordered_map>
#include <string>

namespace chirp::vm {

class Evaluator {
public:
    Value evaluate(std::shared_ptr<ProgramUnit> unit,
                   std::unordered_map<std::string, Value>& globals,
                   std::ostream& out,
                   const std::unordered_map<std::string, Value>* registry = nullptr,
                   const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls = nullptr);
    Value construct_struct(std::shared_ptr<StructTypeDef> type,
                           const std::vector<CallArgument>& args,
                           std::unordered_map<std::string, Value>& globals,
                           std::ostream& out,
                           const std::unordered_map<std::string, Value>* registry = nullptr,
                           const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls = nullptr);
    Value invoke_value(const Value& callee,
                       const std::vector<CallArgument>& args,
                       std::unordered_map<std::string, Value>& globals,
                       std::ostream& out,
                       const std::unordered_map<std::string, Value>* registry = nullptr,
                       const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls = nullptr);
};

} // namespace chirp::vm
