#include "chirp/frontend.h"
#include <stdexcept>

namespace chirp::frontend {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return 10 + c - 'A';
}

void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::string decode_quoted_literal(std::string_view literal) {
    if (literal.size() < 2) {
        throw std::runtime_error("Malformed string literal");
    }

    std::string out;
    for (size_t i = 1; i + 1 < literal.size(); ++i) {
        char c = literal[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }

        if (++i + 1 > literal.size()) {
            throw std::runtime_error("Malformed escape sequence");
        }

        char escaped = literal[i];
        switch (escaped) {
            case '\\': out.push_back('\\'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '0': out.push_back('\0'); break;
            case 'u': {
                if (i + 4 >= literal.size()) {
                    throw std::runtime_error("Malformed unicode escape");
                }
                uint32_t codepoint = 0;
                for (int n = 0; n < 4; ++n) {
                    char h = literal[++i];
                    codepoint = (codepoint << 4) | static_cast<uint32_t>(hex_value(h));
                }
                append_utf8(out, codepoint);
                break;
            }
            default:
                throw std::runtime_error("Unsupported escape sequence");
        }
    }
    return out;
}

std::string decode_fstring_part(std::string_view literal, token_type t) {
    size_t start = (t == token_type::fstring_head || t == token_type::fstring_literal) ? 2 : 1;
    size_t end = (t == token_type::fstring_tail || t == token_type::fstring_literal) ? literal.size() - 1 : literal.size() - 1;
    if (start > end) return "";

    std::string out;
    for (size_t i = start; i < end; ++i) {
        char c = literal[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }

        if (++i >= end) {
            throw std::runtime_error("Malformed escape sequence");
        }

        char escaped = literal[i];
        switch (escaped) {
            case '\\': out.push_back('\\'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case '{': out.push_back('{'); break;
            case '}': out.push_back('}'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '0': out.push_back('\0'); break;
            case 'u': {
                if (i + 4 >= end) {
                    throw std::runtime_error("Malformed unicode escape");
                }
                uint32_t codepoint = 0;
                for (int n = 0; n < 4; ++n) {
                    char h = literal[++i];
                    codepoint = (codepoint << 4) | static_cast<uint32_t>(hex_value(h));
                }
                append_utf8(out, codepoint);
                break;
            }
            default:
                throw std::runtime_error("Unsupported escape sequence");
        }
    }
    return out;
}

uint32_t decode_utf8_char(std::string_view str) {
    if (str.empty()) {
        throw std::runtime_error("Empty character literal");
    }
    unsigned char c1 = str[0];
    if (c1 < 0x80) {
        return c1;
    } else if ((c1 & 0xE0) == 0xC0) {
        if (str.size() < 2) throw std::runtime_error("Malformed UTF-8 in character literal");
        unsigned char c2 = str[1];
        return ((c1 & 0x1F) << 6) | (c2 & 0x3F);
    } else if ((c1 & 0xF0) == 0xE0) {
        if (str.size() < 3) throw std::runtime_error("Malformed UTF-8 in character literal");
        unsigned char c2 = str[1];
        unsigned char c3 = str[2];
        return ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    } else if ((c1 & 0xF8) == 0xF0) {
        if (str.size() < 4) throw std::runtime_error("Malformed UTF-8 in character literal");
        unsigned char c2 = str[1];
        unsigned char c3 = str[2];
        unsigned char c4 = str[3];
        return ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
    }
    throw std::runtime_error("Invalid UTF-8 sequence");
}

} // namespace chirp::frontend
