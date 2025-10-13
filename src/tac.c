#define TAC_DEF
#include <cmpl.h>
#include <nob.h>
#include <stdio.h>
#include <string.h>

static void TACInit(TAC_Builder *tb, Arena *arena) 
{
    tb->head = NULL;
    tb->tail = NULL;
    tb->temp_count = 0;
    tb->arena = arena;
}

static char* NewTemp(TAC_Builder *tb) 
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "_t%d", tb->temp_count++);
    return arena_strdup(tb->arena, buffer);
}

static void TACAppend(TAC_Builder *tb, TAC_Inst *inst) 
{
    if (!tb->head) 
        tb->head = tb->tail = inst;
	else 
	{
        tb->tail->next = inst;
        tb->tail = inst;
    }
}

static TAC_Inst* TACCreate(TAC_Builder *tb, TAC_Op type) 
{
    TAC_Inst *inst = arena_alloc(tb->arena, sizeof(TAC_Inst));
    memset(inst, 0, sizeof(TAC_Inst));
    inst->type = type;
    return inst;
}

static char* ExprToTAC(TAC_Builder *tb, AST_Node *node) 
{
    if (!node) 
		return NULL;
    
    switch (node->type) 
	{
        case AST_NUM: 
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%lld", node->num);
            return arena_strdup(tb->arena, buffer);
        
        case AST_ID: 
            return arena_strdup(tb->arena, node->name);
        
        case AST_BIN_OP: 
            char *left = ExprToTAC(tb, node->left);
            char *right = ExprToTAC(tb, node->right);
            char *result = NewTemp(tb);
            
            TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
            inst->dest = result;
            inst->src1 = left;
            inst->src2 = right;
            inst->op = arena_strdup(tb->arena, node->name);
            TACAppend(tb, inst);
            return result;
        
        case AST_CALL: 
            // Function call
            if (node->left && node->left->type == AST_ID) 
			{
                char *result = NewTemp(tb);
                
                TAC_Inst *inst = TACCreate(tb, TAC_CALL);
                inst->dest = result;
                inst->src1 = arena_strdup(tb->arena, node->left->name);
                TACAppend(tb, inst);
                
                return result;
            }
            return NULL;
        
        default:
            nob_log(NOB_ERROR, "Unsupported expression type in TAC: %d", node->type);
            return NULL;
    }
}

static void StmtToTAC(TAC_Builder *tb, AST_Node *node) 
{
    if (!node) 
		return;
    
    switch (node->type) 
	{
        case AST_ASSIGNMENT: 
            if (node->right && node->right->type == AST_TYPE)
                break;
            char *src = ExprToTAC(tb, node->right);
            
            TAC_Inst *inst = TACCreate(tb, TAC_COPY);
            inst->dest = arena_strdup(tb->arena, node->name);
            inst->src1 = src;
            TACAppend(tb, inst);
            break;
        
        case AST_RETURN: 
            char *src = node->right ? ExprToTAC(tb, node->right) : "0";
            TAC_Inst *inst = TACCreate(tb, TAC_RETURN);
            inst->src1 = src;
            TACAppend(tb, inst);
            break;
        
        case AST_IF: 
			{
            // TODO: Implement control flow with labels/jumps
            // For now, we'll keep using the macro form
            break;
        }
        
        case AST_WHILE: {
            // TODO: Implement control flow with labels/jumps
            break;
        }
        
        case AST_BLOCK: 
            for (size_t i = 0; i < node->children.used; i++)
                StmtToTAC(tb, node->children.data[i]);
            break;
        
        default:
            break;
    }
}

TAC_Inst* FuncBodyToTAC(AST_Node *body, Arena *arena) 
{
    TAC_Builder tb;
    TACInit(&tb, arena);
    
    if (body && body->type == AST_BLOCK) 
        for (size_t i = 0; i < body->children.used; i++)
            StmtToTAC(&tb, body->children.data[i]);
	else
        StmtToTAC(&tb, body);
    
    return tb.head;
}
