# Building and Packaging Quaternion

[![Quaternion-master@Travis](https://img.shields.io/travis/QMatrixClient/Quaternion/master.svg)](https://travis-ci.org/QMatrixClient/Quaternion/branches)
[![Quaternion-master@AppVeyor](https://img.shields.io/appveyor/ci/QMatrixClient/quaternion/master.svg?logo=appveyor)](https://ci.appveyor.com/project/QMatrixClient/quaternion)
[![license](https://img.shields.io/github/license/QMatrixClient/quaternion.svg)](https://github.com/QMatrixClient/quaternion/blob/master/COPYING)
[![Chat](https://img.shields.io/badge/chat-%23quaternion-blue.svg)](https://matrix.to/#/#quaternion:matrix.org)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)


### Getting the source code

The source code is hosted at GitHub: https://github.com/QMatrixClient/Quaternion. The best way for one-off building is checking out a tag for a given release from GitHub (master branch is unstable and may or may not work). If you plan to work on Quaternion code, feel free to fork/clone the repo and check out the master branch.

In any case DO NOT download a _sources_ archive from GitHub Releases - it's incomplete because GitHub Releases doesn't include source trees of submodules (and unfortunately there's no way to suppress generation of those archives).

Quaternion needs libQMatrixClient as a submodule. libQMatrixClient is developed in a separate GitHub repo and can be fetched as a git submodule. If you haven't cloned the Quaternion sources yet, the following will get you all sources in one go:
```
git clone --recursive https://github.com/QMatrixClient/Quaternion.git
```
If you already have cloned Quaternion, do the following in the top-level directory (NOT in `lib` subdirectory):
```
git submodule init
git submodule update
```

### Pre-requisites
- a Linux, OSX or Windows system (desktop versions tried; mobile Linux/Windows might work too)
  - For Ubuntu flavours - zesty or later (or a derivative) is good enough out of the box; older ones will need PPAs at least for a newer Qt; in particular, if you have xenial you're advised to add Kubuntu Backports PPA for it
- a Git client to check out this repo
- CMake (from your package management system or [the official website](https://cmake.org/download/))
- Qt 5 (either Open Source or Commercial), version 5.6 or higher
- a C++ toolchain supported by your version of Qt (see a link for your platform at [the Qt's platform requirements page](http://doc.qt.io/qt-5/gettingstarted.html#platform-requirements))
  - GCC 5 (Windows, Linux, OSX), Clang 5 (Linux), Apple Clang 8.1 (OSX) and Visual C++ 2015 (Windows) are the oldest officially supported
  - any build system that works with CMake should be fine: GNU Make, ninja (any platform), NMake, jom (Windows) are known to work.

#### Linux
Just install things from the list above using your preferred package manager. If your Qt package base is fine-grained you might want to take a look at `CMakeLists.txt` to figure out which specific libraries Quaternion uses (or blindly run cmake and look at error messages). Note also that you'll need several Qt Quick plugins for Quaternion to work (without them, it will compile and run but won't show the messages timeline). In case of Trusty Tar the following line will get you everything necessary to build and run Quaternion (thanks to `@onlnr:matrix.org`):
```
sudo apt-get install git cmake qtdeclarative5-dev qtdeclarative5-qtquick2-plugin qtdeclarative5-controls-plugin
```
On Fedora 26, the following command should be enough for building and running:
```
dnf install git cmake qt5-qtdeclarative-devel qt5-qtquickcontrols
```

#### OS X
`brew install qt5` should get you Qt5. You may need to tell CMake about the path to Qt by passing `-DCMAKE_PREFIX_PATH=<where-Qt-installed>`

#### Windows
1. Install CMake. The commands in further sections imply that cmake is in your PATH - otherwise you have to prepend them with actual paths.
1. Install Qt5, using their official installer. If for some reason you need to use Qt 5.2.1, select its Add-ons component in the installer as well; for later versions, no extras are needed. If you don't have a toolchain and/or IDE, you can easily get one by selecting Qt Creator and at least one toolchain under Qt Creator. At least Qt 5.3 is recommended on Windows; `windeployqt` in Qt 5.2.1 is not functional enough to provide a standalone installation for Quaternion though you can still compile and run from your build directory.
1. Make sure CMake knows about Qt and the toolchain - the easiest way is to run a qtenv2.bat script that can be found in `C:\Qt\<Qt version>\<toolchain>\bin` (assuming you installed Qt to `C:\Qt`). The only thing it does is adding necessary paths to PATH - you might not want to run it on system startup but it's very handy to setup environment before building. Setting CMAKE_PREFIX_PATH, the same way as for OS X (see above), also helps.

There are no official MinGW-based 64-bit packages for Qt. If you're determined to build 64-bit Quaternion, either use a Visual Studio toolchain or build Qt5 yourself as described in Qt documentation. Be prepared that a VS 64-bit build won't give you a working Quaternion - the last time checked Quaternion ran but could not get anything from the network. PRs are welcome.

### Build
In the root directory of the project sources:
```
mkdir build_dir
cd build_dir
cmake .. # Pass -DCMAKE_PREFIX_PATH and -DCMAKE_INSTALL_PREFIX here if needed
cmake --build . --target all
```
This will get you an executable in `build_dir` inside your project sources. `CMAKE_INSTALL_PREFIX` variable of CMake controls where Quaternion will be installed - pass it to `cmake ..` above if you wish to alter the default (see the output from `cmake ..` to find out the configured values).


### Install
In the root directory of the project sources: `cmake --build build_dir --target install`.

If you use GNU Make, `make install` (with `sudo` if needed) will work equally well.

### Package
Packagers are very scarce so far, so please step up and support your favourite system! Notably, we still need a MacOS maintainer - Quaternion sees no actual usage/testing on this platform yet.

#### Flatpak
If you run Linux and your distribution supports flatpak, you can easily build and install Quaternion as a flatpak package:

```
git clone https://github.com/QMatrixClient/Quaternion.git --recursive
cd Quaternion/flatpak
./setup_runtime.sh
./build.sh
flatpak --user install quaternion-nightly com.github.quaternion
```
Whenever you want to update your Quaternion package, do the following from the flatpak directory:

```
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
...then you don't have libqmatrixclient sources - most likely because you didn't do the `git submodule init && git submodule update` dance.

If you have made sure that your toolchain is in order (versions of compilers and Qt are among supported ones, `PATH` is set correctly etc.) but building fails with strange Qt-related errors such as not found symbols or undefined references, double-check that you don't have Qt 4.x packages around ([here is a typical example](https://github.com/QMatrixClient/Quaternion/issues/185)). If you need those packages reinstalling them may help; but if you use Qt4 by default you have to explicitly pass Qt5 location to CMake (see notes about `CMAKE_PREFIX_PATH` in "Building").

See also the Troubleshooting section in [README.md](./README.md)
