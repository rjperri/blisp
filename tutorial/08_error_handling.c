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

/* create Enumeration of possible Error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* create enumeration of possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* Declare lval Struct */
typedef struct {
    int type;
    long num;
    int err;
} lval;

/* Create a new number type lval */
lval lval_err(long x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

/* Create a new number type lval */
lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

/* Print an lval */
void lval_print(lval v) {
    switch (v.type) {
        /* In the case the type is a number print it */
        /* Then 'break' out of the switch */
        case LVAL_NUM: printf("%li", v.num); break;
        
        /* In the case the type is an error */
        case LVAL_ERR:
            /* Check what type of error it is and print it */
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: Division By Zero!"); 
            }
            if (v.err == LERR_BAD_OP) {
                printf("Error: Invalid Operator!"); 
            }
            if (v.err == LERR_BAD_NUM) {
                printf("Error: Invalid Number!"); 
            }
        break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

/* Use Operator string to see which operation to use */
lval eval_op(lval x, char* op, lval y) {

    /* If either value is an error return it */
    if (x.type == LVAL_ERR) {return x;}
    if (y.type == LVAL_ERR) {return y;}
    
    /* Otherwise do maths on the number values */
    if (strcmp(op, "+") == 0) {return lval_num(x.num + y.num);}
    if (strcmp(op, "-") == 0) {return lval_num(x.num - y.num);}
    if (strcmp(op, "*") == 0) {return lval_num(x.num * y.num);}
    if (strcmp(op, "/") == 0) {
        /* If second operand is zero return error */
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }
    if (strcmp(op, "%") == 0) {
        /* If second operand is zero return error */
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num % y.num);
    }
    if (strcmp(op, "^") == 0) {
        lval result = lval_num(1);
        for (int i = 0; i < y.num; i++) {result.num *= x.num;}
        return result;
    }
    
    return lval_err(LERR_BAD_OP); 
}

lval eval(mpc_ast_t* t) {
    
    if (strstr(t->tag, "number")) {
        /* Check if there is some error in conversion */
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }
    
    char* op = t->children[1]->contents;
    lval x = eval(t->children[2]);
    
    /* Iterate the remaining children and combining */
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;    
}

int main(int argc, char** argv) {
    
    /* Create Some Parsers */
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Operator  = mpc_new("operator");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");
    
    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
         number     : /-?[0-9]+/ ;                              \
         operator   : '+' | '-' | '*' | '/' | '%' | '^';        \
         expr       : <number> | '(' <operator> <expr>+ ')' ;   \
         lispy      : /^/ <operator> <expr>+ /$/ ;              \
        ",
        Number, Operator, Expr, Lispy); 
    
    /* print version and exit information */
    puts("Lispy Version 0.0.0.0.4");
    puts("Press Ctrl+c to Exit\n");
    
    /* In a never ending loop */
    while (1) {
        
        /* Output our prompt and get input */
        char* input = readline("lispy> ");
        
        /* Add input to history */
        add_history(input);
        
        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        /* Free retrieved input */
        free(input);
    }
    
    /* Undefine and delte our parsers */
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    
    return 0;
}

