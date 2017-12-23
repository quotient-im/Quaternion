# Quaternion

[![license](https://img.shields.io/github/license/QMatrixClient/quaternion.svg)](https://github.com/QMatrixClient/quaternion/blob/master/COPYING)
![status](https://img.shields.io/badge/status-beta-yellow.svg)
[![release](https://img.shields.io/github/release/QMatrixClient/quaternion/all.svg)](https://github.com/QMatrixClient/Quaternion/releases/latest)
[![Chat](https://img.shields.io/badge/chat-%23quaternion-blue.svg)](https://matrix.to/#/#quaternion:matrix.org)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

Quaternion is a cross-platform desktop IM client for the [Matrix](https://matrix.org) protocol. This file contains general information about application usage and settings. See [BUILDING.md](./BUILDING.md) for building instructions.

## Contacts
Most of talking around Quaternion happens in our Matrix room: [#quaternion:matrix.org](https://matrix.to/#/#quaternion:matrix.org).

You can also file issues at [the project's issue tracker](https://github.com/QMatrixClient/Quaternion/issues). If you have what looks like a security issue, please contact Kitsune Ral ([@kitsune:matrix.org](https://matrix.to/#/@kitsune:matrix.org) or [Kitsune-Ral@users.sf.net](mailto:Kitsune-Ral@users.sf.net)).

## Download and install
The latest release (with links to cross-platform source code archives, as well as archived binaries for Windows) can be found at [GitHub Releases page](https://github.com/QMatrixClient/Quaternion/releases/latest).

Packagers are very scarce so far, so please step up and support your favourite system! Notably, we still need a MacOS maintainer - Quaternion sees no actual usage/testing on this platform yet. Build instructions for the source code can be found in BUILDING.md.

### Requirements
Quaternion needs Qt version 5.6 or higher. On Linux, this is compatible with Debian Stretch and Ubuntu Xenial Xerus with Kubuntu Backports overlay. On Windows, all needed libraries are included in the archive - you don't need to separately install anything.

#### Windows
The archives published on the GitHub Releases page already include the necessary subset of Qt; normally you don't need to additionally download and install anything.

For those who want the very latest version (beware, you may find it not working at times), automatic builds for Windows are packaged by AppVeyor CI upon every commit. To get an archive, surf to the [AppVeyor CI page for Quaternion](https://ci.appveyor.com/project/QMatrixClient/quaternion), then go to "Jobs", click on a job for your architecture and find the archive in "Artifacts". Once you unpack the archive, **delete or rename** the "bearer" directory (see "Troubleshooting" below for details).

#### Linux
Unofficial package for Arch Linux: https://aur.archlinux.org/packages/quaternion/

Flatpaks for Quaternion are available from Flathub. To install, use `flatpak --install https://flathub.org/repo/appstream/com.github.quaternion`. This is still experimental - please file issues at https://github.com/flathub/com.github.quaternion if you believe there's a problem with Flatpakaging.

## Running
Just start the executable in your most preferred way - either from build directory or from the installed location.

### Before you run it on Windows
Since we can't rely on package management on Windows, Qt libraries and a C++ runtime are packaged/installed together with Quaternion. However, OpenSSL libraries (ssleay32.dll and libeay32.dll) are not installed automatically because of export restrictions. Unless you already have them around (e.g., they are a part of any Qt development installation, see `Tools/<MinGW toolchain>/opt/bin`), your best bet is to find and download these libraries yourself, and either install them system-wide (which probably makes sense as soon as you keep them up-to-date; make sure the location is in your PATH) or put them next to quaternion.exe.

### Configuration and cache files
Quaternion stores its configuration in a way standard for Qt applications. It will read and write the configuration in the user-specific location (creating it if non-existent) and will only read the system-wide location with reasonable defaults if the configuration is nowhere to be found.
- Linux:
  - system-wide: `$XDG_CONFIG_DIR/QMatrixClient/quaternion` or `/etc/xdg/QMatrixClient/quaternion`
  - user-specific: `$HOME/.config/QMatrixClient/quaternion`
- OSX:
  - system-wide: `/Library/Preferences/com.QMatrixClient.quaternion.plist`
  - user-specific: `$HOME/Library/Preferences/com.QMatrixClient.quaternion.plist`
- Windows: registry keys under
  - system-wide: `HKEY_LOCAL_MACHINE\Software\QMatrixClient\quaternion`
  - user-specific: `HKEY_CURRENT_USER\Software\QMatrixClient\quaternion`

Several settings are not exposed in the UI:
- `UI/suppress_splash` - set this to 1 if you don't want a splash screen
- `UI/condense_chat` - set this to 1 to make the timeline rendered tighter, without gaps between messages

Since version 0.0.5, Quaternion tries to store your access tokens in a dedicated file with restricted access rights so that only the owner can access them. Every access token is stored in a separate file matching your user id in the following directory:
- Linux: `$HOME/.local/share/QMatrixClient/quaternion`
- OSX: `$HOME/Library/Application Support/QMatrixClient/quaternion`
- Windows: `%LOCALAPPDATA%/QMatrixClient/quaternion`

Unfortunately, Quaternion cannot enforce proper access rights on Windows yet; you'll see a warning about it and will be able to either refuse saving your access token in that case or agree and setup file permissions outside Quaternion.

Quaternion caches the rooms state on the file system in a conventional location for your platform, as follows:
- Linux: `$HOME/.cache/QMatrixClient/quaternion`
- OSX: `$HOME/Library/Cache/QMatrixClient/quaternion`
- Windows: `%LOCALAPPDATA%/QMatrixClient/quaternion/cache`

Cache files are safe to delete at any time. If Quaternion doesn't find them when starting up it downloads the whole state from Matrix servers, which can take much time (up to a minute and even more, depending on the number of rooms and the number of users in them). Deleting cache files may help with problems such as missing avatars, rooms stuck in a wrong state etc.

## Troubleshooting

#### Ever reconnecting though the network is fine
If Quaternion starts displaying the message that it couldn't connect to the server and retries more than a couple of times without success, while you're sure you have the network connection - double-check that you don't have Qt bearer management libraries: they cause issues with some WiFi networks. Try to find "bearer" directory where your Qt is installed (on Windows it's next to Quaternion executable; on Linux it's a part of Qt installation in `/usr/lib/qt5/plugins`). Then delete or rename it (on Windows) or delete the package that this directory is in (on Linux).

#### No messages in the timeline
If Quaternion runs but you can't see any messages in the chat (though you can type them in) - you have a problem with Qt Quick. Most likely, you don't have Qt Quick libraries and/or plugins installed. On Linux, double check "Pre-requisites" above and also see the stdout/stderr logs, they are quite clear in such cases. On Windows and Mac, just open an issue (see "Contacts" in the beginning of this README), because most likely not all necessary Qt parts got copied along with Quaternion (which is a bug).

#### SSL problems
Especially on Windows, if Quaternion starts up but upon an attempt to connect returns a message like "Failed to make SSL context" - you haven't made sure that SSL libraries are reachable by the Quaternion binary. See the "OpenSSL on Windows" section above for details.

#### DLL hell
If you have troubles with dynamic libraries on Windows, [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) helps a lot in navigating the DLL hell - especially when you have a mixed 32/64-bit environment or have different versions of the same library scattered around. OpenSSL, in particular, is notoriously often dragged along by all kinds of software; and you may have other copies of Qt around which you didn't even know about - e.g., with CMake GUI.

#### Logging categories
When chasing bugs and investigating crashes, it helps to increase the debug level. Thanks to [@eang:matrix.org](https://matrix.to/#/@eang:matrix.org]), libqmatrixclient uses Qt logging categories - the "Troubleshooting" section of `lib/README.md` elaborates on how to setup logging. Note that Quaternion itself doesn't use Qt logging categories yet, only the library does.

## Screenshot
![Screenshot](quaternion.png)
