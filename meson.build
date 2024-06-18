project('gnome-online-accounts-gtk', 'c', version : '3.50.3', meson_version : '>= 0.56.0', license : 'GPLv3.0+')

i18n = import('i18n')
gettext_package = meson.project_name()
locale_dir = join_paths(get_option('prefix'), get_option('localedir'))

prefix = get_option('prefix')
datadir = get_option('datadir')
libdir = get_option('libdir')
libexecdir = get_option('libexecdir')

meson_source_root = meson.current_source_dir()

if not get_option('deprecated_warnings')
  add_global_arguments([
      '-Wno-deprecated-declarations',
      '-Wno-deprecated',
      '-Wno-declaration-after-statement',
      '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    ],
    language: 'c',
  )
endif

subdir('data')
subdir('po')
subdir('src')

summary({
  'prefix': prefix,
  'datadir': datadir,
  'libdir': libdir,
  'libexecdir': libexecdir
})
