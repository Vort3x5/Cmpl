#define ARENA_IMPLEMENTATION
#include <cmpl.h>

int main() 
{
    Arena arena = {0};
    LexerDumpTokenize("main :: () { x := 42; }", &arena);
    arena_free(&arena);
    return 0;
}
