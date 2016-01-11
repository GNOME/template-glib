/* tmpl.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <tmpl-glib.h>

gint
main (gint   argc,
      gchar *argv[])
{
  TmplTemplateLocator *locator = NULL;
  GOutputStream *stream = NULL;
  TmplTemplate *tmpl = NULL;
  TmplScope *scope = NULL;
  GFile *file = NULL;
  gchar *output;
  GError *error = NULL;
  gint ret = EXIT_FAILURE;
  gchar zero = 0;

  if (argc != 2)
    {
      g_printerr ("usage: %s TEMPLATE\n", argv [0]);
      return EXIT_FAILURE;
    }

  locator = tmpl_template_locator_new ();
  tmpl_template_locator_prepend_search_path (locator, ".");
  tmpl = tmpl_template_new (locator);
  file = g_file_new_for_commandline_arg (argv [1]);
  scope = tmpl_scope_new (NULL);

  if (!tmpl_template_parse_file (tmpl, file, NULL, &error))
    {
      g_printerr ("ERROR: %s\n", error->message);
      g_clear_error (&error);
      goto cleanup;
    }

  stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  if (!tmpl_template_expand (tmpl, stream, scope, NULL, &error))
    {
      g_printerr ("ERROR: %s\n", error->message);
      g_clear_error (&error);
      goto cleanup;
    }

  g_output_stream_write (stream, &zero, 1, NULL, NULL);
  output = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));
  g_print ("%s\n", output);

  ret = EXIT_SUCCESS;

cleanup:
  g_clear_object (&stream);
  g_clear_object (&locator);
  g_clear_object (&tmpl);
  g_clear_object (&file);

  return ret;
}
