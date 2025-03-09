# Building and Packaging Quaternion

[![license](https://img.shields.io/github/license/quotient-im/quaternion.svg)](https://github.com/quotient-im/Quaternion/blob/dev/LICENSES/GPL-3.0-or-later.txt)
![CI Status](https://img.shields.io/github/actions/workflow/status/quotient-im/Quaternion/ci.yml)
![](https://img.shields.io/github/commit-activity/y/quotient-im/libQuotient.svg)
[![](https://img.shields.io/matrix/quaternion:matrix.org.svg)](https://matrix.to/#/#quaternion:matrix.org)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)


### Getting the source code

The source code is hosted at GitHub: https://github.com/quotient-im/Quaternion.
The best way for one-off building is checking out a tag for a given release
from GitHub (make sure to pass `--recurse-submodules` to `git checkout` if you
use Option 2 - see below). If you plan to work on Quaternion code, feel free
to fork/clone the repo and base your changes on the master branch.

Quaternion needs libQuotient to build. There are two options to use the library:
1. Use a library installation known to CMake - either as a package available
   from your package repository (possibly but not necessarily system-wide),
   or as a result of building the library from the source code in another
   directory. In the latter case CMake internally registers the library
   upon succesfully building it so you shouldn't even need to pass
   `CMAKE_PREFIX_PATH` (still better do pass it, to avoid surprises).
2. As a Git submodule. If you haven't cloned Quaternion sources yet,
   the following will get you all sources in one go:
   ```bash
   git clone --recursive https://github.com/quotient-im/Quaternion.git
   ```
   If you already have cloned Quaternion, do the following in the top-level
   directory (NOT in `lib` subdirectory):
   ```bash
   git submodule init
   git submodule update
   ```
   In either case here, to correctly check out a given tag or branch, make sure
   to also check out submodules:
   ```bash
   git checkout --recurse-submodules <ref>
   ```

Depending on your case, either option can be preferrable. General guidance is:
- Option 1 is strongly recommended for packaging and also good for development
  on Quaternion without changing libQuotient;
- Option 2 is better for one-off building and for active development when
  _both_ Quaternion and libQuotient get changed.
  
These days Option 2 is used by default (with a fallback to Option 1 if no
libQuotient is found under `lib/`). To override that you can pass
`USE_INTREE_LIBQMC` option to CMake: `-DUSE_INTREE_LIBQMC=0` (or `NO`, or `OFF`)
will force Option 1 (using an external libQuotient even when a submodule is
there). The other way works too: if you intend to use libQuotient from
the submodule, pass `-DUSE_INTREE_LIBQMC=1` (or `YES`, or `ON`) to make
sure the build configuration process fails instead of finding an external
libQuotient somewhere when a submodule is unusable for some reason (e.g. when
`--recursive` has been forgotten when cloning).

(Why `LIBQMC`, you ask? Because the old name of libQuotient was libQMatrixClient
and this particular variable still wasn't updated. This might be the last place
using the old name.)

### Pre-requisites
- a recent Linux, macOS or Windows system (desktop versions tried; mobile
  platforms might work too but never tried)
  - Recent enough Linux examples: Debian Bookworm; Fedora 39 or CentOS Stream 9;
    OpenSUSE Leap 15.6; Ubuntu 24.04 (noble)
- Qt 6.4 or newer (either Open Source or Commercial)
- CMake 3.16 or newer (from your package management system or
  [the official website](https://cmake.org/download/))
- A C++ toolchain with solid C++20 support and elements of C++23:
  - GCC 13 (Windows, Linux, macOS), Clang 16 (Linux), Apple Clang 15 (macOS)
    and Visual Studio 2022 (Windows) are the oldest officially supported.
- Any build system that works with CMake should be fine:
  GNU Make, ninja (any platform), NMake, jom (Windows) are known to work.
- optionally, development files for libQuotient 0.9.2 or newer (from your package 
  management system), or prebuilt libQuotient (see "Getting the source code" above);
  libQuotient 0.8.x is _not_ compatible with any Quaternion 0.0.97 release.
- libQuotient dependendencies (see lib/README.md):
  - [Qt Keychain](https://github.com/frankosterfeld/qtkeychain)
  - libolm 3.2.5 or newer (the latest 3.x strongly recommended)
  - OpenSSL 3.x (the version Quaternion runs with must be the same as
    the version used to build Quaternion - or libQuotient, if libQuotient is
    built/installed separately).

#### Linux
Just install things from the list above using your preferred package manager.
If your Qt package base is fine-grained you might want to take a look at
`CMakeLists.txt` to figure out which specific libraries Quaternion uses
(or blindly run cmake and look at error messages). Note also that you'll need
several Qt Quick plugins for Quaternion to work (without them, it will compile
and run but won't show the messages timeline).

On Debian/Ubuntu, the following line should get you everything necessary
to build and run Quaternion:
```bash
sudo apt-get install cmake qt6-declarative-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qml6-module-qtquick-controls qt6-multimedia-dev qtkeychain-qt6-dev libolm-dev libssl-dev
```

On Fedora, the following command should be enough for building and running:
```bash
sudo dnf install cmake qt6-qtdeclarative-devel qt6-qtbase-private-devel qt6-qtmultimedia-devel qt6-qttools-devel qtkeychain-qt6-devel libolm-devel openssl-devel
```

#### macOS
`brew install qt qtkeychain libolm openssl` should get you Qt 6, the matching
build of QtKeychain, and good versions of E2EE dependencies. You have to point
CMake at the installation locations for your libraries, e.g. by adding
`$(brew --prefix qt)` and similar for other libraries to the first cmake
invocation, as follows:
```bash
# if using in-tree libQuotient:
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix qtkeychain)$(brew --prefix libolm);$(brew --prefix openssl)"
# or otherwise:
cmake .. -DCMAKE_PREFIX_PATH="/path/to/libQuotient;$(brew --prefix qt);$(brew --prefix qtkeychain)$(brew --prefix libolm);$(brew --prefix openssl)"
```

#### Windows
1. Install CMake. The commands in further sections imply that cmake is in your
   PATH - otherwise you have to prepend them with actual paths.
1. Install Qt 6, using their official installer.
1. Make sure CMake knows about Qt and the toolchain - the easiest way is to run
   `qtenv*.bat` script that can be found in `C:\Qt\<Qt version>\<toolchain>\bin`
   (assuming you installed Qt to `C:\Qt`). The only thing it does is adding
   necessary paths to `PATH` - you might not want to run it on system startup
   but it's very handy to setup environment before building.
   Setting `CMAKE_PREFIX_PATH` also helps.
1. Get and build [Qt Keychain](https://github.com/frankosterfeld/qtkeychain).
1. Install E2EE dependencies as described in [lib/README.md](lib/README.md),
   section "Building the library".

### Build
In the root directory of the project sources:
```bash
mkdir build_dir
cd build_dir
cmake .. # Pass -D<variable> if needed, see below
cmake --build . --target all
```
This will get you an executable in `build_dir` inside your project sources.
Noteworthy CMake variables that you can use:
- `-DCMAKE_PREFIX_PATH=/path` - add a path to CMake's list of searched paths
  for preinstalled software (Qt, libQuotient, QtKeychain); multiple paths are
  separated by `;` (semicolons).
- `-DCMAKE_INSTALL_PREFIX=/path` - controls where Quaternion will be installed
  (see below on installing from sources).
- `-DUSE_INTREE_LIBQMC=<ON|OFF>` - force using/not-using the in-tree copy of
  libQuotient sources (see "Getting the source code" above).

### Install
In the root directory of the project sources: `cmake --build build_dir --target install`.

### Building as Flatpak
If you run Linux and your distribution supports flatpak, you can easily build
and install Quaternion as a flatpak package. Make sure to have flatpak-builder
installed and then do the following:

```bash
# Optionally, get the source code if not yet
git clone https://github.com/quotient-im/Quaternion.git --recursive
cd Quaternion/flatpak
./setup_runtime.sh
./build.sh
flatpak --user install quaternion-nightly io.github.quotient_im.Quaternion
```
Whenever you want to update your Quaternion package, do the following from the flatpak directory:

```bash
./build.sh
flatpak --user update
```

Be mindful that since Quaternion 0.0.97 beta the Flatpak app-id has changed: it used to be
`com.github.quaternion`, now it's `io.github.quotient_im.quaternion`, to align with
[Flathub verification rules](https://docs.flathub.org/docs/for-app-authors/verification/).
Normally, Flatpak should seamlessly handle an upgrade; if it doesn't, send us an issue.

## Troubleshooting

If `cmake` fails with...
```
CMake Warning at CMakeLists.txt:11 (find_package):
  By not providing "FindQt5Widgets.cmake" in CMAKE_MODULE_PATH this project
  has asked CMake to find a package configuration file provided by
  "Qt5Widgets", but CMake did not find one.
```
...or a similar error referring to Qt5Something - make sure that your
`CMAKE_PREFIX_PATH` actually points to the location where Qt is installed
and that the respective development package is installed (hint: check which
package provides `cmake(Qt5Widgets)`, replacing `Qt5Widgets` with what your
error says).

If `cmake` fails with...
```
CMake Error at CMakeLists.txt:30 (add_subdirectory):
  The source directory

    <quaternion-source-directory>/lib

  does not contain a CMakeLists.txt file.
```
...then you don't have libQuotient sources - most likely because you didn't do
the `git submodule init && git submodule update` dance and don't have
libQuotient development files elsewhere - also, see the beginning of this file.

If you have made sure that your toolchain is in order (versions of compilers
and Qt are among supported ones, `PATH` is set correctly etc.) but building
fails with strange Qt-related errors such as not found symbols or undefined
references
([like in this issue, e.g.](https://github.com/quotient-im/Quaternion/issues/185)),
double-check that you don't mix different versions of Qt. If you need those
packages reinstalling them may help; but if you use that other Qt version by
default to build other projects, you have to explicitly pass the location of
the non-default Qt installation to CMake (see notes about `CMAKE_PREFIX_PATH`
in "Building").

See also the Troubleshooting section in [README.md](./README.md)
