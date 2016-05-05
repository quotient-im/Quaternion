# Quaternion
Quaternion is an IM client for the [Matrix](https://matrix.org) protocol in development.

## Pre-requisites
- a Linux, MacOS or Windows system (desktop versions tried; mobile Linux/Windows might work too)
- a Git client (to check out this repo)
- a C++ toolchain that can deal with Qt (see a link for your platform at http://doc.qt.io/qt-5/gettingstarted.html#platform-requirements)
- CMake (from your package management system or https://cmake.org/download/)
- Qt 5 (either Open Source or Commercial), version 5.3 or higher as of this writing (check the CMakeLists.txt for details)

## Source code
Quaternion uses libqmatrixclient that resides in another repo but is not yet shipped as a separate library - it is fetched as a git submodule. To get all necessary sources, you can either supply `--recursive` to your `git clone` for Quaternion sources or do the following in the root directory of already cloned Quaternion sources (NOT in lib subdirectory):
```
git submodule init
git submodule update
```

## Linux
### Installing pre-requisites
Just install things from "Pre-requisites" using your preferred package manager. If your Qt package base is fine-grained you might want to take a look at CMakeLists.txt to figure out which specific libraries Quaternion uses. You may also try to blindly run cmake and look at error messages.

### Building, installation, running
From the root directory of the project sources:
```
mkdir build
cd build
cmake ../
make
sudo make install # FIXME: Installation seems to be broken atm - run from the build directory instead
quaternion &
```

## OS X
It's basically the same as for Linux (see above). For pre-requisites, `brew install qt5` should get you Qt5. For building, you might need to specify CMAKE_PREFIX_PATH to your Qt5 explicitly. The build sequence (in the root directory of the project sources) would look as follows:
```
mkdir build
cd build
cmake ../ -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt5/5.5.1/ # or whatever version of qt5 brew installed
make
```

## Windows
### The easy way
1. Have a Git client and CMake installed. Make sure they are in your PATH.
1. Install Qt5, using their official installer. No Qt extras are needed; but if you don't have a toolchain and/or IDE, include Qt Creator and at least one toolchain under Qt Creator.
1. Clone Quaternion sources with `--recursive` option.
1. Run the IDE; import the Quaternion sources as a CMake project (FIXME: Not sure if Visual Studio can import CMake projects).
1. Setup the toolchain for the project.
1. Invoke build.
1. Run.

The good thing is that you can get Qt Creator and a MinGW toolchain together with Qt, all from a single installer. The kind-of bad thing is that there are no official MinGW-based 64-bit packages for Qt. If you're determined to build 64-bit Quaternion, either use a Visual Studio toolchain or pass on to the next section.

### The hard way aka make world
#### Building Qt5
This is not recommended unless you really want a 64-bit MinGW-based build. The process is based on [this KDE Community Wiki page](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Windows) and it uses a Gentoo Linux portage system ported to Windows. Have the article handy when going through it. The process is slow and takes a lot of machine time - and, possibly, your own time if you're not friendly with command line and configuration files and/or are unlucky to checkout the portage tree when it's broken. Be ready to reach out to respective mailing lists and interwebs to hunt down building errors.

1. Check out and configure the emerge tool as described in the Wiki page.
1. Enter ```emerge qt qtquickcontrols``` inside the shell made by the kdeenv script (see the KDE TechBase article), double check that the checkout-configure-build-install process has started and leave it running. Leaving it for a night on even an older machine should suffice.
1. Once the build is over, make sure you have the toolchain reachable from the environment you're going to compile Quaternion in. If you plan to use the just-compiled MinGW and CMake to compile Quaternion you might want to add <KDEROOT>/dev-utils/bin and <KDEROOT>/mingw64/bin into your PATH for convenience. If you have other MinGW or CMake installations around you have to carefully select the order of PATH entries and bear in mind that emerge usually takes the latest versions by default (which are not necessarily the same that you installed somewhere else).
  - Alternatively: after building Qt just do the steps from the next section inside the kdeenv wrapper shell that you used to build the pre-requisites (not tried but should work).

Note: ```emerge qt``` builds the default set of Qt components. You can try to reduce the build time by using only a needed subset of Qt components instead of a qt metapackage (nobody tried yet).

#### Building libqmatrixclient and Quaternion
If you use an IDE, import the sources as a CMake project, double-check your build environment (paths, toolchains etc.) and the IDE should do the building for you. Qt Creator (the stock Qt IDE) and CLion were checked to work. 

If you don't use an IDE, make sure that mingw32-make (if you use the MinGW toolchain) and cmake are in your PATH and issue the following commands in the root directory of the project sources:
```
md build
cd build
cmake ../
mingw32-make
```

### Installation
There is no installer configuration for Windows as of yet. You might want to use [the Windows Deployment Tool](http://doc.qt.io/qt-5/windows-deployment.html#the-windows-deployment-tool) that comes with Qt to find all dependencies and put them into the build directory. Though it misses on a library or two it helps a lot. To double-check that you're good to go you can use [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) - this is especially needed when you have a mixed 32/64-bit environment or have different versions of the same library scattered around (libssl is notorious to be dragged along by all kinds of software).

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
