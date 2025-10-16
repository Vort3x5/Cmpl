#define NOB_IMPLEMENTATION
#include "include/nob.h"

bool BuildAll() 
{
    nob_mkdir_if_not_exists("out");
    
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-g", "-I", "include", "-Wno-misleading-indentation");
    nob_cc_output(&cmd, "out/cmpl");
    
    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "src/lexer.c");
    nob_cmd_append(&cmd, "src/parser.c");
    nob_cmd_append(&cmd, "src/generator.c");
    nob_cmd_append(&cmd, "src/tac.c");
    
    return nob_cmd_run(&cmd);
}

int main(int argc, char **argv) 
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (argc < 2) 
	{
        nob_log(NOB_INFO, "Usage: %s <command> [args...]", argv[0]);
        return 1;
    }
    
    const char *command = argv[1];
    
	if (strcmp(command, "all") == 0) 
	{
        bool success = true;
        success &= BuildAll();
        return !(success);
    } 
	else 
	{
        nob_log(NOB_ERROR, "Unknown command: %s", command);
        return 1;
    }
}
