# Quaternion

[![license](https://img.shields.io/github/license/QMatrixClient/quaternion.svg)](https://github.com/QMatrixClient/quaternion/blob/master/COPYING)
![status](https://img.shields.io/badge/status-beta-yellow.svg)
[![release](https://img.shields.io/github/release/QMatrixClient/quaternion/all.svg)](https://github.com/QMatrixClient/Quaternion/releases/latest)
[![Chat](https://img.shields.io/badge/chat-%23quaternion-blue.svg)](https://matrix.to/#/#quaternion:matrix.org)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/1663/badge)](https://bestpractices.coreinfrastructure.org/projects/1663)
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

Flatpaks for Quaternion are available from Flathub. To install, use `flatpak install https://flathub.org/repo/appstream/com.github.quaternion.flatpakref`. This is still experimental - please file issues at https://github.com/flathub/com.github.quaternion if you believe there's a problem with Flatpakaging.

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

Some settings exposed in UI (Settings menu):
- `UI/notifications` - a general setting whether Quaternion should distract
  the user with notifications and how.
  - `none` suppresses notifications entirely (rooms and messages are still
    hightlighted but the tray icon is muted);
  - `non-intrusive` allows the tray icon show notification popups;
  - `intrusive` (default) adds to that activation of Quaternion window
    (i.e. the application blinking in the task bar, or getting raised,
    or otherwise demands attention in an environment-specific way).
- `UI/timeline_layout` - this allows to choose the timeline layout. If this is
  set to "xchat", Quaternion will show the author to the left of each message,
  in a xchat/hexchat style (this was also the only available layout on
  Quaternion before 0.0.9.2). Any other value will select the "default" layout,
  with author labels above blocks of messages.
- `UI/autoload_images` - whether full-size images should be loaded immediately
  once the message is shown on the screen. The default is to automatically load
  full-size images; set this to false (or 0) to disable that and only load
  a thumbnail initially.

Settings not exposed in UI:
- `UI/condense_chat` - set this to 1 to make the timeline rendered tighter,
  eliminating vertical gaps between messages as much as possible.
- `UI/show_noop_events` - set this to 1 to show state events that do not alter
  the state (you'll see "(repeated)" next to most of those).
- `UI/show_author_avatars` - set this to 1 (or true) to show author avatars in
  the timeline (default if the timeline layout is set to default); setting this
  to 0 (or false) will suppress avatars (default for the XChat timeline layout).
- `UI/animations_duration_ms` - defines the base duration (in milliseconds) of
  animation effects in the timline. The default is 400; set it to 0 to disable
  animation.
- `UI/highlight_color` - set this to the color name you prefer for highlighted
  rooms/messages; HTML color names and SVG `#codes` are supported;
  by default it's `orange`.
- `UI/highlight_mode` - set this to `text` if you prefer to use the highlight
  color as the text color (the only option available until 0.0.9.3); the new
  default is to use the background for highlighting.
- `UI/use_human_friendly_dates` - set this to false (or 0) if you do NOT want
  usage of human-friendly dates ("Today", "Monday" instead of the standard
  day-month-year triad) in the UI; the default is true.
- `UI/Fonts/render_type` - select how to render fonts in Quaternion timeline;
  possible values are "NativeRendering" (default) and "QtRendering".
- `UI/RoomsDock/tags_order` - allows to alter the order of tags in the room
  list. The default value for this key will be set by Quaternion if it doesn't
  it so that you could edit it further. This is a list of tags/namespaces;
  `.*` at the end of the string means a namespace, other strings are treated
  as fully specified tags. E.g., the default order looks like this:
  `m.favourite,u.*,org.qmatrixclient.direct,org.qmatrixclient.none,m.lowpriority`.
  If a tag is not mentioned and does not fit any namespace, it will be put at
  the end in lexicographic order. Tags within the same namespace are also
  ordered lexicographically.

Since version 0.0.5, Quaternion tries to store your access tokens in a dedicated file with restricted access rights so that only the owner can access them. Every access token is stored in a separate file matching your user id in the following directory:
- Linux: `$HOME/.local/share/QMatrixClient/quaternion`
- OSX: `$HOME/Library/Application Support/QMatrixClient/quaternion`
- Windows: `%LOCALAPPDATA%/QMatrixClient/quaternion`

Unfortunately, Quaternion cannot enforce proper access rights on Windows yet; you'll see a warning about it and will be able to either refuse saving your access token in that case or agree and setup file permissions outside Quaternion.

Quaternion caches the rooms state on the file system in a conventional location for your platform, as follows:
- Linux: `$HOME/.cache/QMatrixClient/quaternion`
- OSX: `$HOME/Library/Cache/QMatrixClient/quaternion`
- Windows: `%LOCALAPPDATA%/QMatrixClient/quaternion/cache`

Cache files are safe to delete at any time but Quaternion only looks for them when starting up and overwrites them regularly while running; so it only makes sense to delete cache files when Quaternion is not running. If Quaternion doesn't find cache files at startup it downloads the whole state from Matrix servers, which can take much time (up to a minute and even more, depending on the number of rooms and the number of users in them). Deleting cache files may help with problems such as missing avatars, rooms stuck in a wrong state etc.

## Troubleshooting

libqmatrixclient has its own section on troubleshooting - in case of trouble make sure to look into lib/README.md too.

#### Ever reconnecting though the network is fine
If Quaternion starts displaying the message that it couldn't connect to the server and retries more than a couple of times without success, while you're sure you have the network connection - double-check that you don't have Qt bearer management libraries: they cause issues with some WiFi networks. Try to find "bearer" directory where your Qt is installed (on Windows it's next to Quaternion executable; on Linux it's a part of Qt installation in `/usr/lib/qt5/plugins`). Then delete or rename it (on Windows) or delete the package that this directory is in (on Linux).

#### No messages in the timeline
If Quaternion runs but you can't see any messages in the chat (though you can type them in) - you have a problem with Qt Quick. Most likely, you don't have Qt Quick libraries and/or plugins installed. On Linux, double check "Pre-requisites" above and also see the stdout/stderr logs, they are quite clear in such cases. On Windows and Mac, just open an issue (see "Contacts" in the beginning of this README), because most likely not all necessary Qt parts got copied along with Quaternion (which is a bug).

If the logs confirm that QML is up and running but there's still nothing for the timeline, you might have hit an issue with QML view stacking order, such as #356. As a workaround, you may try to use a highly experimental (=prone to crashes on some platforms) mode: pass `-DUSE_QQUICKWIDGET=ON` to CMake. If you _do_ have crashes in this mode, please take time to report in `#qmatrixclient:matrix.org`.

#### SSL problems
Especially on Windows, if Quaternion starts up but upon an attempt to connect returns a message like "Failed to make SSL context" - you haven't made sure that SSL libraries are reachable by the Quaternion binary. See the "OpenSSL on Windows" section above for details.

#### DLL hell
If you have troubles with dynamic libraries on Windows, [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) helps a lot in navigating the DLL hell - especially when you have a mixed 32/64-bit environment or have different versions of the same library scattered around. OpenSSL, in particular, is notoriously often dragged along by all kinds of software; and you may have other copies of Qt around which you didn't even know about - e.g., with CMake GUI.

#### Logging categories
When chasing bugs and investigating crashes, it helps to increase the debug level. Thanks to [@eang:matrix.org](https://matrix.to/#/@eang:matrix.org]), libqmatrixclient uses Qt logging categories - the "Troubleshooting" section of `lib/README.md` elaborates on how to setup logging. Note that Quaternion itself doesn't use Qt logging categories yet, only the library does.

## Screenshot
![Screenshot](quaternion.png)
