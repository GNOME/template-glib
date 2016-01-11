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

  /*
   * First we need to create and parse our template.
   * The template can be expanded multiple times, so you can
   * keep them around and re-use them.
   */
  file = g_file_new_for_path ("simple.tmpl");
  tmpl = tmpl_template_new (NULL);

  if (!tmpl_template_parse_file (tmpl, file, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  /*
   * Now create our scope used to expand the template,
   * and assign the "title" variable in the scope.
   */
  scope = tmpl_scope_new ();

  symbol = tmpl_scope_get (scope, "title");
  tmpl_symbol_assign_string (symbol, "My Title");

  /*
   * Now lets expand the template into a memory stream. We could also
   * just use a unix output stream using STDOUT, but this is more portable
   * if people want to get this running on non-UNIX systems.
   */
  stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  if (!tmpl_template_expand (tmpl, stream, scope, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  /*
   * Because we are converting this to a C String, we need to add a trailing
   * null byte.
   */
  g_output_stream_write (stream, &zero, 1, NULL, NULL);

  /*
   * Okay, finally we can print this to stdout.
   */
  output = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));
  g_print ("%s\n", output);

  /*
   * All our state gets cleaned up thanks to g_autoptr()!
   */

  return EXIT_SUCCESS;
}
