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
    return 0;
}
