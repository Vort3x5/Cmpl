#define ARENA_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#include <cmpl.h>

void CompileJaiFile(const char *src, const char *out, Arena *arena) 
{
    printf("=== Compiling %s ===\n", src);
    
    FILE *f = fopen(src, "r");
    if (!f) 
	{
        fprintf(stderr, "Error: Could not open file %s\n", src);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *source = (char*)arena_alloc(arena, size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    Lexer *lexer = LexerCreate(source, arena);
    Parser *parser = ParserCreate(lexer, arena);
    AST_Node *ast = ParserParseProgram(parser);
    
    if (ParserHadError(parser)) 
	{
        printf("Parser encountered errors!\n");
        return;
    }
    
    if (!ast) 
	{
        printf("Failed to parse program!\n");
        return;
    }
    
    // Print AST for debugging
    printf("\n=== AST ===\n");
    ASTPrintProgram(ast);
    
    // Generate assembly
    printf("\n=== Code Generation ===\n");
    char asm_file[4096];
    snprintf(asm_file, sizeof(asm_file), "%s.asm", out);
    
    if (!Generate(ast, asm_file, arena)) 
	{
        fprintf(stderr, "Code generation failed!\n");
        return;
    }
    
    printf("\n=== Success! ===\n");
    printf("Generated: %s\n", asm_file);
    printf("Executable: %s\n", out);
}

int main(int argc, char **argv) 
{
    Arena arena = {0};
    
    if (argc < 2) 
	{
        printf("=== Running Built-in Tests ===\n\n");
        
        const char *test1 = "main :: () { x := 42; }";
        Lexer *lexer = LexerCreate(test1, &arena);
        Parser *parser = ParserCreate(lexer, &arena);
        AST_Node *ast = ParserParseProgram(parser);
        if (ast) 
			ASTPrintProgram(ast);
        arena_reset(&arena);
        
        printf("\n");
    } 
	else 
	{
        const char *input_file = argv[1];
        const char *out = argc > 2 ? argv[2] : "out/out";
        
        CompileJaiFile(input_file, out, &arena);
    }
    
    arena_free(&arena);
    return 0;
}
