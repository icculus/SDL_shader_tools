/**
 * SDL_shader_tools; tools for SDL GPU shader support.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 */

#define __SDL_SHADER_INTERNAL__ 1
#include "SDL_shader_internal.h"

/* fail() and friends use the Context filename/position, but this is not
   useful once parsing has completed. Use fail_ast() and friends instead,
   which get filename/position from specific AST nodes. */
#define fail COMPILER_MUST_USE_fail_ast_INSTEAD
#define failf COMPILER_MUST_USE_failf_ast_INSTEAD
#define warn COMPILER_MUST_USE_warn_ast_INSTEAD
#define warnf COMPILER_MUST_USE_warnf_ast_INSTEAD

/* The Notorious I.C.E. (Internal Compiler Error)...! */
#define ICE(ctx, ast, why) { \
    ctx->isiced = SDL_TRUE; \
    failf_ast(ctx, ast, "INTERNAL COMPILER ERROR: %s", why); \
    SDL_assert(!why); \
}

#define ICE_IF(ctx, ast, test, why) { \
    if ((test)) { \
        ctx->isiced = SDL_TRUE; \
        failf_ast(ctx, ast, "INTERNAL COMPILER ERROR: %s", why); \
        SDL_assert(!why); \
    } \
}

static SDL_bool is_reserved_keyword(const char *str)
{
    /* !!! FIXME: write me */
    return SDL_FALSE;
}

/* This figures out that an AST expression tree of `(2 * 4) - 5` equals 3.
   It will fail if a non-constant is used in the expression (`1 + x` will fail).
   This is used for things where an int constant is expected (declaring an array)
   but syntactic sugar appreciates a little math (`1024 * 1024` for a megabyte
   instead of `1048576`, etc). */
static SDL_bool ast_calc_int(const void *_expr, Sint32 *_val)
{
    const SDL_SHADER_AstNode *expr = (const SDL_SHADER_AstNode *) _expr;
    const SDL_SHADER_AstNodeType asttype = expr->ast.type;

    if (operator_is_unary(asttype))
    {
        Sint32 x;
        if (!ast_calc_int(expr->unary.operand, &x)) {
            return SDL_FALSE;
        }
        
        switch (asttype) {
            case SDL_SHADER_AST_OP_POSITIVE: *_val = x; return SDL_TRUE;
            case SDL_SHADER_AST_OP_NEGATE: *_val = -x; return SDL_TRUE;
            case SDL_SHADER_AST_OP_COMPLEMENT: *_val = ~x; return SDL_TRUE;
            case SDL_SHADER_AST_OP_PARENTHESES: *_val = x; return SDL_TRUE;
            /* rest of these are increment things (not constant!) or boolean things (not allowed on ints) */
            default: break;
        }
    } else if (operator_is_binary(asttype)) {
        Sint32 x, y;
        if (!ast_calc_int(expr->binary.left, &x) || !ast_calc_int(expr->binary.right, &y)) {
            return SDL_FALSE;
        }
        switch (asttype) {
            case SDL_SHADER_AST_OP_MULTIPLY: *_val = x * y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_DIVIDE: *_val = x / y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_MODULO: *_val = x % y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_ADD: *_val = x + y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_SUBTRACT: *_val = x - y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_LSHIFT: *_val = x << y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_RSHIFT: *_val = x >> y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_BINARYAND: *_val = x & y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_BINARYXOR: *_val = x ^ y; return SDL_TRUE;
            case SDL_SHADER_AST_OP_BINARYOR: *_val = x | y; return SDL_TRUE;
            default: break;
        }

    /* operator_is_ternary(asttype) currently not allowed, but this may change later. */

    } else if (asttype == SDL_SHADER_AST_OP_INT_LITERAL) {
        *_val = expr->intliteral.value;
        return SDL_TRUE;
    }

    return SDL_FALSE;  /* couldn't handle it. non-const or non-integer or something. */
}

static Sint32 resolve_constant_int_from_ast_expression(Context *ctx, const SDL_SHADER_AstExpression *expr, const Sint32 default_value)
{
    Sint32 val = 0;
    if (!ast_calc_int(expr, &val)) {
        fail_ast(ctx, &expr->ast, "Expected constant expression");
        return default_value;
    }
    return val;
}


/*
 * We do not require shaders to predeclare functions; in fact, there is no
 * function declaration grammar, just function definition. Since all
 * functions are at the top of the tree, we can just iterate the
 * translation units quickly to find them all.
 *
 * While we're walking the top level units, we might as well pick
 * out the struct declarations, too.
 */
static void semantic_analysis_build_globals_lists(Context *ctx)
{
    /* it would be easy to build this list while parsing the AST, but it violates some
       ideals of separating stages, and it maybe fragile if we decide that structs
       can be declared inside a function, etc. */
    SDL_SHADER_AstFunction fnhead;
    SDL_SHADER_AstFunction *prevfn = &fnhead;
    SDL_SHADER_AstStructDeclaration structhead;
    SDL_SHADER_AstStructDeclaration *prevstruct = &structhead;
    SDL_SHADER_AstTranslationUnit *i;

    fnhead.nextfn = NULL;
    structhead.nextstruct = NULL;

    for (i = ctx->shader->units->head; i != NULL; i = i->next) {
        switch (i->ast.type) {
            case SDL_SHADER_AST_TRANSUNIT_FUNCTION: {
                SDL_SHADER_AstFunctionUnit *fnunit = (SDL_SHADER_AstFunctionUnit *) i;
                SDL_SHADER_AstFunction *fn = fnunit->fn;
                prevfn->nextfn = fn;
                prevfn = fn;
                break;
            }
            case SDL_SHADER_AST_TRANSUNIT_STRUCT: {
                SDL_SHADER_AstStructDeclarationUnit *structunit = (SDL_SHADER_AstStructDeclarationUnit *) i;
                SDL_SHADER_AstStructDeclaration *structdecl = structunit->decl;
                prevstruct->nextstruct = structdecl;
                prevstruct = structdecl;
                break;
            }
            default: {
                ICE(ctx, &i->ast, "It look like we added a new translation unit type but don't handle it here.");
                break;
            }
        }
    }

    ctx->functions = fnhead.nextfn;
    ctx->structs = structhead.nextstruct;
}

static void semantic_analysis_check_globals_for_duplicates(Context *ctx)
{
    if (ctx->functions) {
        const SDL_SHADER_AstFunction *i;
        for (i = ctx->functions; i != NULL; i = i->nextfn) {
            if (is_reserved_keyword(i->name)) {
                failf_ast(ctx, &i->ast, "Cannot name a function with reserved keyword '%s'", i->name);
            } else {
                const SDL_SHADER_AstFunction *j;
                /* we don't have to check before i->nextfn, because it's either a comparison we already made or it's ourself. */
                for (j = i->nextfn; j != NULL; j = j->nextfn) {
                    /* we do not allow user-defined function overloading, so reusing an identifier at all is an error. */
                    if (i->name == j->name) {  /* strcache'd, so pointer comparison will show equality. */
                        failf_ast(ctx, &j->ast, "redefinition of function '%s'", j->name);
                        failf_ast(ctx, &i->ast, "previous definition of '%s' is here", j->name);
                    }
                }
            }
        }
    }

    if (ctx->structs) {
        const SDL_SHADER_AstStructDeclaration *i;
        for (i = ctx->structs; i != NULL; i = i->nextstruct) {
            if (is_reserved_keyword(i->name)) {
                failf_ast(ctx, &i->ast, "Cannot name a struct with reserved keyword '%s'", i->name);
            } else {
                const SDL_SHADER_AstStructDeclaration *j;
                /* we don't have to check before i->nextstruct, because it's either a comparison we already made or it's ourself. */
                for (j = i->nextstruct; j != NULL; j = j->nextstruct) {
                    if (i->name == j->name) {  /* strcache'd, so pointer comparison will show equality. */
                        failf_ast(ctx, &j->ast, "redefinition of struct '%s'", j->name);
                        failf_ast(ctx, &i->ast, "previous definition of '%s' is here", j->name);
                    }
                }
            }
        }
    }
}

static DataType *alloc_datatype(Context *ctx, const char *name, const DataTypeType dtt)
{
    DataType *dt = NULL;
    const char *strcached = stringcache(ctx->strcache, name);
    if (strcached) {
        dt = (DataType *) Malloc(ctx, sizeof (DataType));
        if (dt) {
            dt->name = strcached;
            dt->dtype = dtt;
            SDL_zero(dt->info);  /* this is safe to fill in after we add it to the hash. */
            hash_insert(ctx->datatypes, strcached, dt);
        }
    }
    return dt;
}

static const DataType *add_scalar_datatype(Context *ctx, const char *name, const DataTypeType dtt)
{
    return alloc_datatype(ctx, name, dtt);
}

static const DataType *add_vector_datatype(Context *ctx, const char *name, const DataType *childdt, const Uint32 elements)
{
    DataType *dt = alloc_datatype(ctx, name, DT_VECTOR);
    if (dt) {
        dt->info.vector.childdt = childdt;
        dt->info.vector.elements = elements;
    }
    return dt;
}

static const DataType *add_matrix_datatype(Context *ctx, const char *name, const DataType *childdt, const Uint32 rows)
{
    DataType *dt = alloc_datatype(ctx, name, DT_MATRIX);
    ICE_IF(ctx, &ctx->ast_before, childdt->dtype != DT_VECTOR, "Created a matrix that doesn't contain vectors");
    if (dt) {
        dt->info.matrix.childdt = childdt;
        dt->info.matrix.rows = rows;
    }
    return dt;
}
        
static const DataType *add_array_datatype(Context *ctx, const char *name, const DataType *childdt, const Uint32 elements)
{
    DataType *dt = alloc_datatype(ctx, name, DT_ARRAY);
    if (dt) {
        dt->info.vector.childdt = childdt;
        dt->info.vector.elements = elements;
    }
    return dt;
}

static const DataType *resolve_datatype(Context *ctx, const SDL_SHADER_AstNodeInfo *ast, const char *name)
{
    const DataType *dt = NULL;
    if (!hash_find(ctx->datatypes, name, (const void **) &dt)) {
        failf_ast(ctx, ast, "Unknown data type '%s'", name);
        dt = ctx->datatype_int;
    } else if (dt == NULL) {
        ICE(ctx, ast, "Successfully looked up datatype but the datatype turned out to be NULL!");
        dt = ctx->datatype_int;  /* oh well */
    }
    return dt;
}

static const char *get_array_datatype_name(Context *ctx, const char *datatype_name, const Sint32 iarraylen)
{
    const char *retval = NULL;
    char stack_name[64];
    const size_t len = SDL_snprintf(stack_name, sizeof (stack_name), "%s[%d]", datatype_name, (int) iarraylen);
    if (len < sizeof (stack_name)) {
        retval = stringcache(ctx->strcache, stack_name);
    } else {
        char *malloc_name = (char *) Malloc(ctx, len + 1);
        if (malloc_name) {
            SDL_snprintf(malloc_name, len + 1, "%s[%d]", datatype_name, (int) iarraylen);
            retval = stringcache(ctx->strcache, malloc_name);
            Free(ctx, malloc_name);
        }
    }
    return retval;
}

static void add_global_user_datatypes(Context *ctx)
{
    const SDL_SHADER_AstStructDeclaration *i;

    if (!ctx->datatype_int) {
        ICE_IF(ctx, &ctx->ast_before, !ctx->out_of_memory, "We don't have an int datatype but aren't out of memory...?");  /* should be only reason... */
        return;
    }

    for (i = ctx->structs; i != NULL; i = i->nextstruct) {
        alloc_datatype(ctx, i->name, DT_STRUCT);  /* add all the structs first, uninitialized, so they can reference each other in any order. */
    }

    for (i = ctx->structs; i != NULL; i = i->nextstruct) {
        Uint32 num_members = 0;
        DataTypeStructMembers *members = NULL;
        SDL_SHADER_AstStructMember *mem;
        DataType *dt = NULL;

        if (!hash_find(ctx->datatypes, i->name, (const void **) &dt)) {
            ICE_IF(ctx, &ctx->ast_before, !ctx->out_of_memory, "Failed to find a datatype we just added, and not out of memory!");  /* no other reason to be missing here, we just added it! */
            continue;
        }
        ICE_IF(ctx, &ctx->ast_before, dt == NULL, "Successfully looked up a datatype, but it's NULL!");
        ICE_IF(ctx, &ctx->ast_before, dt->dtype != DT_STRUCT, "Just added a struct datatype but looking it up found something else!");

        for (mem = i->members->head; mem != NULL; mem = mem->next) {
            num_members++;
        }

        members = (DataTypeStructMembers *) Malloc(ctx, sizeof (DataTypeStructMembers) * num_members);
        if (members) {
            Uint32 memidx = 0;
            for (mem = i->members->head; mem != NULL; mem = mem->next, memidx++) {
                members[memidx].name = mem->name;  /* strcache'd */
                members[memidx].dt = resolve_datatype(ctx, &mem->ast, mem->datatype_name);
                if (mem->arraysize != NULL) {
                    Sint32 iarraylen = resolve_constant_int_from_ast_expression(ctx, mem->arraysize, 1);
                    if (iarraylen <= 0) {
                        fail_ast(ctx, &mem->arraysize->ast, "Array size must be > 0");
                        iarraylen = 1;
                    }
                    const char *arraydt_name = get_array_datatype_name(ctx, mem->datatype_name, iarraylen);  /* strcache'd. */
                    const DataType *arraydt = NULL;
                    if (!hash_find(ctx->datatypes, arraydt_name, (const void **) &arraydt)) {
                        arraydt = add_array_datatype(ctx, arraydt_name, members[memidx].dt, iarraylen);
                    }
                    members[memidx].dt = arraydt;
                }
            }
            ICE_IF(ctx, &ctx->ast_before, memidx != num_members, "We created a struct datatype with an unexpected number of members!");
        }
        dt->info.structure.num_members = num_members;
        dt->info.structure.members = members;
    }
}

static void semantic_analysis_gather_datatypes(Context *ctx)
{
    /* build a table of all available data types. This will be the intrinsic ones (float4x4, etc)
       and any structs the program defined. */
    static const struct { DataTypeType dtt; const char *name; } base_types[] = {
        { DT_BOOLEAN, "bool" }, { DT_INT, "int" }, { DT_UINT, "uint" },
        { DT_HALF, "half" }, { DT_FLOAT, "float" }
    };

    char name[32];
    Uint32 i, j, k;

    for (i = 0; i < SDL_arraysize(base_types); i++) {
        const DataType *scalar = add_scalar_datatype(ctx, base_types[i].name, base_types[i].dtt);
        if (!scalar) {
            continue; /* out of memory, probably. */
        }

        switch (base_types[i].dtt) {
            case DT_INT: ctx->datatype_int = scalar; break;
            case DT_FLOAT: ctx->datatype_float = scalar; break;
            case DT_BOOLEAN: ctx->datatype_boolean = scalar; break;
            default: break;
        }

        for (j = 2; j <= 4; j++) {
            SDL_snprintf(name, sizeof (name), "%s%d", base_types[i].name, j);
            const DataType *vector = add_vector_datatype(ctx, name, scalar, j);
            if (!vector) {
                continue; /* out of memory, probably. */
            }
            for (k = 2; k <= 4; k++) {
                SDL_snprintf(name, sizeof (name), "%s%dx%d", base_types[i].name, j, k);
                add_matrix_datatype(ctx, name, vector, k);
            }
        }
    }

    /* intrinsic types are added, now add any structs the program defined. */
    add_global_user_datatypes(ctx);

    /* Now that datatypes are added, always pull them from ctx->datatypes, so you can just
       compare pointers to decide if a datatype is equal! */
}

/* make sure function and parameter datatypes are resolved before we walk the AST,
   because we might call a function that hasn't been declared at a given point. */
static void semantic_analysis_prepare_functions(Context *ctx)
{
    if (ctx->functions) {
        SDL_SHADER_AstFunction *fn;
        for (fn = ctx->functions; fn != NULL; fn = fn->nextfn) {
            fn->ast.dt = fn->datatype_name ? resolve_datatype(ctx, &fn->ast, fn->datatype_name) : NULL;
            if (fn->params != NULL) {  /* NULL here means "void" */
                SDL_SHADER_AstFunctionParam *i;
                for (i = fn->params->head; i != NULL; i = i->next) {
                    i->ast.dt = resolve_datatype(ctx, &i->ast, i->datatype_name);
                }
            }
        }
    }
}

static SDL_bool ast_is_integer(const void *_ast)
{
    const SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    return ((ast->ast.dt->dtype == DT_INT) || (ast->ast.dt->dtype == DT_UINT)) ? SDL_TRUE : SDL_FALSE;
}

static SDL_bool ast_is_number(const void *_ast)
{
    const SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    switch (ast->ast.dt->dtype) {
        case DT_INT:
        case DT_UINT:
        case DT_HALF:
        case DT_FLOAT:
            return SDL_TRUE;
        default: break;
    }
    return SDL_FALSE;
}

static SDL_bool ast_is_boolean(const void *_ast)
{
    const SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    return (ast->ast.dt->dtype == DT_BOOLEAN) ? SDL_TRUE : SDL_FALSE;
}

static SDL_bool ast_is_mathish(const void *_ast)
{
    const SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    DataTypeType dtt = ast->ast.dt->dtype;
    if (dtt == DT_VECTOR) {
        dtt = ast->ast.dt->info.vector.childdt->dtype;
    } else if (dtt == DT_MATRIX) {
        SDL_assert(ast->ast.dt->info.matrix.childdt->dtype == DT_VECTOR);
        dtt = ast->ast.dt->info.matrix.childdt->info.vector.childdt->dtype;
    }

    switch (dtt) {
        case DT_INT:
        case DT_UINT:
        case DT_HALF:
        case DT_FLOAT:
            return SDL_TRUE;
        default: break;
    }
    return SDL_FALSE;
}

static SDL_bool ast_is_array_dereferenceable(const void *_ast)
{
    const SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    switch (ast->ast.dt->dtype) {
        case DT_ARRAY:
        case DT_VECTOR:
        case DT_MATRIX:
            return SDL_TRUE;
        default: break;
    }
    return SDL_FALSE;
}

/* this means "can use the '.' operator", which means struct dereferences and vector swizzles. */
static SDL_bool ast_is_struct_dereferenceable(const void *_ast)
{
    const SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    switch (ast->ast.dt->dtype) {
        case DT_STRUCT:
        case DT_VECTOR:
            return SDL_TRUE;
        default: break;
    }
    return SDL_FALSE;
}

static SDL_bool ast_is_lvalue(Context *ctx, const SDL_SHADER_AstExpression *expr)
{
    switch (expr->ast.type) {
        case SDL_SHADER_AST_OP_IDENTIFIER:
        case SDL_SHADER_AST_OP_DEREF_ARRAY:
        case SDL_SHADER_AST_OP_DEREF_STRUCT:
            return SDL_TRUE;  /* !!! FIXME: figure out if the thing is const! */
        default: break;
    }

    return SDL_FALSE;
}

static SDL_bool ast_literal_can_promote_to(const SDL_SHADER_AstNodeType asttype, const DataType *dt)
{
    DataTypeType dtt = dt->dtype;
    if (dtt == DT_VECTOR) {
        dtt = dt->info.vector.childdt->dtype;
    } else if (dtt == DT_MATRIX) {
        SDL_assert(dt->info.matrix.childdt->dtype == DT_VECTOR);
        dtt = dt->info.matrix.childdt->info.vector.childdt->dtype;
    }

    if (asttype == SDL_SHADER_AST_OP_INT_LITERAL) {
        switch (dtt) {
            case DT_INT:
            case DT_UINT:
            case DT_HALF:
            case DT_FLOAT:
                return SDL_TRUE;
            default: break;
        }
    } else if (asttype == SDL_SHADER_AST_OP_FLOAT_LITERAL) {
        switch (dtt) {
            case DT_HALF:
            case DT_FLOAT:
                return SDL_TRUE;
            default: break;
        }
    }

    return SDL_FALSE;
}


/*
 * Returns SDL_TRUE if the datatypes of the two provided AST nodes match exactly,
 * and if they don't, if the nodes can implicitly convert as syntactic sugar.
 *
 * For example, `var int x = 5; var float4 y = x;` will fail because you can't
 * assign an int to a float4, but `var float4 y = 5;` works because we'll
 * accept _an int literal_ to mean `float4(5.0, 5.0, 5.0, 5.0)` here.
 *
 * Int literals can be promoted to a float (1 becomes 1.0), and int and float
 * literals can be promoted to vectors.
 *
 * Note this just checks if the datatypes are okay, it won't change the nodes in
 * any way, so a post-semantic-analysis stage will have to deal with that.
 *
 * These void pointers are AST nodes. We'll cast to generic ASTs for you so you
 * don't have to!
 */
static SDL_bool ast_datatypes_match(const void *_a, const void *_b)
{
    const SDL_SHADER_AstNode *a = (const SDL_SHADER_AstNode *) _a;
    const SDL_SHADER_AstNode *b = (const SDL_SHADER_AstNode *) _b;
    if (!a || !b || !a->ast.dt || !b->ast.dt) {
        return SDL_FALSE;  /* I guess... */
    } else if (a->ast.dt == b->ast.dt) {
        return SDL_TRUE;  /* easy peasy */
    } else if (ast_literal_can_promote_to(a->ast.type, b->ast.dt)) {
        return SDL_TRUE;
    } else if (ast_literal_can_promote_to(b->ast.type, a->ast.dt)) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static const char *ast_opstr(const SDL_SHADER_AstNodeType typ)
{
    switch (typ) {
        case SDL_SHADER_AST_OP_POSITIVE: return "+";
        case SDL_SHADER_AST_OP_NEGATE: return "-";
        case SDL_SHADER_AST_OP_COMPLEMENT: return "~";
        case SDL_SHADER_AST_OP_NOT: return "!";
        case SDL_SHADER_AST_OP_PARENTHESES: return "()";
        case SDL_SHADER_AST_OP_MULTIPLY: return "*";
        case SDL_SHADER_AST_OP_DIVIDE: return "/";
        case SDL_SHADER_AST_OP_MODULO: return "%";
        case SDL_SHADER_AST_OP_ADD: return "+";
        case SDL_SHADER_AST_OP_SUBTRACT: return "-";
        case SDL_SHADER_AST_OP_LSHIFT: return "<<";
        case SDL_SHADER_AST_OP_RSHIFT: return ">>";
        case SDL_SHADER_AST_OP_LESSTHAN: return "<";
        case SDL_SHADER_AST_OP_GREATERTHAN: return ">";
        case SDL_SHADER_AST_OP_LESSTHANOREQUAL: return "<=";
        case SDL_SHADER_AST_OP_GREATERTHANOREQUAL: return ">=";
        case SDL_SHADER_AST_OP_EQUAL: return "==";
        case SDL_SHADER_AST_OP_NOTEQUAL: return "!=";
        case SDL_SHADER_AST_OP_BINARYAND: return "&";
        case SDL_SHADER_AST_OP_BINARYXOR: return "^";
        case SDL_SHADER_AST_OP_BINARYOR: return "|";
        case SDL_SHADER_AST_OP_LOGICALAND: return "&&";
        case SDL_SHADER_AST_OP_LOGICALOR: return "||";
        case SDL_SHADER_AST_OP_DEREF_ARRAY: return "[]";
        case SDL_SHADER_AST_OP_DEREF_STRUCT: return ".";
        case SDL_SHADER_AST_OP_CONDITIONAL: return "?";
        case SDL_SHADER_AST_STATEMENT_ASSIGNMENT: return "=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNMUL: return "*=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNDIV: return "/=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNMOD: return "%=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNADD: return "+=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNSUB: return "-=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNLSHIFT: return "<<=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNRSHIFT: return ">>=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNAND: return "&=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNXOR: return "^=";
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNOR: return "|=";
        case SDL_SHADER_AST_STATEMENT_PREINCREMENT: return "++";
        case SDL_SHADER_AST_STATEMENT_POSTINCREMENT: return "++";
        case SDL_SHADER_AST_STATEMENT_PREDECREMENT: return "--";
        case SDL_SHADER_AST_STATEMENT_POSTDECREMENT: return "--";
        default: break;
    }

    SDL_assert(!"Unexpected operator!");  /* can't ICE here, no ctx or ast node, oh well */
    return "[unexpected operator]";
}

static const DataType *semantic_analysis_typecheck_swizzle(Context *ctx, const SDL_SHADER_AstExpression *expr, const char *swizzle)
{
    const DataType *retval;
    char newtype[32];
    const size_t slen = SDL_strlen(swizzle);
    SDL_bool has_rgba = SDL_FALSE;
    SDL_bool has_xyzw = SDL_FALSE;
    size_t i;

    if (expr->ast.dt->dtype != DT_VECTOR) {
        ICE(ctx, &expr->ast, "Expected a vector datatype to validate a swizzle!");
        return NULL;
    }

    if ((slen == 0) || (slen > 4)) {
        failf_ast(ctx, &expr->ast, "Invalid vector swizzle '%s'", swizzle);
        return NULL;
    }

    for (i = 0; i < slen; i++) {
        const char ch = swizzle[i];
        switch (ch) {
            case 'r':
            case 'g':
            case 'b':
            case 'a':
                has_rgba = SDL_TRUE;
                break;
            case 'x':
            case 'y':
            case 'z':
            case 'w':
                has_xyzw = SDL_TRUE;
                break;
            default:
                failf_ast(ctx, &expr->ast, "Invalid vector swizzle '%s'", swizzle);
                return NULL;
        }
    }

    ICE_IF(ctx, &expr->ast, !has_rgba && !has_xyzw, "Unexpected case in swizzle validation!");

    if (has_rgba && has_xyzw) {
        fail_ast(ctx, &expr->ast, "Swizzle cannot mix 'rgba' and 'xyzw' elements");
        return NULL;
    }

    if (slen == 1) {
        SDL_snprintf(newtype, sizeof (newtype), "%s", expr->ast.dt->info.vector.childdt->name);
    } else {
        SDL_snprintf(newtype, sizeof (newtype), "%s%d", expr->ast.dt->info.vector.childdt->name, (int) slen);
    }

    if (!hash_find(ctx->datatypes, stringcache(ctx->strcache, newtype), (const void **) &retval)) {
        ICE(ctx, &expr->ast, "Unexpected swizzled datatype!");
        return NULL;
    }

    ICE_IF(ctx, &expr->ast, retval == NULL, "Successfully looked up a datatype, but it's NULL!");

    return retval;
}

static SDL_bool semantic_analysis_validate_at_attribute(Context *ctx, const SDL_SHADER_AstAtAttribute *atattr, const char *name, const SDL_bool requires_arg)
{
    if (atattr) {
        if (SDL_strcmp(atattr->name, name) == 0) {
            if (atattr->has_argument && !requires_arg) {
                failf_ast(ctx, &atattr->ast, "Attribute '@%s' does not accept any arguments but one was provided", name);
            } else if (!atattr->has_argument && requires_arg) {
                failf_ast(ctx, &atattr->ast, "Attribute '@%s' requires an argument but none were provided", name);
            }
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static void semantic_analysis_validate_function_at_attribute(Context *ctx, SDL_SHADER_AstFunction *fn)
{
    SDL_SHADER_AstAtAttribute *atattr = fn->attribute;

    fn->fntype = SDL_SHADER_AST_FNTYPE_NORMAL;
    if (atattr) {
        if (semantic_analysis_validate_at_attribute(ctx, atattr, "vertex", SDL_FALSE)) {
            fn->fntype = SDL_SHADER_AST_FNTYPE_VERTEX;
        } else if (semantic_analysis_validate_at_attribute(ctx, atattr, "fragment", SDL_FALSE)) {
            fn->fntype = SDL_SHADER_AST_FNTYPE_FRAGMENT;
        } else {
            failf_ast(ctx, &atattr->ast, "Unknown function attribute '@%s' on function '%s'", atattr->name, fn->name);
        }
    }
}

static void semantic_analysis_validate_function_param_at_attribute(Context *ctx, SDL_SHADER_AstFunctionParam *fnparam)
{
    //SDL_SHADER_AstAtAttribute *atattr = fnparam->attribute;
    /* !!! FIXME: write me */
}

static ScopeItem *push_scope(Context *ctx, SDL_SHADER_AstNode *ast)
{
    ScopeItem *item;
    if (ctx->scope_pool == NULL) {
        item = (ScopeItem *) Malloc(ctx, sizeof (*item));
        if (!item) {
            return NULL;
        }
    } else {
        item = ctx->scope_pool;
        ctx->scope_pool = item->next;
    }

    item->ast = ast;
    item->next = ctx->scope_stack;
    ctx->scope_stack = item;

    return item;
}

static void pop_scope(Context *ctx, ScopeItem *item)
{
    if (item) {
        ScopeItem *top_of_stack = ctx->scope_stack;
        ctx->scope_stack = item->next;
        item->next = ctx->scope_pool;
        ctx->scope_pool = top_of_stack;
    }
}

static ScopeItem *find_parent_scope(Context *ctx, SDL_SHADER_AstNodeType typ)
{
    ScopeItem *i;
    for (i = ctx->scope_stack; i != NULL; i = i->next) {
        if (i->ast->ast.type == typ) {
            return i;
        }
    }
    return NULL;
}

static SDL_SHADER_AstStatement *find_break_parent(Context *ctx)
{
    ScopeItem *scope;
    for (scope = ctx->scope_stack; scope != NULL; scope = scope->next) {
        switch (scope->ast->ast.type) {
            case SDL_SHADER_AST_FUNCTION:
                return NULL;  /* hit a parent function and not found? Give up, there's nothing in scope. */

            case SDL_SHADER_AST_STATEMENT_DO:
            case SDL_SHADER_AST_STATEMENT_WHILE:
            case SDL_SHADER_AST_STATEMENT_FOR:
            case SDL_SHADER_AST_STATEMENT_SWITCH:
                return &scope->ast->stmt;
        }
    }

    return NULL;  /* didn't find anything. Should this be an ICE? */
}

static SDL_SHADER_AstStatement *find_continue_parent(Context *ctx)
{
    ScopeItem *scope;
    for (scope = ctx->scope_stack; scope != NULL; scope = scope->next) {
        switch (scope->ast->ast.type) {
            case SDL_SHADER_AST_FUNCTION:
                return NULL;  /* hit a parent function and not found? Give up, there's nothing in scope. */

            case SDL_SHADER_AST_STATEMENT_DO:
            case SDL_SHADER_AST_STATEMENT_WHILE:
            case SDL_SHADER_AST_STATEMENT_FOR:
                return &scope->ast->stmt;
        }
    }

    return NULL;  /* didn't find anything. Should this be an ICE? */
}

/*
 * Assign datatypes to everything in the tree than needs them, and
 * make sure datatypes all meet requirements (and emit warnings/errors if not),
 * and do basic correctness checking that's efficient to do while generally
 * walking the tree (make sure "continue" has a loop in scope, etc).
 *
 * Most of semantic analysis happens here. When the initial call to this
 * returns without generating errors, you can assume the program is
 * valid, various state has been updated with valid information, and can
 * and you can move on to the next stage of compiling.
 */
static void semantic_analysis_treewalk(Context *ctx, void *_ast)
{
    SDL_SHADER_AstNode *ast = (SDL_SHADER_AstNode *) _ast;
    const SDL_SHADER_AstNodeType asttype = ast->ast.type;
    ScopeItem *scope;

    switch (asttype) {
        case SDL_SHADER_AST_OP_POSITIVE:
        case SDL_SHADER_AST_OP_NEGATE:
            semantic_analysis_treewalk(ctx, ast->unary.operand);
            /* !!! FIXME: these should work with numeric vectors and matrices, too */
            if (!ast_is_number(ast->unary.operand)) {
                failf_ast(ctx, &ast->unary.operand->ast, "Datatype for unary '%s' must be a number", ast_opstr(asttype));
                ast->ast.dt = ctx->datatype_int;
            } else {
                ast->ast.dt = ast->unary.operand->ast.dt;
            }
            return;

        case SDL_SHADER_AST_OP_COMPLEMENT:
            /* !!! FIXME: these should work with integer vectors and matrices, too */
            semantic_analysis_treewalk(ctx, ast->unary.operand);
            if (!ast_is_integer(ast->unary.operand)) {
                failf_ast(ctx, &ast->unary.operand->ast, "Datatype for '%s' must be an integer", ast_opstr(asttype));
                ast->ast.dt = ctx->datatype_int;
            } else {
                ast->ast.dt = ast->unary.operand->ast.dt;
            }
            return;

        case SDL_SHADER_AST_OP_NOT:
            semantic_analysis_treewalk(ctx, ast->unary.operand);
            if (!ast_is_boolean(ast->unary.operand)) {  /* GLSL does not dither ints to bools either. */
                failf_ast(ctx, &ast->unary.operand->ast, "Datatype for '%s' must be an boolean", ast_opstr(asttype));
                ast->ast.dt = ctx->datatype_boolean;
            } else {
                ast->ast.dt = ast->unary.operand->ast.dt;
            }
            return;

        case SDL_SHADER_AST_OP_PARENTHESES:
            semantic_analysis_treewalk(ctx, ast->unary.operand);
            ast->ast.dt = ast->unary.operand->ast.dt;
            return;

        case SDL_SHADER_AST_OP_MULTIPLY: {
            SDL_SHADER_AstExpression *left = ast->binary.left;
            SDL_SHADER_AstExpression *right = ast->binary.right;
            SDL_bool inputs_okay = SDL_TRUE;
            semantic_analysis_treewalk(ctx, left);
            if (!ast_is_mathish(left)) {
                failf_ast(ctx, &left->ast, "Can't use a datatype of '%s' with the '%s' operator", left->ast.dt->name, ast_opstr(asttype));
                inputs_okay = SDL_FALSE;
            }
            semantic_analysis_treewalk(ctx, right);
            if (!ast_is_mathish(right)) {
                failf_ast(ctx, &right->ast, "Can't use a datatype of '%s' with the '%s' operator", right->ast.dt->name, ast_opstr(asttype));
                inputs_okay = SDL_FALSE;
            }

            ast->ast.dt = left->ast.dt;  /* we might change this below. */

            /* multiply will let you use any mathish thing, scalar, vector, or matrix, in either order, so we need some special cases here. */
            /* This (mostly?) follows GLSL conventions. */
            if (inputs_okay) {
                const DataTypeType ldtt = left->ast.dt->dtype;
                const DataTypeType rdtt = right->ast.dt->dtype;
                /* some of these don't use ast_datatypes_match because we know they aren't literals, and we need to do deal with child datatypes and not AST nodes */
                if (ldtt == DT_VECTOR) {
                    if (rdtt == DT_VECTOR) {  /* (v * v) gives you datatype v */
                        if (left->ast.dt != right->ast.dt) {
                            failf_ast(ctx, &ast->ast, "Vector datatypes must match with the '%s' operator", ast_opstr(asttype));
                        }
                    } else if (rdtt == DT_MATRIX) {  /* (v * m) gives you datatype v */
                        if (left->ast.dt != right->ast.dt->info.matrix.childdt) {
                            failf_ast(ctx, &ast->ast, "Vector datatype must match matrix columns with the '%s' operator", ast_opstr(asttype));  /* !!! FIXME: decide if we're row or column major. :O */
                        }
                    } else if (!ast_datatypes_match(left, right)) {  /* (v * s) gives you datatype v */
                        /* ast_datatypes_match will catch literals, but we need to check for non-literal scalars too. */
                        if (left->ast.dt->info.vector.childdt != right->ast.dt) {
                            failf_ast(ctx, &ast->ast, "Vector and scalar datatypes must match with the '%s' operator", ast_opstr(asttype));
                        }
                    }
                } else if (ldtt == DT_MATRIX) {
                    if (rdtt == DT_VECTOR) {  /* (m * v) gives you datatype v */
                        if (left->ast.dt->info.matrix.childdt != right->ast.dt) {
                            failf_ast(ctx, &ast->ast, "Vector datatype must match matrix columns with the '%s' operator", ast_opstr(asttype));  /* !!! FIXME: decide if we're row or column major. :O */
                        } else {
                            ast->ast.dt = right->ast.dt;  /* this needs to be a vector. */
                        }
                    } else if (rdtt == DT_MATRIX) {  /* (m * m) gives you datatype m */
                        if (!ast_datatypes_match(left, right)) {
                            failf_ast(ctx, &ast->ast, "Matrix datatypes must match with the '%s' operator", ast_opstr(asttype));
                        }
                    } else if (!ast_datatypes_match(left, right)) {  /* (m * s) gives you datatype m */
                        /* ast_datatypes_match will catch literals, but we need to check for non-literal scalars too. */
                        if (left->ast.dt->info.matrix.childdt->info.vector.childdt != right->ast.dt) {
                            failf_ast(ctx, &ast->ast, "Matrix and scalar datatypes must match with the '%s' operator", ast_opstr(asttype));
                        }
                    }
                } else {
                    ast->ast.dt = right->ast.dt;  /* this needs to be what we multiplied the scalar by. */
                    if (rdtt == DT_VECTOR) {  /* (s * v) gives you datatype v */
                        if (left->ast.dt != right->ast.dt->info.vector.childdt) {
                            failf_ast(ctx, &ast->ast, "Scalar and vector datatypes must match with the '%s' operator", ast_opstr(asttype));
                        }
                    } else if (rdtt == DT_MATRIX) {  /* (s * m) gives you datatype m */
                        if (left->ast.dt != right->ast.dt->info.matrix.childdt->info.vector.childdt) {
                            failf_ast(ctx, &ast->ast, "Scalar and matrix datatype must match with the '%s' operator", ast_opstr(asttype));
                        }
                    } else if (!ast_datatypes_match(left, right)) {  /* this will catch literals multiplied by vectors and matrices. */
                        failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
                    }
                }
            }
            return;
        }

        case SDL_SHADER_AST_OP_DIVIDE:
        case SDL_SHADER_AST_OP_ADD:
        case SDL_SHADER_AST_OP_SUBTRACT:
            semantic_analysis_treewalk(ctx, ast->binary.left);
            if (!ast_is_mathish(ast->binary.left)) {
                failf_ast(ctx, &ast->binary.left->ast, "Can't use a datatype of '%s' with the '%s' operator", ast->binary.left->ast.dt->name, ast_opstr(asttype));
            }
            semantic_analysis_treewalk(ctx, ast->binary.right);
            if (!ast_is_mathish(ast->binary.right)) {
                failf_ast(ctx, &ast->binary.right->ast, "Can't use a datatype of '%s' with the '%s' operator", ast->binary.right->ast.dt->name, ast_opstr(asttype));
            }
            if (!ast_datatypes_match(ast->binary.left, ast->binary.right)) {
                failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
            }
            ast->ast.dt = ast->binary.left->ast.dt;
            return;

        case SDL_SHADER_AST_OP_MODULO:
        case SDL_SHADER_AST_OP_LSHIFT:
        case SDL_SHADER_AST_OP_RSHIFT:
        case SDL_SHADER_AST_OP_BINARYAND:
        case SDL_SHADER_AST_OP_BINARYXOR:
        case SDL_SHADER_AST_OP_BINARYOR:
            /* !!! FIXME: these should work with integer vectors and matrices, too */
            semantic_analysis_treewalk(ctx, ast->binary.left);
            if (!ast_is_integer(ast->binary.left)) {
                failf_ast(ctx, &ast->binary.left->ast, "Datatypes for '%s' operator must be integers", ast_opstr(asttype));
            }
            semantic_analysis_treewalk(ctx, ast->binary.right);
            if (!ast_is_integer(ast->binary.right)) {
                failf_ast(ctx, &ast->binary.right->ast, "Datatypes for '%s' operator must be integers", ast_opstr(asttype));
            }
            if (!ast_datatypes_match(ast->binary.left, ast->binary.right)) {
                failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
            }
            ast->ast.dt = ast->binary.left->ast.dt;
            return;

        case SDL_SHADER_AST_OP_LESSTHAN:
        case SDL_SHADER_AST_OP_GREATERTHAN:
        case SDL_SHADER_AST_OP_LESSTHANOREQUAL:
        case SDL_SHADER_AST_OP_GREATERTHANOREQUAL:
            semantic_analysis_treewalk(ctx, ast->binary.left);
            if (!ast_is_number(ast->binary.left)) {
                failf_ast(ctx, &ast->binary.left->ast, "Datatypes for '%s' operator must be numbers", ast_opstr(asttype));
            }
            semantic_analysis_treewalk(ctx, ast->binary.right);
            if (!ast_is_number(ast->binary.right)) {
                failf_ast(ctx, &ast->binary.right->ast, "Datatypes for '%s' operator must be numbers", ast_opstr(asttype));
            }
            if (!ast_datatypes_match(ast->binary.left, ast->binary.right)) {
                failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
            }
            ast->ast.dt = ctx->datatype_boolean;
            return;

        case SDL_SHADER_AST_OP_EQUAL:
        case SDL_SHADER_AST_OP_NOTEQUAL:
            semantic_analysis_treewalk(ctx, ast->binary.left);
            semantic_analysis_treewalk(ctx, ast->binary.right);
            if (!ast_datatypes_match(ast->binary.left, ast->binary.right)) {
                failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
            }
            ast->ast.dt = ctx->datatype_boolean;
            return;

        case SDL_SHADER_AST_OP_LOGICALAND:
        case SDL_SHADER_AST_OP_LOGICALOR:
            semantic_analysis_treewalk(ctx, ast->binary.left);
            if (!ast_is_boolean(ast->binary.left)) {
                failf_ast(ctx, &ast->binary.left->ast, "Datatypes for '%s' operator must be boolean", ast_opstr(asttype));
            }
            semantic_analysis_treewalk(ctx, ast->binary.right);
            if (!ast_is_boolean(ast->binary.right)) {
                failf_ast(ctx, &ast->binary.right->ast, "Datatypes for '%s' operator must be boolean", ast_opstr(asttype));
            }
            ast->ast.dt = ctx->datatype_boolean;
            return;

        case SDL_SHADER_AST_OP_DEREF_ARRAY:
            semantic_analysis_treewalk(ctx, ast->binary.left);
            if (!ast_is_array_dereferenceable(ast->binary.left)) {
                failf_ast(ctx, &ast->binary.left->ast, "Datatype to the left of '%s' operator must be array, vector, or matrix", ast_opstr(asttype));
                ast->ast.dt = ast->binary.left->ast.dt;  /* oh well */
            } else {
                ast->ast.dt = ast->binary.left->ast.dt->info.array.childdt;
            }
            semantic_analysis_treewalk(ctx, ast->binary.right);
            if (!ast_is_integer(ast->binary.right)) {
                failf_ast(ctx, &ast->binary.right->ast, "Datatype in the '%s' operator must be integer", ast_opstr(asttype));
            }
            /* !!! FIXME: if this is a constant expression, fail if `left` is out of bounds */
            /* !!! FIXME: GLSL appears to allow `my_vec4[x]` where `x` is a variable >= 4 ... what happens in this case at runtime? */
            return;

        case SDL_SHADER_AST_OP_DEREF_STRUCT:
            semantic_analysis_treewalk(ctx, ast->structderef.expr);
            if (!ast_is_struct_dereferenceable(ast->structderef.expr)) {
                failf_ast(ctx, &ast->binary.left->ast, "Datatype to the left of '%s' operator must be a struct or vector", ast_opstr(asttype));
                ast->ast.dt = ast->structderef.expr->ast.dt;  /* oh well. */
            } else {
                switch (ast->structderef.expr->ast.dt->dtype) {
                    case DT_STRUCT: {
                        const char *field = ast->structderef.field;
                        const DataTypeStruct *dtstruct = &ast->structderef.expr->ast.dt->info.structure;
                        const DataTypeStructMembers *mem = dtstruct->members;
                        const Uint32 num_members = dtstruct->num_members;
                        Uint32 i;
                        for (i = 0; i < num_members; i++, mem++) {
                            if (mem->name == field) {  /* this is strcache'd, you can compare pointers. */
                                ast->ast.dt = mem->dt;
                                break;
                            }
                        }

                        if (ast->ast.dt == NULL) {
                            failf_ast(ctx, &ast->ast, "No such field '%s' in struct '%s'", field, ast->structderef.expr->ast.dt->name);
                            ast->ast.dt = ctx->datatype_int;  /* oh well */
                        }
                        break;
                    }

                    case DT_VECTOR:  /* is it a swizzle? */
                        ast->ast.dt = semantic_analysis_typecheck_swizzle(ctx, ast->structderef.expr, ast->structderef.field);  /* this will call fail_ast if necessary. */
                        if (ast->ast.dt == NULL) {
                            ast->ast.dt = ast->structderef.expr->ast.dt;  /* on error, set the expression datatype to the full, unswizzled vector type. */
                        }
                        break;
                
                    default:
                        ICE(ctx, &ast->ast, "Unexpected struct deref type");
                        ast->ast.dt = ctx->datatype_int;  /* oh well */
                        break;
                }
            }
            return;

        case SDL_SHADER_AST_OP_CONDITIONAL:
            semantic_analysis_treewalk(ctx, ast->ternary.left);
            if (!ast_is_boolean(ast->ternary.left)) {
                failf_ast(ctx, &ast->binary.left->ast, "Datatype to the left of '%s' operator must be boolean", ast_opstr(asttype));
            }
            semantic_analysis_treewalk(ctx, ast->ternary.center);
            semantic_analysis_treewalk(ctx, ast->ternary.right);
            if (!ast_datatypes_match(ast->ternary.center, ast->ternary.right)) {
                failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
            }
            ast->ast.dt = ast->ternary.center->ast.dt;
            return;

        case SDL_SHADER_AST_OP_IDENTIFIER:
            for (scope = ctx->scope_stack; scope != NULL; scope = scope->next) {
                const SDL_SHADER_AstNodeType nodetype = scope->ast->ast.type;
                if (nodetype == SDL_SHADER_AST_FUNCTION) {  /* hit the function and not found? Give up. We don't, at the moment, have any global variables. */
                    break;
                } else if ((nodetype == SDL_SHADER_AST_VARIABLE_DECLARATION) && (ast->identifier.name == scope->ast->vardecl.name)) {  /* strcache'd, can compare string pointers */
                    ast->ast.dt = scope->ast->ast.dt;
                    break;
                } else if ((nodetype == SDL_SHADER_AST_FUNCTION_PARAM) && (ast->identifier.name == scope->ast->fnparam.name)) {  /* strcache'd, can compare string pointers */
                    ast->ast.dt = scope->ast->ast.dt;
                    break;
                }
            }

            /* !!! FIXME: if NULL, search functions and datatypes, and if it matches, give a clearer error message. */

            if (ast->ast.dt == NULL) {
                failf_ast(ctx, &ast->ast, "Variable '%s' undeclared", ast->identifier.name);  /* !!! FIXME: gcc limits this to one error per function for each undeclared identifier. */
                ast->ast.dt = ctx->datatype_int;
            }
            return;

        case SDL_SHADER_AST_OP_INT_LITERAL:
            ast->ast.dt = ctx->datatype_int;
            return;

        case SDL_SHADER_AST_OP_FLOAT_LITERAL:
            ast->ast.dt = ctx->datatype_float;
            return;

        case SDL_SHADER_AST_OP_BOOLEAN_LITERAL:
            ast->ast.dt = ctx->datatype_boolean;
            return;

        case SDL_SHADER_AST_OP_CALLFUNC: {  /* this might be a function call or constructor. */
            SDL_SHADER_AstFunctionCallExpression *fncall = &ast->fncall;
            const char *name = fncall->fnname;
            SDL_SHADER_AstFunction *i;
            for (i = ctx->functions; i != NULL; i = i->nextfn) {
                if (i->name == name) {  /* strcache'd, we can compare pointers. */
                    break;
                }
            }

            if (i != NULL) {  /* `i != NULL` means "this is a user-defined function" */
                SDL_SHADER_AstArgument *arg = fncall->arguments ? fncall->arguments->head : NULL;
                SDL_SHADER_AstFunctionParam *param = i->params ? i->params->head : NULL;
                Uint32 num_args = 0;
                Uint32 num_params = 0;
                fncall->ast.dt = i->ast.dt;
                fncall->fn = i;
                while (arg || param) {
                    if (arg) {
                        semantic_analysis_treewalk(ctx, arg->arg);
                        num_args++;
                    }
                    if (param) {
                        /* these are already treewalked, don't do it again. */
                        num_params++;
                    }
                    if (arg && param) {
                        if (!ast_datatypes_match(arg->arg, param)) {
                            failf_ast(ctx, &arg->arg->ast, "Argument #%d does not match function's parameter datatype", (int) num_args);
                        }
                    }

                    if (arg) {
                        arg = arg->next;
                    }
                    if (param) {
                        param = param->next;
                    }
                }

                if (num_args != num_params) {
                    failf_ast(ctx, &fncall->ast, "Function call expected %d arguments, had %d", (int) num_params, (int) num_args);
                }

            // !!! FIXME: } else { search intrinsic functions

            } else if (hash_find(ctx->datatypes, name, (const void **) &fncall->ast.dt)) {  /* if the name is a datatype, this is a constructor. */
                if (fncall->ast.dt == NULL) {
                    ICE(ctx, &ast->ast, "Successfully looked up datatype but the datatype turned out to be NULL!");
                    fncall->ast.dt = ctx->datatype_int;  /* oh well */
                } else {
//                    fixme make sure we have valid arguments.
                }
            } else {
                failf_ast(ctx, &ast->ast, "Function '%s' undeclared", name);  /* !!! FIXME: gcc limits this to one error per function for each undeclared identifier. */
                fncall->ast.dt = ctx->datatype_int;
            }
            return;
        }


        /* NOTE THAT STATEMENT *BLOCKS* WILL ITERATE CHILDREN, AND EACH STATEMENT DOES NOT RECURSE OVER ITS `next` FIELD. */
        /* STATEMENTS _DO_ RECURSE OVER THEIR OWN CHILDREN, AS THEY ARE THE ONLY ONES THAT KNOW ABOUT THEM. */

        case SDL_SHADER_AST_STATEMENT_EMPTY:
            return;  /* no data type on statements, nothing else to do. */

        case SDL_SHADER_AST_STATEMENT_BREAK:
            ast->breakstmt.parent = find_break_parent(ctx);
            if (!ast->breakstmt.parent) {
                fail_ast(ctx, &ast->ast, "Break statement must be inside a loop or switch block");
            }
            return;

        case SDL_SHADER_AST_STATEMENT_CONTINUE:
            ast->contstmt.parent = find_continue_parent(ctx);
            if (!ast->contstmt.parent) {
                fail_ast(ctx, &ast->ast, "Continue statement must be inside a loop or switch block");
            }
            return;

        case SDL_SHADER_AST_STATEMENT_DISCARD:
            for (scope = ctx->scope_stack; scope != NULL; scope = scope->next) {
                const SDL_SHADER_AstNodeType nodetype = scope->ast->ast.type;
                if (nodetype == SDL_SHADER_AST_FUNCTION) {
                    break;
                }
            }
            if (!scope) {
                fail_ast(ctx, &ast->ast, "Discard statement must be inside a function");  /* this _probably_ can't happen, but just in case. */
            } else if (scope->ast->fn.fntype != SDL_SHADER_AST_FNTYPE_FRAGMENT) {
                fail_ast(ctx, &ast->ast, "Discard statements are only allowed in @fragment functions");
            }
            return;  /* no data type on statements, nothing else to do. */

        case SDL_SHADER_AST_STATEMENT_VARDECL:
            semantic_analysis_treewalk(ctx, ast->vardeclstmt.vardecl);
            return;  /* no data type on statements, nothing else to do. */

        case SDL_SHADER_AST_STATEMENT_DO:
            scope = push_scope(ctx, ast);  /* push a scope here for possible `for (var int i = 0; ...` syntax */
            semantic_analysis_treewalk(ctx, ast->dostmt.condition);
            if (!ast_is_boolean(ast->dostmt.condition)) {
                fail_ast(ctx, &ast->binary.right->ast, "Datatype for do-loop condition must be boolean");
            }
            semantic_analysis_treewalk(ctx, ast->dostmt.code);
            pop_scope(ctx, scope);
            return;  /* no data type on statements, nothing else to do. */

        case SDL_SHADER_AST_STATEMENT_WHILE:
            scope = push_scope(ctx, ast);  /* push a scope here for possible `for (var int i = 0; ...` syntax */
            semantic_analysis_treewalk(ctx, ast->whilestmt.condition);
            if (!ast_is_boolean(ast->whilestmt.condition)) {
                fail_ast(ctx, &ast->binary.right->ast, "Datatype for while-loop condition must be boolean");
            }
            semantic_analysis_treewalk(ctx, ast->whilestmt.code);
            pop_scope(ctx, scope);
            return;  /* no data type on statements, nothing else to do. */

        case SDL_SHADER_AST_STATEMENT_FOR:
            scope = push_scope(ctx, ast);  /* push a scope here for possible `for (var int i = 0; ...` syntax */
            semantic_analysis_treewalk(ctx, ast->forstmt.details->initializer);
            semantic_analysis_treewalk(ctx, ast->forstmt.details->condition);
            semantic_analysis_treewalk(ctx, ast->forstmt.details->step);
            semantic_analysis_treewalk(ctx, ast->forstmt.code);
            pop_scope(ctx, scope);
            return;

        case SDL_SHADER_AST_STATEMENT_IF:
            semantic_analysis_treewalk(ctx, ast->ifstmt.condition);
            if (!ast_is_boolean(ast->ifstmt.condition)) {
                fail_ast(ctx, &ast->binary.right->ast, "Datatype for if-statement condition must be boolean");
            }
            semantic_analysis_treewalk(ctx, ast->ifstmt.code);
            if (ast->ifstmt.else_code != NULL) {
                semantic_analysis_treewalk(ctx, ast->ifstmt.code);
            }
            return;  /* no data type on statements, nothing else to do. */

        case SDL_SHADER_AST_STATEMENT_SWITCH: {
            SDL_SHADER_AstSwitchCase *default_case = NULL;
            scope = push_scope(ctx, ast);   /* apparently in C, you can declare variables in a case statement, and they are in scope for anything below it! Wild. */
            semantic_analysis_treewalk(ctx, ast->switchstmt.condition);
            if (!ast_is_integer(ast->switchstmt.condition)) {
                fail_ast(ctx, &ast->switchstmt.condition->ast, "Datatype for switch statement condition must be integer");
            } else {
                SDL_SHADER_AstSwitchCase *i;
                for (i = ast->switchstmt.cases->head; i != NULL; i = i->next) {
                    if (i->condition == NULL) {
                        if (default_case) {
                            fail_ast(ctx, &ast->ast, "Switch statement has multiple default cases");
                            fail_ast(ctx, &default_case->ast, "Previous default case is here");
                        }
                        default_case = i;
                    } else {
                        semantic_analysis_treewalk(ctx, i->condition);
                        if (!ast_is_integer(i->condition)) {
                            fail_ast(ctx, &i->condition->ast, "Datatype for switch case must be integer");
                        } else {
                            resolve_constant_int_from_ast_expression(ctx, i->condition, 0);
                        }
                        /* !!! FIXME: make sure it's not a duplicate */
                    }
                    semantic_analysis_treewalk(ctx, i->code);
                }
            }
            pop_scope(ctx, scope);
            return;  /* no data type on statements, nothing else to do. */
        }

        case SDL_SHADER_AST_STATEMENT_RETURN:
            if (ast->returnstmt.value) {
                semantic_analysis_treewalk(ctx, ast->returnstmt.value);
            }
            scope = find_parent_scope(ctx, SDL_SHADER_AST_FUNCTION);
            if (!scope) {
                fail_ast(ctx, &ast->ast, "Return statement outside of a function");  /* in theory, parsing shouldn't allow this...? */
            } else {
                SDL_SHADER_AstFunction *fn = &scope->ast->fn;
                if ((ast->returnstmt.value == NULL) && (fn->ast.dt != NULL)) {
                    fail_ast(ctx, &ast->ast, "Return statement with no value, but function does not return 'void'");
                } else if ((ast->returnstmt.value != NULL) && (fn->ast.dt == NULL)) {
                    fail_ast(ctx, &ast->ast, "Return statement with a value, but function returns 'void'");
                } else if ((ast->returnstmt.value != NULL) && !ast_datatypes_match(ast->returnstmt.value, fn)) {
                    fail_ast(ctx, &ast->ast, "Return statement value does not match function's datatype");
                }
                /* cheating here, assign the data type to this return statement node, even though statements don't _really_ have a datatype. */
                ast->ast.dt = fn->ast.dt;
            }
            return;

        case SDL_SHADER_AST_STATEMENT_BLOCK: {
            SDL_SHADER_AstStatement *i;
            scope = push_scope(ctx, ast);
            for (i = ast->stmtblock.head; i != NULL; i = i->next) {
                semantic_analysis_treewalk(ctx, i);
            }
            pop_scope(ctx, scope);
            return;
        }

        case SDL_SHADER_AST_STATEMENT_PREINCREMENT:
        case SDL_SHADER_AST_STATEMENT_POSTINCREMENT:
        case SDL_SHADER_AST_STATEMENT_PREDECREMENT:
        case SDL_SHADER_AST_STATEMENT_POSTDECREMENT:
            /* !!! FIXME: these should work with integer vectors and matrices, too */
            semantic_analysis_treewalk(ctx, ast->incrementstmt.assignment);
            if (!ast_is_lvalue(ctx, ast->incrementstmt.assignment)) {
                failf_ast(ctx, &ast->incrementstmt.assignment->ast, "Object for '%s' must be an lvalue", ast_opstr(asttype));
            } else if (!ast_is_number(ast->incrementstmt.assignment)) {
                failf_ast(ctx, &ast->unary.operand->ast, "Datatype for '%s' must be a number", ast_opstr(asttype));
            }
            return;

        case SDL_SHADER_AST_STATEMENT_FUNCTION_CALL:
            semantic_analysis_treewalk(ctx, ast->fncallstmt.expr);
            return;

        case SDL_SHADER_AST_STATEMENT_ASSIGNMENT:
            semantic_analysis_treewalk(ctx, ast->assignstmt.value);
            if (ast->assignstmt.assignments == NULL) {
                ICE(ctx, &ast->ast, "Assignment statement with nothing to assign to!");
            } else {
                SDL_SHADER_AstAssignment *i;
                for (i = ast->assignstmt.assignments->head; i != NULL; i = i->next) {
                    semantic_analysis_treewalk(ctx, i->expr);
                    if (!ast_is_lvalue(ctx, i->expr)) {
                        failf_ast(ctx, &i->expr->ast, "Object to left of '%s' must be an lvalue", ast_opstr(asttype));
                    } else if (!ast_datatypes_match(i->expr, ast->assignstmt.value)) {
                        failf_ast(ctx, &i->expr->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
                    }
                }
            }
            return;

        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNMUL:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNDIV:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNMOD:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNADD:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNSUB:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNLSHIFT:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNRSHIFT:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNAND:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNXOR:
        case SDL_SHADER_AST_STATEMENT_COMPOUNDASSIGNOR:
            semantic_analysis_treewalk(ctx, ast->compoundassignstmt.assignment);
            semantic_analysis_treewalk(ctx, ast->compoundassignstmt.value);
            if (!ast_is_lvalue(ctx, ast->compoundassignstmt.assignment)) {
                failf_ast(ctx, &ast->compoundassignstmt.assignment->ast, "Object to left of '%s' must be an lvalue", ast_opstr(asttype));
            } else if (!ast_datatypes_match(ast->compoundassignstmt.assignment, ast->compoundassignstmt.value)) {
                failf_ast(ctx, &ast->ast, "Datatypes must match with the '%s' operator", ast_opstr(asttype));
            }
            return;

        case SDL_SHADER_AST_FUNCTION:
            scope = push_scope(ctx, ast);
            /* we already resolved the return value datatype in semantic_analysis_prepare_functions() */
            semantic_analysis_validate_function_at_attribute(ctx, &ast->fn);
            if (ast->fn.params != NULL) {  /* NULL here means "void" */
                SDL_SHADER_AstFunctionParam *i;
                for (i = ast->fn.params->head; i != NULL; i = i->next) {
                    semantic_analysis_treewalk(ctx, i);
                }
            }
            semantic_analysis_treewalk(ctx, ast->fn.code);
            pop_scope(ctx, scope);
            return;

        case SDL_SHADER_AST_FUNCTION_PARAM:
            /* we already resolved the datatype, so don't do that here. */
            semantic_analysis_validate_function_param_at_attribute(ctx, &ast->fnparam);
            push_scope(ctx, ast);  /* add this to the scope stack; it will pop when the function leaves scope. */
            return;

        case SDL_SHADER_AST_VARIABLE_DECLARATION:
            // !!! FIXME: need array declaration
            /* !!! FIXME: warn if this shadows a function parameter */
            /* !!! FIXME: fail if there is already a variable in this scope with the same name. */
            ast->ast.dt = resolve_datatype(ctx, &ast->ast, ast->vardecl.datatype_name);
            if (ast->vardecl.initializer != NULL) {
                semantic_analysis_treewalk(ctx, ast->vardecl.initializer);
                if (!ast_datatypes_match(ast, ast->vardecl.initializer)) {
                    failf_ast(ctx, &ast->ast, "Datatypes must match between a variable declaration and its initializer");
                }
            }
            /* note that this adds itself to the scope _after_ walking the initializer, so it'll be an error if
               if the initializer attempts to reference the currently-uninitialized value.
               (or at least it'll look for an initialized identifier of the same name higher up the scope stack! */
            push_scope(ctx, ast);  /* add this to the scope stack; it will pop when the function leaves scope. */
            return;

        case SDL_SHADER_AST_TRANSUNIT_FUNCTION:  /* just walk further into the contained AST node */
            semantic_analysis_treewalk(ctx, ast->fnunit.fn);
            return;

        case SDL_SHADER_AST_TRANSUNIT_STRUCT:  /* just walk further into the contained AST node */
            semantic_analysis_treewalk(ctx, ast->structdeclunit.decl);
            return;

        case SDL_SHADER_AST_SHADER: {
            /* shaders don't get a datatype, but they need to walk the tree to resolve everything else. */
            SDL_SHADER_AstTranslationUnit *i;
            scope = push_scope(ctx, ast);
            for (i = ast->shader.units->head; i != NULL; i = i->next) {
                semantic_analysis_treewalk(ctx, i);
            }
            pop_scope(ctx, scope);
            return;
        }

        case SDL_SHADER_AST_STRUCT_DECLARATION: /* we handled these in semantic_analysis_gather_datatypes, etc */
            ICE_IF(ctx, &ast->ast, !ctx->scope_stack || (ctx->scope_stack->ast->ast.type != SDL_SHADER_AST_SHADER), "Unexpected struct declaration!");
            return;  /* if we start to allow struct declarations outside of global scope, this will need to do something. */

        case SDL_SHADER_AST_SWITCH_CASE:  /* handled with SDL_SHADER_AST_STATEMENT_SWITCH */
        case SDL_SHADER_AST_STRUCT_MEMBER: /* we handled these in semantic_analysis_gather_datatypes, etc */
        case SDL_SHADER_AST_AT_ATTRIBUTE:  /* we don't (currently) do anything here. Specific AST nodes need to validate their params. */
        default:
            ICE(ctx, &ast->ast, "Unexpected AST node type");
            return;
    }
}

static void semantic_analysis(Context *ctx, const SDL_SHADER_CompilerParams *params)
{
    ICE_IF(ctx, &ctx->ast_before, ctx->isfail, "Went on to semantic analysis even though parsing had failed!");

    if (!ctx->shader || !ctx->shader->units || !ctx->shader->units->head) {
        fail_ast(ctx, &ctx->ast_after, "Shader is empty?");
        return;
    }

    semantic_analysis_build_globals_lists(ctx);
    semantic_analysis_check_globals_for_duplicates(ctx);
    semantic_analysis_gather_datatypes(ctx);
    semantic_analysis_prepare_functions(ctx);
    semantic_analysis_treewalk(ctx, ctx->shader);

    ICE_IF(ctx, &ctx->ast_after, ctx->scope_stack != NULL, "Scope stack isn't empty!");
}

static void datatypes_nuke(const void *key, const void *value, void *data)
{
    Context *ctx = (Context *) data;
    DataType *dt = (DataType *) value;

    /* don't free `key` here, it's from ctx->strcache. */

    if (dt->dtype == DT_STRUCT) {
        Free(ctx, (void *) dt->info.structure.members);  /* just free the array, not the datatypes; they'll be elsewhere in the hash. */
    }

    Free(ctx, dt);
}

/* since these keys are strcache'd, you can just compare the pointers instead of the contents. */
int hash_keymatch_datatypes(const void *a, const void *b, void *data)
{
    (void) data;
    return a == b;
}


void compiler_end(Context *ctx)
{
    ScopeItem *scope;
    ScopeItem *scopenext;

    if (!ctx || !ctx->uses_compiler) {
        return;
    }

    hash_destroy(ctx->datatypes);

    for (scope = ctx->scope_stack; scope != NULL; scope = scopenext) {
        scopenext = scope->next;
        Free(ctx, scope);
    }

    for (scope = ctx->scope_pool; scope != NULL; scope = scopenext) {
        scopenext = scope->next;
        Free(ctx, scope);
    }

    ctx->uses_compiler = SDL_FALSE;
}


static const SDL_SHADER_CompileData SDL_SHADER_out_of_mem_data_compile = {
    1, &SDL_SHADER_out_of_mem_error, NULL, NULL, 0, NULL, NULL, NULL
};

static const SDL_SHADER_CompileData *build_compiledata(Context *ctx)
{
    SDL_SHADER_CompileData *retval = NULL;

    if (ctx->out_of_memory) {
        return &SDL_SHADER_out_of_mem_data_compile;
    }

    retval = (SDL_SHADER_CompileData *) Malloc(ctx, sizeof (SDL_SHADER_CompileData));
    if (retval == NULL) {
        return &SDL_SHADER_out_of_mem_data_compile;
    }

    SDL_zerop(retval);
    retval->malloc = (ctx->malloc == SDL_SHADER_internal_malloc) ? NULL : ctx->malloc;
    retval->free = (ctx->free == SDL_SHADER_internal_free) ? NULL : ctx->free;
    retval->malloc_data = ctx->malloc_data;
    retval->error_count = errorlist_count(ctx->errors);
    retval->errors = errorlist_flatten(ctx->errors);

    if (ctx->out_of_memory) {
        Free(ctx, retval);
        return &SDL_SHADER_out_of_mem_data_compile;
    }

    if (!ctx->isfail) {
        retval->source_profile = ctx->source_profile;
        retval->output = ctx->compile_output;
        retval->output_len = ctx->compile_output_len;
        ctx->compile_output = NULL;  /* owned by retval now. Null out so we don't free it. */
        ctx->compile_output_len = 0;
    }

    return retval;
}


/* API entry point... */

const SDL_SHADER_CompileData *SDL_SHADER_Compile(const SDL_SHADER_CompilerParams *params)
{
    const SDL_SHADER_CompileData *retval;
    Context *ctx;

    ctx = parse_to_ast(params);
    if (ctx == NULL) {
        return &SDL_SHADER_out_of_mem_data_compile;
    }

    if (!ctx->isfail) {
        ctx->uses_compiler = SDL_TRUE;
        ctx->ast_before.type = ctx->ast_after.type = SDL_SHADER_AST_SHADER;
        ctx->ast_before.filename = ctx->ast_after.filename = stringcache(ctx->strcache, params->filename);
        ctx->ast_before.dt = ctx->ast_after.dt = NULL;
        ctx->ast_before.line = SDL_SHADER_POSITION_BEFORE;
        ctx->ast_after.line = SDL_SHADER_POSITION_AFTER;
        ctx->datatypes = hash_create(ctx, hash_hash_string, hash_keymatch_datatypes, datatypes_nuke, SDL_FALSE, MallocContextBridge, FreeContextBridge, ctx);
        ctx->scope_stack = NULL;
        ctx->scope_pool = NULL;
    }

    if (!ctx->isfail) {
        semantic_analysis(ctx, params);
    }

    retval = build_compiledata(ctx);
    SDL_assert(retval != NULL);  /* should never return NULL, even if out of memory! */

    context_destroy(ctx);

    return retval;
}

void SDL_SHADER_FreeCompileData(const SDL_SHADER_CompileData *_data)
{
    SDL_SHADER_CompileData *data = (SDL_SHADER_CompileData *) _data;
    if ((data != NULL) && (data != &SDL_SHADER_out_of_mem_data_compile)) {
        SDL_SHADER_Free f = (data->free == NULL) ? SDL_SHADER_internal_free : data->free;
        void *d = data->malloc_data;
        int i;

        /* we don't f(data->source_profile), because that's internal static data. */

        for (i = 0; i < data->error_count; i++) {
            f((void *) data->errors[i].message, d);
            f((void *) data->errors[i].filename, d);
        }

        f((void *) data->errors, d);
        f((void *) data->output, d);
        f(data, d);
    }
}

/* end of SDL_shader_compiler.c ... */

