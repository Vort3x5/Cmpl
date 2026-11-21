#define GEN_DEF

#include <nob.h>
#include <cmpl.h>

static const char *arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static void GenInit(Generator *g, Arena *a)
{
	*g = (Generator){
		.sb = (Nob_String_Builder){0},
		.arena = a,
		.local_offset = 0,
		.temp_count = 0,
		.types = {0},
		.locals = {0},
		.defers = {0},
		.libraries = {0},
		.foreigns = {0},
	};
    
    // Builtins
    TypeInfo builtins[] = {
        { .name = "int", .size = 8, .align = 8 },
        { .name = "s64", .size = 8, .align = 8 },
        { .name = "u64", .size = 8, .align = 8 },
        { .name = "u32", .size = 4, .align = 4 },
        { .name = "u16", .size = 2, .align = 2 },
        { .name = "u8",  .size = 1, .align = 1 },
    };
    for (size_t i = 0; i < sizeof(builtins)/sizeof(builtins[0]); i++)
        arena_da_append(g->arena, &g->types, builtins[i]);
}

static TypeInfo* FindType(Generator *g, const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < g->types.count; i++) 
        if (strcmp(g->types.items[i].name, name) == 0) 
            return &g->types.items[i];
    return NULL;
}

static int GetTypeSize(Generator *g, const char *type_name)
{
    TypeInfo *info = FindType(g, type_name);
    return info ? (int)info->size : 8;
}

static void LocalsClear(Generator *g)
{
    g->locals.count = 0;
}

static void LocalsAdd(Generator *g, const char *name, const char *type_name, int array_len)
{
    for (size_t i = 0; i < g->locals.count; i++) {
        if (strcmp(g->locals.items[i].name, name) == 0) {
            if (type_name)
                g->locals.items[i].type_name = type_name;
            if (array_len > 0)
                g->locals.items[i].array_len = array_len;
            return;
        }
    }
    LocalInfo info = {
        .name = name,
        .type_name = type_name ? type_name : "int",
        .array_len = array_len,
    };
    arena_da_append(g->arena, &g->locals, info);
}

static LocalInfo* LocalsFind(Generator *g, const char *name)
{
    for (size_t i = 0; i < g->locals.count; i++) {
        if (strcmp(g->locals.items[i].name, name) == 0)
            return &g->locals.items[i];
    }
    return NULL;
}

static const char* LocalsGetType(Generator *g, const char *name)
{
    LocalInfo *loc = LocalsFind(g, name);
    return loc ? loc->type_name : "int";
}

static FieldInfo* FindField(TypeInfo *type, const char *field_name)
{
    if (!type) return NULL;
    for (size_t i = 0; i < type->field_count; i++) {
        if (strcmp(type->fields[i].name, field_name) == 0)
            return &type->fields[i];
    }
    return NULL;
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
    return NULL;
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

/* Reserved compiler locals (not user temps). */
#define RESERVED_RET "_ret"
#define RESERVED_BOUND "_for_bound"

static const char *ForeignSymbol(Generator *g, const char *jai_name)
{
    for (size_t i = 0; i < g->foreigns.count; i++) {
        if (strcmp(g->foreigns.items[i].jai_name, jai_name) == 0)
            return g->foreigns.items[i].c_symbol;
    }
    return NULL;
}

static bool TacIsNum(const char *s)
{
    if (!s || !*s) return false;
    if (*s == '-') s++;
    if (!*s) return false;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return false;
    return true;
}

static void TacLoadOperand(Generator *g, const char *reg, const char *operand)
{
    if (TacIsNum(operand))
        GenEmit(g, "    mov %s, %s\n", reg, operand);
    else
        GenEmit(g, "    _LoadVar %s, %s\n", reg, operand);
}

static size_t TypeAlignOf(Generator *g, AST_Node *type_node)
{
    if (!type_node) return 8;
    if (type_node->flags & AST_FLAG_POINTER) return 8;
    int sz = GetTypeSize(g, type_node->name);
    if (sz <= 1) return 1;
    if (sz == 2) return 2;
    if (sz <= 4) return 4;
    TypeInfo *info = FindType(g, type_node->name);
    if (info && info->align) return info->align;
    return 8;
}

static size_t AlignUp(size_t off, size_t align)
{
    if (align == 0) return off;
    return (off + align - 1) & ~(align - 1);
}

static void EmitFieldLoad(Generator *g, const char *var, FieldInfo *field)
{
    size_t sz = field->size ? field->size : 8;
    if (sz == 1)
        GenEmit(g, "    movzx rax, byte [rbp - %s_offset + %zu]\n", var, field->offset);
    else if (sz == 2)
        GenEmit(g, "    movzx rax, word [rbp - %s_offset + %zu]\n", var, field->offset);
    else if (sz == 4)
        GenEmit(g, "    mov eax, dword [rbp - %s_offset + %zu]\n", var, field->offset);
    else
        GenEmit(g, "    mov rax, [rbp - %s_offset + %zu]\n", var, field->offset);
}

static void EmitFieldStore(Generator *g, const char *var, FieldInfo *field)
{
    size_t sz = field->size ? field->size : 8;
    if (sz == 1)
        GenEmit(g, "    mov byte [rbp - %s_offset + %zu], al\n", var, field->offset);
    else if (sz == 2)
        GenEmit(g, "    mov word [rbp - %s_offset + %zu], ax\n", var, field->offset);
    else if (sz == 4)
        GenEmit(g, "    mov dword [rbp - %s_offset + %zu], eax\n", var, field->offset);
    else
        GenEmit(g, "    mov [rbp - %s_offset + %zu], rax\n", var, field->offset);
}

/* Flat TAC emit: temps live in stack slots; no push/pop around calls. */
static void EmitTACInst(Generator *g, TAC_Inst *inst)
{
    switch (inst->type) {
    case TAC_PARAM: {
        int idx = inst->op ? atoi(inst->op) : 0;
        if (idx < 0 || idx > 5) idx = 0;
        TacLoadOperand(g, arg_regs[idx], inst->src1);
        break;
    }
    case TAC_LEA:
        GenEmit(g, "    lea rax, [rbp - %s_offset]\n", inst->src1);
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    case TAC_LOAD:
        TacLoadOperand(g, "rax", inst->src1);
        GenEmit(g, "    mov rax, [rax]\n");
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    case TAC_FIELD: {
        TypeInfo *type = FindType(g, LocalsGetType(g, inst->src1));
        FieldInfo *field = FindField(type, inst->src2);
        if (!field) {
            nob_log(NOB_ERROR, "Unknown field %s.%s", inst->src1, inst->src2);
            break;
        }
        EmitFieldLoad(g, inst->src1, field);
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    }
    case TAC_INDEX: {
        LocalInfo *loc = LocalsFind(g, inst->src1);
        int elem_size = GetTypeSize(g, loc ? loc->type_name : "int");
        TacLoadOperand(g, "rax", inst->src2);
        GenEmit(g, "    mov rcx, rax\n");
        GenEmit(g, "    imul rcx, %d\n", elem_size);
        GenEmit(g, "    mov rax, [rbp + rcx - %s_offset]\n", inst->src1);
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    }
    case TAC_NEG:
        TacLoadOperand(g, "rax", inst->src2);
        GenEmit(g, "    neg rax\n");
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    case TAC_NOT:
        TacLoadOperand(g, "rax", inst->src2);
        GenEmit(g, "    test rax, rax\n");
        GenEmit(g, "    mov rax, 0\n");
        GenEmit(g, "    sete al\n");
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    case TAC_BINOP: {
        const char *op = inst->op ? inst->op : "+";
        TacLoadOperand(g, "rax", inst->src1);
        TacLoadOperand(g, "rbx", inst->src2);
        if (strcmp(op, "+") == 0) GenEmit(g, "    add rax, rbx\n");
        else if (strcmp(op, "-") == 0) GenEmit(g, "    sub rax, rbx\n");
        else if (strcmp(op, "*") == 0) GenEmit(g, "    imul rax, rbx\n");
        else if (strcmp(op, "<<") == 0) {
            GenEmit(g, "    mov rcx, rbx\n");
            GenEmit(g, "    shl rax, cl\n");
        } else if (strcmp(op, ">>") == 0) {
            GenEmit(g, "    mov rcx, rbx\n");
            GenEmit(g, "    sar rax, cl\n");
        } else if (strcmp(op, "==") == 0) {
            GenEmit(g, "    cmp rax, rbx\n    mov rax, 0\n    sete al\n");
        } else if (strcmp(op, "!=") == 0) {
            GenEmit(g, "    cmp rax, rbx\n    mov rax, 0\n    setne al\n");
        } else if (strcmp(op, "<") == 0) {
            GenEmit(g, "    cmp rax, rbx\n    mov rax, 0\n    setl al\n");
        } else if (strcmp(op, ">") == 0) {
            GenEmit(g, "    cmp rax, rbx\n    mov rax, 0\n    setg al\n");
        } else if (strcmp(op, "<=") == 0) {
            GenEmit(g, "    cmp rax, rbx\n    mov rax, 0\n    setle al\n");
        } else if (strcmp(op, ">=") == 0) {
            GenEmit(g, "    cmp rax, rbx\n    mov rax, 0\n    setge al\n");
        } else {
            nob_log(NOB_ERROR, "Unsupported TAC binop: %s", op);
        }
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    }
    case TAC_STORE_FIELD: {
        TacLoadOperand(g, "rax", inst->src1);
        TypeInfo *type = FindType(g, LocalsGetType(g, inst->dest));
        FieldInfo *field = FindField(type, inst->src2);
        if (!field) {
            nob_log(NOB_ERROR, "Unknown field store %s.%s", inst->dest, inst->src2);
            break;
        }
        EmitFieldStore(g, inst->dest, field);
        break;
    }
    case TAC_STORE_INDEX: {
        LocalInfo *loc = LocalsFind(g, inst->dest);
        int elem_size = GetTypeSize(g, loc ? loc->type_name : "int");
        TacLoadOperand(g, "rax", inst->src1);
        TacLoadOperand(g, "rcx", inst->src2);
        GenEmit(g, "    imul rcx, %d\n", elem_size);
        GenEmit(g, "    mov [rbp + rcx - %s_offset], rax\n", inst->dest);
        break;
    }
    case TAC_STORE_DEREF:
        TacLoadOperand(g, "rax", inst->src1);
        TacLoadOperand(g, "rbx", inst->dest);
        GenEmit(g, "    mov [rbx], rax\n");
        break;
    case TAC_COPY:
        TacLoadOperand(g, "rax", inst->src1);
        GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    case TAC_CALL: {
        const char *sym = ForeignSymbol(g, inst->src1);
        if (sym)
            GenEmit(g, "    call %s\n", sym);
        else
            GenEmit(g, "    call func_%s\n", inst->src1);
        if (inst->dest)
            GenEmit(g, "    _StoreVar %s, rax\n", inst->dest);
        break;
    }
    case TAC_RETURN:
        TacLoadOperand(g, "rax", inst->src1 ? inst->src1 : "0");
        GenEmit(g, "    _ReturnRax\n");
        break;
    default:
        break;
    }
}

static void EmitTACList(Generator *g, TAC_Inst *head)
{
    for (TAC_Inst *inst = head; inst; inst = inst->next)
        EmitTACInst(g, inst);
}

/* Evaluate expression via TAC into rax (temps in stack slots). */
static void GenExprStmt(Generator *g, AST_Node *node)
{
    if (!node) return;
    TAC_Builder tb;
    TACInit(&tb, g->arena);
    char *result = ExprToTAC(&tb, node);
    EmitTACList(g, tb.head);
    if (result)
        TacLoadOperand(g, "rax", result);
    else
        GenEmit(g, "    mov rax, 0\n");
}

static void GenCall(Generator *g, AST_Node *call)
{
    GenExprStmt(g, call);
}

static void GenAssign(Generator *g, AST_Node *node)
{
    TAC_Builder tb;
    TACInit(&tb, g->arena);
    StmtToTAC(&tb, node);
    EmitTACList(g, tb.head);
}

static void GenEmitDefers(Generator *g, size_t from)
{
    // LIFO: last deferred runs first
    for (size_t i = g->defers.count; i > from; i--) {
        GenEmit(g, "    ; defer\n");
        GenStmt(g, g->defers.items[i - 1]);
    }
}

static void GenReturn(Generator *g, AST_Node *node)
{
    /* Keep return value in a stack slot so defer calls cannot break SSE ABI. */
    if (!node->right) {
        GenEmit(g, "    mov rax, 0\n");
    } else {
        GenExprStmt(g, node->right);
    }
    GenEmit(g, "    _StoreVar %s, rax\n", RESERVED_RET);
    GenEmitDefers(g, 0);
    GenEmit(g, "    _LoadVar rax, %s\n", RESERVED_RET);
    GenEmit(g, "    _ReturnRax\n");
}

static void GenIf(Generator *g, AST_Node *node) 
{
    GenEmit(g, "    ");
    GenExprStmt(g, node->left);
    GenEmit(g, "\n    _BeginIf\n");
    
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
    GenEmit(g, "    _BeginWhile\n");
    GenEmit(g, "    ");
    GenExprStmt(g, node->left);
    GenEmit(g, "\n    _WhileTest\n");
    
    if (node->body) 
        GenStmt(g, node->body);
    
    GenEmit(g, "    _EndWhile\n");
}

static void GenFor(Generator *g, AST_Node *node)
{
    /* Jai: `for i: a..b` inclusive. Bound kept in slot (no push) so call-in-range is SSE-safe. */
    const char *it = node->name ? node->name : "it";
    bool reverse = (node->flags & AST_FLAG_REVERSE) != 0;

    if (reverse) {
        GenExprStmt(g, node->left);
        GenEmit(g, "    _StoreVar %s, rax\n", RESERVED_BOUND);
        GenExprStmt(g, node->right);
        GenEmit(g, "    _StoreVar %s, rax\n", it);

        GenEmit(g, "    _BeginWhile\n");
        GenEmit(g, "    _LoadVar rax, %s\n", it);
        GenEmit(g, "    _LoadVar rbx, %s\n", RESERVED_BOUND);
        GenEmit(g, "    cmp rax, rbx\n");
        GenEmit(g, "    mov rax, 0\n");
        GenEmit(g, "    setge al\n");
        GenEmit(g, "    _WhileTest\n");
    } else {
        GenExprStmt(g, node->right);
        GenEmit(g, "    _StoreVar %s, rax\n", RESERVED_BOUND);
        GenExprStmt(g, node->left);
        GenEmit(g, "    _StoreVar %s, rax\n", it);

        GenEmit(g, "    _BeginWhile\n");
        GenEmit(g, "    _LoadVar rax, %s\n", it);
        GenEmit(g, "    _LoadVar rbx, %s\n", RESERVED_BOUND);
        GenEmit(g, "    cmp rax, rbx\n");
        GenEmit(g, "    mov rax, 0\n");
        GenEmit(g, "    setle al\n");
        GenEmit(g, "    _WhileTest\n");
    }

    if (node->body)
        GenStmt(g, node->body);

    if (reverse) {
        GenEmit(g, "    _LoadVar rax, %s\n", it);
        GenEmit(g, "    sub rax, 1\n");
        GenEmit(g, "    _StoreVar %s, rax\n", it);
    } else {
        GenEmit(g, "    _LoadVar rax, %s\n", it);
        GenEmit(g, "    add rax, 1\n");
        GenEmit(g, "    _StoreVar %s, rax\n", it);
    }

    GenEmit(g, "    _EndWhile\n");
}

static void GenStruct(Generator *g, AST_Node *node) 
{
    if (!node->name) 
	{
        nob_log(NOB_ERROR, "Struct missing name");
        return;
    }
    
    size_t field_count = 0;
    for (size_t i = 0; i < node->children.used; i++) {
        if (node->children.data[i]->type == AST_FIELD)
            field_count++;
    }
    
    FieldInfo *fields = arena_alloc(g->arena, field_count * sizeof(FieldInfo));
    size_t total_size = 0;
    size_t max_align = 1;
    size_t fi = 0;
    
    for (size_t i = 0; i < node->children.used; i++) 
	{
        AST_Node *field = node->children.data[i];
        if (field->type != AST_FIELD || !field->name || !field->right) 
            continue;
        
        size_t falign = TypeAlignOf(g, field->right);
        size_t fsize;
        if (field->right->flags & AST_FLAG_POINTER)
            fsize = 8;
        else
            fsize = (size_t)GetTypeSize(g, field->right->name);
        total_size = AlignUp(total_size, falign);
        fields[fi].name = field->name;
        fields[fi].offset = total_size;
        fields[fi].size = fsize;
        total_size += fsize;
        if (falign > max_align) max_align = falign;
        fi++;
    }
    total_size = AlignUp(total_size, max_align);
    
    TypeInfo info = {
        .name = node->name,
        .size = total_size,
        .align = max_align,
        .fields = fields,
        .field_count = fi,
    };
    arena_da_append(g->arena, &g->types, info);
    
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
            
        case AST_IF:
            GenIf(g, node);
            break;
            
        case AST_WHILE:
            GenWhile(g, node);
            break;
        
        case AST_FOR_RANGE:
            GenFor(g, node);
            break;
            
        case AST_STRUCT:
            GenStruct(g, node);
            break;
        
        case AST_RETURN:
            GenReturn(g, node);
            break;
        
        case AST_DEFER:
            arena_da_append(g->arena, &g->defers, node->body);
            break;
        
        case AST_CALL:
            GenCall(g, node);
            break;
            
        case AST_BLOCK: {
            size_t defer_start = g->defers.count;
            for (size_t i = 0; i < node->children.used; i++)
                GenStmt(g, node->children.data[i]);
            // Scope exit: run defers registered in this block (LIFO)
            GenEmitDefers(g, defer_start);
            g->defers.count = defer_start;
            break;
        }
            
        default:
            nob_log(NOB_WARNING, "Unsupported statement type: %d", node->type);
            break;
    }
}

static int VarSize(Generator *g, AST_Node *var)
{
    if (var->right && var->right->type == AST_TYPE) {
        if (var->right->flags & AST_FLAG_POINTER)
            return 8;
        int elem = GetTypeSize(g, var->right->name);
        if (var->right->num > 0)
            return elem * (int)var->right->num;
        return elem;
    }
    return 8;
}

static void GenProc(Generator *g, AST_Node *node) 
{
    const char *func_name = node->name ? node->name : "anonymous";
    
    // #foreign: symbol provided by runtime / linker — emit nothing
    if (node->flags & AST_FLAG_FOREIGN) {
        const char *c_sym = (node->body && node->body->name) ? node->body->name : func_name;
        const char *lib = node->str ? node->str : "cmpl_rt";
        ForeignInfo info = {
            .jai_name = func_name,
            .c_symbol = c_sym,
            .lib = lib,
        };
        arena_da_append(g->arena, &g->foreigns, info);
        GenEmit(g, "; #foreign %s %s", lib, c_sym);
        if (strcmp(c_sym, func_name) != 0)
            GenEmit(g, " (as %s)", func_name);
        GenEmit(g, "\n");
        return;
    }
    
    LocalsClear(g);
    g->defers.count = 0;
    
    AST_Array all_vars = {0};
    ASTArrayInit(&all_vars);
    
    for (size_t i = 0; i < node->children.used; i++) {
        AST_Node *param = node->children.data[i];
        if (param->type == AST_VAR && param->name)
            ASTArrayPush(&all_vars, param, g->arena);
    }
    
    CollectVariables(node->body, &all_vars, g->arena);
    
    for (size_t i = 0; i < all_vars.used; i++) {
        AST_Node *var = all_vars.data[i];
        const char *ty = "int";
        int arr_len = 0;
        if (var->right && var->right->type == AST_TYPE && var->right->name) {
            ty = var->right->name;
            arr_len = (int)var->right->num;
        }
        LocalsAdd(g, var->name, ty, arr_len);
    }
    
    int locals_size = 0;
    for (size_t i = 0; i < all_vars.used; i++)
        locals_size += VarSize(g, all_vars.data[i]);
    /* Reserved slots + only as many TAC temps as this body needs. */
    int max_temp = TACMaxTempsInNode(node->body, g->arena);
    int ntemps = max_temp + 1;
    if (ntemps < 0) ntemps = 0;
    locals_size += 16; /* _ret + _for_bound */
    locals_size += ntemps * 8;
    
    if (locals_size % 16 != 0)
        locals_size += 16 - (locals_size % 16);
    
    GenEmit(g, "_FuncBeginWithLocals func_%s, %d\n", func_name, locals_size);
    
    for (size_t i = 0; i < all_vars.used; i++) {
        AST_Node *var = all_vars.data[i];
        GenEmit(g, "    _DeclareVar %s, %d\n", var->name, VarSize(g, var));
    }
    GenEmit(g, "    _DeclareVar %s, 8\n", RESERVED_RET);
    GenEmit(g, "    _DeclareVar %s, 8\n", RESERVED_BOUND);
    for (int t = 0; t < ntemps; t++)
        GenEmit(g, "    _DeclareVar _t%d, 8\n", t);
    
    size_t nparams = node->children.used;
    if (nparams > 6) nparams = 6;
    for (size_t i = 0; i < nparams; i++) {
        AST_Node *param = node->children.data[i];
        if (param->type == AST_VAR && param->name)
            GenEmit(g, "    _StoreVar %s, %s\n", param->name, arg_regs[i]);
    }
    
    GenEmit(g, "\n");
    GenStmt(g, node->body);
    GenEmit(g, "_FuncEnd\n\n");
}

static bool LibraryNeedsLink(AST_Node *lib)
{
    if (!lib || !lib->str) return false;
    return strcmp(lib->str, "cmpl_rt") != 0;
}

static bool ProgramNeedsLink(Generator *g, AST_Node *node)
{
    for (size_t i = 0; i < node->children.used; ++i) {
        AST_Node *decl = node->children.data[i];
        if (decl->type == AST_LIBRARY && LibraryNeedsLink(decl))
            return true;
    }
    return false;
}

static void GenProgram(Generator *g, AST_Node *node, bool need_link) 
{
    GenEmit(g, "; Generated by Jai compiler\n");
    GenEmit(g, "; asmsyntax=fasm\n");
    if (need_link)
        GenEmit(g, "include 'runtime/core_obj.asm'\n\n");
    else
        GenEmit(g, "include 'runtime/core.asm'\n\n");
    
	for (size_t i = 0; i < node->children.used; ++i) 
	{
		AST_Node *decl = node->children.data[i];
		if (decl->type == AST_LIBRARY) {
            arena_da_append(g->arena, &g->libraries, decl);
            GenEmit(g, "; %s :: #%s \"%s\"\n",
                decl->name,
                (decl->flags & AST_FLAG_SYSTEM_LIB) ? "system_library" : "library",
                decl->str ? decl->str : "");
        }
		if (decl->type == AST_STRUCT) 
			GenStruct(g, decl);
	}
    
    // Register foreigns first (so calls resolve)
	for (size_t i = 0; i < node->children.used; ++i) 
	{
		AST_Node *decl = node->children.data[i];
		if (decl->type == AST_PROC && (decl->flags & AST_FLAG_FOREIGN))
			GenProc(g, decl);
	}
    
    // extrn for symbols from real shared libs
    if (need_link) {
        for (size_t i = 0; i < g->foreigns.count; i++) {
            ForeignInfo *f = &g->foreigns.items[i];
            if (f->lib && strcmp(f->lib, "cmpl_rt") != 0)
                GenEmit(g, "extrn %s\n", f->c_symbol);
        }
        GenEmit(g, "\n");
    }
    
	for (size_t i = 0; i < node->children.used; ++i) 
	{
		AST_Node *decl = node->children.data[i];
		if (decl->type == AST_PROC && !(decl->flags & AST_FLAG_FOREIGN))
			GenProc(g, decl);
	}
    
    // Embedded cmpl_rt helpers when used (or always in standalone builds)
    bool uses_rt = !need_link;
    for (size_t i = 0; i < g->foreigns.count && !uses_rt; i++) {
        if (g->foreigns.items[i].lib && strcmp(g->foreigns.items[i].lib, "cmpl_rt") == 0)
            uses_rt = true;
    }
    if (uses_rt) {
        GenEmit(g, "include 'runtime/arena.asm'\n");
        GenEmit(g, "include 'runtime/ffi.asm'\n");
    }
}

bool Generate(AST_Node *ast, const char *output_path, Arena *arena)
{
    Generator g;
    GenInit(&g, arena);
    
    bool need_link = ProgramNeedsLink(&g, ast);
    
    nob_log(NOB_INFO, "Generating assembly code...");
    GenProgram(&g, ast, need_link);
    
    nob_sb_append_null(&g.sb);
    
    nob_log(NOB_INFO, "Writing assembly to %s", output_path);
    if (!nob_write_entire_file(output_path, g.sb.items, g.sb.count - 1)) 
	{
        nob_log(NOB_ERROR, "Failed to write assembly file");
        return false;
    }
    
    // Derive exe path: out/out.asm → out/out
    char exe_path[4096];
    snprintf(exe_path, sizeof(exe_path), "%s", output_path);
    char *dot = strrchr(exe_path, '.');
    if (dot && strcmp(dot, ".asm") == 0)
        *dot = '\0';
    
    if (need_link) {
        // Custom link with proper -rpath
        char obj_path[4096];
        snprintf(obj_path, sizeof(obj_path), "%s.o", exe_path);
        
        nob_log(NOB_INFO, "Assembling object with FASM...");
        Nob_Cmd fasm = {0};
        nob_cmd_append(&fasm, "fasm", output_path, obj_path);
        if (!nob_cmd_run(&fasm)) {
            nob_log(NOB_ERROR, "FASM object assembly failed");
            return false;
        }
        
        // Link with cc + CRT: FASM object exports `main` (see core_obj.asm).
        nob_log(NOB_INFO, "Linking with cc...");
        Nob_Cmd link = {0};
        nob_cmd_append(&link, "cc", "-no-pie", "-o", exe_path, obj_path);
        
        for (size_t i = 0; i < g.libraries.count; i++) {
            AST_Node *lib = g.libraries.items[i];
            if (!LibraryNeedsLink(lib)) continue;
            const char *path = lib->str;
            
            if (lib->flags & AST_FLAG_SYSTEM_LIB) {
                const char *name = path;
                if (strncmp(path, "lib", 3) == 0) name = path + 3;
                /* strip trailing .so if present */
                size_t nlen = strlen(name);
                if (nlen > 3 && strcmp(name + nlen - 3, ".so") == 0)
                    nob_cmd_append(&link, nob_temp_sprintf("-l%.*s", (int)(nlen - 3), name));
                else
                    nob_cmd_append(&link, nob_temp_sprintf("-l%s", name));
            } else {
                /* #library "path/libfoo" — append .so at link time */
                const char *so_path = path;
                size_t plen = strlen(path);
                if (plen < 3 || strcmp(path + plen - 3, ".so") != 0)
                    so_path = nob_temp_sprintf("%s.so", path);
                nob_cmd_append(&link, so_path);
                const char *slash = strrchr(so_path, '/');
                if (slash && slash != so_path) {
                    size_t n = (size_t)(slash - so_path);
                    nob_cmd_append(&link, nob_temp_sprintf("-Wl,-rpath,%.*s", (int)n, so_path));
                }
            }
        }
        /* Vulkan (and friends) come via NEEDED of linked .so — do not hardcode
         * -lvulkan here: Nix ld does not search /usr/lib by default. */
        nob_cmd_append(&link, "-lm", "-ldl", "-lpthread",
            "-Wl,-z,noexecstack");
        
        if (!nob_cmd_run(&link)) {
            nob_log(NOB_ERROR, "link failed");
            return false;
        }
    } else {
        nob_log(NOB_INFO, "Compiling with FASM...");
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "fasm", output_path);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "FASM compilation failed");
            return false;
        }
    }
    
    nob_log(NOB_INFO, "Compilation successful!");
    return true;
}
