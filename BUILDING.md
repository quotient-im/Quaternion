# Building and Packaging Quaternion

[![Quaternion-master@Travis](https://img.shields.io/travis/quotient-im/Quaternion/master.svg)](https://travis-ci.org/quotient-im/Quaternion/branches)
[![Quaternion-master@AppVeyor](https://img.shields.io/appveyor/ci/quotient/quaternion/master.svg?logo=appveyor)](https://ci.appveyor.com/project/quotient/quaternion)
[![license](https://img.shields.io/github/license/quotient-im/quaternion.svg)](https://github.com/quotient-im/Quaternion/blob/master/COPYING)
[![Chat](https://img.shields.io/badge/chat-%23quaternion-blue.svg)](https://matrix.to/#/#quaternion:matrix.org)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)


### Getting the source code

The source code is hosted at GitHub: https://github.com/quotient-im/Quaternion. The best way for one-off building is checking out a tag for a given release from GitHub (master branch is unstable and may or may not work). If you plan to work on Quaternion code, feel free to fork/clone the repo and check out the master branch.

Quaternion needs libQuotient to build. Since version 0.0.9.3 there are two
options to use the library:
1. Use the library installation known to CMake - either as a (possibly but not
   necessarily system-wide) package available from your favourite package
   repository, or as a result of building the library from the source code in
   another directory. In the latter case CMake internally registers the
   library upon succesfully building it so you shouldn't even need to pass
   `CMAKE_PREFIX_PATH`. This option can be forced by passing
   `-DUSE_INTREE_LIBQMC=0` (or `NO`, or `OFF`) to the configuring
   invocation of CMake (the one _without_ `--build`); alternatively, the build
   system will fall back to this option if it doesn't find in-tree sources
   for the library (under `lib/`) and `USE_INTREE_LIBQMC` is unset completely.
2. As a Git submodule. This option can be forced (disabling the fallback
   mentioned above) by passing `-DUSE_INTREE_LIBQMC=1` (or `YES`, or `ON`)
   to CMake. If you haven't cloned Quaternion sources yet, the following will
   get you all sources in one go:
   ```bash
   git clone --recursive https://github.com/quotient-im/Quaternion.git
   ```
   If you already have cloned Quaternion, do the following in the top-level
   directory (NOT in `lib` subdirectory):
   ```bash
   git submodule init
   git submodule update
   ```

Depending on your case, either option can be preferrable. For instance, Option 2
is more convenient if you're actively hacking on Quaternion and libQuotient
at the same time. On the other hand, packagers should make a separate package
for libQuotient so should use Option 1. 0.0.9.3 is the only version using
Option 1 as default; due to popular demand, Option 2 is used by default
(with a fallback to Option 1) from 0.0.9.4 beta onwards.

### Pre-requisites
- a recent Linux, macOS or Windows system (desktop versions tried; mobile
  Linux/Windows might work too but never tried)
  - Recent enough Linux examples: Debian Buster; Fedora 28; OpenSUSE Leap 15;
    Ubuntu Bionic Beaver.
- Qt 5 (either Open Source or Commercial), version 5.9 or higher
  (5.12 is recommended)
- CMake 3.10 or newer (from your package management system or
  [the official website](https://cmake.org/download/))
- A C++ toolchain with C++17 support:
  - GCC 7 (Windows, Linux, macOS), Clang 6 (Linux), Apple Clang 10 (macOS)
    and Visual Studio 2017 (Windows) are the oldest officially supported.
- Any build system that works with CMake should be fine:
  GNU Make, ninja (any platform), NMake, jom (Windows) are known to work.
- optionally, libQuotient 0.6 development files (from your package management
  system), or prebuilt libQuotient (see "Getting the source code" above).
- optionally (but strongly recommended),
  [QtKeychain](https://github.com/frankosterfeld/qtkeychain) to store
  access tokens in libsecret keyring or similar providers.

#### Linux
Just install things from the list above using your preferred package manager.
If your Qt package base is fine-grained you might want to take a look at
`CMakeLists.txt` to figure out which specific libraries Quaternion uses
(or blindly run cmake and look at error messages). Note also that you'll need
several Qt Quick plugins for Quaternion to work (without them, it will compile
and run but won't show the messages timeline). The following line should get you
everything necessary to build and run Quaternion:
```bash
sudo apt-get install cmake qtdeclarative5-dev qttools5-dev qml-module-qtquick-controls qml-module-qtquick-controls2 qtmultimedia5-dev
```
To enable libsecret keyring support, install QtKeychain by
```bash
sudo apt-get install qt5keychain-dev
```
On Fedora 28, the following command should be enough for building and running:
```bash
dnf install cmake qt5-qtdeclarative-devel qt5-qtquickcontrols qt5-qtquickcontrols2 qt5-qtmultimedia-devel
```

#### macOS
`brew install qt5` should get you Qt5. If you want to build with QtKeychain,
call `brew install qtkeychain`.

You have to point CMake at the Qt5 installation location, with something like:
```bash
# if using in-tree libQuotient:
cmake .. -DUSE_QQUICKWIDGET=ON -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
# or otherwise...
cmake .. -DCMAKE_PREFIX_PATH=/path/to/libQuotient -DUSE_QQUICKWIDGET=ON -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
```
(read on to find out about `USE_QQUICKWIDGET`).

#### Windows
1. Install CMake. The commands in further sections imply that cmake is in your
   PATH - otherwise you have to prepend them with actual paths.
1. Install Qt5, using their official installer.
1. Optionally, get the QtKeychain sources and make them available to CMake
   (TODO: elaborate the steps).
1. Make sure CMake knows about Qt and the toolchain - the easiest way is to run
   `qtenv2.bat` script that can be found in `C:\Qt\<Qt version>\<toolchain>\bin`
   (assuming you installed Qt to `C:\Qt`). The only thing it does is adding
   necessary paths to `PATH` - you might not want to run it on system startup
   but it's very handy to setup environment before building.
   Setting `CMAKE_PREFIX_PATH`, the same way as for macOS (see above), also helps.

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
  for preinstalled software (Qt, libQuotient, QtKeychain)
- `-DCMAKE_INSTALL_PREFIX=/path` - controls where Quaternion will be installed
  (see below on installing from sources)
- `-DUSE_INTREE_LIBQMC=<ON|OFF>` - force using/not-using the in-tree copy of
  libQuotient sources (see "Getting the source code" above).
- `-DUSE_QQUICKWIDGET=<ON|OFF>` - by default it's `ON` with Qt 5.12 and `OFF`
  with earlier versions. See the next section for details.

#### QQuickWidget

Quaternion uses a combination of Qt Widgets and QML to render its UI (before
any protests and arguments: this _might_ change at some point in time but
certainly not in any near future). Internally, embedding QML in a Qt Widget
can be done in two ways that provide very similar API but are different
underneath: swallowing a `QQuickView` in a window container and
using `QQuickWidget`. Historically Quaternion used the former method; however,
its implementation in Qt is so ugly that
[it's officially deprecated by The Qt Project](https://blog.qt.io/blog/2014/07/02/qt-weekly-16-qquickwidget/).

Quaternion suffers from it too: if you see healthy QML chirping in the logs
but don't see anything where the timeline should be - you've got issue #355
and the only way for you to fix it is to get Quaternion rebuilt
with `QQuickWidget`. Unfortunately, Quaternion's QML is tricky enough to crash
`QQuickWidget` on Qt versions before 5.12, so there's no single good
configuration.

As of now, Quaternion will build with `QQuickWidget` if Qt 5.12 is detected
and with the legacy mechanism on lower versions. To override this you can pass
`-DUSE_QQUICKWIDGET=ON` to the first (configuring) `cmake` invocation to force
usage of `QQuickWidget` or `-DDISABLE_QQUICKWIDGET=ON` to force the legacy code.
Further on the choice will be made (and an override handle provided) at runtime,
depending on the detected Qt version.

### Install
In the root directory of the project sources: `cmake --build build_dir --target install`.

If you use GNU Make, `make install` (with `sudo` if needed) will work equally well.

### Package
We're still somewhat short of packagers, so please step up and support your
favourite system!

#### Flatpak
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
...then you need to set the right `CMAKE_PREFIX_PATH` variable, see above.

If `cmake` fails with...
```
CMake Error at CMakeLists.txt:30 (add_subdirectory):
  The source directory

    <quaternion-source-directory>/lib

  does not contain a CMakeLists.txt file.
```
...then you don't have libQuotient sources - most likely because you didn't do the `git submodule init && git submodule update` dance.

If you have made sure that your toolchain is in order (versions of compilers and Qt are among supported ones, `PATH` is set correctly etc.) but building fails with strange Qt-related errors such as not found symbols or undefined references, double-check that you don't have Qt 4.x packages around ([here is a typical example](https://github.com/quotient-im/Quaternion/issues/185)). If you need those packages reinstalling them may help; but if you use Qt4 by default you have to explicitly pass Qt5 location to CMake (see notes about `CMAKE_PREFIX_PATH` in "Building").

See also the Troubleshooting section in [README.md](./README.md)
