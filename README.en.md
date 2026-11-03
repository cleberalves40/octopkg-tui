# octopkg-tui

[Português](README.md) | **English**

[![Build & Test](https://github.com/cleberalves40/octopkg-tui/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/octopkg-tui/actions/workflows/build.yml)
[![FreeBSD 16-CURRENT](https://github.com/cleberalves40/octopkg-tui/actions/workflows/freebsd16.yml/badge.svg)](https://github.com/cleberalves40/octopkg-tui/actions/workflows/freebsd16.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2%2B-blue.svg)](LICENSE)

A terminal interface (TUI) for [`pkg(8)`](https://man.freebsd.org/cgi/man.cgi?pkg(8)), the FreeBSD package manager. It is the terminal counterpart to [OctoPkg](https://github.com/aarnt/octopkg) (fork at [cleberalves40/octopkg](https://github.com/cleberalves40/octopkg)), with the same tabs and shortcuts whenever the terminal allows, plus [Nerd Fonts](https://www.nerdfonts.com/) icons.

```
 OctoPkg — pkg(8) [# Installed]                       / Filter by name...
──────────────────────────────────────────────────────────────────────────────
   Name ^                       Version                 Size  Installed on
──────────────────────────────────────────────────────────────────────────────
 d ca_root_nss                  3.104                 1.02 MiB  2026-10-01 10:00
 I cmake                        3.31.6               45.10 MiB  2026-10-01 10:02
 + curl                         8.9.1                 4.36 MiB  2026-10-01 10:00
 L pkg                          2.3.1                24.80 MiB  2026-09-20 08:30
 I vim                          9.1.1000             38.90 MiB  2026-10-05 14:12
──────────────────────────────────────────────────────────────────────────────
 1 i Info   2 f Files (34)   3 a Actions (1)   4 > Output   6 ? Help
curl  Command line tool and library for transferring data with URLs
Status:            Out of date (8.10.0 is available)
Version:           8.9.1
Origin:            ftp/curl
Repository:        FreeBSD
URL:               https://curl.se/
──────────────────────────────────────────────────────────────────────────────
Ready.                                             +1  5 package(s)  1 outdated
F1Help  F5Refresh  F6Sort  F9Menu  F10Quit  +/-Mark  M-mApply
```

(with `--icons=ascii`: **I** installed, **d** dependency, **U** outdated, **N** newer than the repository, **L** locked, **-** not installed; **+** marked for installation and **x** for removal.)

## Status

**v0.7.1.** Lists installed and repository packages, with details and files; searches by name or description (**F7**); filters by category (**F8**); maximizes the list or tabs (**F11**); marks packages for installation (**+**) and removal (**-**); previews and executes transactions (**Alt+M**); installs upgrades (**Ctrl+U**); synchronizes the package database (**Ctrl+K**); cleans the cache; and locks or unlocks packages. It provides OctoPkg commands and, when the system uses Boot Environments, the option to create one first. It includes the 15 bectl-gui themes (**Alt+T**), Preferences (**Alt+,**), search within tabs (**Ctrl+F**), and the OctoPkg News tab (**5**, **Ctrl+G**). The FreeBSD port is validated in CI. See the [changelog](CHANGELOG.en.md).

## Requirements (FreeBSD)

```sh
pkg install ftxui cmake ninja
pkg install nerd-fonts-jetbrainsmono      # icons; optional
```

If `pkg` cannot find `ftxui` (the FreeBSD 15/aarch64 `latest` branch was missing it in October 2026), install it from the `quarterly` branch; see `ci/freebsd-ci.sh`.

For icons, configure the terminal to use **JetBrainsMono Nerd Font Mono**. Without a Nerd Font, use `--icons=unicode` or `--icons=ascii`.

Unlike OctoPkg, octopkg-tui does not depend on Qt: FTXUI is a single package, with no X11 or Wayland dependency. It also works well on, for example, a server accessed over SSH.

## Privileges

The interface runs as a regular user: listing packages, viewing details, and calculating a transaction preview do not require root. Only operations (`pkg install`, `remove`, `upgrade`, `update`, `clean`, `lock`, `unlock`, and Boot Environment `bectl create`) go through **doas** (default) or **sudo**, without a shell. If doas/sudo does not need a password, the operation runs in the background and output appears live in the Output tab; if a password is required, the screen is suspended while it is requested in the terminal. When run as root, `pkg` is invoked directly.

Example `/usr/local/etc/doas.conf` for the `wheel` group, caching authentication for a few minutes:

```
permit persist :wheel cmd /usr/local/sbin/pkg
permit persist :wheel cmd /sbin/bectl
```

(`bectl check`, which determines whether the Boot Environment option is shown, runs as the user.)

To use sudo: `--elevator=sudo`, or `elevator=sudo` in `~/.config/octopkg-tui/config`.

## Installing from the port

The port is in `freebsd/` and is validated in CI. Until it is in the official ports tree:

```sh
mkdir -p /usr/ports/ports-mgmt/octopkg-tui/files
cp freebsd/Makefile freebsd/pkg-descr /usr/ports/ports-mgmt/octopkg-tui/
cp freebsd/pkg-message.in /usr/ports/ports-mgmt/octopkg-tui/files/
cd /usr/ports/ports-mgmt/octopkg-tui && make makesum install clean
```

Submitting it to the official tree as `ports-mgmt/octopkg-tui` is covered in [docs/RELEASING.en.md](docs/RELEASING.en.md).

## Building

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/octopkg-tui
```

Options: `--remote` (start in the Repository view), `--icons=nerd|unicode|ascii`, `--lang=en_US|pt_BR|es_ES`, `--theme=THEME`, `--news=VARIANT`, `--elevator=doas|sudo|none`, `--config=FILE`, `--pkg=EXECUTABLE`, `--bectl=EXECUTABLE`, and `--version`. Preferences are stored in `~/.config/octopkg-tui/config`: table order, search mode, theme, language, and icons (through Preferences, **Alt+,**); `elevator` is set manually.

Themes, the same as bectl-gui: `system` (terminal colors), `octopkg`, `frappe`, `latte`, `mocha`, `macchiato`, `solarized_dark`, `solarized_light`, `selenized_dark`, `selenized_light`, `dracula`, `tokyo_night`, `nord`, `gruvbox`, and `one_dark`. They use 24-bit colors; FTXUI approximates them on terminals with fewer colors.

## Keyboard and mouse

| Key | Action |
|:---|:---|
| <kbd>F5</kbd> / <kbd>Ctrl+R</kbd> | Refresh the list |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>PgUp</kbd> <kbd>PgDn</kbd> <kbd>Home</kbd> <kbd>End</kbd> (or <kbd>j</kbd> <kbd>k</kbd>) | Move the selection |
| <kbd>Ctrl+L</kbd> / <kbd>/</kbd> | Search by name, or by description (<kbd>Esc</kbd>, <kbd>Enter</kbd>, or <kbd>Ctrl+P</kbd> returns to the list) |
| <kbd>F7</kbd> | Search by name or description (name and comment), like OctoPkg's Search menu |
| <kbd>F8</kbd> | Filter by port category (editors, www, devel...); a letter jumps to a matching category |
| <kbd>i</kbd> / <kbd>Alt+I</kbd> | Switch between installed and repository packages |
| <kbd>+</kbd> / <kbd>Insert</kbd> | Mark a package for installation (press again to unmark) |
| <kbd>-</kbd> / <kbd>Delete</kbd> | Mark a package for removal |
| <kbd>Alt+M</kbd> | Apply: preview, confirmation, and transaction |
| <kbd>Ctrl+E</kbd> | Undo package marks |
| <kbd>Ctrl+U</kbd> / <kbd>Ctrl+K</kbd> | Install upgrades / synchronize the package database |
| <kbd>F9</kbd> / <kbd>Alt+A</kbd> <kbd>Alt+B</kbd> <kbd>Alt+C</kbd> <kbd>Alt+J</kbd> | File, Search, Actions, and Help menus |
| <kbd>Ctrl+F</kbd> | Search text in Files, Output, News, and Help tabs (<kbd>F3</kbd> / <kbd>Shift+F3</kbd>: next / previous; <kbd>Esc</kbd> closes) |
| <kbd>Ctrl+G</kbd> | Fetch the latest system news (News tab) |
| <kbd>Alt+,</kbd> / <kbd>Alt+T</kbd> | Preferences (theme, language, icons) / next theme |
| <kbd>F6</kbd> | Choose the sort column: pressing it again reverses the order; choose "pkg order" to restore the original order |
| Click a column header | Sort ascending, descending, or in pkg order |
| <kbd>1</kbd>...<kbd>6</kbd> / <kbd>Alt+1</kbd>...<kbd>Alt+6</kbd> | Info, Files, Actions, Output, News, and Help tabs, using OctoPkg's numbers (<kbd>F1</kbd>: Help) |
| <kbd>Enter</kbd> | Open the selected package's Info tab |
| <kbd>Tab</kbd> | Scroll the tab with the arrow keys (press again, or <kbd>Esc</kbd>, to return to the list) |
| <kbd>F11</kbd> / <kbd>Alt+Z</kbd> | Cycle through maximized list, maximized tabs, and both |
| <kbd>Ctrl+Q</kbd> / <kbd>F10</kbd> / <kbd>q</kbd> | Quit (not during an operation) |

The complete map, including the correspondence with OctoPkg shortcuts, is in [docs/KEYMAP.en.md](docs/KEYMAP.en.md).

## Architecture

| Layer | Location | Responsibility |
|:---|:---|:---|
| Core | `src/core/` | `pkg(8)` without a shell and in English (`LC_ALL=C`); parsing `pkg query`, `pkg rquery`, and previews (`-n`); package states; doas/sudo operations and live output; Boot Environments; news (variant, `fetch(1)`, RSS, and cache); sizes, i18n, and preferences. Pure C++17: ports OctoPkg's `Package`, `UnixCommand`, transaction, and news logic. |
| Model | `src/model/` | Filtering, sorting (version, size, date, state), selection that follows a package, and the visible table window |
| Interface | `src/tui/` | FTXUI: screen, themes, keyboard, mouse, marks, dialogs, menus, and tab search; package lists, details, previews, and background operations |

Queries use `pkg query`/`pkg rquery` formats (fields separated by TAB), rather than parsing the human-readable output of `pkg info` and `pkg search` as OctoPkg does. Results do not depend on column alignment or the pkg version.

## Tests

```sh
cmake -B build -S . -DOCTOPKG_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/freebsd-e2e.sh build    # on FreeBSD, as root: real pkg and interface integration
sh tests/tui-smoke.sh build      # interface in a pseudo-terminal, using a fake pkg
```

- `test_core`: pkg output parsing (including install, removal, and upgrade previews); Repository view states; fake pkg, doas, and bectl executables that verify operation arguments; news (RSS, variant, fake fetch); process execution; and preferences.
- `test_model`: filtering, sorting, selection, and scrolling with 40,000 packages.
- `test_view`: renders the screen as text and drives the interface with keyboard events: marks, previews, dialogs, menus, operations, themes, Preferences, and tab search.
- `test_integration`: against real `pkg(8)`; installs, locks, unlocks, and removes `figlet`, and fetches FreeBSD news with `fetch(1)`.
- `tests/tui-smoke.sh`: runs the interface in a pseudo-terminal with a fake pkg (list, details, menu, mark, Preferences, and output); also runs on Ubuntu against FTXUI 6.1.9.
- `tests/freebsd-e2e.sh`: in addition to the tests above, installs and removes `figlet` through the interface in a pseudo-terminal.

CI runs on Ubuntu (including ASan and UBSan) and FreeBSD 14.5 and 15.1, amd64 and aarch64. This includes the interface in a pseudo-terminal and port validation (`ci/freebsd-port.sh`: portlint, stage-qa, package, and the installed binary). FreeBSD 16.0-CURRENT, which is not offered by any action, runs the same tests in the official snapshot image under QEMU (`ci/freebsd16-vm.sh`): amd64 on every pull request and both architectures weekly and on manual dispatch.

## License

[GPL-2.0-or-later](LICENSE). The core ports code from OctoPkg, © Alexandre Albuquerque Arnt, whose sources are GPL-2.0-or-later (OctoPkg's LICENSE file is LGPL-2.1, which also allows redistribution under GPL-2.0 or later).
