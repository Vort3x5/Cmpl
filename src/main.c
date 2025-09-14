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
    
    TestParseExpression("main :: () { x := 42; }", &arena);
    arena_reset(&arena);
    
    TestParseExpression("main :: () { result := 10 + 5 * 2; }", &arena);
    arena_reset(&arena);
    
    TestParseExpression("main :: () { test := x == 42; }", &arena);
    arena_reset(&arena);
    
    TestParseExpression("main :: () { x := 42; y := x + 10; }", &arena);
    arena_reset(&arena);
    
    arena_free(&arena);
    return 0;
}
