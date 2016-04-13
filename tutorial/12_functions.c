#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

/* If we are compilin on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/**********************************
 * lval stucture and constructors *
 **********************************/
 
/* create enumeration of possible lval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

/* Function pointer to environment */
typedef lval*(*lbuiltin)(lenv*, lval*);

lenv* lenv_new(void);
void lenv_del(lenv* e);
lenv* lenv_copy(lenv* e);

/* Declare lval Struct */
typedef struct lval {
    int type;
    
    /* Basic */
    long num;
    char* err;
    char* sym;
    
    /* Function */
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;
    
    /* Expression */
    int count;
    lval** cell; 
} lval;

/* Create a pointer to a new number type lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Create a pointer to new error type lval */
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    
    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);
    
    /* Allocate 512 bytes of space */
    v->err = malloc(512);
    
    /*printf the error string with a maximum of 511 chars*/
    vsnprintf(v->err, 511, fmt, va);
    
    /* Cleanup or va list */
    va_end(va);
    
    return v;
}

/* Create a pointer to new Symbol type lval */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* Create a pointer to a new empty S-expression lval */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Create a pointer to a new empty q-expression lval */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Create a pointer to a new lbuiltin function */
lval* lval_fun(lbuiltin lbuiltin) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = lbuiltin;
    return v;
}

/* Create a pointer to a new lval lambda function */
lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    
    /* set builtin to NULL */
    v->builtin = NULL;
    
    /* Build a new environment */
    v->env = lenv_new();
    
    /* Set Formals and body */
    v->formals = formals;
    v->body = body;
    
    return v;
}

/* Deconstructor for lval* */
void lval_del(lval* v) {

    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_FUN: 
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
        break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }   
    free(v);
}

/*************************************************************
 * Reading an S-expression and creating the AST using lval's *
 *************************************************************/
 
/* Add an lval to an lval* */
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count); /* adding more space onto the lval* array on the heap, for another s-expression (lval) */
    v->cell[v->count-1] = x; /* now put that s-expression on that slot */
    return v;
}

/* read and catch num errors */
lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

/* recursive function going through AST to find S-expressions and adding to lval* tree structures */
lval* lval_read(mpc_ast_t* t) {
    /* If the Symbol or Number return conversion to that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
    
    /* If root (>) or sexpr then create empty list */
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
    
    /* Fill this list with any valid expression contained withing */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    
    return x;
}

/* copying an lval */
lval* lval_copy(lval* v) {

    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    
    switch (v->type) {
        /* Copy Functions and Numbers Directly */
        case LVAL_FUN: 
            if (v->builtin) {
                x->builtin = v->builtin; 
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
        break;
        case LVAL_NUM: x->num = v->num; break;
        
        /* copy strings using malloc and strcpy */
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;
    
        /* Copy Lists by copying each sub-expression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;
    }
    return x;    
}

/********************
 * Environment lenv *
 ********************/

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
} 

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    /* Iterate over all items in envriroment */
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    
    /* If no symbol check in parent otherwise error */
    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("Unbound Symbol! '%s'", k->sym);
    }
}

void lenv_put(lenv* e, lval* k, lval* v) {
    /* Iterate to see if varible exists in envrinoment */
    for (int i = 0; i < e->count; i++) {
        /* if exissts delete varable and replace with new one */
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    /* If no entry found allocate space for ne entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    
    /* copy contents of lval and symbol string into new location */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
    /* Iterate till e has no parent */
    while (e->par) {
        e = e->par;
    }
    /* put the value in e */
    lenv_put(e, k, v);
}

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}
    
/**********************************
 * Printing Expressions functions *
 **********************************/

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        /* Print Value contained withing */
        lval_print(v->cell[i]);
        
        /* Dont print trailling space if last element */
        if (i != (v->count-1)) {
            putchar(' ');
        } 
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM:      printf("%li", v->num);          break;
        case LVAL_ERR:      printf("Error: %s", v->err);    break;
        case LVAL_SYM:      printf("%s", v->sym);           break;
        case LVAL_SEXPR:    lval_expr_print(v, '(', ')');   break;
        case LVAL_QEXPR:    lval_expr_print(v, '{', '}');   break;
        case LVAL_FUN:
            if (v->builtin) {
                printf("<function>");           
            } else {
                printf("(\\ "); 
                lval_print(v->formals);
                putchar(' '); 
                lval_print(v->body); 
                putchar(')');
            }
        break;      
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}




/**************************
 * Evaluating Expressions *
 **************************/
lval* lval_eval(lenv* e, lval* v);

lval* builtin_op(lenv* e, lval* a, char* op);

lval* lval_pop(lval* v, int i) {
    /* Find the item at 'i' */
    lval* x = v->cell[i];

    /* Shift memory after the item at 'i' over the top */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    /* Decrese the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;    
}

lval* lval_take(lval* v, int i){
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_call(lenv* e, lval* f, lval* a);

lval* lval_eval_sexpr(lenv* e, lval* v) {
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    /* Empty Expression */
    if (v->count == 0) { return v; }

    /* Single Expression */
    if (v->count == 1) { return lval_take(v, 0); }

    /* Ensure First Element is Symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
              "S-Expression starts with incorrect type. "
              "Got %s, Expected %s.",
              ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f); lval_del(v);
        return err;
    }

    /* If so call function to get result */
    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;    
}

/* edit to evaluate using the lenv */
 
lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }
    return v;
}

lval* lval_join(lval* x, lval* y) {
    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }
    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;        
}

/*********************
 * Functions calling *
 *********************/
lval* builtin_eval(lenv* e, lval* a);

lval* lval_call(lenv* e, lval* f, lval* a) {
    /* If Builtin then simply apply that */
    if (f->builtin) {
        return f->builtin(e, a);
    }
    
    /* Record Argument Counts */
    int given = a->count;
    int total = f->formals->count;
    
    /* while arguments still remain to be processed */
    while (a->count) {
        /* if we ran out of formal arguments to bind */
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function passed too many arguments. Got %i, Expected %i.", given, total);
        }

        /* pop the first symbol from the formals */
        lval* sym = lval_pop(f->formals, 0);
        /* Pop tje next argument from the list */
        lval* val = lval_pop(a, 0);
        /* Bind a copy into the function's environment */
        lenv_put(f->env, sym, val);
        /* Delete symbol and value */
        lval_del(sym);
        lval_del(val);
    }
    
    /* Argument list is now bound so can be cleaned up */
    lval_del(a);
    
    /* if all formals have been bound evaluate */
    if (f->formals->count == 0) {
        /* Set environment parent to evaluation environment */
        f->env->par = e;
        /* Evaluate and return */
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        /* Otherwise return partially evaluated function */
        return lval_copy(f);
    }
} 
 
/*********************
 * builtin functions *
 *********************/

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }
#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);

lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);
    lval* v = lval_take(a, 0);
    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}
 
lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);                 
} 
 
lval* builtin_join(lenv* e, lval* a) {
    for(int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }
    lval* x = lval_pop(a, 0);
    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);
    
    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.", func,
            ltype_name(syms->cell[i]->type),
            ltype_name(LVAL_SYM));
    }
    
    LASSERT(a, (syms->count == a->count - 1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count-1);
    
    for (int i = 0; i < syms->count; i++) {
        /* If 'def' define in globally. If 'put' define in locally */
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i+1]);
        }
        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i+1]);
        }
    }
    
    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* builtin_lambda(lenv* e, lval* a) {
    /* Check Two arguments, each of which are Q-Expressions */
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);    
    
    /* Check first Q-Expression contains only Symbols */
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }
    
    /* Pop first two arguments and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);
    
    return lval_lambda(formals, body);
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE(op, a, i, LVAL_NUM);
    }
    
    /* Pop the first element */
    lval* x = lval_pop(a, 0);
    
    /* If no arguments and sub then perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }
    
    /* Pop the next element */
    while (a->count > 0) {
        /* Pop the next element */
        lval* y = lval_pop(a, 0);
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
              if (y->num == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division By Zero!"); break;
              }
              x->num /= y->num;
        }
        lval_del(y);
    }
    
    lval_del(a);
    return x;
} 

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}



void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    
    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    
    /* Variable Runctions */
    lenv_add_builtin(e, "\\",  builtin_lambda); 
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
}


int main(int argc, char** argv) {
    
    /* Create Some Parsers */
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol    = mpc_new("symbol");
    mpc_parser_t* Sexpr     = mpc_new("sexpr");
    mpc_parser_t* Qexpr     = mpc_new("qexpr");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");
    
    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
         number     : /-?[0-9]+/ ;                              \
         symbol     : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;         \
         sexpr      : '(' <expr>* ')';                          \
         qexpr      : '{' <expr>* '}';                          \
         expr       : <number> | <symbol> | <sexpr> | <qexpr> ; \
         lispy      : /^/ <expr>* /$/ ;                         \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy); 
    
    /* print version and exit information */
    puts("Lispy Version 0.0.0.0.6");
    puts("Press Ctrl+c to Exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);
    
    /* In a never ending loop */
    while (1) {
        
        /* Output our prompt and get input */
        char* input = readline("lispy> ");
        
        /* Add input to history */
        add_history(input);
        
        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
             /*Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        /* Free retrieved input */
        free(input);
    }
    
    /* delete environment */
    lenv_del(e);
    
    /* Undefine and delte our parsers */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    
    return 0;
}

