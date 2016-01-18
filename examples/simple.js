const Template = imports.gi.Template;
const Gio = imports.gi.Gio;

// Get our file to process
let file = Gio.File.new_for_path("simple.tmpl");

// Create a new template and parse our input file
let tmpl = new Template.Template();
tmpl.parse_file(file, null);

// Create scope for expansion
let scope = Template.Scope.new ();

// Create and assign "title" variable in scope
let title = scope.get("title");
title.assign_string("Example Title");

// Write to stdout
let stream = Gio.UnixOutputStream.new (0, false);

// Expand the template into stream
let expanded = tmpl.expand_string(scope);
log(expanded);
