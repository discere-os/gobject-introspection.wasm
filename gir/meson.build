gir_giscanner_pymod = []
gir_giscanner_built_files = []
if not get_option('gi_cross_use_prebuilt_gi')
  # The right thing to do is just make these dependencies of the
  # `configure_file` python executable, but that isn't yet supported.
  gir_giscanner_pymod = giscanner_pymod
  gir_giscanner_built_files = giscanner_built_files
endif

# Dependency object used by Meson's GNOME module to allow using
# 'gobject-introspection-no-typelibs-1.0' internally to generate all the
# typelibs that will end in the final dependency 'gobject-introspection-1.0'.
# This uses 'giscanner_pymod' and 'giscanner_pymod' to ensure
# that giscanner is built before generating the gir targets
if get_option('build_introspection_data') == true
  girepo_no_typelibs_dep = declare_dependency(
    dependencies: [girepo_dep],
    link_with: gir_giscanner_pymod,
    sources: gir_giscanner_built_files,
    variables: {
      'girdir': meson.current_build_dir() / 'gir',
    },
  )
  meson.override_dependency('gobject-introspection-no-typelibs-1.0', girepo_no_typelibs_dep)
endif

gir_files = [
  'DBus-1.0.gir',
  'DBusGLib-1.0.gir',
  'fontconfig-2.0.gir',
  'freetype2-2.0.gir',
  'GL-1.0.gir',
  'libxml2-2.0.gir',
  'Vulkan-1.0.gir',
  'xft-2.0.gir',
  'xlib-2.0.gir',
  'xfixes-4.0.gir',
  'xrandr-1.3.gir',
  'win32-1.0.gir',
]

uninstalled_gir_files = []

# Copy gir files to build directory to have them all in a single place.
# This is needed when gobject-introspection is a subproject because Meson
# will add --includedir pointing to build directory automatically.
foreach gir: gir_files
  fs.copyfile(gir)
endforeach

cairo_conf = configuration_data()
if get_option('cairo_libname') != ''
  cairo_library_name = get_option('cairo_libname')
elif host_machine.system() == 'windows'
  if cc.get_id() == 'msvc'
    cairo_library_name = 'cairo-gobject.dll'
  else
    cairo_library_name = 'libcairo-gobject-2.dll'
  endif
elif host_machine.system() == 'darwin'
  cairo_library_name = 'libcairo-gobject.2.dylib'
else
  cairo_library_name = 'libcairo-gobject.so.2'
endif

cairo_conf.set('CAIRO_SHARED_LIBRARY', cairo_library_name)
cairo_conf.set('CAIRO_GIR_PACKAGE', 'cairo-gobject')

cairo_gir = configure_file(
  input: 'cairo-1.0.gir.in',
  output: 'cairo-1.0.gir',
  configuration: cairo_conf,
)
gir_files += [cairo_gir]

typelibdir = join_paths(get_option('libdir'), 'girepository-1.0')
install_data(gir_files, install_dir: girdir)

scanner_command = [
  find_program('g-ir-scanner', native: true),
  '--output=@OUTPUT@',
  '--no-libtool',
  '--quiet',
  '--reparse-validate',
  '--add-include-path', join_paths(meson.current_build_dir()),
  '--add-include-path', join_paths(meson.current_source_dir()),
]

dep_type = glib_dep.type_name()
if dep_type == 'internal'
  # XXX: Instead of hard-coding the subproject directory, we should use
  # gnome.generate_gir() because that will take care of dependencies, include
  # paths, library paths, and more that we now have to handle manually when
  # building with subprojects.
  subprojdir = 'subprojects'
  scanner_command += [
    '--extra-library=glib-2.0',
    '--extra-library=gmodule-2.0',
    '--extra-library=gobject-2.0',
    '--extra-library=gio-2.0',
  ]
endif

if get_option('gi_cross_binary_wrapper') != ''
  scanner_command += ['--use-binary-wrapper=' + get_option('gi_cross_binary_wrapper')]
endif
if get_option('gi_cross_ldd_wrapper') != ''
  scanner_command += ['--use-ldd-wrapper=' + get_option('gi_cross_ldd_wrapper')]
endif
# Take a glob and print to newlines
globber = '''
from glob import glob

# Sort the glob for stable results. Also ensures that gmarshal.h is not
# #include-ed by scannerparser.y first because it does not include glib.h
# itself, which leaves several defines unresolved.
for f in sorted(glob('@0@')):
  print(f)
'''

typelibs = []

gnome = import('gnome')

gir_extra_args = [
  '--accept-unprefixed',
  '--quiet',
  '--warn-all',
  '-D__G_I18N_LIB_H__',
  '-DGETTEXT_PACKAGE=Dummy',
]

# GLib
glib_files = []

if dep_type == 'pkgconfig'
  glib_libdir = get_option('gi_cross_pkgconfig_sysroot_path') + glib_dep.get_variable(pkgconfig: 'libdir')
  glib_incdir = get_option('gi_cross_pkgconfig_sysroot_path') + join_paths(glib_dep.get_variable(pkgconfig: 'includedir'), 'glib-2.0')
  glib_libincdir = join_paths(glib_libdir, 'glib-2.0', 'include')
  glib_files += join_paths(glib_incdir, 'gobject', 'gobject-visibility.h')
  glib_files += join_paths(glib_incdir, 'gobject', 'glib-types.h')
  glib_files += join_paths(glib_libincdir, 'glibconfig.h')
  if giounix_dep.found()
    glib_files += join_paths(glib_incdir, 'glib-unix.h')
  endif
  # Parse glob to get installed header list
  ret = run_command(python, '-c', globber.format(join_paths(glib_incdir, 'glib', '*.h')), check: true)
  if ret.returncode() != 0
    error('Failed to get glib header list')
  endif
  glib_headers = ret.stdout().strip().split('\n')
  # Get a list of all source files
  glib_srcdir = get_option('glib_src_dir')
  if glib_srcdir != ''
    ret = run_command(python, '-c', globber.format(join_paths(glib_srcdir, 'glib', '*.c')), check: true)
    if ret.returncode() != 0
      error('Failed to get glib source list')
    endif
    glib_files += ret.stdout().strip().split('\n')
  endif
  glib_gir_dep = dependency('glib-2.0')
elif dep_type == 'internal'
  glib_subproject = subproject('glib')
  glib_files += glib_subproject.get_variable('glib_sources')
  glib_headers = glib_subproject.get_variable('glib_headers') + glib_subproject.get_variable('glib_sub_headers')
  glib_gir_dep = glib_subproject.get_variable('libglib')
else
  error('Unknown glib dependency type: ' + dep_type)
endif

foreach h : glib_headers
  hstr = '@0@'.format(h)
  if not hstr.endswith('autocleanups.h')
    glib_files += h
  endif
endforeach

# NOTE: Always add this last so that we prefer the annotations in the sources
# (if they are available) since it contains 'backup' annotations that can be
# out of date.
glib_files += files('glib-2.0.c')

glib_gi_target = gnome.generate_gir(
  glib_gir_dep,
  sources: glib_files,
  namespace: 'GLib',
  nsversion: '2.0',
  identifier_prefix: 'G',
  symbol_prefix: 'g',
  header: 'glib.h',
  install: true,
  extra_args: gir_extra_args + ['-DGLIB_COMPILATION'],
  export_packages: ['glib-2.0']
)
typelibs += glib_gi_target[1]

# GObject
gobject_files = []

if dep_type == 'pkgconfig'
  # Get the installed header list
  ret = run_command(python, '-c', globber.format(join_paths(glib_incdir, 'gobject', '*.h')), check: true)
  if ret.returncode() != 0
    error('Failed to get gobject header list')
  endif
  gobject_headers = ret.stdout().strip().split('\n')
  if glib_srcdir != ''
    ret = run_command(python, '-c', globber.format(join_paths(glib_srcdir, 'gobject', '*.c')), check: true)
    if ret.returncode() != 0
      error('Failed to get gobject source list')
    endif
    gobject_files += ret.stdout().strip().split('\n')
  endif
  gobject_gir_dep = dependency('gobject-2.0')
else
  gobject_headers = glib_subproject.get_variable('gobject_install_headers')
  gobject_files += [
    glib_subproject.get_variable('gobject_sources'),
  ]
  gobject_gir_dep = glib_subproject.get_variable('libgobject')
endif

foreach h : gobject_headers
  hstr = '@0@'.format(h)
  if not hstr.endswith('autocleanups.h') and not hstr.endswith('glib-types.h') and not hstr.endswith('gvaluecollector.h') and not hstr.endswith('glib-enumtypes.h')
    gobject_files += h
  endif
endforeach

# NOTE: Always add this last so that we prefer the annotations in the sources
# (if they are available) since it contains 'backup' annotations that can be
# out of date.
gobject_files += files('gobject-2.0.c')

gobject_gi_target = gnome.generate_gir(
  gobject_gir_dep,
  sources: gobject_files,
  namespace: 'GObject',
  nsversion: '2.0',
  identifier_prefix: 'G',
  symbol_prefix: 'g',
  header: 'glib-object.h',
  install: true,
  includes: [glib_gi_target[0]],
  extra_args: gir_extra_args + ['-DGLIB_COMPILATION', '-DGOBJECT_COMPILATION'],
  export_packages: ['gobject-2.0']
)
typelibs += gobject_gi_target[1]

# GModule
gmodule_files = []

if dep_type == 'pkgconfig'
  gmodule_files += join_paths(glib_incdir, 'gmodule.h')
  if glib_srcdir != ''
    gmodule_files += join_paths(glib_srcdir, 'gmodule', 'gmodule.c')
  endif
  gmodule_gir_dep = dependency('gmodule-2.0')
else
  gmodule_files += [
    glib_subproject.get_variable('gmodule_h'),
    glib_subproject.get_variable('gmodule_c'),
    glib_subproject.get_variable('gmoduleconf_h'),
  ]
  gmodule_gir_dep = glib_subproject.get_variable('libgmodule')
endif

# NOTE: Always add this last so that we prefer the annotations in the sources
# (if they are available) since it contains 'backup' annotations that can be
# out of date.
gmodule_files += files('gmodule-2.0.c')

gmodule_gi_target = gnome.generate_gir(
  gobject_gir_dep,
  sources: gmodule_files,
  namespace: 'GModule',
  nsversion: '2.0',
  identifier_prefix: 'G',
  symbol_prefix: 'g',
  header: 'gmodule.h',
  install: true,
  includes: [glib_gi_target[0], gobject_gi_target[0]],
  extra_args: gir_extra_args + ['-DGMODULE_COMPILATION'],
  export_packages: ['gmodule-2.0']
)
typelibs += gmodule_gi_target[1]

## Gio
gio_files = []

if dep_type == 'pkgconfig'
  # Get the installed header list
  ret = run_command(python, '-c', globber.format(join_paths(glib_incdir, 'gio', '*.h')), check: true)
  if ret.returncode() != 0
    error('Failed to get gio header list')
  endif
  gio_headers = ret.stdout().strip().split('\n')
  # Get all gio (and gio-unix) sources. This is not entirely correct, but it's
  # probably fine since it matches what Autotools does. We are more exact in
  # the subproject case.
  if glib_srcdir != ''
    ret = run_command(python, '-c', globber.format(join_paths(glib_srcdir, 'gio', '*.c')), check: true)
    if ret.returncode() != 0
      error('Failed to get gio source list')
    endif
    gio_files += ret.stdout().strip().split('\n')
  endif
  gio_gir_dep = dependency('gio-2.0')
else
  gio_headers = glib_subproject.get_variable('gio_headers')
  gio_sources = glib_subproject.get_variable('gio_sources')
  foreach f : gio_sources
    hstr = '@0@'.format(f)
    # Filter out files with private API
    if not hstr.contains('xdp-dbus')
      gio_files += f
    endif
  endforeach
  gio_files += [
    glib_subproject.get_variable('gioenumtypes_h'),
    glib_subproject.get_variable('gnetworking_h'),
  ]
  gio_gir_dep = glib_subproject.get_variable('libgio')
endif

gio_gi_headers = ['gio/gio.h']
gio_gi_export_packages = ['gio-2.0']
if giounix_dep.found()
  dep_type = giounix_dep.type_name()
  gio_gi_export_packages += ['gio-unix-2.0']
  if dep_type == 'pkgconfig'
    giounix_includedir = get_option('gi_cross_pkgconfig_sysroot_path') + join_paths(giounix_dep.get_variable(pkgconfig: 'includedir'), 'gio-unix-2.0')
    # Get the installed gio-unix header list
    ret = run_command(python, '-c', globber.format(join_paths(giounix_includedir, 'gio', '*.h')), check: true)
    if ret.returncode() != 0
      error('Failed to get gio-unix header list')
    endif
    giounix_headers = ret.stdout().strip().split('\n')
  else
    giounix_headers = glib_subproject.get_variable('gio_unix_include_headers')
  endif
  # No filtering needed
  gio_files += giounix_headers
  # GIO Unix headers must be included explicitly since there is no catch-all
  # header that includes all of them unlike gio/gio.h above
  foreach header : giounix_headers
    hstr = '@0@'.format(header)
    hbase = hstr.split('/')[-1]
    gio_gi_headers += 'gio/@0@'.format(hbase)
  endforeach
endif

foreach h : gio_headers
  hstr = '@0@'.format(h)
  if not hstr.endswith('autocleanups.h')
    gio_files += h
  endif
endforeach


# NOTE: Always add this last so that we prefer the annotations in the sources
# (if they are available) since it contains 'backup' annotations that can be
# out of date.
gio_files += files('gio-2.0.c')

gio_gi_target = gnome.generate_gir(
  gio_gir_dep,
  sources: gio_files,
  namespace: 'Gio',
  nsversion: '2.0',
  identifier_prefix: 'G',
  symbol_prefix: 'g',
  header: gio_gi_headers,
  install: true,
  includes: [glib_gi_target[0], gobject_gi_target[0], gmodule_gi_target[0]],
  extra_args: gir_extra_args + ['-DGIO_COMPILATION', '-DG_SETTINGS_ENABLE_BACKEND'],
  export_packages: gio_gi_export_packages
)
typelibs += gio_gi_target[1]

# GIRepository

girepository_gi_target = gnome.generate_gir(
  gio_gir_dep,
  sources: girepo_gir_sources,
  namespace: 'GIRepository',
  nsversion: '2.0',
  identifier_prefix: 'G',
  symbol_prefix: ['g', 'gi'],
  header: ['girepository.h'],
  install: true,
  includes: [glib_gi_target[0], gobject_gi_target[0]],
  extra_args: gir_extra_args + ['-DGI_COMPILATION'],
  export_packages: ['gobject-introspection-1.0']
)
typelibs += girepository_gi_target[1]

rnc2rng = find_program('rnc2rng', native: true, required: false)
if rnc2rng.found()
  validate_gir = find_program('../misc/validate-gir.py')

  foreach gir: gir_files
    test('validate-@0@'.format(gir).underscorify(),
      python,
      args: [
        validate_gir.full_path(),
        '--schema=@0@'.format(meson.current_source_dir() / '../docs/gir-1.2.rnc'),
        gir,
      ],
      workdir: meson.current_build_dir(),
    )
  endforeach

  foreach gir: uninstalled_gir_files
    gir_name = gir.full_path().split('/')[-1]
    test('validate-@0@'.format(gir_name).underscorify(),
      python,
      args: [
        validate_gir.full_path(),
        '--schema=@0@'.format(meson.current_source_dir() / '../docs/gir-1.2.rnc'),
        gir,
      ],
    )
  endforeach
endif
