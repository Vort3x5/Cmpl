#ifndef CMPL_H
#define CMPL_H

#include <arena.h>

#include <stdbool.h>

typedef char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef long s32;
typedef unsigned long u32;
typedef long long s64;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef enum {
	TOKEN_EOF,
	TOKEN_ID,
	TOKEN_NUM,
	TOKEN_STR,
	TOKEN_PROC,
	TOKEN_ASSIGN,
	TOKEN_L_PAREN,
	TOKEN_R_PAREN,
	TOKEN_L_BRACE,
	TOKEN_R_BRACE,
	TOKEN_L_BRACKET,
	TOKEN_R_BRACKET,
	TOKEN_SEMICOLON,
	TOKEN_COMMA,
	TOKEN_DOT,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_MOD,
	TOKEN_NOT,
	TOKEN_EQ,
	TOKEN_NOT_EQ,
	TOKEN_LESS_EQ,
	TOKEN_LESS,
	TOKEN_GREATER_EQ,
	TOKEN_GREATER,
	TOKEN_AMPERSAND,
	TOKEN_PIPE,
	TOKEN_CARET,
	TOKEN_TILDE,
	TOKEN_ERR,
} Token_Type;

typedef struct {
	Token_Type type;
	char *lexeme;
	u32 line, column;
	union {
		s64 num;
		char *str;
	} value;
} Token;

typedef enum {
	AST_PROGRAM,
	AST_PROC,
	AST_VAR,
	AST_ASSIGNMENT,
	AST_BIN_OP,
	AST_CALL,
	AST_BLOCK,
	AST_NUM,
	AST_ID
} AST_Type;

typedef struct AST_Node AST_Node;

struct AST_Node 
{
	AST_Type type;
	char *name;

	AST_Node *left, *right, *body;
	AST_Array children;

	s64 num;
	char *str;

	u32 line, column;
};

typedef struct {
	AST_Node **data;
	size_t size, used;
} AST_Array;

typedef struct {
	const char *src;
	size_t curr, len;
	u32 line, column;
	Arena *arena;
} Lexer;

#ifdef LEXER_DEF
    const char* type_names[] = {
        [TOKEN_EOF] = "EOF",
        [TOKEN_IDENTIFIER] = "IDENTIFIER",
        [TOKEN_NUMBER] = "NUMBER",
        [TOKEN_STRING] = "STRING",
        [TOKEN_PROCEDURE] = "PROCEDURE",
        [TOKEN_ASSIGN] = "ASSIGN",
        [TOKEN_EQUAL] = "EQUAL",
        [TOKEN_NOT_EQUAL] = "NOT_EQUAL",
        [TOKEN_NOT] = "NOT",
        [TOKEN_LESS] = "LESS",
        [TOKEN_LESS_EQUAL] = "LESS_EQUAL",
        [TOKEN_GREATER] = "GREATER",
        [TOKEN_GREATER_EQUAL] = "GREATER_EQUAL",
        [TOKEN_LPAREN] = "LPAREN",
        [TOKEN_RPAREN] = "RPAREN",
        [TOKEN_LBRACE] = "LBRACE",
        [TOKEN_RBRACE] = "RBRACE",
        [TOKEN_LBRACKET] = "LBRACKET",
        [TOKEN_RBRACKET] = "RBRACKET",
        [TOKEN_SEMICOLON] = "SEMICOLON",
        [TOKEN_COMMA] = "COMMA",
        [TOKEN_DOT] = "DOT",
        [TOKEN_PLUS] = "PLUS",
        [TOKEN_MINUS] = "MINUS",
        [TOKEN_MULTIPLY] = "MULTIPLY",
        [TOKEN_DIVIDE] = "DIVIDE",
        [TOKEN_MODULO] = "MODULO",
        [TOKEN_AMPERSAND] = "AMPERSAND",
        [TOKEN_PIPE] = "PIPE",
        [TOKEN_CARET] = "CARET",
        [TOKEN_TILDE] = "TILDE",
        [TOKEN_ERROR] = "ERROR"
    };
#endif
