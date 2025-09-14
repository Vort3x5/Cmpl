#define NOB_IMPLEMENTATION
#include "include/nob.h"

bool BuildComponent(const char *name, const char *src) 
{
    nob_mkdir_if_not_exists("out");
    
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-I", "include");
    nob_cc_output(&cmd, nob_temp_sprintf("out/%s", name));
	nob_cmd_append(&cmd, "src/main.c");
	nob_cmd_append(&cmd, src);
    return nob_cmd_run(&cmd);
}

bool RunComponent(const char *name) 
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, nob_temp_sprintf("./out/%s", name));
    return nob_cmd_run(&cmd);
}

bool BuildAll() 
{
    nob_mkdir_if_not_exists("out");
    
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-I", "include");
    nob_cc_output(&cmd, "out/cmpl");
    
    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "src/lexer.c");
    // nob_cmd_append(&cmd, "src/parser.c");   // Uncomment when ready
    // nob_cmd_append(&cmd, "src/ast.c");      // Uncomment when ready
    
    return nob_cmd_run(&cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (argc < 2) 
	{
        nob_log(NOB_INFO, "Usage: %s <command> [args...]", argv[0]);
        return 1;
    }
    
    const char *command = argv[1];
    
    if (strcmp(command, "lexer") == 0) {
        if (!BuildComponent("lexer", "src/lexer.c")) 
			return 1;
        return RunComponent("lexer") ? 0 : 1;
        
    } 
	else if (strcmp(command, "parser") == 0) 
	{
        // Combine lexer + parser src when you add parser.c
        const char *src[] = {"src/lexer.c", "src/parser.c"};
        if (!BuildComponent("parser", "src/parser.c")
			|| !BuildComponent("lexer", "src/lexer.c")) 
			return 1;
        return RunComponent("parser") ? 0 : 1;
    } 
	else if (strcmp(command, "ast") == 0) 
	{
        if (!BuildComponent("ast", "src/ast.c")) 
			return 1;
        return RunComponent("ast") ? 0 : 1;
        
    } 
	else if (strcmp(command, "all") == 0) 
	{
        bool success = true;
        success &= BuildComponent("lexer", "src/lexer.c");
        success &= BuildAll();
        return success ? 0 : 1;
        
    } 
	else 
	{
        nob_log(NOB_ERROR, "Unknown command: %s", command);
        return 1;
    }
}
