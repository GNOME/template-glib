Title: Overview

Template-GLib is a library that provides simple templating for GLib-based
applications.  It supports GObject Introspection to allow templates to call
back into application code.

## Expression precedence

Operators bind in this order, from weakest to strongest:

1. Assignment (`=`), including object property assignment.
2. Boolean OR (`||`).
3. Boolean AND (`&&`).
4. Comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`).
5. Addition and subtraction (`+`, `-`).
6. Multiplication and division (`*`, `/`).
7. Unary negation (`-`, `!`).
8. Calls and member access (`()`, `.`).

Assignment chains group to the right. Binary arithmetic, comparisons, and
boolean operators at the same precedence group to the left. Parentheses
override precedence. Boolean operators short-circuit.

For example, `a = 1 < 3` assigns `true`, and `true || false && false`
evaluates to `true`. `!a && b` means `(!a) && b`.

Earlier versions bound assignment more tightly than comparisons and boolean
operators, treated `&&` and `||` equally, and let `!` consume a whole expression.
Use parentheses when that older grouping is intended.
