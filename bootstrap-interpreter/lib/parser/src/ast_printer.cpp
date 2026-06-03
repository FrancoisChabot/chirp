#include "chirp/parser.h"

namespace chirp::parser {

namespace {

class ASTPrinter : public ASTVisitor, public StmtVisitor {
public:
    std::string result;

    void visit(const BinaryExpr& expr) override {
        result += "(";
        switch(expr.op) {
            case BinaryOp::Add: result += "+"; break;
            case BinaryOp::Sub: result += "-"; break;
            case BinaryOp::Mul: result += "*"; break;
            case BinaryOp::Div: result += "/"; break;
            case BinaryOp::Mod: result += "%"; break;
            case BinaryOp::Eq: result += "=="; break;
            case BinaryOp::Neq: result += "!="; break;
            case BinaryOp::Lt: result += "<"; break;
            case BinaryOp::Lte: result += "<="; break;
            case BinaryOp::Gt: result += ">"; break;
            case BinaryOp::Gte: result += ">="; break;
            case BinaryOp::And: result += "&&"; break;
            case BinaryOp::Or: result += "||"; break;
            case BinaryOp::In: result += "in"; break;
            case BinaryOp::NotIn: result += "not in"; break;
            case BinaryOp::Subset: result += "subset"; break;
            case BinaryOp::ProperSubset: result += "proper_subset"; break;
            case BinaryOp::NotSubset: result += "not_subset"; break;
            case BinaryOp::Superset: result += "superset"; break;
            case BinaryOp::ProperSuperset: result += "proper_superset"; break;
            case BinaryOp::NotSuperset: result += "not_superset"; break;
            case BinaryOp::Union: result += "union"; break;
            case BinaryOp::Intersection: result += "intersection"; break;
            case BinaryOp::Range: result += ".."; break;
            case BinaryOp::RangeInclusiveEnd: result += "..="; break;
            case BinaryOp::Dot: result += "."; break;
        }
        result += " ";
        expr.left->accept(*this);
        result += " ";
        expr.right->accept(*this);
        result += ")";
    }
    void visit(const UnaryExpr& expr) override {
        result += "(";
        switch(expr.op) {
            case UnaryOp::Not: result += "!"; break;
            case UnaryOp::Negate: result += "-"; break;
            case UnaryOp::AddressOf: result += "&"; break;
            case UnaryOp::MutableAddressOf: result += "&mut"; break;
            case UnaryOp::Deref: result += "*"; break;
            case UnaryOp::PointerType: result += "->"; break;
            case UnaryOp::MutablePointerType: result += "->mut"; break;
            case UnaryOp::Complement: result += "~"; break;
        }
        result += " ";
        expr.right->accept(*this);
        result += ")";
    }
    void visit(const GroupingExpr& expr) override {
        result += "(group ";
        expr.expression->accept(*this);
        result += ")";
    }
    void visit(const NumberExpr& expr) override { result += expr.value; }
    void visit(const StringExpr& expr) override { result += expr.value; }
    void visit(const BoolExpr& expr) override { result += (expr.value ? "true" : "false"); }
    void visit(const IdentifierExpr& expr) override { result += expr.name; }
    void visit(const IntrinsicExpr& expr) override { result += expr.name; }
    void visit(const UndecidedExpr& expr) override { result += "undecided"; }
    void visit(const SymbolicConstantExpr& expr) override { result += expr.value; }
    
    void visit(const EnumeratedSetExpr& expr) override {
        result += "(set";
        for (const auto& el : expr.elements) {
            result += " ";
            el->accept(*this);
        }
        result += ")";
    }

    void visit(const ConstructedSetExpr& expr) override {
        result += "(c_set ";
        if (expr.binding.is_mut) result += "mut ";
        result += expr.binding.name.lexeme;
        if (expr.binding.type_bound) {
            result += " : ";
            expr.binding.type_bound->accept(*this);
        }
        result += " | ";
        expr.condition->accept(*this);
        result += ")";
    }

    void visit(const WhileExpr& expr) override {
        result += "(while ";
        expr.condition->accept(*this);
        result += " ";
        expr.body->accept(*this);
        result += ")";
    }

    void visit(const ForExpr& expr) override {
        result += "(for ";
        if (expr.iterator_binding.is_mut) result += "mut ";
        result += expr.iterator_binding.name.lexeme;
        if (expr.iterator_binding.type_bound) {
            result += ":";
            expr.iterator_binding.type_bound->accept(*this);
        }
        result += " in ";
        expr.iterable->accept(*this);
        result += " ";
        expr.body->accept(*this);
        result += ")";
    }

    void visit(const IfExpr& expr) override {
        result += "(if ";
        expr.condition->accept(*this);
        result += " ";
        expr.then_branch->accept(*this);
        result += " ";
        expr.else_branch->accept(*this);
        result += ")";
    }

    void visit(const LambdaExpr& expr) override {
        result += "(lambda (";
        for (size_t i = 0; i < expr.parameters.size(); ++i) {
            if (i > 0) result += " ";
            if (expr.parameters[i].is_mut) result += "mut ";
            result += expr.parameters[i].name.lexeme;
            if (expr.parameters[i].type_bound) {
                result += ":";
                expr.parameters[i].type_bound->accept(*this);
            }
        }
        result += ")";
        if (expr.return_bound) {
            result += ":";
            expr.return_bound->accept(*this);
        }
        result += " ";
        expr.body->accept(*this);
        result += ")";
    }

    void visit(const StructExpr& expr) override {
        result += "(struct";
        for (const auto& field : expr.fields) {
            result += " (field ";
            if (field.is_mut) result += "mut ";
            result += field.name.lexeme;
            if (field.type_bound) {
                result += ":";
                field.type_bound->accept(*this);
            }
            if (field.initializer) {
                result += " = ";
                field.initializer->accept(*this);
            }
            result += ")";
        }
        result += ")";
    }

    void visit(const CallExpr& expr) override {
        result += "(call ";
        expr.callee->accept(*this);
        for (const auto& arg : expr.args) {
            result += " ";
            if (arg.name) {
                result += arg.name->lexeme;
                result += "=";
            }
            arg.value->accept(*this);
        }
        result += ")";
    }

    void visit(const IndexExpr& expr) override {
        result += "(index ";
        expr.target->accept(*this);
        for (const auto& arg : expr.args) {
            result += " ";
            arg.value->accept(*this);
        }
        result += ")";
    }

    void visit(const ListExpr& expr) override {
        result += "(list";
        for (const auto& el : expr.elements) {
            result += " ";
            el->accept(*this);
        }
        result += ")";
    }

    void visit(const MatchExpr& expr) override {
        result += "(match ";
        expr.subject->accept(*this);
        for (const auto& arm : expr.arms) {
            result += " (=> ";
            arm.pattern->accept(*this);
            result += " ";
            arm.body->accept(*this);
            result += ")";
        }
        result += ")";
    }

    void visit(const BlockExpr& expr) override {
        result += "(block";
        for (const auto& stmt : expr.statements) {
            result += " ";
            stmt->accept(*this);
        }
        result += ")";
    }

    void visit(const ExprStmt& stmt) override {
        result += "(expr_stmt ";
        stmt.expression->accept(*this);
        result += ")";
    }

    void visit(const LetStmt& stmt) override {
        result += "(let ";
        if (stmt.binding.is_mut) result += "mut ";
        result += stmt.binding.name.lexeme;
        if (stmt.binding.type_bound) {
            result += ":";
            stmt.binding.type_bound->accept(*this);
        }
        if (stmt.binding.initializer) {
            result += " = ";
            stmt.binding.initializer->accept(*this);
        }
        result += ")";
    }

    void visit(const BreakStmt& stmt) override {
        result += "(break ";
        if (stmt.value) {
            stmt.value->accept(*this);
        } else {
            result += "`void";
        }
        result += ")";
    }

    void visit(const AssignStmt& stmt) override {
        result += "(";
        result += stmt.op.lexeme;
        result += " ";
        stmt.target->accept(*this);
        result += " ";
        stmt.value->accept(*this);
        result += ")";
    }

    void visit(const IfStmt& stmt) override {
        result += "(if_stmt ";
        stmt.condition->accept(*this);
        result += " ";
        stmt.then_branch->accept(*this);
        if (stmt.else_branch) {
            result += " ";
            stmt.else_branch->accept(*this);
        }
        result += ")";
    }
};

} // namespace

std::string print_ast(const Expr& expr) {
    ASTPrinter printer;
    expr.accept(printer);
    return printer.result;
}

std::string print_ast(const Stmt& stmt) {
    ASTPrinter printer;
    stmt.accept(printer);
    return printer.result;
}

std::string print_ast(const std::vector<std::unique_ptr<Stmt>>& stmts) {
    std::string result = "";
    for (const auto& stmt : stmts) {
        result += print_ast(*stmt) + "\n";
    }
    return result;
}

} // namespace chirp::parser
