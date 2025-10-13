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
		.types = {0},
	};
}

static void RegisterType(Generator *g, const char *name, int size) 
{
    TypeInfo info = { 
		.name = name, 
		.size = size 
	};
    arena_da_append(g->arena, &g->types, info);
}

static TypeInfo* FindType(Generator *g, const char *name)
{
    for (size_t i = 0; i < g->types.count; i++) 
        if (strcmp(g->types.items[i].name, name) == 0) 
            return &g->types.items[i];
    return NULL;
}

static int GetTypeSize(Generator *g, const char *type_name)
{
    TypeInfo *info = FindType(g, type_name);
    return info ? info->size : 8;
}

static const char* JaiToFasmType(const char *jai_type)
{
    if (strcmp(jai_type, "int") == 0) 
		return "dq";
    if (strcmp(jai_type, "s64") == 0) 
		return "dq";
    if (strcmp(jai_type, "u32") == 0) 
		return "dd";
    if (strcmp(jai_type, "u16") == 0) 
		return "dw";
    if (strcmp(jai_type, "u8") == 0) 
		return "db";
    return NULL;  // It's a struct type
}

static void GenEmit(Generator *g, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	char buffer[4096];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	
	nob_sb_append_cstr(&g->sb, buffer);
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
    if (node->right && node->right->type == AST_CALL) 
	{
        AST_Node *call = node->right;
        if (call->left && call->left->type == AST_ID) 
		{
            GenEmit(g, "    call %s\n", call->left->name);
            GenEmit(g, "    _StoreVar %s, rax\n", node->name);
        }
        return;
    }
    
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

static void GenStruct(Generator *g, AST_Node *node) 
{
    if (!node->name) 
	{
        nob_log(NOB_ERROR, "Struct missing name");
        return;
    }
    
    int total_size = 0;
    for (size_t i = 0; i < node->children.used; i++) 
	{
        AST_Node *field = node->children.data[i];
        if (field->type == AST_FIELD && field->right) 
            total_size += GetTypeSize(g, field->right->name);
    }
    
    RegisterType(g, node->name, total_size);
    
    GenEmit(g, "struc %s\n{\n", node->name);
    
    for (size_t i = 0; i < node->children.used; i++) 
	{
        AST_Node *field = node->children.data[i];
        if (field->type == AST_FIELD && field->name && field->right) 
		{
            const char *fasm_type = JaiToFasmType(field->right->name);
            
            if (fasm_type) 
                GenEmit(g, "    .%s %s ?\n", field->name, fasm_type);
            else
                GenEmit(g, "    .%s %s\n", field->name, field->right->name);
        }
    }
    
    GenEmit(g, "}\n\n");
}

static void GenStmt(Generator *g, AST_Node *node)
{
    if (!node) 
		return;
    
    switch (node->type) 
	{
        case AST_ASSIGNMENT:
			if (node->right && node->right->type == AST_TYPE)
				break;
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
            
        case AST_STRUCT:
            GenStruct(g, node);
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
    
    int locals_size = 0;
    
    if (node->body && node->body->type == AST_BLOCK) 
	{
        for (size_t i = 0; i < node->body->children.used; ++i) 
		{
            AST_Node *stmt = node->body->children.data[i];
            if (stmt->type == AST_ASSIGNMENT) 
			{
                if (stmt->right && stmt->right->type == AST_TYPE) 
				{
                    const char *type_name = stmt->right->name;
                    int type_size = GetTypeSize(g, type_name);
                    locals_size += type_size;
                } 
				else 
                    locals_size += 8;
            }
        }
    }
    
    GenEmit(g, "_FuncBeginWithLocals %s, %d\n", func_name, locals_size);
    
    int offset = 0;
    if (node->body && node->body->type == AST_BLOCK) 
	{
        for (size_t i = 0; i < node->body->children.used; i++) 
		{
            AST_Node *stmt = node->body->children.data[i];
            if (stmt->type == AST_ASSIGNMENT && stmt->name) 
			{
                if (stmt->right && stmt->right->type == AST_TYPE) 
				{
                    const char *type_name = stmt->right->name;
                    int type_size = GetTypeSize(g, type_name);
                    offset += type_size;
                    
                    GenEmit(g, "    _DeclareVar %s, %d\n", stmt->name, type_size);
                    GenEmit(g, "    %s_offset = %d\n", stmt->name, offset);
                } 
				else 
				{
                    offset += 8;
                    GenEmit(g, "    _DeclareVar %s, 8\n", stmt->name);
                    GenEmit(g, "    %s_offset = %d\n", stmt->name, offset);
                }
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
    GenEmit(g, "; Generated by Jai compiler\n");
    GenEmit(g, "; asmsyntax=fasm\n");
    GenEmit(g, "include 'runtime/core.asm'\n\n");
    
	for (size_t i = 0; i < node->children.used; ++i) 
	{
		AST_Node *decl = node->children.data[i];
		if (decl->type == AST_STRUCT) 
			GenProc(g, decl);
	}
    
	for (size_t i = 0; i < node->children.used; ++i) 
	{
		AST_Node *decl = node->children.data[i];
		if (decl->type == AST_PROC) 
			GenProc(g, decl);
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
