#define GEN_DEF

#include <nob.h>
#include <cmpl.h>

static void GenInit(Generator *g, Arena *a)
{
	*g = (Generator){
		.sb = (Nob_String_Builder){0},
		.arena = a,
		.local_offset = 0,
		.temp_count = 0,
	};
}

static void GenEmit(Generator *g, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	nob_sb_appendf(&g->sb, fmt, args);
	va_end(args);
}

static void GenBinOp(Generator *g, AST_Node *node) 
{
    const char *op = node->name;
    
    if (strcmp(op, "+") == 0) 
	{
        GenEmit(g, "<_Add ");
        GenExpr(g, node->left);
        GenEmit(g, ", ");
        GenExpr(g, node->right);
        GenEmit(g, ">");
    } 
	else if (strcmp(op, "-") == 0) 
	{
        GenEmit(g, "<_Sub ");
        GenExpr(g, node->left);
        GenEmit(g, ", ");
        GenExpr(g, node->right);
        GenEmit(g, ">");
    } 
	else if (strcmp(op, "*") == 0) 
	{
        GenEmit(g, "<_Mul ");
        GenExpr(g, node->left);
        GenEmit(g, ", ");
        GenExpr(g, node->right);
        GenEmit(g, ">");
    } 
	else if (strcmp(op, "==") == 0) 
	{
        GenEmit(g, "<_Equal ");
        GenExpr(g, node->left);
        GenEmit(g, ", ");
        GenExpr(g, node->right);
        GenEmit(g, ">");
    } 
	else if (strcmp(op, "<") == 0) 
	{
        GenEmit(g, "<_Less ");
        GenExpr(g, node->left);
        GenEmit(g, ", ");
        GenExpr(g, node->right);
        GenEmit(g, ">");
    } 
	else if (strcmp(op, ">") == 0) 
	{
        GenEmit(g, "<_Greater ");
        GenExpr(g, node->left);
        GenEmit(g, ", ");
        GenExpr(g, node->right);
        GenEmit(g, ">");
    }
}

static void GenExpr(Generator *g, AST_Node *node) 
{
    if (!node) 
		return;
    
    switch (node->type) 
	{
        case AST_NUM:
            GenEmit(g, "_Num %lld", node->num);
            break;
            
        case AST_ID:
            GenEmit(g, "<_Var %s>", node->name);
            break;
            
        case AST_BIN_OP:
            GenBinOp(g, node);
            break;
            
        default:
            nob_log(NOB_ERROR, "Unsupported expression type: %d", node->type);
            break;
    }
}

static void GenAssign(Generator *g, AST_Node *node) 
{
    GenEmit(g, "    _Assign %s, ", node->name);
    GenExpr(g, node->right);
    GenEmit(g, "\n");
}

static void GenRet(Generator *g, AST_Node *node) 
{
    GenEmit(g, "    _Return ");
    if (node->right) 
        GenExpr(g, node->right);
	else 
        GenEmit(g, "_Num 0");
    GenEmit(g, "\n");
}

static void GenIf(Generator *g, AST_Node *node) 
{
    GenEmit(g, "    _BeginIf ");
    GenExpr(g, node->left);
    GenEmit(g, "\n");
    
    if (node->body) 
        GenStmt(g, node->body);
    
    if (node->right) 
	{
        GenEmit(g, "    _Else\n");
        GenStmt(g, node->right);
    }
    
    GenEmit(g, "    _EndIf\n");
}

static void GenWhile(Generator *g, AST_Node *node) 
{
    GenEmit(g, "    _BeginWhile ");
    GenExpr(g, node->left);
    GenEmit(g, "\n");
    
    if (node->body) 
        GenStmt(g, node->body);
    
    GenEmit(g, "    _EndWhile\n");
}

static void GenBlock(Generator *g, AST_Node *node) 
{
    for (size_t i = 0; i < node->children.used; i++) 
        GenStmt(g, node->children.data[i]);
}

static void GenStmt(Generator *g, AST_Node *node) 
{
    if (!node) 
		return;
    
    switch (node->type) 
	{
        case AST_ASSIGNMENT:
            GenAssign(g, node);
            break;
            
        case AST_RETURN:
            GenRet(g, node);
            break;
            
        case AST_IF:
            GenIf(g, node);
            break;
            
        case AST_WHILE:
            GenWhile(g, node);
            break;
            
        case AST_BLOCK:
            GenBlock(g, node);
            break;
            
        default:
            nob_log(NOB_WARNING, "Unsupported statement type: %d", node->type);
            break;
    }
}

static void GenProc(Generator *g, AST_Node *node) 
{
    const char *func_name = node->name ? node->name : "anonymous";
    
    int local_count = 0;
    if (node->body && node->body->type == AST_BLOCK) 
	{
        for (size_t i = 0; i < node->body->children.used; ++i) 
		{
            AST_Node *stmt = node->body->children.data[i];
            if (stmt->type == AST_ASSIGNMENT) 
                ++local_count;
        }
    }
    
    int locals_size = local_count * 8;
    
    GenEmit(g, "_FuncBeginWithLocals %s, %d\n", func_name, locals_size);
    
    int offset = 0;
    if (node->body && node->body->type == AST_BLOCK) 
	{
        for (size_t i = 0; i < node->body->children.used; i++) 
		{
            AST_Node *stmt = node->body->children.data[i];
            if (stmt->type == AST_ASSIGNMENT) 
			{
                offset += 8;
                GenEmit(g, "    _DeclareVar %s, 8\n", stmt->name);
            }
        }
    }
    
    GenEmit(g, "\n");
    
    if (node->body) 
        GenStmt(g, node->body);
    
    GenEmit(g, "_FuncEnd\n\n");
}

static void GenProgram(Generator *g, AST_Node *node) 
{
    // Include runtime
    GenEmit(g, "; Generated by Jai compiler\n");
    GenEmit(g, "; asmsyntax=fasm\n");
    GenEmit(g, "include 'runtime/core.asm'\n\n");
    
    // Generate all procedures
    if (node->children.used > 0) 
	{
        for (size_t i = 0; i < node->children.used; i++) 
		{
            AST_Node *decl = node->children.data[i];
            if (decl->type == AST_PROC) 
                GenProc(g, decl);
        }
    }
}

bool Generate(AST_Node *ast, const char *output_path, Arena *arena)
{

    Generator g;
    GenInit(&g, arena);
    
    nob_log(NOB_INFO, "Generating assembly code...");
    GenProgram(&g, ast);
    
    nob_sb_append_null(&g.sb);
    
    nob_log(NOB_INFO, "Writing assembly to %s", output_path);
    if (!nob_write_entire_file(output_path, g.sb.items, g.sb.count - 1)) 
	{
        nob_log(NOB_ERROR, "Failed to write assembly file");
        return false;
    }
    
    nob_log(NOB_INFO, "Compiling with FASM...");
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "fasm", output_path);
    
    if (!nob_cmd_run(&cmd)) 
	{
        nob_log(NOB_ERROR, "FASM compilation failed");
        return false;
    }
    
    nob_log(NOB_INFO, "Compilation successful!");
    return true;
}
