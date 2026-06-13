#include "chirp/vm.h"
#include "chirp/backend.h"
#include "chirp/frontend.h"
#include "compiler.h"
#include "evaluator.h"
#include <iostream>
#include <stdexcept>

namespace chirp::vm {

class VmSession : public backend::Session {
    std::ostream& out_;
    backend::SessionExpectations expectations_;
    std::unordered_map<std::string, Value> globals_;

public:
    VmSession(std::ostream& out) : out_(out) {
        auto import_fn = [this](const std::vector<CallArgument>& args) -> Value {
            if (!args.empty() && args[0].name.has_value()) {
                throw std::runtime_error("`import does not support named arguments");
            }
            if (!args.empty() && args[0].value.type == ValueType::String) {
                if (args[0].value.as_string == "\"io.print\"") {
                    return Value(NativeFunc([this](const std::vector<CallArgument>& print_args) -> Value {
                        for (size_t i = 0; i < print_args.size(); ++i) {
                            if (print_args[i].name.has_value()) {
                                throw std::runtime_error("`print does not support named arguments");
                            }
                            if (i > 0) out_ << " ";
                            out_ << print_args[i].value.toString();
                        }
                        out_ << "\n";
                        return Value();
                    }));
                } else if (args[0].value.as_string == "\"system.register\"") {
                    return Value(NativeFunc([](const std::vector<CallArgument>&) -> Value { return Value(); }));
                } else if (args[0].value.as_string == "\"values.same\"") {
                    return Value(NativeFunc([](const std::vector<CallArgument>& args) -> Value {
                        if (args.size() == 2 &&
                            !args[0].name.has_value() &&
                            !args[1].name.has_value()) {
                            const Value& left = args[0].value;
                            const Value& right = args[1].value;
                            if (left.type == right.type && left.as_int == right.as_int && left.as_string == right.as_string) {
                                return Value(true);
                            }
                        }
                        return Value(false);
                    }));
                } else if (args[0].value.as_string == "\"types.type_of\"") {
                    return Value(NativeFunc([](const std::vector<CallArgument>& args) -> Value {
                        if (!args.empty() && !args[0].name.has_value()) {
                            return Value::Symbol("type_" + std::to_string(static_cast<int>(args[0].value.type)));
                        }
                        return Value::Symbol("unknown");
                    }));
                } else if (args[0].value.as_string == "\"types.mint_finite\"" || 
                           args[0].value.as_string == "\"types.mint_infinite\"" ||
                           args[0].value.as_string == "\"types.mint_host\"") {
                    return Value(NativeFunc([](const std::vector<CallArgument>& func_args) -> Value {
                        int n = (!func_args.empty() &&
                                 !func_args[0].name.has_value() &&
                                 func_args[0].value.type == ValueType::Int)
                            ? static_cast<int>(func_args[0].value.as_int)
                            : 0;
                        auto values_array = std::make_shared<std::vector<Value>>();
                        for(int i=0; i<n; ++i) {
                            values_array->push_back(Value::Symbol("minted_" + std::to_string(i)));
                        }
                        auto result_struct = std::make_shared<std::unordered_map<std::string, Value>>();
                        (*result_struct)["type"] = Value::Symbol("minted_type");
                        (*result_struct)["values"] = Value::Array(values_array);
                        return Value::Struct(result_struct);
                    }));
                }
            }
            throw std::runtime_error("Unsupported native import in VM");
        };
        globals_["`import"] = Value(NativeFunc(import_fn));
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) override {
        Compiler compiler;
        auto unit = compiler.compile(stmts);

        Evaluator evaluator;
        Value result = evaluator.evaluate(unit, globals_, out_);
        
        // Only print result if it's not null, since Intrinsic print already printed
        if (result.type != ValueType::Null) {
            out_ << result.toString() << "\n";
        }
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) override {
        // Ignore label for now
        execute(stmts);
    }

    void execute_source(std::string source, std::string label) override {
        auto tokens = frontend::tokenize(source);
        auto stmts = frontend::parse(tokens);
        execute(stmts, label);
    }

    void execute_boot_source(std::string source, std::string label) override {
        auto tokens = frontend::tokenize(source);
        auto stmts = frontend::parse(tokens);
        // Execute but ignore output
        Compiler compiler;
        auto unit = compiler.compile(stmts);
        Evaluator evaluator;
        evaluator.evaluate(unit, globals_, out_);
    }

    void set_chirp_root(std::string path) override {
        // no-op for now
    }

    backend::SessionExpectations getExpectations() const override {
        return expectations_;
    }
};

std::unique_ptr<chirp::backend::Session> createSession(std::ostream& out, bool testing_enabled) {
    return std::make_unique<VmSession>(out);
}

} // namespace chirp::vm
