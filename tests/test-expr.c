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

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Tmpl/Expr/test1", test1);
  return g_test_run ();
}
