project('template-glib', 'c',
          version: '3.25.2',
          license: 'LGPLv2.1+',
    meson_version: '>= 0.38.1',
  default_options: [ 'warning_level=1', 'buildtype=debugoptimized' ],
)

version_arr = meson.project_version().split('.')
template_glib_version_major = version_arr[0].to_int()
template_glib_version_minor = version_arr[1].to_int()
template_glib_version_micro = version_arr[2].to_int()

apiversion = '1.0'
soversion = 0

if template_glib_version_minor.is_odd()
  template_glib_interface_age = 0
else
  template_glib_interface_age = template_glib_version_micro
endif

# maintaining compatibility with the previous libtool versioning
# current = minor * 100 + micro - interface
# revision = interface
current = template_glib_version_minor * 100 + template_glib_version_micro - template_glib_interface_age
revision = template_glib_interface_age
libversion = '@0@.@1@.@2@'.format(soversion, current, revision)

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', 'libtemplate_glib')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

configure_file(
  output: 'config.h',
  configuration: config_h,
)

add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(),
  '-I' + join_paths(meson.source_root(), 'src'),
  '-DTMPL_GLIB_COMPILATION',
], language: 'c')

cc = meson.get_compiler('c')

global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  ['-Werror=format-security', '-Werror=format=2'],
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wmissing-include-dirs',
  '-Wnested-externs',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-strict-aliasing',
  '-Wno-uninitialized',
  '-Wno-unused-parameter',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
if get_option('enable_profiling')
  test_c_args += '-pg'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach
add_project_arguments(
  global_c_args,
  language: 'c'
)

# Setup various paths that subdirectory meson.build files need
girdir = join_paths(get_option('datadir'), 'gir-1.0')
typelibdir = join_paths(get_option('libdir'), 'girepository-1.0')
vapidir = join_paths(get_option('datadir'), 'vala', 'vapi')

gnome = import('gnome')

subdir('src')
subdir('tests')
subdir('doc')
