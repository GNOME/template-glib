using GLib;
using Template;

class Article : Object {
	public string title {
		get { return "Sample Article"; }
	}

	public string author {
		get { return Environment.get_user_name(); }
	}

	private string _date = (new DateTime.now_local()).to_string();
	public string date {
		get { return  _date; }
	}

	private string[] _sections = {"Iterate", "over", "strv"};
	public string[] sections {
		get { return _sections; }
	}
}

static int main (string[] args) {
	var file = GLib.File.new_for_path("article.tmpl");
	var tmpl = new Template.Template (null);

	try {
		tmpl.parse_file (file, null);

		var scope = new Template.Scope ();

		scope["article"].assign_object(new Article());

		var expanded = tmpl.expand_string (scope);
		stdout.printf ("%s\n", expanded);
	} catch (GLib.Error ex) {
		stderr.printf ("%s\n", ex.message);
		return 1;
	}
	return 0;
}
