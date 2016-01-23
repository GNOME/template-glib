using GLib;
using Template;

static int main (string[] argv)
{
	var file = GLib.File.new_for_path("simple.tmpl");
	var tmpl = new Template.Template (null);

	try {
		tmpl.parse_file (file, null);

		var scope = new Template.Scope ();

		var title = scope.get ("title");
		title.assign_string ("Example Title");

		var expanded = tmpl.expand_string (scope);
		stdout.printf ("%s\n", expanded);
	} catch (GLib.Error ex) {
		stderr.printf ("%s\n", ex.message);
		return 1;
	}

	return 0;
}
