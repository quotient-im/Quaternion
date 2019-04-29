# Quaternion

<a href='https://matrix.org'><img src='https://matrix.org/docs/projects/images/made-for-matrix.png' alt='Made for Matrix' height=64 target=_blank /></a>

[![license](https://img.shields.io/github/license/QMatrixClient/quaternion.svg)](https://github.com/QMatrixClient/quaternion/blob/master/COPYING)
![status](https://img.shields.io/badge/status-beta-yellow.svg)
[![release](https://img.shields.io/github/release/QMatrixClient/quaternion/all.svg)](https://github.com/QMatrixClient/Quaternion/releases/latest)
[![](https://img.shields.io/matrix/qmatrixclient%3Amatrix.org.svg)](https://matrix.to/#/!PCzUtxtOjUySxSelof:matrix.org)
[![](https://img.shields.io/cii/percentage/1663.svg?label=CII%20best%20practices)](https://bestpractices.coreinfrastructure.org/projects/1663/badge)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

Quaternion is a cross-platform desktop IM client for the [Matrix](https://matrix.org) protocol. This file contains general information about application usage and settings. See [BUILDING.md](./BUILDING.md) for building instructions.

## Contacts
Most of talking around Quaternion happens in our Matrix room: [#quaternion:matrix.org](https://matrix.to/#/#quaternion:matrix.org).

You can also file issues at [the project's issue tracker](https://github.com/QMatrixClient/Quaternion/issues). If you have what looks like a security issue, please contact Kitsune Ral ([@kitsune:matrix.org](https://matrix.to/#/@kitsune:matrix.org) or [Kitsune-Ral@users.sf.net](mailto:Kitsune-Ral@users.sf.net)).

## Download and install
The latest release (with links to cross-platform source code archives, as well
as archived binaries for Windows and macOS) can be found on the
[GitHub Releases page](https://github.com/QMatrixClient/Quaternion/releases/latest).

For those who want the very latest version (beware, you may find it not working
at times), automatic builds from continuous integration pipelines can be found
as follows (all these builds come with a fairly recent Qt bundled):
- Windows: Surf to the
  [AppVeyor CI page for Quaternion](https://ci.appveyor.com/project/QMatrixClient/quaternion),
  then go to "Jobs", click on a job for your architecture and find the archive
  in "Artifacts".
- Linux and macOS: go to the
  [Quaternion CI repo at bintray](https://bintray.com/qmatrixclient/ci/Quaternion),
  pick a version under "Versions", then click on "Files" and find a .dmg for
  macOS and an AppImage for Linux.

If you want to build Quaternion from sources, see [BUILDING.md](./BUILDING.md).
Packagers are still scarce, so please step up and support your favourite system!

### Requirements
Quaternion needs Qt version 5.7 or higher. On Linux, this is compatible with Debian Stretch and Ubuntu Xenial Xerus with Kubuntu Backports overlay. On Windows, all needed Qt libraries are included in the archive - you only need to separately install OpenSSL (see below).

#### Windows
Since we can't rely on package management on Windows, Qt libraries and a C++ runtime are packaged/installed together with Quaternion. However, OpenSSL libraries (ssleay32.dll and libeay32.dll) are not installed automatically because of export restrictions. Unless you already have them around (e.g., they are a part of any Qt development installation, see `Tools/<MinGW toolchain>/opt/bin`), your best bet is to:
- find the pre-compiled libraries yourself (searching Internet for "OpenSSL Windows" should work);
- download them
- either install them system-wide (which probably makes sense as soon as you keep them up-to-date; make sure the location is in your PATH) or put them next to quaternion.exe.

#### Linux
Unofficial package for Arch Linux: https://aur.archlinux.org/packages/quaternion/

Flatpaks for Quaternion are available from Flathub. To install, use `flatpak install https://flathub.org/repo/appstream/com.github.quaternion.flatpakref`. This is still experimental - please file issues at https://github.com/flathub/com.github.quaternion if you believe there's a problem with Flatpakaging.

#### macOS
You can download the latest release from [GitHub](https://github.com/QMatrixClient/Quaternion/releases/latest).

Alternatively, you can install Quaternion from [Homebrew Cask](https://brew.sh)
```
brew cask install quaternion
```

## Running
Just start the executable in your most preferred way - either from the build
directory or from the installed location. If you're interested in tweaking
configuration beyond what's available in the UI, read the "Configuration"
section further below.

## Translation
Quaternion uses [Lokalise.co](https://lokalise.co) for the translation effort.
It's easy to participate:
[join the project at Lokalise.co](https://lokalise.co/public/730769035bbc328c31e863.62506391/),
ask to add your language (either in #qmatrixclient:matrix.org or in
the Lokalise project chat) and start translating! Many languages are still
longing for contributors.

## Configuration
The only non-trivial command-line option available so far is `--locale` - it
allows you to override the locale Quaternion uses (an equivalent of setting
`LC_ALL` variable on UNIX-based systems). As of version 0.0.9.3, Quaternion has
no translations to other languages, so you will hardly notice the difference in
messages; number and date formats will be following the setting though. Version
0.0.9.4 gains German, Polish, and Russian translations.

Quaternion stores its configuration in a way standard for Qt applications. It will read and write the configuration in the user-specific location (creating it if non-existent) and will only read the system-wide location with reasonable defaults if the configuration is nowhere to be found.
- Linux:
  - system-wide: `$XDG_CONFIG_DIR/QMatrixClient/quaternion` or `/etc/xdg/QMatrixClient/quaternion`
  - user-specific: `$HOME/.config/QMatrixClient/quaternion.conf`
- macOS:
  - system-wide: `/Library/Preferences/com.QMatrixClient.quaternion.plist`
  - user-specific: `$HOME/Library/Preferences/com.QMatrixClient.quaternion.plist`
- Windows: registry keys under
  - system-wide: `HKEY_LOCAL_MACHINE\Software\QMatrixClient\quaternion`
  - user-specific: `HKEY_CURRENT_USER\Software\QMatrixClient\quaternion`

Some settings exposed in UI (Settings and View menus):
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
- `UI/use_shuttle_dial` - Quaternion will use a shuttle dial instead of
  a classic scrollbar for the timeline's vertical scrolling control. Shuttle
  dials usually control change velocity instead of value; in this case,
  moving the dial away from the neutral position increases the speed of
  scrolling. This is more convenient if you need to quickly move around without
  knowing position relative to the edges, as is the case of a Matrix timeline;
  however, the control is unusual and not all people like it. Shuttle scrollbar
  is enabled by default; set this to false (or 0) to use the classic scrollbar.
- `UI/autoload_images` - whether full-size images should be loaded immediately
  once the message is shown on the screen. The default is to automatically load
  full-size images; set this to false (or 0) to disable that and only load
  a thumbnail initially.
- `UI/show_noop_events` - set this to 1 to show state events that do not alter
  the state (you'll see "(repeated)" next to most of those).
- `UI/RoomsDock/tags_order` - allows to alter the order of tags in the room
  list. The default value for this key will be set by Quaternion if it doesn't
  it so that you could edit it further. This is a list of tags/namespaces;
  `.*` at the end of the string means a namespace, other strings are treated
  as fully specified tags. E.g., the default order looks like this:
  `m.favourite,u.*,org.qmatrixclient.direct,org.qmatrixclient.none,m.lowpriority`.
  If a tag is not mentioned and does not fit any namespace, it will be put at
  the end in lexicographic order. Tags within the same namespace are also
  ordered lexicographically.

Settings not exposed in UI:
- `UI/condense_chat` - set this to 1 to make the timeline rendered tighter,
  eliminating vertical gaps between messages as much as possible.
- `UI/show_author_avatars` - set this to 1 (or true) to show author avatars in
  the timeline (default if the timeline layout is set to default); setting this
  to 0 (or false) will suppress avatars (default for the XChat timeline layout).
- `UI/suppress_local_echo` - set this to 1 (or true) to suppress showing local
  echo (events sent from the current application but not yet confirmed by
  the server). By default local echo is shown.
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
- `UI/quote_style` - the quote template. The `\\1` means the quoted string.
  By default it's `> \\1\n`.
- `UI/quote_regex` - set to `^([\\s\\S]*)` to add `UI/quote_style` only at
  the beginning and end of the quote. By default it's `(.+)(?:\n|$)`.

Since version 0.0.9.4, AppImage binaries for Linux and .dmg files for macOS
are compiled with Qt Keychain support. It means that Quaternion will try
to store your access token(s) in the secure storage configured for your
platform. If the storage or Qt Keychain are not available (Qt Keychain is off
by default on Windows - you have to rebuild Quaternion to enable it),
Quaternion will try to store your access token(s) in a dedicated file with
restricted access rights so that only the owner can access them. Every
access token is stored in a separate file matching your user id in
the following directory:
- Linux: `$HOME/.local/share/QMatrixClient/quaternion`
- macOS: `$HOME/Library/Application Support/QMatrixClient/quaternion`
- Windows: `%LOCALAPPDATA%/QMatrixClient/quaternion`

Unfortunately, Quaternion cannot enforce proper access rights on Windows yet;
you'll see a warning about it and will be able to either refuse saving your
access token in that case or agree and setup file permissions outside Quaternion.
The work is ongoing to enable Qt Keychain on Windows by default.

Quaternion caches the rooms state and user/room avatars on the file system in a conventional location for your platform, as follows:
- Linux: `$HOME/.cache/QMatrixClient/quaternion`
- macOS: `$HOME/Library/Cache/QMatrixClient/quaternion`
- Windows: `%LOCALAPPDATA%/QMatrixClient/quaternion/cache`

Cache files are safe to delete at any time but Quaternion only looks for them
when starting up and overwrites them regularly while running; so it only
makes sense to delete cache files when Quaternion is not running. If Quaternion
doesn't find or cannot fully load cache files at startup it downloads
the whole state from Matrix servers. It tries to optimise this process by
lazy-loading if the server supports it; in an unlucky case when the server
cannot do lazy-loading, initial sync can take much time (up to a minute and
even more, depending on the number of rooms and the number of users in them).
Deleting cache files may help with problems such as missing avatars,
rooms stuck in a wrong state etc.

## Troubleshooting

libqmatrixclient has its own section on troubleshooting - make sure to look into its README.md too.

#### Continuously reconnecting though the network is fine
If Quaternion starts displaying the message that it couldn't connect to the server and retries more than a couple of times without success, while you're sure you have the network connection - double-check that you don't have Qt bearer management libraries around, as they cause issues with some WiFi networks. To do that, try to find "bearer" directory where your Qt is installed (on Windows it's next to Quaternion executable; on Linux it's a part of Qt installation, usually in `/usr/lib/qt5/plugins`). Then delete or rename it (on Windows) or delete the package that this directory is in (on Linux).

#### No messages in the timeline
If Quaternion runs but you can't see any messages in the chat (though you can
type them in) - you have either of two problems with Qt Quick (or if you are
extremely unlucky, both):
- You might not have Qt Quick libraries and/or plugins installed. On Linux,
  double check "Pre-requisites" above and also see the stdout/stderr logs,
  they are quite clear in such cases. On Windows and Mac, just open an issue
  (see "Contacts" in the beginning of this README), because most likely
  not all necessary Qt parts were installed along with Quaternion (which is a bug).
- If the logs confirm that QML is up and running but there's still nothing
  for the timeline, you might have hit an issue with QML view stacking order,
  such as #355/#356. If you use Qt 5.12, please file a bug (it should not happen
  with Qt 5.12 at all). With older Qt, you will have to build Quaternion from
  sources, passing `-DUSE_QQUICKWIDGET=ON` to CMake. Note that it's prone to
  crashing on some platforms so it's best to still find a way to use
  Quaternion with Qt 5.12.

#### SSL problems
Especially on Windows, if Quaternion starts up but upon an attempt to connect returns a message like "Failed to make SSL context" - you haven't made sure that SSL libraries are reachable by the Quaternion binary. Re-read the chapter "Requirements", section "Windows" in the beginning of this file and do as it advises.

#### DLL hell
If you have troubles with dynamic libraries on Windows, [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) helps a lot in navigating the DLL hell - especially when you have a mixed 32/64-bit environment or have different versions of the same library scattered around. OpenSSL, in particular, is notoriously often dragged along by all kinds of software; and you may have other copies of Qt around which you didn't even know about - e.g., with CMake GUI.

#### Logging
If you run Quaternion from a console on Windows and want to see log messages,
set `QT_LOGGING_TO_CONSOLE=1` so that the output is redirected to the console.

When chasing bugs and investigating crashes, it helps to increase the debug level. Thanks to [@eang:matrix.org](https://matrix.to/#/@eang:matrix.org]), libqmatrixclient uses Qt logging categories - the "Troubleshooting" section of the library's `README.md` elaborates on how to setup logging. Note that Quaternion itself doesn't use Qt logging categories yet, only the library does.

You may also want to set `QT_MESSAGE_PATTERN` to make logs slightly more informative (see https://doc.qt.io/qt-5/qtglobal.html#qSetMessagePattern for the format description). My (@kitsune's) `QT_MESSAGE_PATTERN` looks as follows:
`%{time h:mm:ss.zzz}|%{category}|%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}|%{message}` (the scary `%{if}`s are just encoding the logging level into its initial letter).

## Screenshot
![Screenshot](quaternion.png)
