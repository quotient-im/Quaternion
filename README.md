# Quaternion

<a href='https://matrix.org'><img src='https://matrix.org/docs/projects/images/made-for-matrix.png' alt='Made for Matrix' height=64 target=_blank /></a>

[![license](https://img.shields.io/github/license/quotient-im/quaternion.svg)](https://github.com/quotient-im/Quaternion/blob/master/COPYING)
![status](https://img.shields.io/badge/status-beta-yellow.svg)
[![release](https://img.shields.io/github/release/quotient-im/quaternion/all.svg)](https://github.com/quotient-im/Quaternion/releases/latest)
[![](https://img.shields.io/matrix/quotient:matrix.org.svg)](https://matrix.to/#/#quotient:matrix.org)
[![](https://img.shields.io/cii/percentage/1663.svg?label=CII%20best%20practices)](https://bestpractices.coreinfrastructure.org/projects/1663/badge)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/quotient-im/Quaternion.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/quotient-im/Quaternion/context:cpp)
[![CI builds hosted by: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)
[![merge-chance-badge](https://img.shields.io/endpoint?url=https%3A%2F%2Fmerge-chance.info%2Fbadge%3Frepo%3Dquotient-im/quaternion)](https://merge-chance.info/target?repo=quotient-im/quaternion)

Quaternion is a cross-platform desktop IM client for the [Matrix](https://matrix.org) protocol. This file contains general information about application usage and settings. See [BUILDING.md](./BUILDING.md) for building instructions.

## Contacts
Most of talking around Quaternion happens in the room of its parent project,
Quotient: [#quotient:matrix.org](https://matrix.to/#/#quotient:matrix.org).
You can file issues at
[the project's issue tracker](https://github.com/quotient-im/Quaternion/issues).
If you find what looks like a security issue, please follow
[special instructions](./SECURITY.md).

## Download and install

For GNU/Linux, the recommended way to install Quaternion is via your
distribution's package manager. Users of macOS can use a Homebrew
package. The source code for the latest release as well as binaries
for major platforms can also be found at the
[GitHub Releases page](https://github.com/quotient-im/Quaternion/releases).
Make sure to read the notes below depending to your environment.

### Requirements
Quaternion 0.0.95 packages on Linux need Qt version 5.9 or higher;
Debian Buster, Ubuntu Bionic Beaver, Fedora 28 and OpenSUSE 15 are new enough.
The packages provided on the download page (see below) have all necessary
Qt libraries bundled. On Windows, you need to separately install OpenSSL
(see the next section).

### Windows
Since we can't rely on package management on Windows, all needed libraries and
a C++ runtime are packaged/installed together with Quaternion - except OpenSSL,
because of export restrictions.
Unless you already have OpenSSL around (e.g., it is a part of any
Qt development installation), you should install it yourself.
[OpenSSL's Wiki](https://wiki.openssl.org/index.php/Binaries) lists a few links
to OpenSSL installers. They come in different build configurations; Quaternion
archives provided at GitHub need OpenSSL made with/for Visual Studio (not MinGW).
Normally you should install OpenSSL 1.1.x. Older releases (0.0.9.4 and before)
used OpenSSL 1.0.x but neither that version of OpenSSL nor those Quaternion
releases are supported; don't use them unless you really know what you're doing.

### macOS
You can download the latest release from
[GitHub](https://github.com/quotient-im/Quaternion/releases/latest).

Alternatively, you can install Quaternion with [Homebrew Cask](https://brew.sh)
```
brew install quaternion
```

### Linux and others
Quaternion is packaged for many distributions, including various versions of
Debian, Ubuntu and OpenSUSE, as well as Arch Linux, NixOS and FreeBSD.
A pretty comprehensive list can be found at
[Repology](https://repology.org/project/quaternion/versions).

Flatpaks for Quaternion are available from Flathub. To install, use:
```
flatpak install https://flathub.org/repo/appstream/com.github.quaternion.flatpakref
```
Please file issues at https://github.com/flathub/com.github.quaternion
if you believe there's a problem specific to Flatpak.

The GitHub Releases page offers AppImage binaries for Linux; however, it's
recommended to only use AppImage binaries if Quaternion is not available
from your distribution's repos and Flatpak doesn't work for you.
Distribution-specific packages better integrate into the system (particularly,
the desktop environment) and include all relevant customisations (e.g. themes)
and fixes (e.g. security). If you wish to use features depending on newer Qt
(such as Markdown) than your distribution provides, consider installing
Quaternion as a [Flatpak](https://flathub.org/apps/details/com.github.quaternion).
Both Flatpak packages and distribution-specific packages are built in a more
reproducible and controlled way than AppImages assembled within this project;
unlike AppImages, they are also (usually) signed by the repo which gives
certain protection from tampering.

### Development builds

Thanks to generous and supportive folks at [Cloudsmith](https://cloudsmith.io)
who provide free hosting to OSS projects, those who want to check out the latest
(not necessarily the greatest, see below) code can find packages produced by
continuous integration (CI) in the
[Quaternion repo there](https://cloudsmith.io/~quotient/repos/quaternion/groups/).

A few important notes on these packages in case you're new to them:
- All these builds come bundled with recent Qt (5.14 on Linux and 5.15 on other
  platforms, as of this writing).
- They are only provided for testing; feedback on _any_ release is welcome
  as long as you know which build you run; but do not expect the developers
  to address issues in any but the latest snapshot.
- In case it's still unclear: these builds are UNSTABLE by default; some may
  not run at all, and if they do, they may ~~tell you obscenities in your
  local language, steal your smartphone, and share your private photos~~
  scramble the messages you send, interfere with or even break other clients
  including Element ones, and generally corrupt your account in ways unexpected
  and hard to fix (all of that actually happened in the past). Do NOT run these
  builds if you're not prepared to deal with the problems.
- If you understand the above, have your backups in order and are still willing
  to try things out or just generally help with the project - make sure to
  `/join #quotient:matrix.org` and have the URL you downloaded handy. In case
  of trouble, ~~show this label to your doctor~~ send the URL to the binary you
  used in the chat room (you may need to use another client or Quaternion
  version for that), describe what happened and we'll try to pull you out of it.

If you want to build Quaternion from sources, see [BUILDING.md](./BUILDING.md).

## Running
Just start the executable in your most preferred way - either from the build
directory or from the installed location. If you're interested in tweaking
configuration beyond what's available in the UI, read the "Configuration"
section further below.

## Translation
Quaternion uses [Lokalise.co](https://lokalise.co) for the translation effort.
It's easy to participate:
[join the project at Lokalise.co](https://lokalise.co/public/730769035bbc328c31e863.62506391/),
ask to add your language (either in #quotient:matrix.org or in
the Lokalise project chat) and start translating! Many languages are still
longing for contributors.

## Configuration
The only non-trivial command-line option available so far is `--locale` - it
allows you to override the locale Quaternion uses (an equivalent of setting
`LC_ALL` variable on UNIX-based systems). Version 0.0.95 comes with German,
Russian, Polish, and Spanish translations.

Quaternion stores its configuration in a way standard for Qt applications, as
described below. It will read and write the configuration in the user-specific
location (creating it if non-existent) and will only read the system-wide
location with reasonable defaults if the configuration is not found at
the user-specific one.

- Linux:
  - user-specific: `$HOME/.config/Quotient/quaternion.conf`
  - system-wide: `$XDG_CONFIG_DIR/Quotient/quaternion` or
    `/etc/xdg/Quotient/quaternion`
- macOS:
  - user-specific: `$HOME/Library/Preferences/im.quotient.quaternion.plist`
  - system-wide: `/Library/Preferences/im.quotient.quaternion.plist`
- Windows: registry keys under
  - user-specific: `HKEY_CURRENT_USER\Software\Quotient\quaternion`
  - system-wide: `HKEY_LOCAL_MACHINE\Software\Quotient\quaternion`

ALL settings listed below reside in `UI` section of the configuration file
or (for Windows) registry.

Some settings exposed in the user interface (Settings and View menus) are:

- `notifications` - a general setting whether Quaternion should distract
  the user with notifications and how.
  - `none` suppresses notifications entirely (rooms and messages are still
    hightlighted but the tray icon is muted);
  - `non-intrusive` allows the tray icon show notification popups;
  - `intrusive` (default) adds to that activation of Quaternion window
    (i.e. the application blinking in the task bar, or getting raised,
    or otherwise demands attention in an environment-specific way).
- `timeline_layout` - this allows to choose the timeline layout. If this is
  set to "xchat", Quaternion will show the author to the left of each message,
  in a xchat/hexchat style (this was also the only available layout on
  Quaternion before 0.0.9.2). Any other value will select the "default" layout,
  with author labels above blocks of messages.
- `use_shuttle_dial` - Quaternion will use a shuttle dial instead of
  a classic scrollbar for the timeline's vertical scrolling control. To start
  scrolling move the shuttle dial away from its neutral position in the middle;
  the further away you move it, the faster you scroll in that direction.
  Releasing the dial resets it back to the neutral position and stops scrolling.
  This is more convenient if you need to move around without knowing
  the position relative to the edges, as is the case of a Matrix timeline;
  however, the control is somewhat unconventional and not all people like it.
  The shuttle dial is enabled by default; set this to false (or 0) to use
  the classic scrollbar.
- `autoload_images` - whether full-size images should be loaded immediately
  once the message is shown on the screen. The default is to automatically load
  full-size images; set this to false (or 0) to disable that and only load
  a thumbnail initially.
- `show_noop_events` - set this to 1 to show state events that do not alter
  the state (you'll see "(repeated)" next to most of those).
- `RoomsDock/tags_order` - allows to alter the order of tags in the room
  list. This is a comma-separated list of tags/namespaces;
  a few characters have special meaning as described below. If a tag is
  not mentioned and does not fit any namespace, it will be put at the end of
  the room list in lexicographic order. Tags within the same namespace are
  also ordered lexicographically.
  
  `.*` (only recognised at the end of the string) means the whole namespace;
  strings that don't end with this are treated as fully specified tags.
  
  `-` in front of the tag/namespace means it should not be used for grouping;
  e.g., if you don't want People group you can add `-im.quotient.direct`
  anywhere in the list. `im.quotient.none` ("Rooms") always exists and
  cannot be disabled, only its position in the list is taken into account.
  
  The default tags order is as follows:
  `m.favourite,u.*,im.quotient.direct,im.quotient.none,m.lowpriority`,
  meaning: Favourites, followed by all user custom tags, then People,
  rooms with no enabled tags (the "Rooms" group) and finally Low priority
  rooms. If Quaternion doesn't find the setting in the configuration it
  will write down this line to the configuration so that you don't need
  to enter it from scratch.

Settings not exposed in UI:

- `show_author_avatars` - set this to 1 (or true) to show author avatars in
  the timeline (default if the timeline layout is set to default); setting this
  to 0 (or false) will suppress avatars (default for the XChat timeline layout).
- `suppress_local_echo` - set this to 1 (or true) to suppress showing local
  echo (events sent from the current application but not yet confirmed by
  the server). By default local echo is shown.
- `animations_duration_ms` - defines the base duration (in milliseconds) of
  animation effects in the timline. The default is 400; set it to 0 to disable
  animation.
- `outgoing_color` - set this to the color name you prefer for text you sent;
  HTML color names and SVG `#codes` are supported; by default it's `#204A87`
  (navy blue).
- `highlight_color` - set this to the color name you prefer for highlighted
  rooms/messages; HTML color names and SVG `#codes` are supported;
  by default it's `orange`.
- `highlight_mode` - set this to `text` if you prefer to use the text color
  for highlighting (the only option available until 0.0.9.3); the new
  default is to use the background for highlighting.
- `use_human_friendly_dates` - set this to false (or 0) if you do NOT want
  usage of human-friendly dates ("Today", "Monday" instead of the standard
  day-month-year triad) in the UI; the default is true.
- `quote_style` - the quote template. The `\\1` means the quoted string.
  By default it's `> \\1\n`.
- `quote_regex` - set to `^([\\s\\S]*)` to add `UI/quote_style` only at
  the beginning and end of the quote. By default it's `(.+)(?:\n|$)`.
- `Fonts/render_type` - select how to render fonts in Quaternion timeline;
  possible values are "NativeRendering" (default) and "QtRendering".
- `Fonts/family` - override the font family for the whole application.
  If not specified, the default font for your environment is used.
- `Fonts/pointSize` - override the font size (in points) for the whole
  application. If not specified, the default size for your environment is used.
- `Fonts/timeline_family` - font family (for example `Monospace`) to
  display messages in the timeline. If not specified, the application-wide font
  family is used.
- `Fonts/timeline_pointSize` - font size (in points) to display messages
  in the timeline. If not specified, the application-wide point size is used.
- `maybe_read_timer` - threshold time interval in milliseconds for a displayed
  message to be considered as read.
- `use_keychain` - set this to false (or 0) if you explicitly do NOT want to
  use keychain but prefer to store access token in a dedicated file instead (see
  the next paragraph); the default is true.
- `hyperlink_users` - set this to false (or 0) if you do NOT want to
  hyperlink matrix user IDs in messages. By default it's true.
- `auto_markdown` - since version 0.0.95 beta 3, and only if built with Qt 5.14
  or newer (that pertains to all binaries at GitHub Releases as well as
  to Flatpaks), Quaternion supports Markdown when entering messages. Quaternion
  only treats the message as Markdown if the message starts with `/md` command
  (the command itself is removed from the message before sending). Setting
  `auto_markdown` to `true` enables Markdown parsing in all messages that
  _do not_ start with `/plain` instead. By default, this setting is `false`
  since the current Qt support of Markdown is buggy, it cannot be enabled in
  builds with older Qt and the whole functionality is experimental. If you
  have it enabled, feel free to submit bug reports at the usual place.

Since version 0.0.95, all Quaternion binaries at GitHub Releases are compiled
with Qt Keychain support. It means that Quaternion will try to store your access
token(s) in a secure storage configured for your platform. If the storage or
Qt Keychain are not available, Quaternion will try to store your access token(s)
in a dedicated file with restricted access rights so that only the owner can
access them (this doesn't really work on Windows - see below), with the name
made from your user id and Matrix device id, in the following directory:

- Linux: `$HOME/.local/share/Quotient/quaternion`
- macOS: `$HOME/Library/Application Support/Quotient/quaternion`
- Windows: `%LOCALAPPDATA%/Quotient/quaternion`

Unfortunately, Quaternion cannot enforce proper access rights on Windows;
you'll see a warning about it and will be able to either refuse saving your
access token in that case or agree and setup file permissions outside Quaternion.

Quaternion caches the rooms state and user/room avatars on the file system
in a conventional location for your platform, as follows:

- Linux: `$HOME/.cache/Quotient/quaternion`
- macOS: `$HOME/Library/Cache/Quotient/quaternion`
- Windows: `%LOCALAPPDATA%/Quotient/quaternion/cache`

Cache files are safe to delete at any time but Quaternion only looks for them
when starting up and overwrites them regularly while running; so it only
makes sense to delete cache files when Quaternion is not running. If Quaternion
doesn't find or cannot fully load cache files at startup it downloads
the whole state from Matrix servers. It tries to optimise this process by
lazy-loading if the server supports it; in an unlucky case when the server
cannot do lazy-loading, initial sync can take much time (up to a minute and
even more, depending on the number of rooms and the number of users in them);
in the worst case, a larger user account without lazy-loading may crash
Quaternion when using Qt older than 5.15.

Deleting cache files may help with problems such as missing avatars,
rooms stuck in a wrong state etc.

## Troubleshooting

Quaternion uses libQuotient under the hood; some Quaternion problems are
actually problems of libQuotient. If you haven't found your case below, check
also the troubleshooting section in libQuotient README.md.

#### Continuously reconnecting though the network is fine
If Quaternion starts displaying the message that it couldn't connect to
the server and retries more than a couple of times without success, while
you're sure you have the network connection - double-check that you don't have
Qt bearer management libraries around, as they cause issues with some WiFi
networks. To do that, try to find "bearer" directory where your Qt is installed
(on Windows it's next to Quaternion executable; on Linux it's a part of
Qt installation, usually in `/usr/lib/qt5/plugins`). Then delete or rename it
(on Windows) or delete the package that this directory is in (on Linux).

Bearer management functionality is officially deprecated and does nothing since
Qt 5.15; if you face connectivity problems with Qt 5.15, file an issue at
[libQuotient repo](https://github.com/quotient-im/libQuotient/issues).

#### No messages in the timeline
If Quaternion runs but you can't see any messages in the chat (though you can
type them in) - you have either of two problems with Qt Quick (if you are
extremely unlucky, both):

- You might not have Qt Quick libraries and/or plugins installed. On Linux,
  this may be a case when you are not using the official packages for your
  distro. Check the stdout/stderr logs, they are quite clear in such cases.
  On Windows and Mac, just open an issue (see "Contacts" in the beginning of
  this README), because most likely not all necessary Qt parts were installed
  along with Quaternion (which is a bug).
- If the logs confirm that QML is up and running but there's still nothing
  for the timeline, you might have hit an issue with QML view stacking order,
  such as #355/#356. If you use Qt 5.12 or newer (as is the case on Windows
  and macOS recently), please file a bug: it should not happen with
  recent Qt at all. If you are on Linux and have to use older Qt, you have
  to build Quaternion from sources, passing `-DUSE_QQUICKWIDGET=ON` to CMake.
  Note that it's prone to crashing on some platforms so it's best to still
  find a way to run Quaternion with Qt 5.12 (using AppImage, e.g.).

#### SSL problems
Especially on Windows, if Quaternion starts up but upon an attempt to connect
returns a message like "Failed to make SSL context" - correct SSL libraries
are not reachable by the Quaternion binary. Re-read the chapter "Requirements",
section "Windows" in the beginning of this file and do as it advises (make sure
in particular that you use the correct version of OpenSSL).

#### DLL hell
If you have troubles with dynamic libraries on Windows,
[the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/)
helps a lot in navigating the DLL hell - especially when you have a mixed
32/64-bit environment or have different versions of the same library scattered
around. OpenSSL, in particular, is very often dragged along by all kinds
of software; and you may have other copies of Qt around which you didn't even
know about - e.g., with CMake GUI. Entries in PATH for such programs may lead
to the operating system choosing those bundled libraries instead of those you
intend to use.

#### Logging
If you run Quaternion from a console on Windows and want to see log messages,
set `QT_LOGGING_TO_CONSOLE=1` so that the output is redirected to the console.

When chasing bugs and investigating crashes, it helps to increase the debug
level. Thanks to [@eang:matrix.org](https://matrix.to/#/@eang:matrix.org]),
libQuotient uses Qt logging categories - the "Troubleshooting" section of
the library's `README.md` elaborates on how to setup logging. Note that
Quaternion itself doesn't use Qt logging categories yet, only the library does.

You may also want to set `QT_MESSAGE_PATTERN` to make logs slightly more
informative (see https://doc.qt.io/qt-5/qtglobal.html#qSetMessagePattern
for the format description). My (@kitsune's) `QT_MESSAGE_PATTERN` looks as
follows:
```
`%{time h:mm:ss.zzz}|%{category}|%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}|%{message}`
```
(the scary `%{if}`s are just encoding the logging level into its initial letter).

## Screenshot
![Screenshot](quaternion.png)
