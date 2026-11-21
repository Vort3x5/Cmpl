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
            TAC_Op kind = TAC_NEG;
            if (node->name && strcmp(node->name, "!") == 0)
                kind = TAC_NOT;
            TAC_Inst *inst = TACCreate(tb, kind);
            inst->dest = result;
            inst->src2 = operand;
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
        /* Evaluate every argument fully first (nested calls included), then
         * load argument registers — otherwise inner call clobbers rdi/rsi. */
        char *args[MAX_CALL_ARGS];
        for (size_t i = 0; i < nargs; i++)
            args[i] = ExprToTAC(tb, node->children.data[i]);
        for (size_t i = 0; i < nargs; i++) {
            TAC_Inst *p = TACCreate(tb, TAC_PARAM);
            p->src1 = args[i];
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
        TAC_Inst *inst = TACCreate(tb, TAC_LEA);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->right->name);
        TACAppend(tb, inst);
        return result;
    }

    case AST_DEREF: {
        char *ptr = ExprToTAC(tb, node->right);
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_LOAD);
        inst->dest = result;
        inst->src1 = ptr;
        TACAppend(tb, inst);
        return result;
    }

    case AST_FIELD_ACCESS: {
        if (!node->left || node->left->type != AST_ID) {
            nob_log(NOB_ERROR, "TAC: only var.field supported");
            return NULL;
        }
        char *result = NewTemp(tb);
        TAC_Inst *inst = TACCreate(tb, TAC_FIELD);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->left->name);
        inst->src2 = arena_strdup(tb->arena, node->name);
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
        TAC_Inst *inst = TACCreate(tb, TAC_INDEX);
        inst->dest = result;
        inst->src1 = arena_strdup(tb->arena, node->left->name);
        inst->src2 = idx;
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
            TAC_Inst *inst = TACCreate(tb, TAC_STORE_FIELD);
            inst->dest = arena_strdup(tb->arena, node->left->left->name);
            inst->src1 = src;
            inst->src2 = arena_strdup(tb->arena, node->left->name);
            TACAppend(tb, inst);
        } else if (node->left && node->left->type == AST_INDEX) {
            char *idx = ExprToTAC(tb, node->left->right);
            TAC_Inst *inst = TACCreate(tb, TAC_STORE_INDEX);
            inst->dest = arena_strdup(tb->arena, node->left->left->name);
            inst->src1 = src;
            inst->src2 = idx;
            TACAppend(tb, inst);
        } else if (node->left && node->left->type == AST_DEREF) {
            char *ptr = ExprToTAC(tb, node->left->right);
            TAC_Inst *inst = TACCreate(tb, TAC_STORE_DEREF);
            inst->dest = ptr;
            inst->src1 = src;
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
        char *src = node->right ? ExprToTAC(tb, node->right) : arena_strdup(tb->arena, "0");
        TAC_Inst *inst = TACCreate(tb, TAC_RETURN);
        inst->src1 = src;
        TACAppend(tb, inst);
        break;
    }

    case AST_CALL: {
        ExprToTAC(tb, node);
        break;
    }

    case AST_IF:
    case AST_WHILE:
    case AST_FOR_RANGE:
    case AST_DEFER:
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

static int MaxInt(int a, int b)
{
    return a > b ? a : b;
}

/* Highest _tN index needed by any expression/statement in this subtree. */
int TACMaxTempsInNode(AST_Node *node, Arena *arena)
{
    if (!node)
        return -1;

    int max_t = -1;

    switch (node->type) {
    case AST_ASSIGNMENT:
    case AST_RETURN:
    case AST_CALL: {
        TAC_Builder tb;
        TACInit(&tb, arena);
        if (node->type == AST_ASSIGNMENT || node->type == AST_RETURN)
            StmtToTAC(&tb, node);
        else
            ExprToTAC(&tb, node);
        max_t = MaxInt(max_t, TACGetMaxTemp(tb.head));
        max_t = MaxInt(max_t, tb.temp_count - 1);
        break;
    }
    case AST_IF:
        if (node->left) {
            TAC_Builder tb;
            TACInit(&tb, arena);
            ExprToTAC(&tb, node->left);
            max_t = MaxInt(max_t, tb.temp_count - 1);
        }
        max_t = MaxInt(max_t, TACMaxTempsInNode(node->body, arena));
        max_t = MaxInt(max_t, TACMaxTempsInNode(node->right, arena));
        break;
    case AST_WHILE:
        if (node->left) {
            TAC_Builder tb;
            TACInit(&tb, arena);
            ExprToTAC(&tb, node->left);
            max_t = MaxInt(max_t, tb.temp_count - 1);
        }
        max_t = MaxInt(max_t, TACMaxTempsInNode(node->body, arena));
        break;
    case AST_FOR_RANGE:
        if (node->left) {
            TAC_Builder tb;
            TACInit(&tb, arena);
            ExprToTAC(&tb, node->left);
            max_t = MaxInt(max_t, tb.temp_count - 1);
        }
        if (node->right) {
            TAC_Builder tb;
            TACInit(&tb, arena);
            ExprToTAC(&tb, node->right);
            max_t = MaxInt(max_t, tb.temp_count - 1);
        }
        max_t = MaxInt(max_t, TACMaxTempsInNode(node->body, arena));
        break;
    case AST_DEFER:
        max_t = MaxInt(max_t, TACMaxTempsInNode(node->body, arena));
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->children.used; i++)
            max_t = MaxInt(max_t, TACMaxTempsInNode(node->children.data[i], arena));
        break;
    default:
        break;
    }
    return max_t;
}
