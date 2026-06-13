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
        auto import_fn = [this](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && args[0].type == ValueType::String) {
                if (args[0].as_string == "\"io.print\"") {
                    return Value([this](const std::vector<Value>& print_args) -> Value {
                        for (size_t i = 0; i < print_args.size(); ++i) {
                            if (i > 0) out_ << " ";
                            out_ << print_args[i].toString();
                        }
                        out_ << "\n";
                        return Value();
                    });
                }
            }
            throw std::runtime_error("Unsupported native import in VM");
        };
        globals_["`import"] = Value(import_fn);
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
