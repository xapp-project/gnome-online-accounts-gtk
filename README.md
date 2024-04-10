# GNOME Online Accounts GTK

A GTK Frontend for GNOME Online Accounts

![image](https://i.imgur.com/8eq39QC.png)

# Supported desktops/distributions

GNOME Online Accounts GTK is part of the XApp project.

It is designed to work in all desktop environments and all Linux distributions.

# Branches

The `master` branch produces 3.50.x releases, built for GTK4 and GNOME Online Acccounts 3.50.

The `3.49` branch produces 3.49.x releases, built for GTK3 and GNOME Online Accounts 3.49.

# Dependencies

## 3.50.x

- libgoa (>= 3.50)
- libgoa-backend (>= 3.50)
- gtk4
- glib
- meson (>= 0.53.0)

## 3.49.x

- libgoa (>= 3.49.2)
- libgoa-backend (>= 3.49.2)
- gtk3
- glib
- meson (>= 0.53.0)

## Building and installing from source

### Debian and derivatives

```bash
# install build tools if necessary
sudo apt install build-essential devscripts equivs git meson

# clone this git repository, switch into cloned directory
git clone https://github.com/linuxmint/gnome-online-accounts-gtk.git && cd gnome-online-accounts-gtk

# generate build-dependency package and install it
mk-build-deps -s sudo -i

# build .deb packages
debuild --no-sign

# install packages
sudo debi
```

## Other Distributions

The concrete packages to install depend on your distro.
Please note that we can't guarantee that the available library versions are compatible.

1. Install the following build tools
   1. meson
   2. ninja
2. Install the libraries mentioned above in the Dependencies section
3. Clone this git repository and run the following commands in the cloned directory:

```bash
# prepare build, options can be set with -Doption=value
# Please check the output for errors and the effective build options.
meson --prefix=/usr/local build

# compile and install
sudo ninja -C build install

# to uninstall:
sudo ninja -C build uninstall
```

# Translations

The translations for this project are hosted on [Launchpad](https://translations.launchpad.net/linuxmint/latest/+pots/gnome-online-accounts-gtk).

The content of `po/` is overwritten when the translations are imported from Launchpad.

If you want to help translating this project, please use Launchpad. Do not modify the files in `po/` directly.

