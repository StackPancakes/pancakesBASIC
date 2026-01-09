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
        IDENTIFIER,

        /* Punctuation or Misc */
        COMMA, LPAREN, RPAREN, COLON,

        END_OF_FILE,

        UNKNOWN
};

export constexpr std::string_view TokenTypeName(TokenType const t)
{
    switch (t)
    {
        case TokenType::PRINT: return "PRINT";
        case TokenType::STRING: return "STRING";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "UNKNOWN";
    }
}
