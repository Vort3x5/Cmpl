#include <cmpl.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static inline bool IsAlpha(char c)
	{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

static inline bool IsDigit(char c)
	{ return c >= '0' && c <= '9'; }

static inline bool IsAlNum(char c)
	{ return IsAlpha(c) || IsDigit(c); }

static inline bool IsWhitespace(char c)
	{ return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static inline char LexerPeek(Lexer *lexer)
{
	if (lexer->curr >= lexer->len)
		return '\0';
	return lexer->src[lexer->curr];
}

static inline char LexerPeekNext(Lexer *lexer)
{
	if (lexer->curr + 1 >= lexer->len)
		return '\0';
	return lexer->src[lexer->curr + 1];
}

static inline char LexerNextC(Lexer *lexer)
{
	if (lexer->current >= lexer->length)
		return '\0';

	char c = lexer->source[lexer->current++];
	if (c == '\n')
	{
		++(lexer->line);
		lexer->column = 1;
	}
	else
		++(lexer->column);
	return c;
}

static void LexerSkipWhitespace(Lexer *lexer)
{
	while (true)
	{
		char c = LexerPeek(lexer);

		if (IsWhitespace(c))
			LexerNextC(lexer);
		else if (c == '/' && LexerPeekNext(lexer) == '/')
		{
			while (LexerPeek(lexer) != '\n' && LexerPeek(lexer) != '\0')
				LexerNextC(lexer);
		}
		else if (c == '/' && LexerPeekNext(lexer) == '*')
		{
			LexerNextC(lexer);
			LexerNextC(lexer);
			while (true)
			{
				char current = LexerPeek(lexer);
				if (current == '\0')
					break;

				if (current == '*' && LexerPeekNext(lexer) == '/')
				{
					LexerNextC(lexer);
					LexerNextC(lexer);
					break;
				}
				LexerNextC(lexer);
			}
		}
		else
			break;
	}
}

static Token LexerMakeToken(Lexer *lexer, Token_Type type, const char *lexeme, u32 start_line, u32 start_column)
{
	Token token = {
		.type = type,
		.line = start_line,
		.column = start_column,
	};

	if (lexeme)
	{
		size_t len = strlen(lexeme);
		token.lexeme = arena_alloc(lexer->arena, len + 1);
		memcpy(token.lexeme, lexeme, len + 1);
	}

	return token;
}

static Token LexerScanIds(Lexer *lexer)
{
	u32 start_line = lexer->line;
	u32 start_column = lexer->column;
	size_t start = lexer->current - 1;

	while (IsAlNum(LexerPeek(lexer)))
		LexerNextC(lexer);

	size_t length = lexer->curr - start;
	char *lexeme = arena_alloc(lexer->arena, length + 1);
	memcpy(lexeme, &(lexer->soure[start]), length);
	lexeme[length] = '\0';

	Token_Type type = TOKEN_ID;

	Token token = LexerMakeToken(lexer, type, lexeme, start_line, start_column);
	return token;
}

static Token LexerScanNum(Lexer *lexer)
{
	u32 start_line = lexer->line;
	u32 start_column = lexer->column;
	size_t start = lexer->current - 1;

	// TODO: Handle floating point numbers
	while (IsDigit(LexerPeek(lexer)))
		LexerNextC(lexer);

	size_t length = lexer->current - start;
    char* lexeme = arena_alloc(lexer->arena, length + 1);
    memcpy(lexeme, &(lexer->source[start]), length);
    lexeme[length] = '\0';
    
    Token token = LexerMakeToken(lexer, TOKEN_NUMBER, lexeme, start_line, start_column);
    
    token.value.number = 0;
    for (size_t i = 0; i < length; ++i)
        token.value.number = token.value.number * 10 + (lexeme[i] - '0');
    
    return token;
}
