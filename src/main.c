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

	TestParseExpression(
        "add :: (a: int, b: int) -> int { return a + b; }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "main :: () { result := add(10, 20); }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "calculate :: (x: float, y: float, z: float) -> float { return x * y + z; }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "main :: () { result := multiply(5 + 3, 10 - 2); }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "main :: () { if (x > 10) { return true; } }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "main :: () { if (x == 0) { return 1; } else { return x * 2; } }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "main :: () { result := max(add(5, 3), multiply(2, 4)); }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "factorial :: (n: int) -> int {"
        "  if (n <= 1) { return 1; } "
        "  else { return n * factorial(n - 1); } "
        "}",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "print_number :: (n: int) { printf(n); return; }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "main :: () -> int {"
        "  x := get_input(); "
        "  if (x > 0) { "
        "    result := process(x); "
        "    return result; "
        "  } else { "
        "    return -1; "
        "  } "
        "}",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "add :: (a: int, b: int) -> int { return a + b; } "
        "subtract :: (a: int, b: int) -> int { return a - b; } "
        "main :: () { x := add(10, 5); y := subtract(20, 8); }",
        &arena
    );
    arena_reset(&arena);
    
    TestParseExpression(
        "get_random :: () -> int { return 42; }",
        &arena
    );
    arena_reset(&arena);
    
    arena_free(&arena);
    
    return 0;
}
