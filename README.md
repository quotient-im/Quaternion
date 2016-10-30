# Quaternion
Quaternion is a cross-platform desktop IM client for the [Matrix](https://matrix.org) protocol in development.

## Version 0.0.1 out now!
The release (including binaries for Windows) can be found [here](https://github.com/Fxrh/Quaternion/releases/tag/v0.0.1).

## Pre-requisites
- a Linux, MacOS or Windows system (desktop versions tried; mobile Linux/Windows might work too)
- a Git client (to check out this repo)
- a C++ toolchain that can deal with C++11 and Qt5 (see a link for your platform at http://doc.qt.io/qt-5/gettingstarted.html#platform-requirements)
- CMake (from your package management system or https://cmake.org/download/)
- Qt 5 (either Open Source or Commercial), version 5.2.1 or higher as of this writing (check the CMakeLists.txt for details)

## Source code
Quaternion uses libqmatrixclient that resides in another repo but is not yet shipped as a separate library - it is fetched as a git submodule. To get all necessary sources, you can either supply `--recursive` to your `git clone` for Quaternion sources or do the following in the root directory of already cloned Quaternion sources (NOT in lib subdirectory):
```
git submodule init
git submodule update
```

## Installing pre-requisites
### Linux
Just install things from "Pre-requisites" using your preferred package manager. If your Qt package base is fine-grained you might want to take a look at CMakeLists.txt to figure out which specific libraries Quaternion uses. You may also try to blindly run cmake and look at error messages.

### OS X
`brew install qt5` should get you Qt5. You may need to tell CMake about the path to Qt by passing -DCMAKE_PREFIX_PATH=<where-Qt-installed>

### Windows
1. Install a Git client and CMake. Make sure they are in your PATH.
1. Install Qt5, using their official installer. No Qt extras are needed; but if you don't have a toolchain and/or IDE, include Qt Creator and at least one toolchain under Qt Creator.
1. Make sure CMake knows about the toolchain - the easiest thing is to have it in your PATH as well.

Unfortunately there are no official MinGW-based 64-bit packages for Qt. If you're determined to build 64-bit Quaternion, either use a Visual Studio toolchain or build Qt5 yourself as described in Qt documentation.

### Building
From the root directory of the project sources:
```
mkdir build
cd build
cmake ../ # Pass -DCMAKE_PREFIX_PATH here if needed
cmake --build .
```
This will get you an executable in the build directory inside your project sources.

## Running
Just start the executable in your most preferred way. This implies at the moment that respective Qt5 libraries are in your PATH or next to the executable.

### Installation
There's no automated way to install it at the moment; `sudo make install` should work on Linux, though.

On Windows, you might want to use [the Windows Deployment Tool](http://doc.qt.io/qt-5/windows-deployment.html#the-windows-deployment-tool) that comes with Qt to find all dependencies and put them into the build directory. Though it misses on a library or two it helps a lot. To double-check that you're good to go you can use [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) - this is especially needed when you have a mixed 32/64-bit environment or have different versions of the same library scattered around (libssl is notorious to be dragged along by all kinds of software).

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

## Screenshot
![Screenshot](quaternion.png)
