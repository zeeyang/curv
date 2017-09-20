// Copyright Doug Moen 2016-2017.
// Distributed under The MIT License.
// See accompanying file LICENCE.md or https://opensource.org/licenses/MIT

// TODO: Convert to use the re2c scanner generator. Then add UTF-8 support.

#include <curv/scanner.h>
#include <curv/exception.h>
#include <curv/context.h>

using namespace std;
namespace curv {

Scanner::Scanner(const Script& s, Frame* f)
:
    script_(s),
    eval_frame_(f),
    string_begin_(),
    ptr_(s.begin()),
    lookahead_()
{
}

void
Scanner::push_token(Token tok)
{
    //cerr << "push_token " << tok << "\n";
    lookahead_.push_back(tok);
}

Token
Scanner::get_token()
{
    if (!lookahead_.empty()) {
        auto tok = lookahead_.back();
        lookahead_.pop_back();
        //cerr << "get_token from lookahead" << tok << "\n";
        return tok;
    }

    Token tok;
    const char* p = ptr_;
    const char* first = script_.first;
    const char* last = script_.last;

    if (string_begin_.kind_ != Token::k_missing) {
        // We are inside a string literal.
        if (p >= last) {
            throw Exception(At_Token(string_begin_, *this),
                "unterminated string literal");
        }
        tok.first_white_ = tok.first_ = p - first;
        if (*p == '"') {
            ++p;
            if (p < last && *p == '"') {
                ++p;
                tok.kind_ = Token::k_char_escape;
                goto success;
            }
            tok.kind_ = Token::k_quote;
            string_begin_.kind_ = Token::k_missing;
            goto success;
        }
        if (*p == '$') {
            ++p;
            if (p == last) {
                throw Exception(At_Token(string_begin_, *this),
                    "unterminated string literal");
            }
            if (*p == '$') {
                ++p;
                tok.kind_ = Token::k_char_escape;
                goto success;
            }
            if (*p == '(') {
                ++p;
                tok.kind_ = Token::k_dollar_paren;
                goto success;
            }
            ++p;
            tok.first_ = p - 2 - first;
            tok.last_ = p - first;
            tok.kind_ = Token::k_bad_token;
            ptr_ = p;
            throw Exception(At_Token(tok, *this), "illegal string escape");
        }
        while (p < last && *p != '$' && *p != '"')
            ++p;
        tok.kind_ = Token::k_string_segment;
        goto success;
    }

    // collect whitespace and comments
    tok.first_white_ = p - first;
    while (p < last) {
        if (isspace(*p)) {
            ++p;
        }
        else if (*p == '/' && p+1 < last) {
            if (p[1] == '/') {
                // A '//' comment continues to the end of the file or the line.
                p += 2;
                for (;;) {
                    if (p == last) break;
                    if (*p == '\n') {
                        ++p;
                        break;
                    }
                    ++p;
                }
            }
            else if (p[1] == '*') {
                // A '/*' comment continues to the first '*/', as in C.
                // An unterminated comment is an error.
                const char* begin_comment = p;
                p += 2;
                for (;;) {
                    if (p+1 < last && p[0]=='*' && p[1]=='/') {
                        p += 2;
                        break;
                    }
                    if (p == last) {
                        ptr_ = p;
                        tok.kind_ = Token::k_bad_token;
                        tok.first_ = begin_comment - first;
                        tok.last_ = last - first;
                        throw Exception(At_Token(tok, *this),
                            "unterminated comment");
                    }
                    ++p;
                }
            }
            else
                break;
        }
        else
            break;
    }
    tok.first_ = p - first;

    // recognize end of script
    if (p == last) {
        tok.kind_ = Token::k_end;
        goto success;
    }

    // Recognize a numeral. Compatible with C and strtod().
    //   numeral ::= significand exponent?
    //   significand ::= digits | digits "." | "." digits | digits "." digits
    //   exponent ::= "e" sign? digits
    //   sign ::= "+" | "-"
    //   digits ::= /[0-9]+/
    if (isdigit(*p) || (*p == '.' && p+1 < last && isdigit(p[1]))) {
        while (p < last && isdigit(*p))
            ++p;
        if (p < last && *p == '.' && !(p+1 < last && p[1]=='.')) {
            ++p;
            while (p < last && isdigit(*p))
                ++p;
        }
        if (p < last && (*p == 'e' || *p == 'E')) {
            ++p;
            if (p < last && (*p == '+' || *p == '-'))
                ++p;
            if (p == last || !isdigit(*p)) {
                while (p < last && (isalnum(*p) || *p == '_'))
                    ++p;
                tok.last_ = p - first;
                ptr_ = p;
                throw Exception(At_Token(tok, *this), "bad numeral");
            }
            while (p < last && isdigit(*p))
                ++p;
        }
        if (p < last && (isalpha(*p) || *p == '_')) {
            while (p < last && (isalnum(*p) || *p == '_'))
                ++p;
            tok.last_ = p - first;
            ptr_ = p;
            throw Exception(At_Token(tok, *this), "bad numeral");
        }
        tok.kind_ = Token::k_num;
        goto success;
    }

    // recognize an identifier
    if (isalpha(*p) || *p == '_') {
        while (p < last && (isalnum(*p) || *p == '_'))
            ++p;
        aux::Range<const char*> id(first+tok.first_, p);
        if (id == "by")
            tok.kind_ = Token::k_by;
        else if (id == "else")
            tok.kind_ = Token::k_else;
        else if (id == "for")
            tok.kind_ = Token::k_for;
        else if (id == "if")
            tok.kind_ = Token::k_if;
        else if (id == "in")
            tok.kind_ = Token::k_in;
        else if (id == "var")
            tok.kind_ = Token::k_var;
        else if (id == "while")
            tok.kind_ = Token::k_while;
        else
            tok.kind_ = Token::k_ident;
        goto success;
    }

    // recognize remaining tokens
    switch (*p++) {
    case '(':
        tok.kind_ = Token::k_lparen;
        goto success;
    case ')':
        tok.kind_ = Token::k_rparen;
        goto success;
    case '[':
        tok.kind_ = Token::k_lbracket;
        goto success;
    case ']':
        tok.kind_ = Token::k_rbracket;
        goto success;
    case '{':
        tok.kind_ = Token::k_lbrace;
        goto success;
    case '}':
        tok.kind_ = Token::k_rbrace;
        goto success;
    case '.':
        if (p < last && *p == '.') {
            if (p+1 < last && p[1] == '<') {
                tok.kind_ = Token::k_open_range;
                p += 2;
            } else if (p+1 < last && p[1] == '.') {
                tok.kind_ = Token::k_ellipsis;
                p += 2;
            } else {
                tok.kind_ = Token::k_range;
                ++p;
            }
        } else
            tok.kind_ = Token::k_dot;
        goto success;
    case ',':
        tok.kind_ = Token::k_comma;
        goto success;
    case ';':
        tok.kind_ = Token::k_semicolon;
        goto success;
    case ':':
        if (p < last && *p == '=') {
            tok.kind_ = Token::k_assign;
            ++p;
        } else
            tok.kind_ = Token::k_colon;
        goto success;
    case '+':
        tok.kind_ = Token::k_plus;
        goto success;
    case '-':
        if (p < last && *p == '>') {
            tok.kind_ = Token::k_right_arrow;
            ++p;
        } else
            tok.kind_ = Token::k_minus;
        goto success;
    case '*':
        tok.kind_ = Token::k_times;
        goto success;
    case '/':
        tok.kind_ = Token::k_over;
        goto success;
    case '^':
        tok.kind_ = Token::k_power;
        goto success;
    case '\'':
        tok.kind_ = Token::k_apostrophe;
        goto success;
    case '@':
        tok.kind_ = Token::k_at;
        goto success;
    case '=':
        if (p < last && *p == '=') {
            tok.kind_ = Token::k_equal;
            ++p;
        } else
            tok.kind_ = Token::k_equate;
        goto success;
    case '!':
        if (p < last && *p == '=') {
            tok.kind_ = Token::k_not_equal;
            ++p;
        } else
            tok.kind_ = Token::k_not;
        goto success;
    case '<':
        if (p < last && *p == '=') {
            tok.kind_ = Token::k_less_or_equal;
            ++p;
        } else if (p < last && *p == '<') {
            tok.kind_ = Token::k_left_call;
            ++p;
        } else
            tok.kind_ = Token::k_less;
        goto success;
    case '>':
        if (p < last && *p == '=') {
            tok.kind_ = Token::k_greater_or_equal;
            ++p;
        } else if (p < last && *p == '>') {
            tok.kind_ = Token::k_right_call;
            ++p;
        } else
            tok.kind_ = Token::k_greater;
        goto success;
    case '&':
        if (p < last && *p == '&') {
            tok.kind_ = Token::k_and;
            ++p;
        } else
            goto error;
        goto success;
    case '|':
        if (p < last && *p == '|') {
            tok.kind_ = Token::k_or;
            ++p;
        } else
            goto error;
        goto success;
    case '"':
        tok.kind_ = Token::k_quote;
        tok.last_ = p - first;
        string_begin_ = tok;
        goto success;
    }

    // report an error
error:
    tok.last_ = p - first;
    tok.kind_ = Token::k_bad_token;
    ptr_ = p;
    throw Exception(At_Token(tok, *this), illegal_character_message(p[-1]));

success:
    tok.last_ = p - first;
    ptr_ = p;
    //cerr << "get_token fresh " << tok << "\n";
    return tok;
}

} // namespace curv
