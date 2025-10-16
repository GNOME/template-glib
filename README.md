# Template-GLib

Template-GLib is a library to help you generate text based on a template and
user defined state. Template-GLib does not use a language runtime, so it is
safe to use from any GObject-Introspectable language.

Template-GLib allows you to access properties on GObjects as well as call
simple methods via GObject-Introspection. See our examples for how to call
methods.

## Authors

 * Christian Hergert <chergert@redhat.com>

## Documentation

 * API documentation can be found at https://gnome.pages.gitlab.gnome.org/template-glib/template-glib-1.0/

## Examples

See `examples/` for more detailed examples.

The following is contrived to show you the versatility of the syntax.
Template expressions and blocks are defined inside of {{ and }}.

```tmpl
{{if (foo.bar)*10/30.0+1 != foo.baz()}}
  {{for item in foo.list}}
    {{item.foo}}
  {{end}}
{{else if x > 10}}
bar
{{else}}
foo
{{end}}
```

### Includes

 * `{{include "path-to-template.tmpl"}}`

### Conditionals

 * `{{if <expression>}}`
 * `{{else if <expression>}}`
 * `{{else <expression>}}`
 * `{{for <identifier> in <expression>}}`

`if` and `for` should have a closing `{{end}}`

### Expressions

Expressions are parsed using a formal grammer, which you can find in the
flex and bison files at `src/tmpl-expr-scanner.l` and `tmpl-expr-parser.y`
respectively.

All numbers are currently represented as double-precision floating point.
This may change in the near future.

These can be used inside of {{ and }} to be evaluated. They can also be used
as expressions using the above conditional blocks.

```
true                       => true
false                      => false
" "                        => " "
" " * 3                    => "   "
1 + 3                      => 4
1 - 3                      => -2
1 * 3                      => 3
1 / 3                      => .333333
a = (1*3)                  => (a assigned 3)
a * a                      => 9
!true                      => false
!!true                     => true

func min(a,b) = if a < b then a; else b;;
min(1,2)                   => 1

obj.foo                    => (Gets GObject property "foo")
obj.foo=1                  => (Sets GObject property "foo")
obj.foo()                  => (Calls foo() on obj, via GObject Introspection)

# Note that we do not yet have support for accessing
# children of the Typelib. That is coming.
require Gtk                => (GITypelib of Gtk)
require Gtk version "3.0"  => (GITypelib of Gtk, requiring 3.0)
```

### Template Loading

See `TmplTemplateLoader` to control the lookup of included templates.
You probably want to do this if you are using this in something like a
webserver. By default, the search path is empty, and will not allow
including external resources.

Search paths will not allow loading templates above the search path entry.

### Scope

You can assign state into the template using `TmplScope`.

```c
/* scope can be inherited, by passing a parent instead of NULL */
TmplScope *scope = tmpl_scope_new (NULL);
TmplSymbol *symbol = tmpl_scope_get (scope, "foo");

tmpl_symbol_assign_boolean (symbol, TRUE);
tmpl_symbol_assign_double (symbol, 123.4);
tmpl_symbol_assign_string (symbol, "foo");
tmpl_symbol_assign_object (symbol, g_object_new (MY_TYPE_FOO, NULL));

/* or if you can assign via a GValue */
GValue value = G_VALUE_INIT;
g_value_set_boxed (&value, my_boxed);
tmpl_symbol_assign_value (symbol, &value);
g_value_unset (&value);
```

## Contributing

See CONTRIBUTING.md for information on how you can contribute.

