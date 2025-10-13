#include <stdio.h>
#define PARSER_DEF
#include <cmpl.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static AST_Node *ASTNodeCreate(Parser *parser, AST_Type type) 
{
    AST_Node *node = arena_alloc(parser->arena, sizeof(AST_Node));
    memset(node, 0, sizeof(AST_Node));
    node->type = type;
    node->line = parser->prev.line;
    node->column = parser->prev.column;
    return node;
}

static void ASTArrayInit(AST_Array *array) 
{
	*array = (AST_Array){
		.data = NULL,
		.size = 0,
		.used = 0,
	};
}

static void ASTArrayPush(AST_Array *array, AST_Node *node, Arena *arena) 
{
    if (array->used >= array->size) 
	{
        size_t new_size = array->size == 0 ? 8 : array->size * 2;
        AST_Node **new_data = arena_alloc(arena, new_size * sizeof(AST_Node*));
        
        if (array->data) 
            memcpy(new_data, array->data, array->used * sizeof(AST_Node*));
        
        array->data = new_data;
        array->size = new_size;
    }
    
    array->data[array->used++] = node;
}

static void ParserError(Parser *parser, const char *message) 
{
    if (parser->panic_mode) 
		return;
    
    parser->panic_mode = true;
    parser->had_err = true;
    
    printf("[Line %zu, Col %zu] Parser Error", parser->prev.line, parser->prev.column);
    if (parser->prev.type == TOKEN_EOF) 
        printf(" at end");
    else if (parser->prev.type == TOKEN_ERR);
        // Nothing
    else
        printf(" at '%s'", parser->prev.lexeme);
    
    printf(": %s\n", message);
}

static void ParserAdvance(Parser *parser) 
{
    parser->prev = parser->curr;
    
	while (true)
	{
        parser->curr = LexerNextToken(parser->lexer);
        if (parser->curr.type != TOKEN_ERR) 
			break;
        
        ParserError(parser, parser->curr.lexeme);
    }
}

static bool ParserCheck(Parser *parser, Token_Type type) 
	{ return parser->curr.type == type; }

static bool ParserMatch(Parser *parser, Token_Type type) 
{
    if (!ParserCheck(parser, type)) 
		return false;
    ParserAdvance(parser);
    return true;
}

static void ParserConsume(Parser *parser, Token_Type type, const char *message) 
{
    if (parser->curr.type == type) 
	{
        ParserAdvance(parser);
        return;
    }
    
    ParserError(parser, message);
}

static void ParserSynchronize(Parser *parser) 
{
    parser->panic_mode = false;
    
    while (parser->curr.type != TOKEN_EOF) 
	{
        if (parser->prev.type == TOKEN_SEMICOLON) 
			return;
        
        switch (parser->curr.type) 
		{
            case TOKEN_ID:
                // Look for procedure declarations or other top-level constructs
                return;
            default:
                break;
        }
        
        ParserAdvance(parser);
    }
}

static AST_Node *ParseNumber(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_NUM);
    node->num = parser->prev.value.num;
    return node;
}

static AST_Node *ParseIdentifier(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_ID);
    node->name = arena_strdup(parser->arena, parser->prev.lexeme);
    return node;
}

static AST_Node *ParsePrimary(Parser *parser) 
{
    if (ParserMatch(parser, TOKEN_NUM)) 
        return ParseNumber(parser);
    
	if (ParserMatch(parser, TOKEN_ID))
	{
		AST_Node *expr = ParseIdentifier(parser);
		
		// Handle postfix operations: array[index], struct.field, func()
		while (true) 
		{
			if (ParserCheck(parser, TOKEN_L_BRACKET)) 
			{
				ParserAdvance(parser);
				AST_Node *index_node = ASTNodeCreate(parser, AST_INDEX);
				index_node->left = expr;
				index_node->right = ParseExpression(parser);
				ParserConsume(parser, TOKEN_R_BRACKET, "Expected ']' after array index");
				expr = index_node;
			} 
			else if (ParserMatch(parser, TOKEN_DOT)) 
			{
				if (!ParserMatch(parser, TOKEN_ID)) 
				{
					ParserError(parser, "Expected field name after '.'");
					break;
				}
				AST_Node *member_node = ASTNodeCreate(parser, AST_FIELD_ACCESS);
				member_node->left = expr;
				member_node->name = arena_strdup(parser->arena, parser->prev.lexeme);
				expr = member_node;
			} 
			else if (ParserCheck(parser, TOKEN_L_PAREN)) 
				expr = ParseCall(parser, expr);
			else 
				break;
		}
		
		return expr;
	}
    
    if (ParserMatch(parser, TOKEN_L_PAREN)) 
	{
        AST_Node *expr = ParseExpression(parser);
        ParserConsume(parser, TOKEN_R_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    ParserError(parser, "Expected expression");
    return NULL;
}

static AST_Node *ParseUnary(Parser *parser) 
{
    if (ParserMatch(parser, TOKEN_NOT) || ParserMatch(parser, TOKEN_MINUS)) 
	{
        Token operator = parser->prev;
        AST_Node *right = ParseUnary(parser);
        
        AST_Node *node = ASTNodeCreate(parser, AST_BIN_OP);
        node->name = arena_strdup(parser->arena, operator.lexeme);
        node->left = NULL;
        node->right = right;
        return node;
    }
    
    return ParsePrimary(parser);
}

static AST_Node *ParseFactor(Parser *parser) 
{
    AST_Node *expr = ParseUnary(parser);
    
    while (ParserMatch(parser, TOKEN_DIV) || ParserMatch(parser, TOKEN_MUL) || ParserMatch(parser, TOKEN_MOD)) 
	{
        Token operator = parser->prev;
        AST_Node *right = ParseUnary(parser);
        
        AST_Node *node = ASTNodeCreate(parser, AST_BIN_OP);
        node->name = arena_strdup(parser->arena, operator.lexeme);
        node->left = expr;
        node->right = right;
        expr = node;
    }
    
    return expr;
}

static AST_Node *ParseTerm(Parser *parser) 
{
    AST_Node *expr = ParseFactor(parser);
    
    while (ParserMatch(parser, TOKEN_MINUS) || ParserMatch(parser, TOKEN_PLUS)) 
	{
        Token operator = parser->prev;
        AST_Node *right = ParseFactor(parser);
        
        AST_Node *node = ASTNodeCreate(parser, AST_BIN_OP);
        node->name = arena_strdup(parser->arena, operator.lexeme);
        node->left = expr;
        node->right = right;
        expr = node;
    }
    
    return expr;
}

static AST_Node *ParseComparison(Parser *parser) 
{
    AST_Node *expr = ParseTerm(parser);
    
    while (ParserMatch(parser, TOKEN_GREATER) || ParserMatch(parser, TOKEN_GREATER_EQ) ||
           ParserMatch(parser, TOKEN_LESS) || ParserMatch(parser, TOKEN_LESS_EQ)) 
	{
        Token operator = parser->prev;
        AST_Node *right = ParseTerm(parser);
        
        AST_Node *node = ASTNodeCreate(parser, AST_BIN_OP);
        node->name = arena_strdup(parser->arena, operator.lexeme);
        node->left = expr;
        node->right = right;
        expr = node;
    }
    
    return expr;
}

static AST_Node *ParseEquality(Parser *parser) 
{
    AST_Node *expr = ParseComparison(parser);
    
    while (ParserMatch(parser, TOKEN_NOT_EQ) || ParserMatch(parser, TOKEN_EQ)) 
	{
        Token operator = parser->prev;
        AST_Node *right = ParseComparison(parser);
        
        AST_Node *node = ASTNodeCreate(parser, AST_BIN_OP);
        node->name = arena_strdup(parser->arena, operator.lexeme);
        node->left = expr;
        node->right = right;
        expr = node;
    }
    
    return expr;
}

static AST_Node *ParseExpression(Parser *parser) 
	{ return ParseEquality(parser); }

static AST_Node *ParseVariableAssignment(Parser *parser) 
{
    Token name = parser->prev;
    
    ParserConsume(parser, TOKEN_ASSIGN, "Expected ':=' in variable assignment");
    AST_Node *value = ParseExpression(parser);
    ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after assignment");
    
    AST_Node *node = ASTNodeCreate(parser, AST_ASSIGNMENT);
    node->name = arena_strdup(parser->arena, name.lexeme);
    node->right = value;
    
    return node;
}

static AST_Node *ParseExpressionStatement(Parser *parser) 
{
    AST_Node *expr = ParseExpression(parser);

	if (expr && expr->type == AST_CALL) 
	{
        ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after function call");
        return expr;
    }

    ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after expression");
    return expr;
}

static AST_Node *ParseStatement(Parser *parser) 
{
	if (ParserMatch(parser, TOKEN_RETURN))
        return ParseReturn(parser);

    if (ParserMatch(parser, TOKEN_IF))
        return ParseIf(parser);

    if (ParserMatch(parser, TOKEN_FOR))
        return ParseFor(parser);
        
    if (ParserMatch(parser, TOKEN_WHILE))
        return ParseWhile(parser);

	if (ParserCheck(parser, TOKEN_ID)) 
	{
		size_t saved_curr = parser->lexer->curr;
		uint32_t saved_line = parser->lexer->line;
		uint32_t saved_column = parser->lexer->column;
		Token saved_curr_token = parser->curr;
		Token saved_prev_token = parser->prev;
		
		ParserAdvance(parser);

		if (ParserCheck(parser, TOKEN_COLON)) 
		{
            Token var_name = parser->prev;
            ParserAdvance(parser); // consume ':'
            
            if (ParserMatch(parser, TOKEN_ID)) 
			{
                Token type_name = parser->prev;
                ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after type declaration");
                
                AST_Node *node = ASTNodeCreate(parser, AST_ASSIGNMENT);
                node->name = arena_strdup(parser->arena, var_name.lexeme);
                
                AST_Node *type_node = ASTNodeCreate(parser, AST_TYPE);
                type_node->name = arena_strdup(parser->arena, type_name.lexeme);
                node->right = type_node;
                
                return node;
            }
        }
		
		if (ParserCheck(parser, TOKEN_ASSIGN)) 
			return ParseVariableAssignment(parser);
		
		if (ParserCheck(parser, TOKEN_EQ_ASSIGN)) 
		{
			ParserAdvance(parser);
			AST_Node *rhs = ParseExpression(parser);
			ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after assignment");
			
			AST_Node *node = ASTNodeCreate(parser, AST_ASSIGNMENT);
			node->name = arena_strdup(parser->arena, saved_curr_token.lexeme);
			node->right = rhs;
			return node;
		}
		
		parser->lexer->curr = saved_curr;
		parser->lexer->line = saved_line;
		parser->lexer->column = saved_column;
		parser->curr = saved_curr_token;
		parser->prev = saved_prev_token;
		
		AST_Node *lhs = ParseExpression(parser);
		
		if (lhs && lhs->type == AST_CALL) 
		{
			ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after function call");
			return lhs;
		}
		
		ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after expression");
		return lhs;
	}
    
    if (ParserCheck(parser, TOKEN_L_BRACE)) 
        return ParseBlock(parser);

    return ParseExpressionStatement(parser);
}

static AST_Node *ParseBlock(Parser *parser) 
{
    AST_Node *block = ASTNodeCreate(parser, AST_BLOCK);
    ASTArrayInit(&block->children);
    
    ParserConsume(parser, TOKEN_L_BRACE, "Expected '{'");
    
    while (!ParserCheck(parser, TOKEN_R_BRACE) && !ParserCheck(parser, TOKEN_EOF)) 
	{
        AST_Node *stmt = ParseStatement(parser);
        if (stmt) 
            ASTArrayPush(&block->children, stmt, parser->arena);
        
        if (parser->panic_mode) ParserSynchronize(parser);
    }
    
    ParserConsume(parser, TOKEN_R_BRACE, "Expected '}'");
    return block;
}

static AST_Node *ParseCall(Parser *parser, AST_Node *function)
{
    AST_Node *call = ASTNodeCreate(parser, AST_CALL);
    call->left = function;
    ASTArrayInit(&call->children);
    
    ParserConsume(parser, TOKEN_L_PAREN, "Expected '(' after function name");
    
    if (!ParserCheck(parser, TOKEN_R_PAREN)) 
	{
        do 
		{
            AST_Node *arg = ParseExpression(parser);
            ASTArrayPush(&call->children, arg, parser->arena);
        } while (ParserMatch(parser, TOKEN_COMMA));
    }
    
    ParserConsume(parser, TOKEN_R_PAREN, "Expected ')' after arguments");
    return call;
}

static AST_Node *ParseReturn(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_RETURN);
    
    if (!ParserCheck(parser, TOKEN_SEMICOLON)) 
        node->right = ParseExpression(parser);
    
    ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after return statement");
    return node;
}

static AST_Node *ParseIf(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_IF);
    
    // Parentheses are optional in Jai
    bool has_parens = ParserMatch(parser, TOKEN_L_PAREN);
    node->left = ParseExpression(parser); // condition
    if (has_parens) 
        ParserConsume(parser, TOKEN_R_PAREN, "Expected ')' after if condition");
    
    node->body = ParseStatement(parser); // then branch
    
    if (ParserMatch(parser, TOKEN_ELSE)) 
        node->right = ParseStatement(parser); // else branch
    
    return node;
}

static AST_Node *ParseProcedure(Parser *parser) 
{
    Token name = parser->prev;
    
    ParserConsume(parser, TOKEN_PROC, "Expected '::'");
    ParserConsume(parser, TOKEN_L_PAREN, "Expected '(' after '::'");
    
    AST_Node *proc = ASTNodeCreate(parser, AST_PROC);
    proc->name = arena_strdup(parser->arena, name.lexeme);
    ASTArrayInit(&proc->children);
    
    if (!ParserCheck(parser, TOKEN_R_PAREN)) 
	{
        do 
		{
            if (ParserMatch(parser, TOKEN_ID)) 
			{
                Token param_name = parser->prev;
                
                AST_Node *param = ASTNodeCreate(parser, AST_VAR);
                param->name = arena_strdup(parser->arena, param_name.lexeme);
                
                if (ParserMatch(parser, TOKEN_COLON)) 
				{
                    if (ParserMatch(parser, TOKEN_ID)) 
					{
                        AST_Node *type_node = ASTNodeCreate(parser, AST_TYPE);
                        type_node->name = arena_strdup(parser->arena, parser->prev.lexeme);
                        param->right = type_node;
                    }
                }
                
                ASTArrayPush(&proc->children, param, parser->arena);
            }
        } while (ParserMatch(parser, TOKEN_COMMA));
    }
    
    ParserConsume(parser, TOKEN_R_PAREN, "Expected ')' after parameters");
    
    if (ParserMatch(parser, TOKEN_ARROW)) 
	{
        if (ParserMatch(parser, TOKEN_ID)) 
		{
            AST_Node *return_type = ASTNodeCreate(parser, AST_TYPE);
            return_type->name = arena_strdup(parser->arena, parser->prev.lexeme);
            proc->left = return_type;
        }
    }
    
    proc->body = ParseBlock(parser);
    return proc;
}

static AST_Node *ParseDeclaration(Parser *parser) 
{
    if (ParserMatch(parser, TOKEN_ID)) 
    {
        Token name = parser->prev;
        
        if (ParserCheck(parser, TOKEN_PROC)) 
		{
            // Look ahead to see if it's a struct definition
            size_t saved_curr = parser->lexer->curr;
            uint32_t saved_line = parser->lexer->line;
            uint32_t saved_column = parser->lexer->column;
            Token saved_curr_token = parser->curr;
            Token saved_prev_token = parser->prev;
            
            ParserAdvance(parser); // consume ::
            
            if (ParserMatch(parser, TOKEN_STRUCT)) 
			{
                // It's a struct: Name :: struct { }
                AST_Node *struct_def = ParseStruct(parser);
                struct_def->name = arena_strdup(parser->arena, name.lexeme);
                return struct_def;
            }
            
            // Not a struct, restore state and parse as procedure
            parser->lexer->curr = saved_curr;
            parser->lexer->line = saved_line;
            parser->lexer->column = saved_column;
            parser->curr = saved_curr_token;
            parser->prev = saved_prev_token;
            
            // Now parse as normal procedure
            return ParseProcedure(parser);
        } 
		else 
		{
            // Put identifier back and parse as statement
            parser->curr = parser->prev;
            parser->prev = (Token){0};
            return ParseStatement(parser);
        }
    }
    
    return ParseStatement(parser);
}

static AST_Node *ParseFor(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_FOR_RANGE);
    
    bool reverse = ParserMatch(parser, TOKEN_LESS);
    
    Token iterator_name = {0};
    bool has_iterator_name = false;
    
    if (ParserCheck(parser, TOKEN_ID)) 
	{
        Token peek_ahead = parser->curr;
        ParserAdvance(parser);
        
        if (ParserCheck(parser, TOKEN_COLON)) 
		{
            // We have: identifier :
            iterator_name = peek_ahead;
            has_iterator_name = true;
            ParserAdvance(parser); // consume :
        } 
		else {
            // Put the identifier back, it's the start of range expression
            parser->curr = parser->prev;
            parser->prev = peek_ahead;
        }
    }
    
    // Parse range: start..end
    AST_Node *start = ParseExpression(parser);
    
    if (!ParserMatch(parser, TOKEN_RANGE)) 
	{
        ParserError(parser, "Expected '..' in for loop range");
        return node;
    }
    
    AST_Node *end = ParseExpression(parser);
    
    // Store range information in node
    node->left = start;     // range start
    node->right = end;      // range end
    
    if (has_iterator_name) 
        node->name = arena_strdup(parser->arena, iterator_name.lexeme);
	else 
        node->name = arena_strdup(parser->arena, "it"); // default iterator name
    
    // Store reverse flag (you'll need to add a flag field to AST_Node or encode it)
    // For now, we can store it in a new field or ignore it
    
    // Parse body
    node->body = ParseStatement(parser);

	if (reverse)
		node->flags |= AST_FLAG_REVERSE;
    
    return node;
}

static AST_Node *ParseWhile(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_WHILE);
    
	bool has_parens = ParserMatch(parser, TOKEN_L_PAREN);
    node->left = ParseExpression(parser); // condition
	if (has_parens)
		ParserConsume(parser, TOKEN_R_PAREN, 
				"Expected ')' if ')' was provided at the beginning of while condition");
    
    node->body = ParseStatement(parser);
    
    return node;
}

static AST_Node *ParseStruct(Parser *parser) 
{
    AST_Node *node = ASTNodeCreate(parser, AST_STRUCT);
    ASTArrayInit(&node->children); // For fields
    
    ParserConsume(parser, TOKEN_L_BRACE, "Expected '{' after 'struct'");
    
    // Parse fields: name: type;
    while (!ParserCheck(parser, TOKEN_R_BRACE) && !ParserCheck(parser, TOKEN_EOF)) 
    {
        if (ParserMatch(parser, TOKEN_ID)) 
        {
            Token field_name = parser->prev;
            AST_Node *field = ASTNodeCreate(parser, AST_FIELD);
            field->name = arena_strdup(parser->arena, field_name.lexeme);
            
            if (ParserMatch(parser, TOKEN_COLON) && ParserMatch(parser, TOKEN_ID)) 
			{
				AST_Node *type_node = ASTNodeCreate(parser, AST_TYPE);
				type_node->name = arena_strdup(parser->arena, parser->prev.lexeme);
				field->right = type_node;
			}
            
            ParserConsume(parser, TOKEN_SEMICOLON, "Expected ';' after field");
            ASTArrayPush(&node->children, field, parser->arena);
        }
    }
    
    ParserConsume(parser, TOKEN_R_BRACE, "Expected '}' after struct fields");
    return node;
}

Parser *ParserCreate(Lexer *lexer, Arena *arena) 
{
    assert(lexer != NULL);
    assert(arena != NULL);
    
    Parser *parser = arena_alloc(arena, sizeof(Parser));
    parser->lexer = lexer;
    parser->arena = arena;
    parser->had_err = false;
    parser->panic_mode = false;
    
    // Prime the parser with first two tokens
    ParserAdvance(parser);
    
    return parser;
}

AST_Node *ParserParseProgram(Parser *parser) 
{
    AST_Node *program = ASTNodeCreate(parser, AST_PROGRAM);
    ASTArrayInit(&program->children);
    
    while (!ParserCheck(parser, TOKEN_EOF)) 
	{
        AST_Node *decl = ParseDeclaration(parser);
        if (decl) 
            ASTArrayPush(&program->children, decl, parser->arena);
        
        if (parser->panic_mode) 
			ParserSynchronize(parser);
    }
    
    return parser->had_err ? NULL : program;
}

bool ParserHadError(Parser *parser) 
	{ return parser->had_err; }

void ASTPrintNode(AST_Node* node, int depth) 
{
    if (!node) 
	{
        printf("%*sNULL\n", depth * 2, "");
        return;
    }
    
    printf("%*s%s", depth * 2, "", ast_names[node->type]);
    
    if (node->name) 
        printf(" '%s'", node->name);
    
    if (node->type == AST_NUM) 
        printf(" (%lld)", node->num);

	if (node->type == AST_BIN_OP && !node->left)
        printf(" (UNARY)");
    
    printf("\n");
    
    if (node->children.used > 0) 
	{
        for (size_t i = 0; i < node->children.used; ++i) 
            ASTPrintNode(node->children.data[i], depth + 1);
    }

    if (node->type == AST_IF) 
	{
		if (node->left) 
		{
			printf("%*scondition:\n", (depth + 1) * 2, "");
			ASTPrintNode(node->left, depth + 2);
		}
		
		if (node->body) 
		{
			printf("%*sthen:\n", (depth + 1) * 2, "");
			ASTPrintNode(node->body, depth + 2);
		}
		
		if (node->right) 
		{
			printf("%*selse:\n", (depth + 1) * 2, "");
			ASTPrintNode(node->right, depth + 2);
		}
		
		return;
	}

	if (node->type == AST_FOR_RANGE)
	{
		if (node->name)
			printf(" iterator='%s'", node->name);

		if (node->flags & AST_FLAG_REVERSE)
			printf(" (REVERSE)");
		printf("\n");
		
		if (node->left) 
		{
			printf("%*sstart:\n", (depth + 1) * 2, "");
			ASTPrintNode(node->left, depth + 2);
		}
		
		if (node->right) 
		{
			printf("%*send:\n", (depth + 1) * 2, "");
			ASTPrintNode(node->right, depth + 2);
		}
		
		if (node->body) 
		{
			printf("%*sbody:\n", (depth + 1) * 2, "");
			ASTPrintNode(node->body, depth + 2);
		}
		
		return;
	}

    if (node->left) 
	{
        printf("%*sleft:\n", (depth + 1) * 2, "");
        ASTPrintNode(node->left, depth + 2);
    }
    
    
	if (node->right) 
	{
        printf("%*sright:\n", (depth + 1) * 2, "");
        ASTPrintNode(node->right, depth + 2);
    }
    
    if (node->body) 
	{
        printf("%*sbody:\n", (depth + 1) * 2, "");
        ASTPrintNode(node->body, depth + 2);
    }
}

void ASTPrintProgram(AST_Node* program) 
{
    printf("=== AST DUMP ===\n");
    ASTPrintNode(program, 0);
    printf("=== END AST DUMP ===\n\n");
}
