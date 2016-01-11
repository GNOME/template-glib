#include <tmpl-glib.h>
#include <stdlib.h>

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(TmplScope) scope = NULL;
  g_autoptr(TmplTemplate) tmpl = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GOutputStream) stream = NULL;
  TmplSymbol *symbol = NULL;
  const gchar *output;
  gchar zero = 0;

  file = g_file_new_for_path ("simple.tmpl");
  tmpl = tmpl_template_new (NULL);

  if (!tmpl_template_parse_file (tmpl, file, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  scope = tmpl_scope_new (NULL);

  symbol = tmpl_scope_get (scope, "title");
  tmpl_symbol_assign_string (symbol, "My Title");

  /* We could use a unix output stream, or socket stream, as well */
  stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  if (!tmpl_template_expand (tmpl, stream, scope, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  /* add trailing \0, not necessary if we weren't printing with g_print */
  g_output_stream_write (stream, &zero, 1, NULL, NULL);
  output = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));
  g_print ("%s\n", output);

  return EXIT_SUCCESS;
}
