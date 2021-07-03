#include <stdio.h>
#include <stdlib.h>

// #include with "" instead of <> searches local folder first
#include "mpc.h"  // micro parser combinator lib

// Preprocessing directive below checks if the _WIN32 macro is defined, meaning we are on Windows
// There exist other similar predefined macros for other OS like __linux, __APPLE__ or __ANDROID__
#ifdef _WIN32

// Windows requires no libedit since the default behaviour on the terminal gives those features
// Instead we provide fake substitutes
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
	// Output prompt
	fputs(prompt, stdout);
	
	// Read the user input into the buffer
	fgets(buffer, 2048, stdin);
	
	// Copy the buffer and null-terminate it to make it into a string
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	
	return cpy;
}

void add_history(char* unused) {}

#else

// On Linux & Mac we use libedit for line edition and history on input
#include <editline/readline.h>
#include <editline/history.h>

#endif

// Lsp value, either a number or an error
typedef struct {
	int type;
	long num;
	int err;
} lval;

// Valid lval types
enum {
	LVAL_NUM,
	LVAL_ERR
};

// Valid lval errors
enum {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM
};

// Creates a number lval
lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

// Creates an error lval
lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

// Print an lval, handling all possible cases
void lval_print(lval v) {
	switch (v.type) {
		case LVAL_NUM:
			printf("%li", v.num); break;
		case LVAL_ERR:
			if (v.err == LERR_DIV_ZERO) {
				printf("Error: Division by zero");
			}
			if (v.err == LERR_BAD_OP) {
				printf("Error: Invalid operator");
			}
			if (v.err == LERR_BAD_NUM) {
				printf("Error: Invalid number");
			}
			break;
	}
}

// Print an lval, followed by a newline
void lval_println(lval v) {
	lval_print(v);
	putchar('\n');
}

lval eval_op(lval x, char* op, lval y) {
	
	// If either lval is an error, return it
	if (x.type == LVAL_ERR) { return x; }
	if (x.type == LVAL_ERR) { return x; }
	
	// Otherwise, evaluate the operators
	if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
	if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
	if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
	if (strcmp(op, "/") == 0) {
		// Return error on division by 0
		return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
	}
	
	// If operator is not recognized return error
	return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {

	// If tagged a number, evaluate to int directly
	if (strstr(t->tag, "number")) {
		// Error if conversion fails
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}
	
	// Operator is always the second child
	char* op = t->children[1]->contents;
	
	// Store third child in x
	lval x = eval(t->children[2]);
	
	// Iterate remaining children and combine result
	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	
	return x;
}


int main(int argc, char** argv) {
	
	// Declare parsers
	mpc_parser_t* Number      = mpc_new("number");
	mpc_parser_t* Operator    = mpc_new("operator");
	mpc_parser_t* Expr        = mpc_new("expr");
	mpc_parser_t* Lsp         = mpc_new("lsp");

	// Define them
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                  \
		number   : /-?[0-9]+/ ;                            \
		operator : '+' | '-' | '*' | '/' ;                 \
		expr     : <number> | '(' <operator> <expr>+ ')' ; \
		lsp      : /^/ <operator> <expr>+ /$/ ;            \
		",
		Number, Operator, Expr, Lsp);

	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.3");
	puts("Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		
		char* input = readline("lsp> ");
		add_history(input);
		
		// Attempt to parse user input
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lsp, &r)) {
			// On success evaluate AST and print result
			lval result = eval(r.output);
			lval_println(result);
			mpc_ast_delete(r.output);
		} else {
			// Otherwise print error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	// Undefine and delete our parsers
	mpc_cleanup(4, Number, Operator, Expr, Lsp);
	
	return 0;
	
}
