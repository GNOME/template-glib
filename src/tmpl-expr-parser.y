%define api.pure
%define parse.error verbose
%name-prefix "tmpl_expr_parser_"
%defines
%parse-param { TmplExprParser *parser }
%lex-param { void *scanner }

%{
# include <glib/gprintf.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "tmpl-expr.h"
# include "tmpl-expr-private.h"
# include "tmpl-expr-parser-private.h"

#pragma GCC diagnostic ignored "-Wswitch-default"
%}

%union {
  TmplExpr *a;         /* ast node */
  double d;            /* number */
  char *s;             /* symbol/string */
  GPtrArray *sl;       /* symlist */
  TmplExprBuiltin fn;  /* builtin call */
  int b;               /* boolean */
  int cmp;             /* comparison */
  int boolcmp;         /* boolean comparison */
}

%{
int tmpl_expr_parser_lex (YYSTYPE *, void *scanner);

void
tmpl_expr_parser_error (TmplExprParser *parser,
                        const gchar    *message)
{
  g_assert (parser != NULL);
  g_assert (message != NULL);

  g_clear_pointer (&parser->ast, tmpl_expr_unref);

  g_free (parser->error_str);
  parser->error_str = g_strdup (message);
}

static void
add_expr_to_parser (TmplExprParser *parser,
                    TmplExpr       *node)
{
  if (parser->ast != NULL)
    {
      if (parser->ast->any.type != TMPL_EXPR_STMT_LIST)
        {
          GPtrArray *ar = g_ptr_array_new_with_free_func ((GDestroyNotify)tmpl_expr_unref);
          g_ptr_array_add (ar, parser->ast);
          g_ptr_array_add (ar, node);
          parser->ast = tmpl_expr_new_stmt_list (ar);
        }
      else
        {
          g_ptr_array_add (parser->ast->stmt_list.stmts, node);
        }
    }
  else
    {
      parser->ast = node;
    }
}

static TmplExpr *
create_function (char      *name,
                 GPtrArray *symlist,
                 TmplExpr  *list)
{
  char **strv = NULL;

  if (symlist != NULL)
    {
      g_ptr_array_add (symlist, NULL);
      strv = (char **)(gpointer)g_ptr_array_free (symlist, FALSE);
    }

  return tmpl_expr_new_func (name, strv, list);
}

static void
define_function (TmplExprParser *parser,
                 char           *name,
                 GPtrArray      *symlist,
                 TmplExpr       *list)
{
  add_expr_to_parser (parser, create_function (name, symlist, list));
}

static TmplExpr *
add_to_list (TmplExpr *stmt,
             TmplExpr *list)
{
  g_assert (stmt != NULL);

  if (list == NULL)
    {
      GPtrArray *ar = g_ptr_array_new_with_free_func ((GDestroyNotify)tmpl_expr_unref);
      g_ptr_array_add (ar, stmt);
      return tmpl_expr_new_stmt_list (ar);
    }

  g_assert (list->any.type == TMPL_EXPR_STMT_LIST);
  g_ptr_array_insert (list->stmt_list.stmts, 0, stmt);
  return list;
}

# define scanner parser->scanner
%}

%token <b> BOOL
%token CONSTANT_NULL
%token <d> NUMBER
%token <s> NAME STRING_LITERAL
%token <fn> BUILTIN
%token <s> REQUIRE VERSION
%token EOL
%token DEF END
%token IF THEN ELSE WHILE DO FUNC
%token NOP

%left <cmp> CMP
%left <boolcmp> BOOLCMP
%right '='
%left '+' '-'
%left '*' '/'

%nonassoc '|' UMINUS

%type <a> exp stmt list explist
%type <sl> symlist

%start expr

%%

expr: /* nothing */ EOL {
    YYACCEPT;
  }
  | stmt EOL {
    add_expr_to_parser (parser, $1);
    YYACCEPT;
  }
  | stmt ';' {
    add_expr_to_parser (parser, $1);
    YYACCEPT;
  }

  | DEF NAME '(' ')' EOL list END {
    define_function (parser, $2, NULL, $6);
    YYACCEPT;
  }
  | DEF NAME '(' ')' list END {
    define_function (parser, $2, NULL, $5);
    YYACCEPT;
  }
  | DEF NAME '(' ')' EOL END {
    define_function (parser, $2, NULL, NULL);
    YYACCEPT;
  }

  | DEF NAME '(' symlist ')' EOL list END {
    define_function (parser, $2, $4, $7);
    YYACCEPT;
  }
  | DEF NAME '(' symlist ')' list END {
    define_function (parser, $2, $4, $6);
    YYACCEPT;
  }
  | DEF NAME '(' symlist ')' EOL END {
    define_function (parser, $2, $4, NULL);
    YYACCEPT;
  }
;

stmt: IF exp THEN list {
    $$ = tmpl_expr_new_flow (TMPL_EXPR_IF, $2, $4, NULL);
  }
  | IF exp THEN list ELSE list {
    $$ = tmpl_expr_new_flow (TMPL_EXPR_IF, $2, $4, $6);
  }
  | WHILE exp DO list {
    $$ = tmpl_expr_new_flow (TMPL_EXPR_WHILE, $2, $4, NULL);
  }
  | exp
;

list: /* nothing */ { $$ = NULL; }
  | stmt ';' list {
    $$ = add_to_list ($1, $3);
  }
  | stmt EOL list {
    $$ = add_to_list ($1, $3);
  }
;

exp: exp CMP exp {
    $$ = tmpl_expr_new_simple ($2, $1, $3);
  }
  | exp BOOLCMP exp {
    $$ = tmpl_expr_new_simple ($2, $1, $3);
  }
  | exp '+' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_ADD, $1, $3);
  }
  | exp '-' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_SUB, $1, $3);
  }
  | exp '*' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_MUL, $1, $3);
  }
  | exp '/' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_DIV, $1, $3);
  }
  | '(' exp ')' {
    $$ = $2;
  }
  | '-' exp %prec UMINUS {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_UNARY_MINUS, $2, NULL);
  }
  | NUMBER {
    $$ = tmpl_expr_new_number ($1);
  }
  | BOOL {
    $$ = tmpl_expr_new_boolean ($1);
  }
  | CONSTANT_NULL {
    $$ = tmpl_expr_new_null ();
  }
  | STRING_LITERAL {
    $$ = tmpl_expr_new_string ($1, -1);
    g_free ($1);
  }
  | NAME {
    $$ = tmpl_expr_new_symbol_ref ($1);
    g_free ($1);
  }
  | NAME '=' exp {
    $$ = tmpl_expr_new_symbol_assign ($1, $3);
    g_free ($1);
  }
  | BUILTIN '(' explist ')' {
    $$ = tmpl_expr_new_fn_call ($1, $3);
  }
  | NAME '(' explist ')' {
    $$ = tmpl_expr_new_user_fn_call ($1, $3);
    g_free ($1);
  }
  | NAME '(' ')' {
    $$ = tmpl_expr_new_user_fn_call ($1, NULL);
    g_free ($1);
  }
  | exp '.' NAME '(' ')' {
    $$ = tmpl_expr_new_gi_call ($1, $3, NULL);
    g_free ($3);
  }
  | exp '.' NAME '(' explist ')' {
    $$ = tmpl_expr_new_gi_call ($1, $3, $5);
    g_free ($3);
  }
  | exp '.' NAME {
    $$ = tmpl_expr_new_getattr ($1, $3);
    g_free ($3);
  }
  | exp '.' NAME '=' exp {
    $$ = tmpl_expr_new_setattr ($1, $3, $5);
    g_free ($3);
  }
  | exp '(' ')' {
    $$ = tmpl_expr_new_anon_call ($1, NULL);
  }
  | exp '(' explist ')' {
    $$ = tmpl_expr_new_anon_call ($1, $3);
  }
  | '!' exp {
    $$ = tmpl_expr_new_invert_boolean ($2);
  }
  | NOP {
    $$ = tmpl_expr_new_nop ();
  }
  | REQUIRE NAME {
    $$ = tmpl_expr_new_require ($2, NULL);
    g_free ($2);
  }
  | REQUIRE NAME VERSION STRING_LITERAL {
    $$ = tmpl_expr_new_require ($2, $4);
    g_free ($2);
    g_free ($4);
  }
  | FUNC '(' ')' stmt {
    $$ = create_function (NULL, NULL, $4);
  }
  | FUNC '(' symlist ')' stmt {
    $$ = create_function (NULL, $3, $5);
  }
;

explist: exp
  | exp ',' explist {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_ARGS, $1, $3);
  }
;

symlist: NAME {
    $$ = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add ($$, $1);
  }
  | NAME ',' symlist {
    $$ = $3;
    g_ptr_array_insert ($$, 0, $1);
  }
;

