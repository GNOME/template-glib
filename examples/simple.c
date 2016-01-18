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
  TmplSymbol *symbol = NULL;
  gchar *str;

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
   * Expand the template based on our scope. You can also expand into a
   * GOutputStream, instead of a string.
   */
  if (!(str = tmpl_template_expand_string (tmpl, scope, &error)))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  g_print ("%s\n", str);
  g_free (str);

  /*
   * All our state gets cleaned up thanks to g_autoptr()!
   */

  return EXIT_SUCCESS;
}
