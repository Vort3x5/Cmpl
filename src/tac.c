#define TAC_DEF
#include <cmpl.h>
#include <nob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CALL_ARGS 6

void TACInit(TAC_Builder *tb, Arena *arena)
{
    tb->head = NULL;
    tb->tail = NULL;
    tb->temp_count = 0;
    tb->arena = arena;
}

static char *NewTemp(TAC_Builder *tb)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "_t%d", tb->temp_count++);
    return arena_strdup(tb->arena, buffer);
}

int TACGetMaxTemp(TAC_Inst *tac)
{
    int max_temp = -1;
    for (TAC_Inst *inst = tac; inst != NULL; inst = inst->next) {
        const char *fields[] = { inst->dest, inst->src1, inst->src2 };
        for (int i = 0; i < 3; i++) {
            if (fields[i] && fields[i][0] == '_' && fields[i][1] == 't') {
                int temp_num = atoi(fields[i] + 2);
                if (temp_num > max_temp)
                    max_temp = temp_num;
            }
        }
    }
    return max_temp;
}

static void TACAppend(TAC_Builder *tb, TAC_Inst *inst)
{
    if (!tb->head)
        tb->head = tb->tail = inst;
    else {
        tb->tail->next = inst;
        tb->tail = inst;
    }
}

static TAC_Inst *TACCreate(TAC_Builder *tb, TAC_Op type)
{
    TAC_Inst *inst = arena_alloc(tb->arena, sizeof(TAC_Inst));
    memset(inst, 0, sizeof(TAC_Inst));
    inst->type = type;
    return inst;
}

static bool IsUnaryBinOp(AST_Node *node)
{
    return node && node->type == AST_BIN_OP && node->left == NULL && node->right != NULL;
}

char *ExprToTAC(TAC_Builder *tb, AST_Node *node)
{
    if (!node)
        return NULL;

    switch (node->type) {
    case AST_NUM: {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lld", (long long)node->num);
        return arena_strdup(tb->arena, buffer);
    }

    case AST_ID:
        return arena_strdup(tb->arena, node->name);

    case AST_BIN_OP: {
        if (IsUnaryBinOp(node)) {
            char *operand = ExprToTAC(tb, node->right);
            char *result = NewTemp(tb);
            TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
            inst->dest = result;
            inst->src1 = arena_strdup(tb->arena, "0");
            inst->src2 = operand;
            /* Encode unary as 0 - x or !x via op name with 'u' prefix for ! */
            if (node->name && strcmp(node->name, "!") == 0)
                inst->op = arena_strdup(tb->arena, "!");
            else if (node->name && strcmp(node->name, "-") == 0)
                inst->op = arena_strdup(tb->arena, "neg");
            else
                inst->op = arena_strdup(tb->arena, node->name ? node->name : "-");
            TACAppend(tb, inst);
            return result;
        }

        char *left = ExprToTAC(tb, node->left);
        char *right = ExprToTAC(tb, node->right);
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
        inst->dest = result;
        inst->src1 = left;
        inst->src2 = right;
        inst->op = arena_strdup(tb->arena, node->name ? node->name : "+");
        TACAppend(tb, inst);
        return result;
    }

    case AST_CALL: {
        if (!node->left || node->left->type != AST_ID)
            return NULL;
        size_t nargs = node->children.used;
        if (nargs > MAX_CALL_ARGS)
            nargs = MAX_CALL_ARGS;
        for (size_t i = 0; i < nargs; i++) {
            char *arg = ExprToTAC(tb, node->children.data[i]);
            TAC_Inst *p = TACCreate(tb, TAC_PARAM);
            p->src1 = arg;
            char idx[8];
            snprintf(idx, sizeof(idx), "%zu", i);
            p->op = arena_strdup(tb->arena, idx);
            TACAppend(tb, p);
        }
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_CALL);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->left->name);
        TACAppend(tb, inst);
        return result;
    }

    case AST_ADDR_OF: {
        if (!node->right || node->right->type != AST_ID) {
            nob_log(NOB_ERROR, "TAC: only *local address-of supported");
            return NULL;
        }
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->right->name);
        inst->op = arena_strdup(tb->arena, "lea");
        TACAppend(tb, inst);
        return result;
    }

    case AST_DEREF: {
        char *ptr = ExprToTAC(tb, node->right);
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
        inst->dest = result;
        inst->src1 = ptr;
        inst->op = arena_strdup(tb->arena, "load");
        TACAppend(tb, inst);
        return result;
    }

    case AST_FIELD_ACCESS: {
        /* Leave as special: emit via generator helper — store name in dest pattern */
        if (!node->left || node->left->type != AST_ID) {
            nob_log(NOB_ERROR, "TAC: only var.field supported");
            return NULL;
        }
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->left->name);
        inst->src2 = arena_strdup(tb->arena, node->name); /* field name */
        inst->op = arena_strdup(tb->arena, "field");
        TACAppend(tb, inst);
        return result;
    }

    case AST_INDEX: {
        if (!node->left || node->left->type != AST_ID) {
            nob_log(NOB_ERROR, "TAC: only arr[i] supported");
            return NULL;
        }
        char *idx = ExprToTAC(tb, node->right);
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_BINOP);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->left->name);
        inst->src2 = idx;
        inst->op = arena_strdup(tb->arena, "index");
        TACAppend(tb, inst);
        return result;
    }

    default:
        nob_log(NOB_ERROR, "Unsupported expression type in TAC: %d", node->type);
        return NULL;
    }
}

void StmtToTAC(TAC_Builder *tb, AST_Node *node)
{
    if (!node)
        return;

    switch (node->type) {
    case AST_ASSIGNMENT: {
        if (node->right && node->right->type == AST_TYPE)
            break;
        char *src = ExprToTAC(tb, node->right);
        if (node->left && node->left->type == AST_FIELD_ACCESS) {
            TAC_Inst *inst = TACCreate(tb, TAC_COPY);
            inst->dest = arena_strdup(tb->arena, node->left->left->name);
            inst->src1 = src;
            inst->src2 = arena_strdup(tb->arena, node->left->name);
            inst->op = arena_strdup(tb->arena, "store_field");
            TACAppend(tb, inst);
        } else if (node->left && node->left->type == AST_INDEX) {
            char *idx = ExprToTAC(tb, node->left->right);
            TAC_Inst *inst = TACCreate(tb, TAC_COPY);
            inst->dest = arena_strdup(tb->arena, node->left->left->name);
            inst->src1 = src;
            inst->src2 = idx;
            inst->op = arena_strdup(tb->arena, "store_index");
            TACAppend(tb, inst);
        } else if (node->left && node->left->type == AST_DEREF) {
            char *ptr = ExprToTAC(tb, node->left->right);
            TAC_Inst *inst = TACCreate(tb, TAC_COPY);
            inst->dest = ptr;
            inst->src1 = src;
            inst->op = arena_strdup(tb->arena, "store_deref");
            TACAppend(tb, inst);
        } else {
            TAC_Inst *inst = TACCreate(tb, TAC_COPY);
            inst->dest = arena_strdup(tb->arena, node->name);
            inst->src1 = src;
            TACAppend(tb, inst);
        }
        break;
    }

    case AST_RETURN: {
        char *src = node->right ? ExprToTAC(tb, node->right) : "0";
        TAC_Inst *inst = TACCreate(tb, TAC_RETURN);
        inst->src1 = src;
        TACAppend(tb, inst);
        break;
    }

    case AST_CALL: {
        ExprToTAC(tb, node); /* side-effect call; result temp unused */
        break;
    }

    case AST_IF:
    case AST_WHILE:
    case AST_FOR_RANGE:
    case AST_DEFER:
        /* Handled by generator control-flow wrappers */
        break;

    case AST_BLOCK:
        for (size_t i = 0; i < node->children.used; i++)
            StmtToTAC(tb, node->children.data[i]);
        break;

    default:
        break;
    }
}

TAC_Inst *FuncBodyToTAC(AST_Node *body, Arena *arena)
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
