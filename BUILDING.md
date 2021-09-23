# Building and Packaging Quaternion

[![Quaternion-master@Travis](https://img.shields.io/travis/quotient-im/Quaternion/master.svg)](https://travis-ci.org/quotient-im/Quaternion/branches)
[![Quaternion-master@AppVeyor](https://img.shields.io/appveyor/ci/quotient/quaternion/master.svg?logo=appveyor)](https://ci.appveyor.com/project/quotient/quaternion)
[![license](https://img.shields.io/github/license/quotient-im/quaternion.svg)](https://github.com/quotient-im/Quaternion/blob/master/COPYING)
[![Chat](https://img.shields.io/badge/chat-%23quaternion-blue.svg)](https://matrix.to/#/#quaternion:matrix.org)
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

### Pre-requisites
- a recent Linux, macOS or Windows system (desktop versions tried; mobile
  platforms might work too but never tried)
  - Recent enough Linux examples: Debian Buster; Fedora 28; OpenSUSE Leap 15;
    Ubuntu Bionic Beaver.
- Qt 5 (either Open Source or Commercial), version 5.11 or higher
  (5.14+ is recommended). Quaternion 0.0.95 and earlier does not build with Qt 6.
- CMake 3.10 or newer (from your package management system or
  [the official website](https://cmake.org/download/))
- A C++ toolchain with C++17 support:
  - GCC 7 (Windows, Linux, macOS), Clang 6 (Linux), Apple Clang 10 (macOS)
    and Visual Studio 2017 (Windows) are the oldest officially supported.
- Any build system that works with CMake should be fine:
  GNU Make, ninja (any platform), NMake, jom (Windows) are known to work.
- optionally, libQuotient 0.6.x development files (from your package management
  system), or prebuilt libQuotient (see "Getting the source code" above).
  libQuotient 0.7 (in development as of this writing) is _not_ compatible with
  Quaternion 0.0.95.
- optionally (but strongly recommended),
  [QtKeychain](https://github.com/frankosterfeld/qtkeychain) to store
  access tokens in libsecret keyring or similar providers.

#### Linux
Just install things from the list above using your preferred package manager.
If your Qt package base is fine-grained you might want to take a look at
`CMakeLists.txt` to figure out which specific libraries Quaternion uses
(or blindly run cmake and look at error messages). Note also that you'll need
several Qt Quick plugins for Quaternion to work (without them, it will compile
and run but won't show the messages timeline). On Debian/Ubuntu, the following
line should get you everything necessary to build and run Quaternion:
```bash
sudo apt-get install cmake qtdeclarative5-dev qttools5-dev qml-module-qtquick-controls qml-module-qtquick-controls2 qtmultimedia5-dev
```
To enable keyring support, also install QtKeychain by
```bash
sudo apt-get install qt5keychain-dev
```
On Fedora 28, the following command should be enough for building and running:
```bash
sudo dnf install cmake qt5-qtdeclarative-devel qt5-qtquickcontrols qt5-qtquickcontrols2 qt5-qtmultimedia-devel
```
and QtKeychain can be installed with
```bash
sudo dnf install qtkeychain-qt5-devel
```

#### macOS
`brew install qt5` should get you Qt5. If you want to build with QtKeychain,
also call `brew install qtkeychain`.

You have to point CMake at the Qt5 installation location, with something like:
```bash
# if using in-tree libQuotient:
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
# or otherwise...
cmake .. -DCMAKE_PREFIX_PATH=/path/to/libQuotient -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
```

#### Windows
1. Install CMake. The commands in further sections imply that cmake is in your
   PATH - otherwise you have to prepend them with actual paths.
1. Install Qt5, using their official installer.
1. Make sure CMake knows about Qt and the toolchain - the easiest way is to run
   `qtenv2.bat` script that can be found in `C:\Qt\<Qt version>\<toolchain>\bin`
   (assuming you installed Qt to `C:\Qt`). The only thing it does is adding
   necessary paths to `PATH` - you might not want to run it on system startup
   but it's very handy to setup environment before building.
   Setting `CMAKE_PREFIX_PATH`, the same way as for macOS (see above), also helps.
1. If needed, get and build
   [QtKeychain](https://github.com/frankosterfeld/qtkeychain).

### Build
In the root directory of the project sources:
```bash
mkdir build_dir
cd build_dir
cmake .. # Pass -D<variable> if needed
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
- `-DUSE_QQUICKWIDGET=<ON|OFF>` - by default it's `ON` with Qt 5.12 and `OFF`
  with earlier versions. See the next section for details.

#### QQuickWidget

Quaternion uses a combination of Qt Widgets and QML to render its UI (this
_might_ change at some point in time but certainly not in any near future).
Internally, embedding QML in a Qt Widget used to be done in two ways providing
very similar API but different underneath: swallowing a `QQuickView` object in
a window container and using `QQuickWidget`. Historically Quaternion used
the former method; however, `QQuickView` implementation in Qt is so ugly that
[it's officially deprecated by The Qt Project](https://blog.qt.io/blog/2014/07/02/qt-weekly-16-qquickwidget/).

Quaternion suffered from that ugliness too (see, e.g., #355 - a completely blank
timeline despite the QML engine being up and running). As of now, Quaternion
uses better and more reliable `QQuickWidget` when built with Qt 5.12 or newer.
Unfortunately, Quaternion's QML is tricky enough to crash the less mature
`QQuickWidget` code on Qt versions before 5.12, so if you have to use an older
Qt version Quaternion will build with `QQuickView` by default.

To override the defaults you can pass `-DUSE_QQUICKWIDGET=ON` to the first
(configuring) `cmake` invocation to force usage of `QQuickWidget` even with
older Qt; or `-DDISABLE_QQUICKWIDGET=ON` to force usage of `QQuickView` with
newer Qt - by the way, if you have to do that because of some problem with
`QQuickWidget`, please file an issue at Quaternion).

With `QQuickWidget` considered stable and reliable now, `QQuickView` mode
will be entirely deprecated in Quaternion 0.0.96 (that will require Qt 5.12+),
and later removed.

### Install
In the root directory of the project sources: `cmake --build build_dir --target install`.

If you use GNU Make, `make install` (with `sudo` if needed) will work equally well.

### Building as Flatpak
If you run Linux and your distribution supports flatpak, you can easily build and install Quaternion as a flatpak package:

```bash
git clone https://github.com/quotient-im/Quaternion.git --recursive
cd Quaternion/flatpak
./setup_runtime.sh
./build.sh
flatpak --user install quaternion-nightly com.github.quaternion
```
Whenever you want to update your Quaternion package, do the following from the flatpak directory:

```bash
./build.sh
flatpak --user update
```

## Troubleshooting

If `cmake` fails with...
```
CMake Warning at CMakeLists.txt:11 (find_package):
  By not providing "FindQt5Widgets.cmake" in CMAKE_MODULE_PATH this project
  has asked CMake to find a package configuration file provided by
  "Qt5Widgets", but CMake did not find one.
```
...or a similar error referring to Qt5 - make sure that your `CMAKE_PREFIX_PATH`
actually points to the location where Qt5 is installed, see above.

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
references, double-check that you don't have Qt 4.x (or Qt 6.x) packages around
([here is a typical example](https://github.com/quotient-im/Quaternion/issues/185)).
If you need those packages reinstalling them may help; but if you use Qt4/6
by default you have to explicitly pass Qt5 location to CMake (see notes about
`CMAKE_PREFIX_PATH` in "Building").

See also the Troubleshooting section in [README.md](./README.md)
