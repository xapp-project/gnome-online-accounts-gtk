bin_conf = configuration_data()
bin_conf.set('bindir', get_option('prefix') / get_option('bindir'))

# Desktop file
configure_file(
  input: 'gnome-online-accounts-gtk.desktop.in.in',
  output: 'gnome-online-accounts-gtk.desktop',
  configuration: bin_conf,
  install: true,
  install_dir: datadir / 'applications',
)


i18n.merge_file(
   input: configure_file(
   input: 'gnome-online-accounts-gtk.desktop.in.in',
   output: 'gnome-online-accounts-gtk.desktop.in',
   configuration: bin_conf
   ),
   output: 'gnome-online-accounts-gtk.desktop',
   type: 'desktop',
   po_dir: join_paths(meson_source_root, 'po'),
   install: true,
   install_dir: datadir / 'applications'
)

subdir('icons')

# CSS stylesheet

install_data('style.css', 
  install_dir: join_paths(datadir, meson.project_name())
)
