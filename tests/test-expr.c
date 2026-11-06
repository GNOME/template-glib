/* test-expr.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "tmpl-glib.h"

static char *
get_test_file (const char *name)
{
  GFile *file;
  char *data = NULL;
  gsize len = 0;

  g_assert_nonnull (g_getenv ("G_TEST_SRCDIR"));

  file = g_file_new_build_filename (g_getenv ("G_TEST_SRCDIR"), name, NULL);
  g_file_load_contents (file, NULL, &data, &len, NULL, NULL);
  g_object_unref (file);

  return data;
}

static void
test1 (void)
{
  GError *error = NULL;
  char *contents = get_test_file ("test1.script");
  TmplExpr *expr = tmpl_expr_from_string (contents, &error);
  TmplScope *scope = tmpl_scope_new ();
  GValue ret = G_VALUE_INIT;
  gboolean r;

  g_assert_no_error (error);
  g_assert_nonnull (expr);

  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);

  if (!G_VALUE_HOLDS_DOUBLE (&ret))
    g_printerr ("Expected double, got %s\n",
                G_VALUE_TYPE_NAME (&ret));

  g_assert_true (G_VALUE_HOLDS_DOUBLE (&ret));
  g_assert_cmpint (g_value_get_double (&ret), ==, 1234.0);

  g_value_unset (&ret);
  tmpl_scope_unref (scope);
  tmpl_expr_unref (expr);
  g_free (contents);
}

static void
test_variant_dict (void)
{
  TmplScope *scope = tmpl_scope_new ();
  GVariant *dict;
  GError *error = NULL;
  TmplExpr *expr;
  GValue ret = G_VALUE_INIT;
  gboolean r;

  dict = g_variant_new_parsed ("{'name': <'Alice'>, 'active': <true>}");
  tmpl_scope_set_variant (scope, "d", g_steal_pointer (&dict));

  expr = tmpl_expr_from_string ("d.name", &error);
  g_assert_no_error (error);
  g_assert_nonnull (expr);

  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_STRING (&ret));
  g_assert_cmpstr (g_value_get_string (&ret), ==, "Alice");
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  expr = tmpl_expr_from_string ("d.active", &error);
  g_assert_no_error (error);
  g_assert_nonnull (expr);

  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&ret));
  g_assert_true (g_value_get_boolean (&ret));
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  tmpl_scope_unref (scope);
}

static void
test_variant_array_iter (void)
{
  TmplTemplate *tmpl;
  TmplScope *scope;
  GVariant *array;
  GError *error = NULL;
  char *str;
  gboolean r;

  tmpl = tmpl_template_new (NULL);
  r = tmpl_template_parse_string (tmpl,
                                  "{{for x in items}}{{x}},{{end}}",
                                  &error);
  g_assert_no_error (error);
  g_assert_true (r);

  scope = tmpl_scope_new ();
  array = g_variant_ref_sink (g_variant_new_parsed ("[<'a'>, <'b'>, <'c'>]"));
  tmpl_scope_set_variant (scope, "items", array);
  g_variant_unref (array);

  str = tmpl_template_expand_string (tmpl, scope, &error);
  g_assert_no_error (error);
  g_assert_nonnull (str);
  g_assert_cmpstr (str, ==, "a,b,c,");

  g_free (str);
  tmpl_scope_unref (scope);
  g_assert_finalize_object (tmpl);
}

static void
test_variant_dict_mixed_types (void)
{
  TmplScope *scope = tmpl_scope_new ();
  GError *error = NULL;
  TmplExpr *expr;
  GValue ret = G_VALUE_INIT;
  gboolean r;

  tmpl_scope_set_variant (scope, "d",
                          g_variant_new_parsed ("{'name': <'Alice'>, 'score': <3.14>, 'count': <int32 42>}"));

  expr = tmpl_expr_from_string ("d.name", &error);
  g_assert_no_error (error);
  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_STRING (&ret));
  g_assert_cmpstr (g_value_get_string (&ret), ==, "Alice");
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  expr = tmpl_expr_from_string ("d.score", &error);
  g_assert_no_error (error);
  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_DOUBLE (&ret));
  g_assert_cmpfloat (g_value_get_double (&ret), ==, 3.14);
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  expr = tmpl_expr_from_string ("d.count", &error);
  g_assert_no_error (error);
  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_INT (&ret));
  g_assert_cmpint (g_value_get_int (&ret), ==, 42);
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  tmpl_scope_unref (scope);
}

static void
test_variant_nested_dict (void)
{
  TmplScope *scope = tmpl_scope_new ();
  GError *error = NULL;
  TmplExpr *expr;
  GValue ret = G_VALUE_INIT;
  gboolean r;

  tmpl_scope_set_variant (scope, "d",
                          g_variant_new_parsed ("{'meta': <{'author': <'Bob'>, 'draft': <true>}>}"));

  expr = tmpl_expr_from_string ("d.meta.author", &error);
  g_assert_no_error (error);
  g_assert_nonnull (expr);

  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_STRING (&ret));
  g_assert_cmpstr (g_value_get_string (&ret), ==, "Bob");
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  expr = tmpl_expr_from_string ("d.meta.draft", &error);
  g_assert_no_error (error);
  g_assert_nonnull (expr);

  r = tmpl_expr_eval (expr, scope, &ret, &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&ret));
  g_assert_true (g_value_get_boolean (&ret));
  g_value_unset (&ret);
  tmpl_expr_unref (expr);

  tmpl_scope_unref (scope);
}

static void
test_variant_array_of_dicts (void)
{
  TmplTemplate *tmpl;
  TmplScope *scope;
  GError *error = NULL;
  char *str;
  gboolean r;

  tmpl = tmpl_template_new (NULL);
  r = tmpl_template_parse_string (tmpl,
                                  "{{for x in items}}{{x.name}},{{end}}",
                                  &error);
  g_assert_no_error (error);
  g_assert_true (r);

  scope = tmpl_scope_new ();
  tmpl_scope_set_variant (scope, "items",
                          g_variant_new_parsed ("[<{'name': <'Alice'>}>, <{'name': <'Bob'>}>]"));

  str = tmpl_template_expand_string (tmpl, scope, &error);
  g_assert_no_error (error);
  g_assert_nonnull (str);
  g_assert_cmpstr (str, ==, "Alice,Bob,");

  g_free (str);
  tmpl_scope_unref (scope);
  g_assert_finalize_object (tmpl);
}

static void
test_precedence (void)
{
  static const struct
  {
    const char *text;
    gboolean expected;
  } cases[] = {
    { "a = 1 < 3; a", TRUE },
    { "a = 3 < 1; a", FALSE },
    { "a = b = 1 < 3; a && b", TRUE },
    { "a = true && false; a", FALSE },
    { "true || false && false", TRUE },
    { "(true || false) && false", FALSE },
    { "false && true || true", TRUE },
    { "1 < 2 && 3 < 4", TRUE },
    { "1 > 2 || 3 < 4", TRUE },
    { "!false && false", FALSE },
    { "!(false && false)", TRUE },
    { "!!true", TRUE },
    { "!bool(0) && true", TRUE },
    { "2 + 3 * 4 == 14", TRUE },
    { "10 - 3 - 2 == 5", TRUE },
    { "-abs(-2) * 3 == -6", TRUE },
    { "f = func(x) x * 2 + 1; f(3) == 7", TRUE },
    { "(func(x) x * 2 + 1)(3) == 7", TRUE },
    { "a = 0; true || false && (a = 1); a == 0", TRUE },
    { "action.enabled = 3 < 1; action.enabled", FALSE },
    { "action.enabled = 1 < 3; !action.enabled", FALSE },
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      g_autoptr(TmplScope) scope = tmpl_scope_new ();
      g_autoptr(GSimpleAction) action = g_simple_action_new ("test", NULL);
      g_autoptr(TmplExpr) expr = NULL;
      g_autoptr(GError) error = NULL;
      GValue value = G_VALUE_INIT;
      gboolean ret;

      g_test_message ("%s", cases[i].text);
      tmpl_scope_set_object (scope, "action", action);
      expr = tmpl_expr_from_string (cases[i].text, &error);
      g_assert_no_error (error);
      g_assert_nonnull (expr);

      ret = tmpl_expr_eval (expr, scope, &value, &error);
      g_assert_no_error (error);
      g_assert_true (ret);
      g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
      g_assert_cmpint (g_value_get_boolean (&value), ==, cases[i].expected);
      g_value_unset (&value);
    }
}

static void
test_syntax_errors (void)
{
  static const char *cases[] = {
    "1 +",
    "name + )",
    "func(a,b) 1 +",
    "def f(a,b)\n1 +\nend",
    "def f(a,b)\n",
    "if true then else )",
    "require Gio version",
    "require Gio version 1",
    "f(1,)",
    "\"text\" + )",
    "a = 1; b = 2 +",
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      g_autoptr(TmplExpr) expr = NULL;
      g_autoptr(GError) error = NULL;

      g_test_message ("%s", cases[i]);
      expr = tmpl_expr_from_string (cases[i], &error);
      g_assert_error (error, TMPL_ERROR, TMPL_ERROR_SYNTAX_ERROR);
      g_assert_null (expr);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Tmpl/Expr/test1", test1);
  g_test_add_func ("/Tmpl/Expr/variant-dict", test_variant_dict);
  g_test_add_func ("/Tmpl/Expr/variant-array-iter", test_variant_array_iter);
  g_test_add_func ("/Tmpl/Expr/variant-dict-mixed-types", test_variant_dict_mixed_types);
  g_test_add_func ("/Tmpl/Expr/variant-nested-dict", test_variant_nested_dict);
  g_test_add_func ("/Tmpl/Expr/variant-array-of-dicts", test_variant_array_of_dicts);
  g_test_add_func ("/Tmpl/Expr/precedence", test_precedence);
  g_test_add_func ("/Tmpl/Expr/syntax-errors", test_syntax_errors);
  return g_test_run ();
}
