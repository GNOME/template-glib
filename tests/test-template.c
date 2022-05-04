/* test-template.c
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

#include <tmpl-glib.h>

static char *
get_file_contents (GFile *file)
{
  char *ret = NULL;
  gsize len;

  if (!g_file_load_contents (file, NULL, &ret, &len, NULL, NULL))
    return NULL;

  return ret;
}

static GFile *
get_test_file (const char *name)
{
  g_assert_nonnull (g_getenv ("G_TEST_SRCDIR"));
  return g_file_new_build_filename (g_getenv ("G_TEST_SRCDIR"), name, NULL);
}

static void
test1 (void)
{
  GFile *file = get_test_file ("test-template-test1.tmpl");
  GFile *expected = get_test_file ("test-template-test1.tmpl.expected");
  TmplTemplate *tmpl = NULL;
  TmplScope *scope = NULL;
  GError *error = NULL;
  char *str = NULL;
  char *estr = NULL;
  gboolean r;

  tmpl = tmpl_template_new (NULL);
  r = tmpl_template_parse_file (tmpl, file, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (r);

  scope = tmpl_scope_new ();
  tmpl_scope_set_string (scope, "title", "My Title");
  str = tmpl_template_expand_string (tmpl, scope, &error);
  g_assert_no_error (error);
  g_assert_nonnull (str);

  estr = get_file_contents (expected);
  g_assert_cmpstr (str, ==, estr);

  g_free (estr);
  g_free (str);
  tmpl_scope_unref (scope);
  g_assert_finalize_object (file);
  g_assert_finalize_object (expected);
  g_assert_finalize_object (tmpl);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Tmpl/Template/test1", test1);
  return g_test_run ();
}
