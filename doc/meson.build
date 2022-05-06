subdir('xml')

private_headers = [
  'config.h',

  'tmpl-enums.h',
  'tmpl-branch-node.h',
  'tmpl-condition-node.h',
  'tmpl-debug.h',
  'tmpl-expr-node.h',
  'tmpl-expr-parser-private.h',
  'tmpl-expr-parser.h',
  'tmpl-expr-private.h',
  'tmpl-expr-scanner.h',
  'tmpl-gi-private.h',
  'tmpl-glib.h',
  'tmpl-iter-node.h',
  'tmpl-iterator.h',
  'tmpl-lexer.h',
  'tmpl-node.h',
  'tmpl-parser.h',
  'tmpl-text-node.h',
  'tmpl-token-input-stream.h',
  'tmpl-token.h',
  'tmpl-util-private.h',
]

glib_prefix = dependency('glib-2.0').get_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
docpath = join_paths(get_option('datadir'), 'gtk-doc', 'html')

gnome.gtkdoc('template-glib',
           main_xml: 'template-glib-docs.sgml',
            src_dir: [
              join_paths(meson.source_root(), 'src'),
              join_paths(meson.build_root(), 'src'),
            ],

       dependencies: libtemplate_glib_dep,
  gobject_typesfile: 'template-glib.types',
          scan_args: [
            '--rebuild-types',
            '--ignore-decorators=_TMPL_EXTERN',
            '--ignore-headers=' + ' '.join(private_headers),
          ],

       fixxref_args: [
         '--html-dir=@0@'.format(docpath),
         '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
         '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
         '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
       ],
            install: true)
