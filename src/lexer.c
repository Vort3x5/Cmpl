#define LEXER_DEF
#include <cmpl.h>

#include <arena.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
	if (lexer->curr >= lexer->len)
		return '\0';

	char c = lexer->src[lexer->curr++];
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
				char curr = LexerPeek(lexer);
				if (curr == '\0')
					break;

				if (curr == '*' && LexerPeekNext(lexer) == '/')
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
	size_t start = lexer->curr - 1; // First character already consumed

	while (IsAlNum(LexerPeek(lexer)))
		LexerNextC(lexer);

	size_t len = lexer->curr - start;
	char *lexeme = arena_alloc(lexer->arena, len + 1);
	memcpy(lexeme, &(lexer->src[start]), len);
	lexeme[len] = '\0';

	Token_Type type = TOKEN_ID;

	Token token = LexerMakeToken(lexer, type, lexeme, start_line, start_column);
	return token;
}

static Token LexerScanNum(Lexer *lexer)
{
	u32 start_line = lexer->line;
	u32 start_column = lexer->column;
	size_t start = lexer->curr - 1; // First digit already consumed

	// TODO: Handle floating point numbers
	while (IsDigit(LexerPeek(lexer)))
		LexerNextC(lexer);

	size_t len = lexer->curr - start;
    char* lexeme = arena_alloc(lexer->arena, len + 1);
    memcpy(lexeme, &(lexer->src[start]), len);
    lexeme[len] = '\0';
    
    Token token = LexerMakeToken(lexer, TOKEN_NUM, lexeme, start_line, start_column);
    
    token.value.num = 0;
    for (size_t i = 0; i < len; ++i)
        token.value.num = token.value.num * 10 + (lexeme[i] - '0');
    
    return token;
}

static Token LexerScanStr(Lexer* lexer) 
{
	u32 start_line = lexer->line;
	u32 start_column = lexer->column;
	size_t start = lexer->curr; // " character consumed
    
    while (LexerPeek(lexer) != '"' && LexerPeek(lexer) != '\0') 
	{
		// TODO: Multiline strings
        if (LexerPeek(lexer) == '\n')
            return LexerMakeToken(lexer, TOKEN_ERR, "Unterminated str", start_line, start_column);
        
        if (LexerPeek(lexer) == '\\') 
		{
            LexerNextC(lexer);
            if (LexerPeek(lexer) != '\0') 
                LexerNextC(lexer);
        } 
		else 
            LexerNextC(lexer);
    }
    
    if (LexerPeek(lexer) == '\0') 
        return LexerMakeToken(lexer, TOKEN_ERR, "Unterminated str", start_line, start_column);
    
    // Extract str content (without quotes)
    size_t len = lexer->curr - start;
    char* content = arena_alloc(lexer->arena, len + 1);
    memcpy(content, &(lexer->src[start]), len);
    content[len] = '\0';
    
    char* processed = arena_alloc(lexer->arena, len + 1);
    size_t write_pos = 0;
    for (size_t read_pos = 0; read_pos < len; ++read_pos) 
	{
        if (content[read_pos] == '\\' && read_pos + 1 < len) 
		{
            switch (content[read_pos + 1]) 
			{
                case 'n': 
					processed[++write_pos] = '\n';
					++read_pos;
					break;
                case 't': 
					processed[++write_pos] = '\t';
					++read_pos;
					break;
                case 'r': 
					processed[++write_pos] = '\r';
					++read_pos;
					break;
                case '\\':
					processed[++write_pos] = '\\';
					++read_pos;
					break;
                case '"': 
					processed[++write_pos] = '"';
					++read_pos;
					break;
                default: 
					processed[++write_pos] = content[read_pos]; 
					break;
            }
        } 
		else
            processed[++write_pos] = content[read_pos];
    }
    processed[write_pos] = '\0';
    LexerNextC(lexer); // consume closing quote
    
    Token token = LexerMakeToken(lexer, TOKEN_STR, content, start_line, start_column);
    token.value.str = processed;
    return token;
}

Lexer* LexerCreate(const char* src, Arena* arena) 
{
    assert(src != NULL);
    assert(arena != NULL);
    
    Lexer* lexer = arena_alloc(arena, sizeof(Lexer));
	*lexer = (Lexer){
		.src = src,
		.curr = 0,
		.len = strlen(src),
		.line = 1,
		.column = 1,
		.arena = arena,
	};
    return lexer;
}

Token LexerNextToken(Lexer* lexer) 
{
    assert(lexer != NULL);
    LexerSkipWhitespace(lexer);
    int start_line = lexer->line;
    int start_column = lexer->column;
    
    char c = LexerNextC(lexer);
    if (c == '\0') 
        return LexerMakeToken(lexer, TOKEN_EOF, "", start_line, start_column);
    
    if (IsAlpha(c)) 
        return LexerScanIds(lexer);
    
    if (IsDigit(c))
        return LexerScanNum(lexer);
    
    if (c == '"')
        return LexerScanStr(lexer);
    
    switch (c) 
	{
        case ':':
            if (LexerPeek(lexer) == ':') 
			{
                LexerNextC(lexer);
                return LexerMakeToken(lexer, TOKEN_PROC, "::", start_line, start_column);
            } 
			else if (LexerPeek(lexer) == '=') 
			{
                LexerNextC(lexer);
                return LexerMakeToken(lexer, TOKEN_ASSIGN, ":=", start_line, start_column);
            } 
			else
                return LexerMakeToken(lexer, TOKEN_ERR, "Unexpected character", start_line, start_column);
            
        case '=':
            if (LexerPeek(lexer) == '=') 
			{
                LexerNextC(lexer);
                return LexerMakeToken(lexer, TOKEN_EQ, "==", start_line, start_column);
            } 
			else 
                return LexerMakeToken(lexer, TOKEN_ERR, "Single '=' not supported, use ':=' for assignment", start_line, start_column);
            
        case '!':
            if (LexerPeek(lexer) == '=') 
			{
                LexerNextC(lexer);
                return LexerMakeToken(lexer, TOKEN_NOT_EQ, "!=", start_line, start_column);
            } 
			else 
                return LexerMakeToken(lexer, TOKEN_NOT, "!", start_line, start_column);
            
        case '<':
            if (LexerPeek(lexer) == '=') 
			{
                LexerNextC(lexer);
                return LexerMakeToken(lexer, TOKEN_LESS_EQ, "<=", start_line, start_column);
            } 
			else 
                return LexerMakeToken(lexer, TOKEN_LESS, "<", start_line, start_column);
            
        case '>':
            if (LexerPeek(lexer) == '=') 
			{
                LexerNextC(lexer);
                return LexerMakeToken(lexer, TOKEN_GREATER_EQ, ">=", start_line, start_column);
            } 
			else 
                return LexerMakeToken(lexer, TOKEN_GREATER, ">", start_line, start_column);
    }
    
    switch (c) 
	{
        case '(': return LexerMakeToken(lexer, TOKEN_L_PAREN, "(", start_line, start_column);
        case ')': return LexerMakeToken(lexer, TOKEN_R_PAREN, ")", start_line, start_column);
        case '{': return LexerMakeToken(lexer, TOKEN_L_BRACE, "{", start_line, start_column);
        case '}': return LexerMakeToken(lexer, TOKEN_R_BRACE, "}", start_line, start_column);
        case '[': return LexerMakeToken(lexer, TOKEN_L_BRACKET, "[", start_line, start_column);
        case ']': return LexerMakeToken(lexer, TOKEN_R_BRACKET, "]", start_line, start_column);
        case ';': return LexerMakeToken(lexer, TOKEN_SEMICOLON, ";", start_line, start_column);
        case ',': return LexerMakeToken(lexer, TOKEN_COMMA, ",", start_line, start_column);
        case '.': return LexerMakeToken(lexer, TOKEN_DOT, ".", start_line, start_column);
        case '+': return LexerMakeToken(lexer, TOKEN_PLUS, "+", start_line, start_column);
        case '-': return LexerMakeToken(lexer, TOKEN_MINUS, "-", start_line, start_column);
        case '*': return LexerMakeToken(lexer, TOKEN_MUL, "*", start_line, start_column);
        case '/': return LexerMakeToken(lexer, TOKEN_DIV, "/", start_line, start_column);
        case '%': return LexerMakeToken(lexer, TOKEN_MOD, "%", start_line, start_column);
        case '&': return LexerMakeToken(lexer, TOKEN_AMPERSAND, "&", start_line, start_column);
        case '|': return LexerMakeToken(lexer, TOKEN_PIPE, "|", start_line, start_column);
        case '^': return LexerMakeToken(lexer, TOKEN_CARET, "^", start_line, start_column);
        case '~': return LexerMakeToken(lexer, TOKEN_TILDE, "~", start_line, start_column);
    }
    
    char unknown[2] = {c, '\0'};
    return LexerMakeToken(lexer, TOKEN_ERR, unknown, start_line, start_column);
}

Token LexerPeekToken(Lexer* lexer) 
{
    size_t saved_current = lexer->curr;
    int saved_line = lexer->line;
    int saved_column = lexer->column;
    
    Token token = LexerNextToken(lexer);
    
    lexer->curr = saved_current;
    lexer->line = saved_line;
    lexer->column = saved_column;
    
    return token;
}

void LexerPrintToken(Token token) 
{
    printf("Token{type=%s, lexeme=\"%s\", line=%zu, col=%zu", 
           type_names[token.type], 
           token.lexeme ? token.lexeme : "",
           token.line, 
           token.column
		   );
    
    if (token.type == TOKEN_NUM) 
        printf(", value=%lld", token.value.num);
	else if (token.type == TOKEN_STR) 
        printf(", str=\"%s\"", token.value.str ? token.value.str : "");
    printf("}\n");
}

void LexerDumpTokenize(const char* src, Arena* arena) 
{
    printf("=== LEXER DUMP OUTPUT ===\n");
    printf("Source: %s\n", src);
    printf("Tokens:\n");
    
    Lexer* lexer = LexerCreate(src, arena);
    Token token;
    do 
	{
        token = LexerNextToken(lexer);
        printf("  ");
        LexerPrintToken(token);
    } while (token.type != TOKEN_EOF && token.type != TOKEN_ERR);
    
    printf("=== END DUMP OUTPUT ===\n\n");
}
