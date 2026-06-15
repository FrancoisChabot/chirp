#pragma once

#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <string>

namespace chirp::frontend {

enum class token_type {
    // Single-character tokens
    left_paren, right_paren,
    left_brace, right_brace,
    left_bracket, right_bracket,
    comma, dot, minus, plus, semicolon, colon, slash, star,
    ampersand, ampersand_mut, pipe, percent, bang,

    // One or two character tokens
    bang_equal,
    equal, equal_equal,
    greater, greater_equal,
    less, less_equal,
    plus_equal, minus_equal, star_equal, slash_equal, percent_equal,
    dot_dot,             // ..
    dot_dot_equal,       // ..=
    arrow,      // ->
    arrow_mut,  // ->mut
    fat_arrow,  // =>
    and_and,    // &&
    or_or,      // ||

    // Unicode tokens
    in_op,      // ∈
    not_in_op,  // ∉

    union_op,   // ∪
    intersection_op, // ∩
    tilde,      // ~

    // Literals
    identifier,
    intrinsic,  // e.g., `any
    string,
    fstring_head,
    fstring_middle,
    fstring_tail,
    fstring_literal,
    character,
    number,
    symbolic_constant,

    // Keywords
    kw_let,
    kw_struct,
    kw_if,
    kw_else,
    kw_while,
    kw_for,
    kw_break,
    kw_match,
    kw_do,
    kw_debug,
    kw_enum,

    eof,
    error
};

struct token {
    token_type type;
    std::string_view lexeme;
    int line;
    int column;
    std::string_view leading_trivia;
};

std::vector<token> tokenize(std::string_view input);
std::string format_text(std::string_view source);

class Expr;
std::unique_ptr<Expr> parse_expression(const std::vector<token>& tokens);

class Stmt;
std::vector<std::unique_ptr<Stmt>> parse(const std::vector<token>& tokens);


// This AST is a surface-syntax parse artifact.
// The interpreter is expected to lower it into IR and operate on that IR, not
// execute or assign semantic authority to these nodes directly.
// As a consequence, the AST may contain syntactically valid constructs that are
// guaranteed to fail later semantic/IR validation, such as `(1 + 3) = 5;`.




enum class BinaryOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Lte, Gt, Gte,
    And, Or,
    In, NotIn,

    Union, Intersection,
    Range, RangeInclusiveEnd, RangeInclusiveStart, RangeInclusiveBoth,
    Dot
};

enum class UnaryOp {
    Not, Negate,
    AddressOf, MutableAddressOf,
    Deref,
    PointerType, MutablePointerType,
    Complement
};

// Forward declarations
class BinaryExpr;
class UnaryExpr;
class GroupingExpr;
class NumberExpr;
class StringExpr;
class CharExpr;

class IdentifierExpr;
class IntrinsicExpr;

class SymbolicConstantExpr;
class EnumeratedSetExpr;
class ConstructedSetExpr;
class AnonymousStructLiteralExpr;
class IfExpr;
class WhileExpr;
class ForExpr;
class LambdaExpr;
class SignatureExpr;
class BlockExpr;
class StructExpr;
class CallExpr;
class IndexExpr;
class ListExpr;
class MatchExpr;
class EnumExpr;
class FStringExpr;

class ExprStmt;
class LetStmt;
class BreakStmt;
class AssignStmt;
class IfStmt;
class DebugStmt;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(const BinaryExpr& expr) = 0;
    virtual void visit(const UnaryExpr& expr) = 0;
    virtual void visit(const GroupingExpr& expr) = 0;
    virtual void visit(const NumberExpr& expr) = 0;
    virtual void visit(const StringExpr& expr) = 0;
    virtual void visit(const CharExpr& expr) = 0;

    virtual void visit(const IdentifierExpr& expr) = 0;
    virtual void visit(const IntrinsicExpr& expr) = 0;

    virtual void visit(const SymbolicConstantExpr& expr) = 0;
    virtual void visit(const EnumeratedSetExpr& expr) = 0;
    virtual void visit(const ConstructedSetExpr& expr) = 0;
    virtual void visit(const AnonymousStructLiteralExpr& expr) = 0;
    virtual void visit(const IfExpr& expr) = 0;
    virtual void visit(const WhileExpr& expr) = 0;
    virtual void visit(const ForExpr& expr) = 0;
    virtual void visit(const LambdaExpr& expr) = 0;
    virtual void visit(const SignatureExpr& expr) = 0;
    virtual void visit(const BlockExpr& expr) = 0;
    virtual void visit(const StructExpr& expr) = 0;
    virtual void visit(const CallExpr& expr) = 0;
    virtual void visit(const IndexExpr& expr) = 0;
    virtual void visit(const ListExpr& expr) = 0;
    virtual void visit(const MatchExpr& expr) = 0;
    virtual void visit(const EnumExpr& expr) = 0;
    virtual void visit(const FStringExpr& expr) = 0;
};

class StmtVisitor {
public:
    virtual ~StmtVisitor() = default;

    virtual void visit(const ExprStmt& stmt) = 0;
    virtual void visit(const LetStmt& stmt) = 0;
    virtual void visit(const BreakStmt& stmt) = 0;
    virtual void visit(const AssignStmt& stmt) = 0;
    virtual void visit(const IfStmt& stmt) = 0;
    virtual void visit(const DebugStmt& stmt) = 0;
};

enum class PurityState { Unchecked, Checking, Pure, Unpure };

class Expr {
public:
    virtual ~Expr() = default;
    virtual void accept(ASTVisitor& visitor) const = 0;
};

class Stmt {
public:
    virtual ~Stmt() = default;
    virtual void accept(StmtVisitor& visitor) const = 0;
};

struct NamedBinding {
    token name;
    bool is_mut;
    bool is_final;
    std::unique_ptr<Expr> type_bound;
    std::unique_ptr<Expr> initializer;

    NamedBinding(token name, bool is_mut = false, std::unique_ptr<Expr> type_bound = nullptr, std::unique_ptr<Expr> initializer = nullptr, bool is_final = false)
        : name(std::move(name)), is_mut(is_mut), is_final(is_final), type_bound(std::move(type_bound)), initializer(std::move(initializer)) {}
};

struct Argument {
    std::optional<token> name;
    std::unique_ptr<Expr> value;
    
    Argument(std::optional<token> name, std::unique_ptr<Expr> value)
        : name(std::move(name)), value(std::move(value)) {}
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    BinaryOp op;
    std::unique_ptr<Expr> right;
    token diagnostic_token;

    BinaryExpr(std::unique_ptr<Expr> left, BinaryOp op, std::unique_ptr<Expr> right, token diag)
        : left(std::move(left)), op(op), right(std::move(right)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class UnaryExpr : public Expr {
public:
    UnaryOp op;
    std::unique_ptr<Expr> right;
    token diagnostic_token;

    UnaryExpr(UnaryOp op, std::unique_ptr<Expr> right, token diag)
        : op(op), right(std::move(right)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class GroupingExpr : public Expr {
public:
    std::unique_ptr<Expr> expression;

    explicit GroupingExpr(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class NumberExpr : public Expr {
public:
    std::string_view value;
    token diagnostic_token;

    NumberExpr(std::string_view value, token diag) 
        : value(value), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class StringExpr : public Expr {
public:
    std::string_view value;
    token diagnostic_token;

    StringExpr(std::string_view value, token diag) 
        : value(value), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class FStringExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> parts;
    token diagnostic_token;

    FStringExpr(std::vector<std::unique_ptr<Expr>> parts, token diag)
        : parts(std::move(parts)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class CharExpr : public Expr {
public:
    std::string_view value;
    token diagnostic_token;

    CharExpr(std::string_view value, token diag) 
        : value(value), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};



class IdentifierExpr : public Expr {
public:
    std::string_view name;
    token diagnostic_token;

    IdentifierExpr(std::string_view name, token diag) 
        : name(name), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class IntrinsicExpr : public Expr {
public:
    std::string_view name;
    token diagnostic_token;

    IntrinsicExpr(std::string_view name, token diag) 
        : name(name), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};



class SymbolicConstantExpr : public Expr {
public:
    std::string_view value;
    token diagnostic_token;

    SymbolicConstantExpr(std::string_view value, token diag) 
        : value(value), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class EnumeratedSetExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    token diagnostic_token;

    EnumeratedSetExpr(std::vector<std::unique_ptr<Expr>> elements, token diag)
        : elements(std::move(elements)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class ConstructedSetExpr : public Expr {
public:
    NamedBinding binding;
    std::unique_ptr<Expr> condition;
    token diagnostic_token;

    ConstructedSetExpr(NamedBinding binding, std::unique_ptr<Expr> condition, token diag)
        : binding(std::move(binding)), condition(std::move(condition)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class AnonymousStructLiteralExpr : public Expr {
public:
    std::vector<Argument> fields;
    token diagnostic_token;

    AnonymousStructLiteralExpr(std::vector<Argument> fields, token diag)
        : fields(std::move(fields)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class IfExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> then_branch;
    std::unique_ptr<Expr> else_branch;
    token diagnostic_token;

    IfExpr(std::unique_ptr<Expr> condition, std::unique_ptr<Expr> then_branch, std::unique_ptr<Expr> else_branch, token diag)
        : condition(std::move(condition)), then_branch(std::move(then_branch)), else_branch(std::move(else_branch)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class WhileExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> body;
    token diagnostic_token;

    WhileExpr(std::unique_ptr<Expr> condition, std::unique_ptr<Expr> body, token diag)
        : condition(std::move(condition)), body(std::move(body)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class ForExpr : public Expr {
public:
    NamedBinding iterator_binding;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Expr> body;
    token diagnostic_token;

    ForExpr(NamedBinding iterator_binding, std::unique_ptr<Expr> iterable, std::unique_ptr<Expr> body, token diag)
        : iterator_binding(std::move(iterator_binding)), iterable(std::move(iterable)), body(std::move(body)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class LambdaExpr : public Expr {
public:
    std::vector<NamedBinding> parameters;
    std::unique_ptr<Expr> return_bound;
    std::unique_ptr<Expr> body;
    token diagnostic_token;
    mutable PurityState purity_state = PurityState::Unchecked;

    LambdaExpr(std::vector<NamedBinding> parameters, std::unique_ptr<Expr> return_bound, std::unique_ptr<Expr> body, token diag)
        : parameters(std::move(parameters)), return_bound(std::move(return_bound)), body(std::move(body)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class SignatureExpr : public Expr {
public:
    std::vector<NamedBinding> parameters;
    std::unique_ptr<Expr> return_bound;
    token diagnostic_token;

    SignatureExpr(std::vector<NamedBinding> parameters, std::unique_ptr<Expr> return_bound, token diag)
        : parameters(std::move(parameters)), return_bound(std::move(return_bound)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class StructExpr : public Expr {
public:
    std::vector<NamedBinding> fields;
    token diagnostic_token;

    StructExpr(std::vector<NamedBinding> fields, token diag)
        : fields(std::move(fields)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class EnumExpr : public Expr {
public:
    std::vector<std::string> variants;
    token diagnostic_token;
    uint64_t node_id;

    EnumExpr(std::vector<std::string> variants, token diag)
        : variants(std::move(variants)), diagnostic_token(std::move(diag)) {
        static uint64_t next_id = 1;
        node_id = next_id++;
    }

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    std::vector<Argument> args;
    token diagnostic_token;

    CallExpr(std::unique_ptr<Expr> callee, std::vector<Argument> args, token diag)
        : callee(std::move(callee)), args(std::move(args)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class IndexExpr : public Expr {
public:
    std::unique_ptr<Expr> target;
    std::vector<Argument> args;
    token diagnostic_token;

    IndexExpr(std::unique_ptr<Expr> target, std::vector<Argument> args, token diag)
        : target(std::move(target)), args(std::move(args)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class ListExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    token diagnostic_token;

    ListExpr(std::vector<std::unique_ptr<Expr>> elements, token diag)
        : elements(std::move(elements)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

struct MatchArm {
    std::unique_ptr<Expr> pattern;
    std::unique_ptr<Expr> body;
    
    MatchArm(std::unique_ptr<Expr> pattern, std::unique_ptr<Expr> body)
        : pattern(std::move(pattern)), body(std::move(body)) {}
};

class MatchExpr : public Expr {
public:
    std::unique_ptr<Expr> subject;
    std::vector<MatchArm> arms;
    token diagnostic_token;

    MatchExpr(std::unique_ptr<Expr> subject, std::vector<MatchArm> arms, token diag)
        : subject(std::move(subject)), arms(std::move(arms)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class BlockExpr : public Expr {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    token diagnostic_token;

    BlockExpr(std::vector<std::unique_ptr<Stmt>> statements, token diag)
        : statements(std::move(statements)), diagnostic_token(std::move(diag)) {}

    void accept(ASTVisitor& visitor) const override { visitor.visit(*this); }
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;
    token diagnostic_token;

    ExprStmt(std::unique_ptr<Expr> expression, token diag)
        : expression(std::move(expression)), diagnostic_token(std::move(diag)) {}

    void accept(StmtVisitor& visitor) const override { visitor.visit(*this); }
};

class LetStmt : public Stmt {
public:
    NamedBinding binding;
    bool is_public;
    token diagnostic_token;

    LetStmt(NamedBinding binding, token diag, bool is_public = false)
        : binding(std::move(binding)), is_public(is_public), diagnostic_token(std::move(diag)) {}

    void accept(StmtVisitor& visitor) const override { visitor.visit(*this); }
};

class BreakStmt : public Stmt {
public:
    std::unique_ptr<Expr> value;
    token diagnostic_token;

    BreakStmt(std::unique_ptr<Expr> value, token diag)
        : value(std::move(value)), diagnostic_token(std::move(diag)) {}

    void accept(StmtVisitor& visitor) const override { visitor.visit(*this); }
};

class AssignStmt : public Stmt {
public:
    std::unique_ptr<Expr> target;
    token op;
    std::unique_ptr<Expr> value;
    token diagnostic_token;

    AssignStmt(std::unique_ptr<Expr> target, token op, std::unique_ptr<Expr> value, token diag)
        : target(std::move(target)), op(std::move(op)), value(std::move(value)), diagnostic_token(std::move(diag)) {}

    void accept(StmtVisitor& visitor) const override { visitor.visit(*this); }
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch;
    token diagnostic_token;

    IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> then_branch, std::unique_ptr<Stmt> else_branch, token diag)
        : condition(std::move(condition)), then_branch(std::move(then_branch)), else_branch(std::move(else_branch)), diagnostic_token(std::move(diag)) {}

    void accept(StmtVisitor& visitor) const override { visitor.visit(*this); }
};

class DebugStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    token diagnostic_token;

    DebugStmt(std::vector<std::unique_ptr<Stmt>> statements, token diag)
        : statements(std::move(statements)), diagnostic_token(std::move(diag)) {}

    void accept(StmtVisitor& visitor) const override { visitor.visit(*this); }
};

std::string print_ast(const Expr& expr);
std::string print_ast(const Stmt& stmt);
std::string print_ast(const std::vector<std::unique_ptr<Stmt>>& stmts);

int hex_value(char c);
void append_utf8(std::string& out, uint32_t codepoint);
std::string decode_quoted_literal(std::string_view literal);
std::string decode_fstring_part(std::string_view literal, token_type t);
uint32_t decode_utf8_char(std::string_view str);

} // namespace chirp::frontend
