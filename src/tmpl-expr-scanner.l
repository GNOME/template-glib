%option reentrant
%option prefix="tmpl_expr_parser_"
%option bison-bridge
%option noyywrap
%option yylineno
%option nodefault

%option header-file="tmpl-expr-scanner.h"
%option outfile="tmpl-expr-scanner.c"

%{
# include "tmpl-error.h"
# include "tmpl-expr-private.h"
# include "tmpl-expr-parser-private.h"
# include "tmpl-expr-parser.h"

#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wunused-function"

static char *
copy_literal (const char *str)
{
  gsize len;
  if (str[0] == 'L')
    str++;
  g_assert (str[0] == '\"');
  str++;
  len = strlen (str);
  g_assert (len > 0);
  g_assert (str[len-1] == '\"');
  return g_strndup (str, len - 1);
}

%}

%option extra-type="TmplExprParser *"

 /* float exponent */
EXP ([Ee][-+]?[0-9]+)

%%

%{
TmplExprParser *parser = yyextra;
%}

 /* single character ops */
"+" |
"-" |
"*" |
"/" |
"=" |
"," |
"." |
"!" |
";" |
"(" |
")" { return yytext [0]; }

 /* comparison ops */
">"  { yylval->cmp = TMPL_EXPR_GT;  return CMP; }
"<"  { yylval->cmp = TMPL_EXPR_LT;  return CMP; }
"!=" { yylval->cmp = TMPL_EXPR_NE;  return CMP; }
"==" { yylval->cmp = TMPL_EXPR_EQ;  return CMP; }
">=" { yylval->cmp = TMPL_EXPR_GTE; return CMP; }
"<=" { yylval->cmp = TMPL_EXPR_LTE; return CMP; }

 /* boolean ops */
"||" { yylval->boolcmp = TMPL_EXPR_OR; return BOOLCMP; }
"&&" { yylval->boolcmp = TMPL_EXPR_AND; return BOOLCMP; }

 /* keywords */
"if"       { return IF; }
"then"     { return THEN; }
"else"     { return ELSE; }
"while"    { return WHILE; }
"do"       { return DO; }
"func"     { return FUNC; }
"def"      { return DEF; }
"end"      { return END; }
"require"  { return REQUIRE; }
"version"  { return VERSION; }
"pass"     { return NOP; }

 /* booleans */
"true"     { yylval->b = 1; return BOOL; }
"false"    { yylval->b = 0; return BOOL; }
"True"     { yylval->b = 1; return BOOL; }
"False"    { yylval->b = 0; return BOOL; }
"TRUE"     { yylval->b = 1; return BOOL; }
"FALSE"    { yylval->b = 0; return BOOL; }

 /* constants */
"null"     { return CONSTANT_NULL; }

 /* builtin functions */
"assert"   { yylval->fn = TMPL_EXPR_BUILTIN_ASSERT; return BUILTIN; }
"abs"      { yylval->fn = TMPL_EXPR_BUILTIN_ABS; return BUILTIN; }
"ceil"     { yylval->fn = TMPL_EXPR_BUILTIN_CEIL; return BUILTIN; }
"floor"    { yylval->fn = TMPL_EXPR_BUILTIN_FLOOR; return BUILTIN; }
"hex"      { yylval->fn = TMPL_EXPR_BUILTIN_HEX; return BUILTIN; }
"log"      { yylval->fn = TMPL_EXPR_BUILTIN_LOG; return BUILTIN; }
"print"    { yylval->fn = TMPL_EXPR_BUILTIN_PRINT; return BUILTIN; }
"printerr" { yylval->fn = TMPL_EXPR_BUILTIN_PRINTERR; return BUILTIN; }
"repr"     { yylval->fn = TMPL_EXPR_BUILTIN_REPR; return BUILTIN; }
"sqrt"     { yylval->fn = TMPL_EXPR_BUILTIN_SQRT; return BUILTIN; }
"typeof"   { yylval->fn = TMPL_EXPR_BUILTIN_TYPEOF; return BUILTIN; }
"sin"      { yylval->fn = TMPL_EXPR_BUILTIN_SIN; return BUILTIN; }
"tan"      { yylval->fn = TMPL_EXPR_BUILTIN_TAN; return BUILTIN; }
"cos"      { yylval->fn = TMPL_EXPR_BUILTIN_COS; return BUILTIN; }
"char"     { yylval->fn = TMPL_EXPR_BUILTIN_CAST_CHAR; return BUILTIN; }
"byte"     { yylval->fn = TMPL_EXPR_BUILTIN_CAST_BYTE; return BUILTIN; }
"i32"      { yylval->fn = TMPL_EXPR_BUILTIN_CAST_I32; return BUILTIN; }
"u32"      { yylval->fn = TMPL_EXPR_BUILTIN_CAST_U32; return BUILTIN; }
"i64"      { yylval->fn = TMPL_EXPR_BUILTIN_CAST_I64; return BUILTIN; }
"u64"      { yylval->fn = TMPL_EXPR_BUILTIN_CAST_U64; return BUILTIN; }
"float"    { yylval->fn = TMPL_EXPR_BUILTIN_CAST_FLOAT; return BUILTIN; }
"double"   { yylval->fn = TMPL_EXPR_BUILTIN_CAST_DOUBLE; return BUILTIN; }
"bool"     { yylval->fn = TMPL_EXPR_BUILTIN_CAST_BOOL; return BUILTIN; }

 /* string literals */
L?\"(\\.|[^\\"])*\" { yylval->s = copy_literal (yytext); return STRING_LITERAL; }

 /* @ to escape reserved keywords */
@[a-zA-Z_][a-zA-Z0-9_]* {
  yylval->s = g_strdup (yytext+1);
  return NAME;
}

 /* names */
[a-zA-Z_][a-zA-Z0-9_]* {
  yylval->s = g_strdup (yytext);
  return NAME;
}

[0-9]+"."[0-9]*{EXP}? |
"."?[0-9]+{EXP}? { yylval->d = atof(yytext); return NUMBER; }

"//".*
[ \t]   /* ignore whitespace */
"\\\n"  /* ignore escpaed newlines */
#[^\n]* { return EOL; } /* line-wise comments should EOL things */
"\n"    { return EOL; }
<<EOF>> { parser->reached_eof = TRUE; return EOL; }
.       {
          g_free (parser->error_str);
          parser->error_str = g_strdup (yytext);
          parser->error_line = yylineno;
        }

%%

void
tmpl_expr_parser_init_scanner (TmplExprParser *parser)
{
  g_assert (parser != NULL);

  yylex_init (&parser->scanner);
  yyset_extra (parser, parser->scanner);
}

void
tmpl_expr_parser_destroy_scanner (TmplExprParser *parser)
{
  g_assert (parser != NULL);

  yylex_destroy (parser->scanner);
}

void
tmpl_expr_parser_flush (TmplExprParser *parser)
{
  struct yyguts_t *yyg;

  g_assert (parser != NULL);

  yyg = (struct yyguts_t *)parser->scanner;
  tmpl_expr_parser__flush_buffer (YY_CURRENT_BUFFER, parser->scanner);
}

gboolean
tmpl_expr_parser_parse_string (TmplExprParser  *self,
                               const gchar    *input,
                               GError        **error)
{
  YY_BUFFER_STATE buf;
  int ret = 0;

  g_return_val_if_fail (self != NULL, FALSE);

  buf = tmpl_expr_parser__scan_string (input, self->scanner);
  while (!self->reached_eof && ret == 0)
    ret = tmpl_expr_parser_parse (self);
  tmpl_expr_parser__delete_buffer (buf, self->scanner);

  if (ret == 0 && self->error_str == NULL)
    return TRUE;

  if (self->error_str)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "%s", self->error_str);
      return FALSE;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_SYNTAX_ERROR,
               "Failed to parse expression");

  return FALSE;
}

gboolean
tmpl_expr_parser_init (TmplExprParser  *self,
                       GError        **error)
{
  g_return_val_if_fail (self != NULL, FALSE);

  memset (self, 0, sizeof *self);
  tmpl_expr_parser_init_scanner (self);

  return TRUE;
}

void
tmpl_expr_parser_destroy (TmplExprParser *self)
{
  if (self != NULL)
    {
      tmpl_expr_parser_destroy_scanner (self);
      g_clear_pointer (&self->ast, tmpl_expr_unref);
      g_clear_pointer (&self->error_str, g_free);
    }
}
