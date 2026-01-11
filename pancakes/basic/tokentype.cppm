module;
#include <string_view>
export module pancakes.basic.tokentype;

export enum class TokenType
{
    /* Keyword */
    PRINT, INPUT, LET, IF, THEN, ELSE, END_IF, FOR, TO,
        STEP, NEXT, WHILE, WEND, DO, LOOP, UNTIL, GOTO, GOSUB,
        RETURN, SUB, END_SUB, FUNCTION, END_FUNCTION, END, DIM, REM,

        /* Operators */
        PLUS, MINUS, MULTIPLY, DIVIDE, INT_DIVIDE, POWER, EQ, NEQ,
        LT, GT, LE, GE, AND, OR, NOT, XOR,

        /* Literals or Constants */
        NUMBER, STRING,

        /* Identifiers */
        IDENTIFIER, SPC, TAB,

        /* Punctuation or Misc */
        COMMA, LPAREN, RPAREN, COLON, SEMICOLON, APOSTROPHE,

        END_OF_LINE,
        END_OF_FILE,

        UNKNOWN
};

export constexpr std::string_view TokenTypeName(TokenType const t)
{
    switch (t)
    {
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::END_OF_LINE: return "END_OF_LINE";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::PRINT: return "PRINT";
        case TokenType::STRING: return "STRING";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::INPUT: return "INPUT";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::COMMA: return "COMMA";
        case TokenType::APOSTROPHE: return "APOSTROPHE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        default: return "UNKNOWN";
    }
}
