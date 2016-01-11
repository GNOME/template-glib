bin_PROGRAMS = template-glib-eval
lib_LTLIBRARIES = libtemplate-glib-1.0.la

AM_CPPFLAGS = \
	-D_GNU_SOURCE \
	-DTMPL_GLIB_COMPILATION

EXTRA_DIST = \
	tmpl-expr-parser.c \
	tmpl-expr-parser.h \
	tmpl-expr-scanner.c \
	tmpl-expr-scanner.h

template_glib_eval_CFLAGS = $(TMPL_CFLAGS)
template_glib_eval_LDADD = libtemplate-glib-1.0.la
template_glib_eval_SOURCES = main.c

pkgincludedir = $(includedir)/template-glib-1.0
pkginclude_HEADERS = \
	tmpl-error.h \
	tmpl-expr-types.h \
	tmpl-expr.h \
	tmpl-glib.h \
	tmpl-scope.h \
	tmpl-symbol.h \
	tmpl-template-locator.h \
	tmpl-template.h \
	$(NULL)

libtemplate_glib_1_0_la_CFLAGS = $(TMPL_CFLAGS)
libtemplate_glib_1_0_la_LIBADD = $(TMPL_LIBS)
libtemplate_glib_1_0_la_SOURCES = \
	$(pkginclude_HEADERS) \
	tmpl-branch-node.c \
	tmpl-branch-node.h \
	tmpl-condition-node.c \
	tmpl-condition-node.h \
	tmpl-debug.h \
	tmpl-error.c \
	tmpl-expr-eval.c \
	tmpl-expr-node.c \
	tmpl-expr-node.h \
	tmpl-expr-parser-private.h \
	tmpl-expr-parser.y \
	tmpl-expr-private.h \
	tmpl-expr-scanner.l \
	tmpl-expr.c \
	tmpl-gi-private.h \
	tmpl-gi.c \
	tmpl-iter-node.c \
	tmpl-iter-node.h \
	tmpl-iterator.c \
	tmpl-iterator.h \
	tmpl-lexer.c \
	tmpl-lexer.h \
	tmpl-node.c \
	tmpl-node.h \
	tmpl-parser.c \
	tmpl-parser.h \
	tmpl-scope.c \
	tmpl-symbol.c \
	tmpl-template-locator.c \
	tmpl-template.c \
	tmpl-text-node.c \
	tmpl-text-node.h \
	tmpl-token-input-stream.c \
	tmpl-token-input-stream.h \
	tmpl-token.c \
	tmpl-token.h \
	tmpl-util-private.h \
	tmpl-util.c \
	$(NULL)

GITIGNOREFILES = \
	tmpl-expr-parser.c \
	tmpl-expr-parser.h \
	tmpl-debug.h \
	lex.*

-include $(top_srcdir)/git.mk
