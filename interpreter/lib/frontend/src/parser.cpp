#include "chirp/frontend.h"

#include <stdexcept>

namespace chirp::frontend {

namespace {

class Parser {
    const std::vector<token>& tokens;
    size_t current = 0;

    const token& peek() const {
        return tokens[current];
    }

    const token& peek_next() const {
        if (current + 1 >= tokens.size()) return tokens.back();
        return tokens[current + 1];
    }

    const token& previous() const {
        return tokens[current - 1];
    }

    bool is_at_end() const {
        return peek().type == token_type::eof;
    }

    bool check(token_type type) const {
        if (is_at_end()) return false;
        return peek().type == type;
    }

    const token& advance() {
        if (!is_at_end()) current++;
        return previous();
    }

    template<typename... Args>
    bool match(Args... types) {
        bool matches = (... || check(types));
        if (matches) {
            advance();
            return true;
        }
        return false;
    }

    const token& consume(token_type type, const char* message) {
        if (check(type)) return advance();
        throw std::runtime_error(std::string(message) + " at line " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + " (found: '" + std::string(peek().lexeme) + "')");
    }

    BinaryOp get_binary_op(token_type t) {
        switch (t) {
            case token_type::plus: return BinaryOp::Add;
            case token_type::minus: return BinaryOp::Sub;
            case token_type::star: return BinaryOp::Mul;
            case token_type::slash: return BinaryOp::Div;
            case token_type::percent: return BinaryOp::Mod;
            case token_type::equal_equal: return BinaryOp::Eq;
            case token_type::bang_equal: return BinaryOp::Neq;
            case token_type::less: return BinaryOp::Lt;
            case token_type::less_equal: return BinaryOp::Lte;
            case token_type::greater: return BinaryOp::Gt;
            case token_type::greater_equal: return BinaryOp::Gte;
            case token_type::and_and: return BinaryOp::And;
            case token_type::or_or: return BinaryOp::Or;
            case token_type::in_op: return BinaryOp::In;
            case token_type::not_in_op: return BinaryOp::NotIn;
            case token_type::subset_op: return BinaryOp::Subset;
            case token_type::proper_subset_op: return BinaryOp::ProperSubset;
            case token_type::not_subset_op: return BinaryOp::NotSubset;
            case token_type::superset_op: return BinaryOp::Superset;
            case token_type::proper_superset_op: return BinaryOp::ProperSuperset;
            case token_type::not_superset_op: return BinaryOp::NotSuperset;
            case token_type::union_op: return BinaryOp::Union;
            case token_type::intersection_op: return BinaryOp::Intersection;
            case token_type::dot_dot: return BinaryOp::Range;
            case token_type::dot_dot_equal: return BinaryOp::RangeInclusiveEnd;
            case token_type::dot: return BinaryOp::Dot;
            default: throw std::runtime_error("Invalid binary op");
        }
    }

    UnaryOp get_unary_op(token_type t) {
        switch (t) {
            case token_type::bang: return UnaryOp::Not;
            case token_type::minus: return UnaryOp::Negate;
            case token_type::star: return UnaryOp::Deref;
            case token_type::tilde: return UnaryOp::Complement;
            default: throw std::runtime_error("Invalid unary op");
        }
    }

    bool is_lambda_ahead() const {
        size_t temp = current;
        int parens = 0;
        
        while (temp < tokens.size()) {
            if (tokens[temp].type == token_type::left_paren) {
                parens++;
            } else if (tokens[temp].type == token_type::right_paren) {
                parens--;
                if (parens == 0) {
                    temp++;
                    break;
                }
            }
            temp++;
        }
        
        if (parens != 0) return false;
        
        if (temp < tokens.size()) {
            if (tokens[temp].type == token_type::fat_arrow) return true;
            if (tokens[temp].type == token_type::colon) {
                size_t look = temp + 1;
                while (look < tokens.size()) {
                    if (tokens[look].type == token_type::fat_arrow) return true;
                    if (tokens[look].type == token_type::semicolon ||
                        tokens[look].type == token_type::right_brace ||
                        tokens[look].type == token_type::right_paren ||
                        tokens[look].type == token_type::eof) {
                        break;
                    }
                    look++;
                }
            }
        }
        return false;
    }

public:
    explicit Parser(const std::vector<token>& tokens) : tokens(tokens) {}

    std::vector<std::unique_ptr<Stmt>> parse() {
        std::vector<std::unique_ptr<Stmt>> statements;
        while (!is_at_end()) {
            statements.push_back(statement());
        }
        return statements;
    }

    std::unique_ptr<Expr> parse_expression_only() {
        auto expr = expression();
        if (!is_at_end()) {
            throw std::runtime_error("Unexpected token after expression.");
        }
        return expr;
    }

private:
    token consume_binding_name() {
        if (match(token_type::identifier, token_type::intrinsic)) {
            return previous();
        }
        throw std::runtime_error("Expect identifier for binding. at line " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + " (found: '" + std::string(peek().lexeme) + "')");
    }

    NamedBinding parse_binding(bool require_initializer, bool allow_initializer, bool allow_function_sugar = false) {
        bool is_mut = match(token_type::kw_mut);
        token name = consume_binding_name();
        
        bool is_function = false;
        std::vector<NamedBinding> lambda_params;
        token paren;
        if (allow_function_sugar && check(token_type::left_paren)) {
            paren = peek();
            match(token_type::left_paren);
            is_function = true;
            if (!check(token_type::right_paren)) {
                do {
                    lambda_params.push_back(parse_binding(false, false, false));
                } while (match(token_type::comma));
            }
            consume(token_type::right_paren, "Expect ')' after function parameters.");
        }

        std::unique_ptr<Expr> type_bound = nullptr;
        if (match(token_type::colon)) {
            type_bound = expression();
        }
        std::unique_ptr<Expr> initializer = nullptr;
        if (match(token_type::equal)) {
            if (!allow_initializer) {
                throw std::runtime_error("Initializer not allowed in this binding context.");
            }
            initializer = expression();
        } else if (require_initializer) {
            throw std::runtime_error("Expect '=' and initializer for binding.");
        }

        if (is_function) {
            if (!initializer) {
                throw std::runtime_error("Function binding must have a body.");
            }
            initializer = std::make_unique<LambdaExpr>(std::move(lambda_params), std::move(type_bound), std::move(initializer), paren);
            type_bound = nullptr;
        }

        return NamedBinding(std::move(name), is_mut, std::move(type_bound), std::move(initializer));
    }

    std::unique_ptr<Stmt> statement() {
        if (match(token_type::kw_let)) return let_declaration();
        if (match(token_type::kw_break)) return break_statement();
        if (check(token_type::kw_if)) return if_leading_statement();
        return expression_statement();
    }

    std::unique_ptr<Stmt> let_declaration() {
        token let_tok = previous();
        NamedBinding binding = parse_binding(true, true, true);
        consume(token_type::semicolon, "Expect ';' after let declaration.");
        return std::make_unique<LetStmt>(std::move(binding), let_tok);
    }

    std::unique_ptr<Stmt> break_statement() {
        token break_tok = previous();
        std::unique_ptr<Expr> value;
        if (match(token_type::semicolon)) {
            token void_tok = break_tok;
            void_tok.type = token_type::intrinsic;
            void_tok.lexeme = "`void";
            value = std::make_unique<IntrinsicExpr>("`void", void_tok);
        } else {
            value = expression();
            consume(token_type::semicolon, "Expect ';' after break value.");
        }
        return std::make_unique<BreakStmt>(std::move(value), break_tok);
    }

    std::unique_ptr<Stmt> if_leading_statement() {
        size_t start = current;
        try {
            auto candidate = expression_statement();
            if (dynamic_cast<ExprStmt*>(candidate.get()) != nullptr) {
                return candidate;
            }
        } catch (const std::runtime_error&) {
        }

        current = start;
        match(token_type::kw_if);
        return if_statement();
    }

    std::unique_ptr<Stmt> if_statement() {
        token if_tok = previous();
        consume(token_type::left_paren, "Expect '(' after 'if'.");
        auto condition = expression();
        consume(token_type::right_paren, "Expect ')' after if condition.");

        auto then_branch = statement_branch();
        std::unique_ptr<Stmt> else_branch = nullptr;
        if (match(token_type::kw_else)) {
            else_branch = statement_branch();
        }
        match(token_type::semicolon);

        return std::make_unique<IfStmt>(std::move(condition), std::move(then_branch), std::move(else_branch), if_tok);
    }

    std::unique_ptr<Stmt> statement_branch() {
        if (check(token_type::left_brace) || check(token_type::kw_do)) {
            token start_tok = peek();
            auto expr = expression();
            return std::make_unique<ExprStmt>(std::move(expr), start_tok);
        }
        return statement();
    }

    std::unique_ptr<Stmt> expression_statement() {
        token start_tok = peek();
        auto expr = expression();
        
        if (match(token_type::equal, token_type::plus_equal, token_type::minus_equal, token_type::star_equal, token_type::slash_equal, token_type::percent_equal)) {
            token op = previous();
            auto value = expression();
            consume(token_type::semicolon, "Expect ';' after assignment.");
            return std::make_unique<AssignStmt>(std::move(expr), std::move(op), std::move(value), start_tok);
        }

        consume(token_type::semicolon, "Expect ';' after expression.");
        return std::make_unique<ExprStmt>(std::move(expr), start_tok);
    }

    std::unique_ptr<Expr> expression() {
        return logic_or();
    }

    std::unique_ptr<Expr> logic_or() {
        auto expr = logic_and();
        while (match(token_type::or_or)) {
            token op_tok = previous();
            auto right = logic_and();
            expr = std::make_unique<BinaryExpr>(std::move(expr), BinaryOp::Or, std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> logic_and() {
        auto expr = equality();
        while (match(token_type::and_and)) {
            token op_tok = previous();
            auto right = equality();
            expr = std::make_unique<BinaryExpr>(std::move(expr), BinaryOp::And, std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> equality() {
        auto expr = comparison();
        while (match(token_type::equal_equal, token_type::bang_equal)) {
            token op_tok = previous();
            auto right = comparison();
            expr = std::make_unique<BinaryExpr>(std::move(expr), get_binary_op(op_tok.type), std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> comparison() {
        auto expr = range();
        while (match(token_type::greater, token_type::greater_equal, token_type::less, token_type::less_equal, 
                     token_type::in_op, token_type::not_in_op, token_type::subset_op, token_type::proper_subset_op, 
                     token_type::not_subset_op, token_type::superset_op, token_type::proper_superset_op, token_type::not_superset_op)) {
            token op_tok = previous();
            auto right = range();
            expr = std::make_unique<BinaryExpr>(std::move(expr), get_binary_op(op_tok.type), std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> range() {
        auto expr = term();
        while (match(token_type::dot_dot, token_type::dot_dot_equal)) {
            token op_tok = previous();
            auto right = term();
            expr = std::make_unique<BinaryExpr>(std::move(expr), get_binary_op(op_tok.type), std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> term() {
        auto expr = factor();
        while (match(token_type::minus, token_type::plus, token_type::union_op)) {
            token op_tok = previous();
            auto right = factor();
            expr = std::make_unique<BinaryExpr>(std::move(expr), get_binary_op(op_tok.type), std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> factor() {
        auto expr = unary();
        while (match(token_type::slash, token_type::star, token_type::percent, token_type::intersection_op)) {
            token op_tok = previous();
            auto right = unary();
            expr = std::make_unique<BinaryExpr>(std::move(expr), get_binary_op(op_tok.type), std::move(right), op_tok);
        }
        return expr;
    }

    std::unique_ptr<Expr> unary() {
        if (match(token_type::ampersand)) {
            token op_tok = previous();
            UnaryOp op = match(token_type::kw_mut) ? UnaryOp::MutableAddressOf : UnaryOp::AddressOf;
            auto right = unary();
            return std::make_unique<UnaryExpr>(op, std::move(right), op_tok);
        }

        if (match(token_type::arrow)) {
            token op_tok = previous();
            UnaryOp op = match(token_type::kw_mut) ? UnaryOp::MutablePointerType : UnaryOp::PointerType;
            auto right = unary();
            return std::make_unique<UnaryExpr>(op, std::move(right), op_tok);
        }

        if (match(token_type::bang, token_type::minus, token_type::star, token_type::tilde)) {
            token op_tok = previous();
            auto right = unary();
            return std::make_unique<UnaryExpr>(get_unary_op(op_tok.type), std::move(right), op_tok);
        }
        return postfix();
    }

    std::unique_ptr<Expr> postfix() {
        auto expr = primary();
        while (true) {
            if (match(token_type::dot)) {
                token op_tok = previous();
                auto right = primary();
                expr = std::make_unique<BinaryExpr>(std::move(expr), BinaryOp::Dot, std::move(right), op_tok);
            } else if (match(token_type::left_paren)) {
                token paren = previous();
                std::vector<Argument> args;
                bool has_named = false;
                bool has_positional = false;

                if (!check(token_type::right_paren)) {
                    do {
                        if (check(token_type::identifier) && peek_next().type == token_type::equal) {
                            token name = consume(token_type::identifier, "");
                            consume(token_type::equal, "");
                            auto val = expression();
                            args.push_back(Argument(name, std::move(val)));
                            has_named = true;
                        } else {
                            auto val = expression();
                            args.push_back(Argument(std::nullopt, std::move(val)));
                            has_positional = true;
                        }
                    } while (match(token_type::comma));
                }
                consume(token_type::right_paren, "Expect ')' after arguments.");
                
                if (has_named && has_positional) {
                    throw std::runtime_error("Cannot mix named and positional arguments.");
                }
                
                expr = std::make_unique<CallExpr>(std::move(expr), std::move(args), paren);
            } else if (match(token_type::left_bracket)) {
                token bracket = previous();
                std::vector<Argument> args;
                if (!check(token_type::right_bracket)) {
                    do {
                        auto val = expression();
                        args.push_back(Argument(std::nullopt, std::move(val)));
                    } while (match(token_type::comma));
                } else {
                    throw std::runtime_error("Expect at least one argument for indexing.");
                }
                consume(token_type::right_bracket, "Expect ']' after index arguments.");
                
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(args), bracket);
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> primary() {
        if (match(token_type::kw_struct)) {
            token struct_tok = previous();
            consume(token_type::left_brace, "Expect '{' after 'struct'.");
            std::vector<NamedBinding> fields;
            if (!check(token_type::right_brace)) {
                while (true) {
                    fields.push_back(parse_binding(false, true, true));
                    if (!match(token_type::comma)) break;
                    if (check(token_type::right_brace)) break;
                }
            }
            consume(token_type::right_brace, "Expect '}' after struct fields.");
            return std::make_unique<StructExpr>(std::move(fields), struct_tok);
        }

        if (match(token_type::kw_for)) {
            token for_tok = previous();
            consume(token_type::left_paren, "Expect '(' after 'for'.");
            NamedBinding binding = parse_binding(false, false);
            consume(token_type::in_op, "Expect '∈' after for loop iterator.");
            auto iterable = expression();
            consume(token_type::right_paren, "Expect ')' after for loop iterable.");
            auto body = expression();
            return std::make_unique<ForExpr>(std::move(binding), std::move(iterable), std::move(body), for_tok);
        }

        if (match(token_type::kw_while)) {
            token while_tok = previous();
            consume(token_type::left_paren, "Expect '(' after 'while'.");
            auto condition = expression();
            consume(token_type::right_paren, "Expect ')' after while condition.");
            auto body = expression();
            return std::make_unique<WhileExpr>(std::move(condition), std::move(body), while_tok);
        }

        if (match(token_type::kw_match)) {
            token match_tok = previous();
            auto subject = expression();
            consume(token_type::left_brace, "Expect '{' after match subject.");
            std::vector<MatchArm> arms;
            if (!check(token_type::right_brace)) {
                do {
                    auto pattern = expression();
                    consume(token_type::fat_arrow, "Expect '=>' after match pattern.");
                    auto body = expression();
                    arms.emplace_back(std::move(pattern), std::move(body));
                } while (match(token_type::comma) && !check(token_type::right_brace));
            }
            consume(token_type::right_brace, "Expect '}' after match arms.");
            return std::make_unique<MatchExpr>(std::move(subject), std::move(arms), match_tok);
        }

        if (match(token_type::kw_if)) {
            token if_tok = previous();
            consume(token_type::left_paren, "Expect '(' after 'if'.");
            auto condition = expression();
            consume(token_type::right_paren, "Expect ')' after if condition.");
            auto then_branch = expression();
            consume(token_type::kw_else, "Expect 'else' branch for if expression.");
            auto else_branch = expression();
            return std::make_unique<IfExpr>(std::move(condition), std::move(then_branch), std::move(else_branch), if_tok);
        }

        if (match(token_type::kw_false)) return std::make_unique<BoolExpr>(false, previous());
        if (match(token_type::kw_true)) return std::make_unique<BoolExpr>(true, previous());
        if (match(token_type::kw_undecided)) return std::make_unique<UndecidedExpr>(previous());
        
        if (match(token_type::left_bracket)) {
            token bracket_tok = previous();
            std::vector<std::unique_ptr<Expr>> elements;
            if (!check(token_type::right_bracket)) {
                do {
                    elements.push_back(expression());
                } while (match(token_type::comma) && !check(token_type::right_bracket));
            }
            consume(token_type::right_bracket, "Expect ']' after list elements.");
            return std::make_unique<ListExpr>(std::move(elements), bracket_tok);
        }

        if (match(token_type::number)) return std::make_unique<NumberExpr>(previous().lexeme, previous());
        if (match(token_type::string)) return std::make_unique<StringExpr>(previous().lexeme, previous());
        if (match(token_type::character)) return std::make_unique<StringExpr>(previous().lexeme, previous());
        if (match(token_type::symbolic_constant)) return std::make_unique<SymbolicConstantExpr>(previous().lexeme, previous());
        
        if (match(token_type::identifier)) return std::make_unique<IdentifierExpr>(previous().lexeme, previous());
        if (match(token_type::intrinsic)) return std::make_unique<IntrinsicExpr>(previous().lexeme, previous());

        if (check(token_type::left_paren)) {
            if (is_lambda_ahead()) {
                token paren_tok = advance(); 
                std::vector<NamedBinding> parameters;
                if (!check(token_type::right_paren)) {
                    do {
                        parameters.push_back(parse_binding(false, false));
                    } while (match(token_type::comma));
                }
                consume(token_type::right_paren, "Expect ')' after lambda parameters.");
                
                std::unique_ptr<Expr> return_bound = nullptr;
                if (match(token_type::colon)) {
                    return_bound = expression();
                }
                
                consume(token_type::fat_arrow, "Expect '=>' for lambda body.");
                auto body = expression();
                return std::make_unique<LambdaExpr>(std::move(parameters), std::move(return_bound), std::move(body), paren_tok);
            } else {
                advance(); 
                auto expr = expression();
                consume(token_type::right_paren, "Expect ')' after grouping expression.");
                return std::make_unique<GroupingExpr>(std::move(expr));
            }
        }

        if (match(token_type::kw_do)) {
            token do_tok = previous();
            consume(token_type::left_brace, "Expect '{' after 'do'.");
            std::vector<std::unique_ptr<Stmt>> stmts;
            while (!check(token_type::right_brace) && !is_at_end()) {
                if (match(token_type::kw_let)) {
                    stmts.push_back(let_declaration());
                } else if (match(token_type::kw_break)) {
                    stmts.push_back(break_statement());
                } else {
                    size_t start = current;
                    try {
                        token expr_start_tok = peek();
                        auto expr = expression();
                        
                        if (check(token_type::right_brace)) {
                            stmts.push_back(std::make_unique<BreakStmt>(std::move(expr), expr_start_tok));
                            continue;
                        }
                        
                        if (match(token_type::equal, token_type::plus_equal, token_type::minus_equal,
                                  token_type::star_equal, token_type::slash_equal, token_type::percent_equal)) {
                            token op = previous();
                            auto value = expression();
                            consume(token_type::semicolon, "Expect ';' after assignment.");
                            stmts.push_back(std::make_unique<AssignStmt>(std::move(expr), std::move(op), std::move(value), expr_start_tok));
                            continue;
                        }
                        
                        if (match(token_type::semicolon)) {
                            stmts.push_back(std::make_unique<ExprStmt>(std::move(expr), expr_start_tok));
                            continue;
                        }
                        
                        current = start;
                    } catch (const std::runtime_error&) {
                        current = start;
                    }
                    
                    stmts.push_back(statement());
                }
            }
            consume(token_type::right_brace, "Expect '}' after block.");
            return std::make_unique<BlockExpr>(std::move(stmts), do_tok);
        }

        if (match(token_type::left_brace)) {
            token brace_tok = previous();
            
            if (match(token_type::right_brace)) {
                return std::make_unique<EnumeratedSetExpr>(std::vector<std::unique_ptr<Expr>>{}, brace_tok);
            }

            if (check(token_type::identifier) && (peek_next().type == token_type::pipe || peek_next().type == token_type::colon)) {
                NamedBinding binding = parse_binding(false, false);
                
                consume(token_type::pipe, "Expect '|' in constructed set.");
                auto condition = expression();
                consume(token_type::right_brace, "Expect '}' after constructed set condition.");
                
                return std::make_unique<ConstructedSetExpr>(std::move(binding), std::move(condition), brace_tok);
            }

            std::vector<std::unique_ptr<Expr>> elements;
            elements.push_back(expression());
            while (match(token_type::comma)) {
                if (check(token_type::right_brace)) break; 
                elements.push_back(expression());
            }
            consume(token_type::right_brace, "Expect '}' after enumerated set elements.");
            return std::make_unique<EnumeratedSetExpr>(std::move(elements), brace_tok);
        }

        throw std::runtime_error("Expect expression.");
    }
};

} // anonymous namespace

std::unique_ptr<Expr> parse_expression(const std::vector<token>& tokens) {
    if (tokens.empty() || tokens.front().type == token_type::eof) {
        return nullptr;
    }
    Parser parser(tokens);
    return parser.parse_expression_only();
}

std::vector<std::unique_ptr<Stmt>> parse(const std::vector<token>& tokens) {
    if (tokens.empty() || tokens.front().type == token_type::eof) {
        return {};
    }
    Parser parser(tokens);
    return parser.parse();
}

} // namespace chirp::frontend
