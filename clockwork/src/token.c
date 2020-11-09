#include "token.h"

#include <stdio.h>
#include <assert.h>
#include <ctype.h>

const char* token_type_names[] = {
    [TOKEN_EOF] = "EOF",
    [TOKEN_INT] = "int",
    [TOKEN_FLOAT] = "float",
    [TOKEN_STR] = "string",
    [TOKEN_NAME] = "name",
    [TOKEN_LSHIFT] = "<<",
    [TOKEN_RSHIFT] = ">>",
    [TOKEN_EQ] = "==",
    [TOKEN_NOTEQ] = "!=",
    [TOKEN_LTEQ] = "<=",
    [TOKEN_GTEQ] = ">=",
    [TOKEN_AND] = "&&",
    [TOKEN_OR] = "||",
    [TOKEN_INC] = "++",
    [TOKEN_DEC] = "--",
    [TOKEN_COLON_ASSIGN] = ":=",
    [TOKEN_ADD_ASSIGN] = "+=",
    [TOKEN_SUB_ASSIGN] = "-=",
    [TOKEN_OR_ASSIGN] = "|=",
    [TOKEN_LSHIFT_ASSIGN] = "<<=",
    [TOKEN_RSHIFT_ASSIGN] = ">>=",
    [TOKEN_AND_ASSIGN] = "&=",
    [TOKEN_XOR_ASSIGN] = "^=",
    [TOKEN_MUL_ASSIGN] = "*=",
    [TOKEN_DIV_ASSIGN] = "/=",
    [TOKEN_MOD_ASSIGN] = "%=",
};

const char* token_type_name(TokenType type)
{
    return token_type_names[type];
}

size_t copy_token_type_str(char* dest, size_t dest_size, TokenType type)
{
    size_t n = 0;
    const char* name = token_type_name(type);
    if (name) {
        n = snprintf(dest, dest_size, "%s", name);
    }
    else if (type < 128 && isprint(type)) {
        n = snprintf(dest, dest_size, "%c", type);
    }
    else {
        n = snprintf(dest, dest_size, "<ASCII %d>", type);
    }
    return n;
}

const char* temp_token_type_str(TokenType type)
{
    static char buf[256];
    size_t n = copy_token_type_str(buf, sizeof(buf), type);
    assert(n + 1 <= sizeof(buf));
    return buf;
}

void print_token(Token token)
{
    switch (token.type)
    {
    case TOKEN_INT:
        printf("TOKEN INT: %llu\n", token.ival);
        break;
    case TOKEN_FLOAT:
        printf("TOKEN FLOAT: %f\n", token.fval);
        break;
    case TOKEN_NAME:
        printf("TOKEN NAME: %.*s\n", (int)(token.end - token.start), token.start);
        break;
    default:
        printf("TOKEN '%c'\n", token.type);
        break;
    }
}