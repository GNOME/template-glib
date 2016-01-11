EXTRA_DATA = \
	article.tmpl \
	simple.tmpl \
	$(NULL)

noinst_PROGRAMS = simple

simple_SOURCES = simple.c
simple_CFLAGS = $(TMPL_CFLAGS) -I$(top_srcdir)/src
simple_LDADD = $(top_builddir)/src/libtemplate-glib-1.0.la

-include $(top_srcdir)/git.mk
