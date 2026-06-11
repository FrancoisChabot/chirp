#include "chirp/frontend.h"
#include <cctype>
#include <cstdint>

namespace chirp::frontend {

namespace {

class Lexer {
    std::string_view source;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;
    int start_column = 1;
    std::string_view pending_trivia;
    std::vector<token> tokens;
    std::vector<int> template_brace_counts;

    bool is_at_end() const { 
        return current >= source.length(); 
    }

    char advance() {
        char c = source[current++];
        if (c == '\n') {
            line++;
            column = 1;
        } else if ((c & 0xC0) != 0x80) { 
            // Only increment column on starting bytes of UTF-8 sequences
            column++;
        }
        return c;
    }

    char peek() const {
        if (is_at_end()) return '\0';
        return source[current];
    }

    char peek_next() const {
        if (current + 1 >= source.length()) return '\0';
        return source[current + 1];
    }

    bool match(char expected) {
        if (is_at_end()) return false;
        if (source[current] != expected) return false;
        advance();
        return true;
    }

    bool match_str(std::string_view expected) {
        if (current + expected.length() > source.length()) return false;
        if (source.substr(current, expected.length()) == expected) {
            for (size_t i = 0; i < expected.length(); i++) {
                advance();
            }
            return true;
        }
        return false;
    }

    static bool is_identifier_continue(unsigned char c) {
        return std::isalnum(c) || c == '_';
    }

    bool match_word(std::string_view expected) {
        if (current + expected.length() > source.length()) return false;
        if (source.substr(current, expected.length()) != expected) return false;
        unsigned char next = current + expected.length() < source.length()
            ? static_cast<unsigned char>(source[current + expected.length()])
            : '\0';
        if (is_identifier_continue(next)) return false;
        for (size_t i = 0; i < expected.length(); i++) {
            advance();
        }
        return true;
    }

    static bool is_continuation_byte(unsigned char c) {
        return (c & 0xC0) == 0x80;
    }

    static bool is_hex_digit(char c) {
        return std::isxdigit(static_cast<unsigned char>(c));
    }

    static uint32_t hex_value(char c) {
        if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(10 + c - 'a');
        return static_cast<uint32_t>(10 + c - 'A');
    }

    unsigned char peek_byte(size_t offset = 0) const {
        if (current + offset >= source.length()) return '\0';
        return static_cast<unsigned char>(source[current + offset]);
    }

    bool has_bytes(size_t count) const {
        return current + count <= source.length();
    }

    bool consume_utf8_codepoint() {
        if (is_at_end()) return false;

        unsigned char c = peek_byte();
        if (c == '\n' || c == '\r') return false;

        if (c <= 0x7F) {
            advance();
            return true;
        }

        size_t byte_count = 0;
        bool valid = false;

        if (c >= 0xC2 && c <= 0xDF) {
            byte_count = 2;
            valid = has_bytes(byte_count) && is_continuation_byte(peek_byte(1));
        } else if (c == 0xE0) {
            byte_count = 3;
            valid = has_bytes(byte_count) &&
                peek_byte(1) >= 0xA0 && peek_byte(1) <= 0xBF &&
                is_continuation_byte(peek_byte(2));
        } else if (c >= 0xE1 && c <= 0xEC) {
            byte_count = 3;
            valid = has_bytes(byte_count) &&
                is_continuation_byte(peek_byte(1)) &&
                is_continuation_byte(peek_byte(2));
        } else if (c == 0xED) {
            byte_count = 3;
            valid = has_bytes(byte_count) &&
                peek_byte(1) >= 0x80 && peek_byte(1) <= 0x9F &&
                is_continuation_byte(peek_byte(2));
        } else if (c >= 0xEE && c <= 0xEF) {
            byte_count = 3;
            valid = has_bytes(byte_count) &&
                is_continuation_byte(peek_byte(1)) &&
                is_continuation_byte(peek_byte(2));
        } else if (c == 0xF0) {
            byte_count = 4;
            valid = has_bytes(byte_count) &&
                peek_byte(1) >= 0x90 && peek_byte(1) <= 0xBF &&
                is_continuation_byte(peek_byte(2)) &&
                is_continuation_byte(peek_byte(3));
        } else if (c >= 0xF1 && c <= 0xF3) {
            byte_count = 4;
            valid = has_bytes(byte_count) &&
                is_continuation_byte(peek_byte(1)) &&
                is_continuation_byte(peek_byte(2)) &&
                is_continuation_byte(peek_byte(3));
        } else if (c == 0xF4) {
            byte_count = 4;
            valid = has_bytes(byte_count) &&
                peek_byte(1) >= 0x80 && peek_byte(1) <= 0x8F &&
                is_continuation_byte(peek_byte(2)) &&
                is_continuation_byte(peek_byte(3));
        }

        if (!valid) return false;

        for (size_t i = 0; i < byte_count; i++) {
            advance();
        }
        return true;
    }

    bool consume_unicode_escape() {
        // The initial backslash and 'u' have already been consumed.
        uint32_t codepoint = 0;
        for (int i = 0; i < 4; i++) {
            if (is_at_end() || !is_hex_digit(peek())) return false;
            codepoint = (codepoint << 4) | hex_value(peek());
            advance();
        }

        return codepoint < 0xD800 || codepoint > 0xDFFF;
    }

    bool consume_escape_codepoint() {
        if (!match('\\') || is_at_end()) return false;

        char c = advance();
        switch (c) {
            case '\\':
            case '\'':
            case '"':
            case 'n':
            case 'r':
            case 't':
            case '0':
                return true;
            case 'u':
                return consume_unicode_escape();
            default:
                return false;
        }
    }

    void consume_invalid_character_literal() {
        while (peek() != '\'' && !is_at_end()) {
            advance();
        }
        if (!is_at_end()) {
            advance(); // consume closing quote for recovery
        }
        add_token(token_type::error);
    }

    void add_token(token_type type) {
        tokens.push_back(token{
            type,
            source.substr(start, current - start),
            line,
            start_column,
            pending_trivia
        });
        pending_trivia = "";
    }

    void skip_whitespace() {
        while (true) {
            char c = peek();
            switch (c) {
                case ' ':
                case '\r':
                case '\t':
                case '\n':
                    advance();
                    break;
                case '/':
                    if (peek_next() == '/') {
                        while (peek() != '\n' && !is_at_end()) advance();
                    } else {
                        return;
                    }
                    break;
                default:
                    return;
            }
        }
    }

    void identifier() {
        while (std::isalnum(peek()) || peek() == '_') {
            advance();
        }
        std::string_view text = source.substr(start, current - start);
        token_type type = token_type::identifier;
        
        if (text == "let") type = token_type::kw_let;
        else if (text == "struct") type = token_type::kw_struct;
        else if (text == "if") type = token_type::kw_if;
        else if (text == "else") type = token_type::kw_else;
        else if (text == "while") type = token_type::kw_while;
        else if (text == "for") type = token_type::kw_for;
        else if (text == "break") type = token_type::kw_break;
        else if (text == "match") type = token_type::kw_match;
        else if (text == "do") type = token_type::kw_do;
        else if (text == "debug") type = token_type::kw_debug;
        else if (text == "enum") type = token_type::kw_enum;

        add_token(type);
    }

    void intrinsic() {
        while (std::isalnum(peek()) || peek() == '_') {
            advance();
        }
        if (current - start == 1) {
            add_token(token_type::error);
        } else {
            std::string_view text = source.substr(start, current - start);
            if (text == "`in") add_token(token_type::in_op);
            else if (text == "`union") add_token(token_type::union_op);
            else if (text == "`intersection") add_token(token_type::intersection_op);
            else add_token(token_type::intrinsic);
        }
    }

    void number() {
        while (std::isdigit(peek())) advance();

        if (peek() == '.' && std::isdigit(peek_next())) {
            advance(); // consume '.'
            while (std::isdigit(peek())) advance();
        }

        add_token(token_type::number);
    }

    void string() {
        while (peek() != '"' && !is_at_end()) {
            if (peek() == '\\') advance();
            advance();
        }
        if (is_at_end()) {
            add_token(token_type::error);
            return;
        }
        advance(); // consume '"'
        add_token(token_type::string);
    }

    void scan_template_string_part(bool is_head) {
        while (peek() != '"' && peek() != '{' && !is_at_end()) {
            if (peek() == '\\') {
                advance();
                if (!is_at_end()) advance();
                continue;
            }
            advance();
        }
        if (is_at_end()) {
            add_token(token_type::error);
            return;
        }
        if (peek() == '"') {
            advance(); // consume '"'
            add_token(is_head ? token_type::fstring_literal : token_type::fstring_tail);
        } else if (peek() == '{') {
            advance(); // consume '{'
            add_token(is_head ? token_type::fstring_head : token_type::fstring_middle);
            template_brace_counts.push_back(0);
        }
    }

    void character() {
        if (is_at_end() || peek() == '\'' || peek() == '\n' || peek() == '\r') {
            consume_invalid_character_literal();
            return;
        }

        bool valid = peek() == '\\'
            ? consume_escape_codepoint()
            : consume_utf8_codepoint();

        if (!valid || is_at_end() || peek() != '\'') {
            consume_invalid_character_literal();
            return;
        }

        advance(); // consume '\''
        add_token(token_type::character);
    }

    void symbolic_constant() {
        // '#' is already consumed by advance() before this is called
        if (!std::isalpha(peek()) && peek() != '_') {
            add_token(token_type::error);
            return;
        }
        while (std::isalnum(peek()) || peek() == '_') {
            advance();
        }
        add_token(token_type::symbolic_constant);
    }

    void scan_token() {
        size_t trivia_start = current;
        skip_whitespace();
        pending_trivia = source.substr(trivia_start, current - trivia_start);
        
        if (is_at_end()) return;

        start = current;
        start_column = column;

        if (match_str("∈")) { add_token(token_type::in_op); return; }
        if (match_str("∉")) { add_token(token_type::not_in_op); return; }

        if (match_str("∪")) { add_token(token_type::union_op); return; }
        if (match_str("∩")) { add_token(token_type::intersection_op); return; }
        if (match_str("..=")) { add_token(token_type::dot_dot_equal); return; }

        char c = advance();

        if (c == 'f' && peek() == '"') {
            advance(); // consume '"'
            scan_template_string_part(true);
            return;
        }

        if (std::isalpha(c) || c == '_') { identifier(); return; }
        if (c == '`') { intrinsic(); return; }
        if (c == '~') { add_token(token_type::tilde); return; }
        if (std::isdigit(c)) { number(); return; }

        switch (c) {
            case '(': add_token(token_type::left_paren); break;
            case ')': add_token(token_type::right_paren); break;
            case '{': 
                if (!template_brace_counts.empty()) {
                    template_brace_counts.back()++;
                }
                add_token(token_type::left_brace); 
                break;
            case '}': 
                if (!template_brace_counts.empty()) {
                    if (template_brace_counts.back() > 0) {
                        template_brace_counts.back()--;
                        add_token(token_type::right_brace);
                    } else {
                        template_brace_counts.pop_back();
                        scan_template_string_part(false);
                    }
                } else {
                    add_token(token_type::right_brace);
                }
                break;
            case '[': add_token(token_type::left_bracket); break;
            case ']': add_token(token_type::right_bracket); break;
            case ',': add_token(token_type::comma); break;
            case '.': 
                if (match('.')) add_token(token_type::dot_dot);
                else add_token(token_type::dot);
                break;
            case '-':
                if (match('>')) {
                    if (match_word("mut")) add_token(token_type::arrow_mut);
                    else add_token(token_type::arrow);
                }
                else if (match('=')) add_token(token_type::minus_equal);
                else add_token(token_type::minus);
                break;
            case '+': 
                if (match('=')) add_token(token_type::plus_equal);
                else add_token(token_type::plus); 
                break;
            case ';': add_token(token_type::semicolon); break;
            case ':': add_token(token_type::colon); break;
            case '*': 
                if (match('=')) add_token(token_type::star_equal);
                else add_token(token_type::star); 
                break;
            case '%': 
                if (match('=')) add_token(token_type::percent_equal);
                else add_token(token_type::percent); 
                break;
            case '|':
                if (match('|')) add_token(token_type::or_or);
                else add_token(token_type::pipe);
                break;
            case '&':
                if (match('&')) add_token(token_type::and_and);
                else if (match_word("mut")) add_token(token_type::ampersand_mut);
                else add_token(token_type::ampersand);
                break;
            case '!':
                if (match('=')) add_token(token_type::bang_equal);
                else add_token(token_type::bang);
                break;
            case '=':
                if (match('=')) add_token(token_type::equal_equal);
                else if (match('>')) add_token(token_type::fat_arrow);
                else add_token(token_type::equal);
                break;
            case '<':
                if (match('=')) add_token(token_type::less_equal);
                else add_token(token_type::less);
                break;
            case '>':
                if (match('=')) add_token(token_type::greater_equal);
                else add_token(token_type::greater);
                break;
            case '/': 
                if (match('=')) add_token(token_type::slash_equal);
                else add_token(token_type::slash); 
                break;
            case '"': string(); break;
            case '\'': character(); break;
            case '#': symbolic_constant(); break;
            default:
                add_token(token_type::error);
                break;
        }
    }

public:
    Lexer(std::string_view input) : source(input) {}

    std::vector<token> scan_tokens() {
        while (!is_at_end()) {
            scan_token();
        }
        tokens.push_back(token{token_type::eof, "", line, column, pending_trivia});
        return tokens;
    }
};

} // anonymous namespace

std::vector<token> tokenize(std::string_view input) {
    Lexer lexer(input);
    return lexer.scan_tokens();
}

std::string format_text(std::string_view source) {
    std::string out;
    auto tokens = tokenize(source);
    for (const auto& t : tokens) {
        out.append(t.leading_trivia);
        if (t.type == token_type::eof) {
            break;
        }
        
        if (t.type == token_type::in_op && t.lexeme == "`in") {
            out.append("∈");
        } else if (t.type == token_type::union_op && t.lexeme == "`union") {
            out.append("∪");
        } else if (t.type == token_type::intersection_op && t.lexeme == "`intersection") {
            out.append("∩");
        } else {
            out.append(t.lexeme);
        }
    }
    return out;
}

} // namespace chirp::frontend
