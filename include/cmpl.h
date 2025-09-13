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
	TOKEN_SEMICOLON,
	TOKEN_COMMA,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_ERR
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
