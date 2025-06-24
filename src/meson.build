libtemplate_glib_header_subdir = 'template-glib-' + apiversion
libtemplate_glib_header_dir = join_paths(includedir, libtemplate_glib_header_subdir)

version_data = configuration_data()
version_data.set('TMPL_MAJOR_VERSION', template_glib_version_major)
version_data.set('TMPL_MINOR_VERSION', template_glib_version_minor)
version_data.set('TMPL_MICRO_VERSION', template_glib_version_micro)
version_data.set('TMPL_VERSION', meson.project_version())
version_data.set10('ENABLE_TRACING', get_option('tracing'))

core_lib_include_directories = [
  include_directories('.'),
  include_directories('..'),
]

tmpl_version_h = configure_file(
          input: 'tmpl-version.h.in',
         output: 'tmpl-version.h',
    install_dir: libtemplate_glib_header_dir,
  configuration: version_data)

tmpl_debug_h = configure_file(
          input: 'tmpl-debug.h.in',
         output: 'tmpl-debug.h',
    install_dir: libtemplate_glib_header_dir,
  configuration: version_data)

libtemplate_glib_enum_headers = [
  'tmpl-error.h',
  'tmpl-expr-types.h',
]

libtemplate_glib_enums = gnome.mkenums('tmpl-enums',
       h_template: 'tmpl-enums.h.in',
       c_template: 'tmpl-enums.c.in',
          sources: libtemplate_glib_enum_headers,
   install_header: true,
      install_dir: libtemplate_glib_header_dir,
)

libtemplate_glib_generated_headers = [
  tmpl_debug_h,
  tmpl_version_h,
  libtemplate_glib_enums[1],
]

libtemplate_glib_public_headers = [
  'tmpl-error.h',
  'tmpl-expr-types.h',
  'tmpl-expr.h',
  'tmpl-glib.h',
  'tmpl-scope.h',
  'tmpl-symbol.h',
  'tmpl-template-locator.h',
  'tmpl-template.h',
  'tmpl-version-macros.h',
]

libtemplate_glib_public_sources = [
  'tmpl-error.c',
  'tmpl-expr.c',
  'tmpl-scope.c',
  'tmpl-symbol.c',
  'tmpl-template.c',
  'tmpl-template-locator.c',

  libtemplate_glib_enums[0],
]

libtemplate_glib_deps = [
  dependency('gio-2.0'),
  dependency('girepository-2.0'),
  cc.find_library('m', required: false),
]

flex = find_program('flex')
bison = find_program('bison')
sed = find_program('sed')

tmpl_expr_parser = custom_target('tmpl-expr-parser',
    input: 'tmpl-expr-parser.y',
   output: ['tmpl-expr-parser.c', 'tmpl-expr-parser.h'],
  command: [bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@']

)

# This is an awful workaround, flex files embed options to control the output
# this fails with meson which wants to output to specific directories
# and just to avoid any regressions we don't want to modify the autotools files
tmpl_expr_scanner_flex = custom_target('tmpl-expr-scanner-flex',
    input: 'tmpl-expr-scanner.l',
   output: 'tmpl-expr-meson-scanner.l',
  capture: true,
  command: [sed, '-e', 's/^%option header-file.*$//',
                 '-e', 's/^%option outfile.*$//', '@INPUT@']
)
tmpl_expr_scanner = custom_target('tmpl-expr-scanner',
    input: tmpl_expr_scanner_flex,
   output: 'tmpl-expr-scanner.c',
  command: [flex, '-o', '@OUTPUT@', '@INPUT@']
)

libtemplate_glib_sources = [
  libtemplate_glib_generated_headers,
  libtemplate_glib_public_headers,
  libtemplate_glib_public_sources,
  tmpl_expr_parser,
  tmpl_expr_scanner,

  'tmpl-branch-node.c',
  'tmpl-branch-node.h',
  'tmpl-condition-node.c',
  'tmpl-condition-node.h',
  'tmpl-expr-eval.c',
  'tmpl-expr-node.c',
  'tmpl-expr-node.h',
  'tmpl-expr-parser-private.h',
  'tmpl-expr-private.h',
  'tmpl-gi-private.h',
  'tmpl-gi.c',
  'tmpl-iter-node.c',
  'tmpl-iter-node.h',
  'tmpl-iterator.c',
  'tmpl-iterator.h',
  'tmpl-lexer.c',
  'tmpl-lexer.h',
  'tmpl-node.c',
  'tmpl-node.h',
  'tmpl-parser.c',
  'tmpl-parser.h',
  'tmpl-text-node.c',
  'tmpl-text-node.h',
  'tmpl-token-input-stream.c',
  'tmpl-token-input-stream.h',
  'tmpl-token.c',
  'tmpl-token.h',
  'tmpl-util-private.h',
  'tmpl-util.c',
]

core_lib_c_args = [
  '-DTMPL_GLIB_COMPILATION',
]

core_lib = static_library('template_glib', libtemplate_glib_sources,
         dependencies: libtemplate_glib_deps,
               c_args: release_args + core_lib_c_args,
  include_directories: core_lib_include_directories,
)
core_lib_dep = declare_dependency(
              sources: libtemplate_glib_generated_headers,
         dependencies: libtemplate_glib_deps,
           link_whole: core_lib,
  include_directories: core_lib_include_directories,
)

libtemplate_glib = library(
  'template_glib-' + apiversion,

     link_whole: core_lib,
   dependencies: libtemplate_glib_deps,
      soversion: soversion,
        version: libversion,
darwin_versions: darwin_versions,
        install: true,
)

libtemplate_glib_dep = declare_dependency(
              sources: libtemplate_glib_generated_headers,
         dependencies: libtemplate_glib_deps,
            link_with: libtemplate_glib,
  include_directories: core_lib_include_directories,
)

if generate_gir
  libtemplate_glib_gir = gnome.generate_gir(libtemplate_glib,
                sources: libtemplate_glib_generated_headers + libtemplate_glib_public_headers + libtemplate_glib_public_sources,
              nsversion: apiversion,
              namespace: 'Template',
        export_packages: 'template-glib-@0@'.format(apiversion),
          symbol_prefix: 'tmpl',
      identifier_prefix: 'Tmpl',
              link_with: libtemplate_glib,
               includes: [ 'Gio-2.0' ],
                install: true,
        install_dir_gir: girdir,
    include_directories: core_lib_include_directories,
    install_dir_typelib: typelibdir,
             extra_args: [
               '--c-include=tmpl-glib.h',
               '-DTMPL_GLIB_COMPILATION',
             ],
  )

  if get_option('vapi')
    libtemplate_glib_vapi = gnome.generate_vapi('template-glib-' + apiversion,
          sources: libtemplate_glib_gir[0],
         packages: [ 'gio-2.0' ],
          install: true,
      install_dir: vapidir,
    )

  endif
endif

install_headers(libtemplate_glib_public_headers,
  install_dir: libtemplate_glib_header_dir
)

if meson.version().version_compare('>=0.57')
  meson.override_dependency('template-glib-@0@'.format(apiversion), libtemplate_glib_dep)
endif

pkgg = import('pkgconfig')

pkgg.generate(
    libraries: [libtemplate_glib],
      subdirs: join_paths(package_subdir, libtemplate_glib_header_subdir),
      version: meson.project_version(),
         name: 'Template-GLib',
     filebase: 'template-glib-@0@'.format(apiversion),
  description: 'A templating library for GLib',
     requires: 'gio-2.0',
  install_dir: join_paths(libdir, 'pkgconfig'),
)
