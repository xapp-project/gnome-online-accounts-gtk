gnome = import('gnome')
pkg = import('pkgconfig')

top_srcdir = meson.project_source_root()

config_entries = {
    'GETTEXT_PACKAGE' : '"@0@"'.format(meson.project_name()),
    'LOCALEDIR': '"@0@"'.format(prefix / get_option('localedir')),
    'PACKAGE_STRING': '"gnome-online-accounts-gtk @0@"'.format(meson.project_version())
}
config = configuration_data(config_entries)
configure_file(output: 'config.h', configuration: config)

deps = [
  dependency('glib-2.0', version: '>= 2.44'),
  dependency('gtk4'),
  dependency('goa-backend-1.0', version: '>= 3.25.3'),
]

resources = gnome.compile_resources('gnome-online-accounts-gtk-resources',
    'gnome-online-accounts-gtk.gresource.xml',
    c_name: 'oa_resources',
    source_dir: meson.current_source_dir(),
)

sources = files(
    'gnome-online-accounts-gtk.c'
)

executable(
  'gnome-online-accounts-gtk',
  sources + resources,
  dependencies: deps,
  install: true
)
