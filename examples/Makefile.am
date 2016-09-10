EXTRA_DATA = \
	article.tmpl \
	simple.tmpl \
	$(NULL)

noinst_PROGRAMS = simple

simple_SOURCES = simple.c
simple_CFLAGS = $(TMPL_CFLAGS) -I$(top_srcdir)/src
simple_LDADD = $(TMPL_LIBS) $(top_builddir)/src/libtemplate-glib-1.0.la

if ENABLE_VAPIGEN
noinst_PROGRAMS += simple-vala

simple_vala_SOURCES = simple-vala.vala
simple_vala_CFLAGS = $(TMPL_CFLAGS) -I$(top_srcdir)/src
simple_vala_LDADD = $(TMPL_LIBS) $(top_builddir)/src/libtemplate-glib-1.0.la
simple_vala_VALAFLAGS = \
	--vapidir $(top_builddir)/src \
	--pkg template-glib-1.0

endif

-include $(top_srcdir)/git.mk
