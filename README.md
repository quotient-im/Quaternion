# Quaternion

[![license](https://img.shields.io/github/license/QMatrixClient/quaternion.svg)](https://github.com/QMatrixClient/quaternion/blob/master/COPYING)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

Quaternion is a cross-platform desktop IM client for the [Matrix](https://matrix.org) protocol in development.

## Version 0.0.1 out now!
The release (including binaries for Windows) can be found [here](https://github.com/QMatrixClient/Quaternion/releases/tag/v0.0.1).

## Download
### Binaries
There's no official packaging at the moment. Some links to unofficial packages follow:
Arch Linux: https://aur.archlinux.org/packages/quaternion/

Windows: automatic builds are packaged upon every commit (CI) at https://ci.appveyor.com/project/QMatrixClient/quaternion (go to "Jobs", select the job for your architecture, then "Artifacts")

#### OpenSSL on Windows
Since we can't rely on package management on Windows, Qt libraries and the runtime are packaged/installed together with Quaternion. However, two libraries, namely ssleay32.dll and libeay32.dll from OpenSSL project, are not installed automatically because of export restrictions. Unless you already have them around (e.g., they are a part of any Qt development installation, see `Tools/<MinGW toolchain>/opt/bin`), your best bet is to download these libraries yourself and either install them system-wide (which probably makes sense as soon as you keep them up-to-date) or put them next to quaternion.exe. Having the libraries system-wide implies they are in your PATH.

In case of trouble with libraries on Windows, [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) helps a lot in navigating the DLL hell - especially when you have a mixed 32/64-bit environment or have different versions of the same library scattered around. OpenSSL, in particular, is notoriously often dragged along by all kinds of software; and you may have other copies of Qt around which you didn't even know about - e.g., with CMake GUI.

### Source code
Quaternion source code repository at GitHub: https://github.com/QMatrixClient/Quaternion

Quaternion uses libqmatrixclient that is developed in a separate GitHub repo and is fetched as a git submodule. To get all necessary sources:
- if you haven't cloned the Quaternion sources yet:
```
git clone --recursive https://github.com/QMatrixClient/Quaternion.git
```
- if you already have cloned Quaternion, do the following in the top-level directory (NOT in lib subdirectory):
```
git submodule init
git submodule update
```

There are very few tags so far; there will be more, as new versions are released.

## Contacts
Most of talking around Quaternion happens in our Matrix room: [#quaternion:matrix.org](https://matrix.to/#/#quaternion:matrix.org).

You can also file issues at [the project's issue tracker](https://github.com/QMatrixClient/Quaternion/issues). If you have what looks like a security issue, please see respective instructions in CONTRIBUTING.md.

## Building

### Flatpak

If you run Linux and your distribution supports flatpak, you can easily build and install Quaternion as a flatpak package:

```
git clone https://github.com/QMatrixClient/Quaternion.git --recursive
cd Quaternion/flatpak
./setup_runtime.sh
./build.sh
flatpak --user install quaternion-nightly com.github.quaternion
```
Whenever you want to update your Quaternion package, just run again the `build.sh` script and then use:

```
flatpak --user update
```

### Pre-requisites
- a Linux, MacOS or Windows system (desktop versions tried; mobile Linux/Windows might work too)
  - For Ubuntu flavours - Trusty Tar or later (or a derivative) is good enough; older ones will need PPAs at least for a newer Qt
- a Git client (to check out this repo)
- CMake (from your package management system or [the official website](https://cmake.org/download/))
- Qt 5 (either Open Source or Commercial), version 5.2.1 or higher as of this writing (check the CMakeLists.txt for most up-to-date information). Qt 5.3 or higher recommended on Windows.
- a C++ toolchain supported by your version of Qt (see a link for your platform at [the Qt's platform requirements page](http://doc.qt.io/qt-5/gettingstarted.html#platform-requirements))
  - GCC 4.8, Clang 3.5.0, Visual C++ 2015 are the oldest officially supported as of this writing

#### Linux
Just install things from the list above using your preferred package manager. If your Qt package base is fine-grained you might want to take a look at `CMakeLists.txt` to figure out which specific libraries Quaternion uses (or blindly run cmake and look at error messages). Note also that you'll need several Qt Quick plugins for Quaternion to work (without them, it will compile and run but won't show the messages timeline). In case of Trusty Tar the following line will get you everything necessary to build and run Quaternion (thanks to `@onlnr:matrix.org`):
```
sudo apt-get install git cmake qtdeclarative5-dev qtdeclarative5-qtquick2-plugin qtdeclarative5-controls-plugin
```
On Fedora 26, the following command should be enough for building and running:
```
yum install git cmake qt5-qtdeclarative-devel qt5-qtquickcontrols
```

#### OS X
`brew install qt5` should get you Qt5. You may need to tell CMake about the path to Qt by passing `-DCMAKE_PREFIX_PATH=<where-Qt-installed>`

#### Windows
1. Install a Git client and CMake. The commands in further sections imply that git and cmake are in your PATH - otherwise you have to prepend them with your actual paths.
1. Install Qt5, using their official installer. If for some reason you need to use Qt 5.2.1, select its Add-ons component in the installer as well; for later versions, no extras are needed. If you don't have a toolchain and/or IDE, you can easily get one by selecting Qt Creator and at least one toolchain under Qt Creator. At least Qt 5.3 is recommended on Windows; `windeployqt` in Qt 5.2.1 is not functional enough to provide a standalone installation for Quaternion though you can still compile and run it from your build directory.
1. Make sure CMake knows about Qt and the toolchain - the easiest way is to run a qtenv2.bat script that can be found in `C:\Qt\<Qt version>\<toolchain>\bin` (assuming you installed Qt to `C:\Qt`). The only thing it does is adding necessary paths to PATH - you might not want to run it on system startup but it's very handy to setup environment before building. Setting CMAKE_PREFIX_PATH, the same way as for OS X (see above), also helps.

There are no official MinGW-based 64-bit packages for Qt. If you're determined to build 64-bit Quaternion, either use a Visual Studio toolchain or build Qt5 yourself as described in Qt documentation.

### Building
In the root directory of the project sources:
```
mkdir build_dir
cd build_dir
cmake .. # Pass -DCMAKE_PREFIX_PATH and -DCMAKE_INSTALL_PREFIX here if needed
cmake --build . --target all
```
This will get you an executable in `build_dir` inside your project sources. `CMAKE_INSTALL_PREFIX` variable of CMake controls where Quaternion will be installed - pass it to `cmake ..` above if you wish to alter the default (see the output from `cmake ..` to find out the configured values).

### Running
Linux, MacOS: Just start the executable in your most preferred way.

Windows: the same but make sure that Qt5 dlls and their dependencies are either in your PATH (see the note about `qtenv2.bat` in "Pre-requisites" above) or next to the executable. See also "OpenSSL on Windows" section above).

### Installation
In the root directory of the project sources: `cmake --build build_dir --target install`.

On Linux, `make install` (with `sudo` if needed) will work equally well.

## Troubleshooting

If `cmake` fails with...
```
CMake Warning at CMakeLists.txt:11 (find_package):
  By not providing "FindQt5Widgets.cmake" in CMAKE_MODULE_PATH this project
  has asked CMake to find a package configuration file provided by
  "Qt5Widgets", but CMake did not find one.
```
...then you need to set the right -DCMAKE_PREFIX_PATH variable, see above.

If `cmake` fails with...
```
CMake Error at CMakeLists.txt:30 (add_subdirectory):
  The source directory

    <quaternion-source-directory>/lib

  does not contain a CMakeLists.txt file.
```
...then you don't have libqmatrixclient sources - most likely because you didn't do the `git submodule init && git submodule update` dance.

If Quaternion runs but you can't see any messages in the chat (though you can type them in) - you have a problem with Qt Quick. Most likely, you don't have Qt Quick libraries and/or plugins installed. On Linux, double check "Pre-requisites" above and also see the stdout/stderr logs, they are quite clear in such cases. On Windows and Mac, just open an issue (see "Contacts" section in the beginning of this README), because most likely not all necessary Qt parts got copied along with Quaternion (which is a bug).

Especially on Windows, if Quaternion starts up but upon an attempt to connect returns a message like "Failed to make SSL context" - you haven't made sure that SSL libraries are reachable buy the Quaternion binary. See the "OpenSSL on Windows" section above for details.

When chasing bugs and investigating crashes, it helps to increase the debug level. Thanks to @eang:matrix.org, libqmatrixclient uses Qt logging categories - the "Troubleshooting" section of `lib/README.md` elaborates on how to setup logging.

## Screenshot
![Screenshot](quaternion.png)
