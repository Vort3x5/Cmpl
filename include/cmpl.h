#pragma once

#include <arena.h>
#include <stdint.h>
#include <nob.h>

#include <stdbool.h>

#define AST_FLAG_REVERSE 0x1

typedef enum {
	TOKEN_EOF,
	TOKEN_ID,
	TOKEN_NUM,
	TOKEN_STR,
	TOKEN_PROC,
	TOKEN_ASSIGN,
	TOKEN_EQ_ASSIGN,
	TOKEN_L_PAREN,
	TOKEN_R_PAREN,
	TOKEN_L_BRACE,
	TOKEN_R_BRACE,
	TOKEN_L_BRACKET,
	TOKEN_R_BRACKET,
	TOKEN_SEMICOLON,
	TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_ARROW,
    TOKEN_RANGE,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_STRUCT,
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
	uint32_t line, column;
	union {
		int64_t num;
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
	AST_ID,
	AST_INDEX,
    AST_RETURN,
    AST_IF,
    AST_FOR,
    AST_FOR_RANGE,
    AST_WHILE,
    AST_STRUCT,
    AST_FIELD,
	AST_FIELD_ACCESS,
    AST_TYPE,
} AST_Type;

typedef struct AST_Node AST_Node;

typedef struct {
	AST_Node **data;
	size_t size, used;
} AST_Array;

struct AST_Node {
	AST_Type type;
	char *name;

	AST_Node *left, *right, *body;
	AST_Array children;

	int64_t num;
	char *str;

	uint32_t line, column, flags;
};

typedef struct {
	const char *src;
	size_t curr, len;
	uint32_t line, column;
	Arena *arena;
} Lexer;

typedef struct {
    Lexer *lexer;
    Token curr;
    Token prev;
    Arena *arena;
    bool had_err;
    bool panic_mode;
} Parser;

typedef struct {
	const char *name;
	size_t size;
} TypeInfo;

typedef struct {
	Nob_String_Builder sb;
	Arena *arena;
	int local_offset, temp_count;
	struct {
		TypeInfo *items;
		size_t count, capacity;
	} types;
} Generator;

typedef enum {
    TAC_ASSIGN,     
    TAC_BINOP,      
    TAC_COPY,       
    TAC_CALL,       
    TAC_PARAM,      
    TAC_RETURN,     
    TAC_LABEL,      
    TAC_JUMP,       
    TAC_JUMP_IF,    
    TAC_JUMP_IF_NOT,
} TAC_Op;

typedef struct TAC_Inst TAC_Inst;
struct TAC_Inst {
    TAC_Op type;
    char *dest;
    char *src1;
    char *src2;
    char *op;  
    TAC_Inst *next;
};

typedef struct {
    TAC_Inst *head;
    TAC_Inst *tail;
    int temp_count;
    Arena *arena;
} TAC_Builder;

#ifdef LEXER_DEF
    const char* token_names[] = {
        [TOKEN_EOF] = "EOF",
        [TOKEN_ID] = "IDENTIFIER",
        [TOKEN_NUM] = "NUMBER",
        [TOKEN_STR] = "STRING",
        [TOKEN_PROC] = "PROCEDURE",
        [TOKEN_ASSIGN] = "ASSIGN",
        [TOKEN_EQ] = "EQUAL",
        [TOKEN_EQ_ASSIGN] = "EQUAL_ASSIGN",
        [TOKEN_NOT_EQ] = "NOT_EQUAL",
        [TOKEN_NOT] = "NOT",
        [TOKEN_LESS] = "LESS",
        [TOKEN_LESS_EQ] = "LESS_EQUAL",
        [TOKEN_GREATER] = "GREATER",
        [TOKEN_GREATER_EQ] = "GREATER_EQUAL",
        [TOKEN_L_PAREN] = "LPAREN",
        [TOKEN_R_PAREN] = "RPAREN",
        [TOKEN_L_BRACE] = "LBRACE",
        [TOKEN_R_BRACE] = "RBRACE",
        [TOKEN_L_BRACKET] = "LBRACKET",
        [TOKEN_R_BRACKET] = "RBRACKET",
        [TOKEN_SEMICOLON] = "SEMICOLON",
		[TOKEN_COLON] = "COLON",
		[TOKEN_ARROW] = "ARROW",
		[TOKEN_IF] = "IF",
		[TOKEN_ELSE] = "ELSE", 
		[TOKEN_FOR] = "FOR",
		[TOKEN_WHILE] = "WHILE",
		[TOKEN_RETURN] = "RETURN",
		[TOKEN_STRUCT] = "STRUCT",
        [TOKEN_COMMA] = "COMMA",
        [TOKEN_DOT] = "DOT",
        [TOKEN_PLUS] = "PLUS",
        [TOKEN_MINUS] = "MINUS",
        [TOKEN_MUL] = "MULTIPLY",
        [TOKEN_DIV] = "DIVIDE",
        [TOKEN_MOD] = "MODULO",
        [TOKEN_AMPERSAND] = "AMPERSAND",
        [TOKEN_PIPE] = "PIPE",
        [TOKEN_CARET] = "CARET",
        [TOKEN_TILDE] = "TILDE",
        [TOKEN_ERR] = "ERROR"
    };
#endif

#ifdef PARSER_DEF
	const char* ast_names[] = {
		[AST_PROGRAM] = "PROGRAM",
		[AST_PROC] = "PROCEDURE",
		[AST_VAR] = "VARIABLE", 
		[AST_ASSIGNMENT] = "ASSIGNMENT",
		[AST_BIN_OP] = "BINARY_OPERATION",
		[AST_CALL] = "CALL",
		[AST_BLOCK] = "BLOCK",
		[AST_NUM] = "NUMBER",
		[AST_ID] = "IDENTIFIER",
		[AST_INDEX] = "INDEX",
		[AST_RETURN] = "RETURN",
		[AST_IF] = "IF",
		[AST_FOR] = "FOR",
		[AST_FOR_RANGE] = "FOR_RANGE",
		[AST_WHILE] = "WHILE",
		[AST_STRUCT] = "STRUCT",
		[AST_FIELD] = "FIELD",
		[AST_FIELD_ACCESS] = "FIELD_ACCESS",
		[AST_TYPE] = "TYPE",
	};

	static AST_Node *ParseExpression(Parser *parser);
	static AST_Node *ParseStatement(Parser *parser);
	static AST_Node *ParseBlock(Parser *parser);
	static AST_Node *ParseCall(Parser *parser, AST_Node *function);
	static AST_Node *ParseReturn(Parser *parser);
	static AST_Node *ParseIf(Parser *parser);
	static AST_Node *ParseFor(Parser *parser);
	static AST_Node *ParseWhile(Parser *parser);
	static AST_Node *ParseStruct(Parser *parser);
#endif

#ifdef GEN_DEF
    static void GenExpr(Generator *g, AST_Node *node);
    static void GenStmt(Generator *g, AST_Node *node);
#endif

#ifdef TAC_DEF
	static void StmtToTAC(TAC_Builder *tb, AST_Node *node);
#endif

Lexer* LexerCreate(const char* src, Arena* arena);
Token LexerNextToken(Lexer* lexer);
Token LexerPeekToken(Lexer* lexer);
void LexerPrintToken(Token token);
void LexerDumpTokenize(const char* src, Arena* arena);

Parser* ParserCreate(Lexer* lexer, Arena* arena);
AST_Node* ParserParseProgram(Parser* parser);
bool ParserHadError(Parser* parser);
void ASTPrintNode(AST_Node* node, int depth);
void ASTPrintProgram(AST_Node* program);

int TACGetMaxTemp(TAC_Inst *tac);
void TACInit(TAC_Builder *tb, Arena *arena);
char* ExprToTAC(TAC_Builder *tb, AST_Node *node);
TAC_Inst* FuncBodyToTAC(AST_Node *body, Arena *arena);
bool Generate(AST_Node *ast, const char *output_path, Arena *arena);
