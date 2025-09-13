project('template-glib', 'c',
          version: '3.38.1',
          license: 'LGPLv2.1+',
    meson_version: '>= 0.51.0',
  default_options: [ 'warning_level=2', 'buildtype=debugoptimized', 'c_std=gnu11' ],
)

package_subdir = get_option('package_subdir')

if meson.is_subproject() and package_subdir == ''
  error('-Dpackage_subdir must be set when using @0@ as a subproject'.format(meson.project_name()))
endif

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

darwin_versions = [current + 1, '@0@.@1@'.format(current + 1, revision)]

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', 'libtemplate_glib')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

cc = meson.get_compiler('c')

release_args = []
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
if not get_option('buildtype').startswith('debug')
  add_project_arguments(['-DG_DISABLE_CAST_CHECKS'], language: 'c')
  release_args += [ '-DG_DISABLE_ASSERT' ]
  test_link_args += [
    '-Wl,-Bsymbolic',
    '-fno-plt',
  ]
endif
foreach link_arg: test_link_args
  if cc.links('int main () { return 0; }', name: link_arg, args: link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(global_link_args, language: 'c')

configure_file(
  output: 'config.h',
  configuration: config_h,
)

cc = meson.get_compiler('c')

global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wmissing-include-dirs',
  '-Wnested-externs',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wuninitialized',
  ['-Werror=format-security', '-Werror=format=2' ],
  '-Werror=empty-body',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=pointer-arith',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=misleading-indentation',
  '-Werror=missing-include-dirs',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=return-type',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=undef',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
if get_option('profiling')
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
libdir = join_paths(get_option('libdir'), package_subdir)
includedir = join_paths(get_option('includedir'), package_subdir)
girdir = join_paths(get_option('datadir'), package_subdir, 'gir-1.0')
typelibdir = join_paths(get_option('libdir'), package_subdir, 'girepository-1.0')
if package_subdir == ''
  vapidir = join_paths(get_option('datadir'), 'vala', 'vapi')
else
  vapidir = join_paths(get_option('datadir'), package_subdir, 'vapi')
endif

gnome = import('gnome')
i18n = import('i18n')

gir = find_program('g-ir-scanner', required: get_option('introspection'))
generate_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())
generate_vapi = generate_gir and get_option('vapi')

subdir('src')
subdir('po')

if get_option('tests')
  subdir('tests')
endif

if get_option('docs')
  subdir('doc')
endif
