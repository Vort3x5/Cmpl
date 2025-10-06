#define ARENA_IMPLEMENTATION
#include <cmpl.h>

void TestParseExpression(const char* source, Arena* arena) 
{
    printf("=== Testing Expression: %s ===\n", source);
    
    Lexer* lexer = LexerCreate(source, arena);
    Parser* parser = ParserCreate(lexer, arena);
    
    AST_Node* ast = ParserParseProgram(parser);
    
    if (ParserHadError(parser)) 
        printf("Parser encountered errors!\n");
	else if (ast) 
        ASTPrintProgram(ast);
    
    printf("\n");
}

int main() 
{
	Arena arena = {0};

	// Test 6: Simple struct
	TestParseExpression(
		"Vector2 :: struct { x: float; y: float; }",
		&arena
	);

	// Test 7: Struct with multiple field types
	TestParseExpression(
		"Player :: struct { name: string; health: int; alive: bool; }",
		&arena
	);

	// Test 8: Nested struct
	TestParseExpression(
		"Entity :: struct { position: Vector2; velocity: Vector2; }",
		&arena
	);

	// Test 9: Struct with procedure
	TestParseExpression(
		"Vector2 :: struct { x: float; y: float; } "
		"add :: (a: Vector2, b: Vector2) -> Vector2 { return a; }",
		&arena
	);

	// Test 10: For loop in procedure with struct
	TestParseExpression(
		"update :: () { for 0..100 { entities[it].x = it; } }",
		&arena
	);

	arena_reset(&arena);
	arena_free(&arena);
    return 0;
}
