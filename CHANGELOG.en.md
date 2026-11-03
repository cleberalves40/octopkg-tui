# Changelog

All notable changes to octopkg-tui are documented here. This changelog follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows [Semantic Versioning](https://semver.org/) with `vX.Y.Z` tags.

[Português](CHANGELOG.md) | **English**

## [0.7.1] - 2026-10-20

FreeBSD 16.0-CURRENT validation.

### Added
- **FreeBSD 16-CURRENT** workflow (`.github/workflows/freebsd16.yml` and `ci/freebsd16-vm.sh`), like bectl-gui's validation: no CI action offers 16.0-CURRENT, so the script boots the official snapshot image under QEMU (KVM on amd64, emulated on aarch64), verifies its SHA256, and runs in the VM the same tests as the 14.5 and 15.1 jobs: core, model, and view tests; integration with real `pkg(8)` (installing and removing figlet, and news); the interface in a pseudo-terminal; and port validation.
- On pull requests and `main`, only amd64 runs; aarch64 runs on manual dispatch, every Monday (CURRENT snapshots change weekly), and on pushes to the `validate-freebsd16` branch.

### Validated
- FreeBSD 16.0-CURRENT, 2026-10-19 snapshot (`main-n290219-041802b362aa`), with clang 21.1.8: amd64 (FTXUI 7.0.3) and aarch64 (FTXUI 6.1.9), all tests passing with no compiler warnings, and the port validated (portlint without errors).

## [0.7.0] - 2026-10-20

Maximize the list or tabs.

### Added
- **F11** (or **Alt+Z**, for terminals that use F11 for fullscreen) cycles through three layouts, combining OctoPkg's F10 and F11 behavior on one key: package list only at full height; tabs only, with arrow keys scrolling the tab without pressing Tab; and both, as before. The status bar shows the active layout; F10 still quits, as in mc and htop.
- Tests cover all three layouts, arrow keys moving the selection or scrolling the tab as appropriate, and returning to the split layout.

## [0.6.0] - 2026-10-20

Category filter.

### Added
- **F8** filters by port category, the part of an origin before the slash (`editors` in `editors/vim`): a scrollable list of categories in the current view, with package counts and the current category marked. A letter jumps to the next category starting with it; "(all categories)" clears the filter. Works in Installed and Repository views and combines with name or description search.
- The header shows the active category (`[Installed · editors]`); the Search menu includes "Filter by category..." and "All categories".
- Tests cover categories and filtering in the model, F8 (letter jump, selection, combining filters, clearing), the Search menu, and an empty result list.

## [0.5.0] - 2026-10-20

Search by description.

### Added
- Description search, like OctoPkg's Search menu: the filter (Ctrl+L or `/`) searches by name or, in this mode, by both package name and comment, case-insensitively. For example, "editor" finds vim, nano, and emacs. Works in Installed and Repository views.
- **F7** toggles the mode. The **Search** menu (**Alt+B**, between File and Actions) shows the current mode and provides access to the filter (Ctrl+L) and tab search (Ctrl+F). The filter field shows the mode ("Filter by description..."), and the header shows "Description:" while active. The selection is saved as `search=` in the configuration file.
- Tests cover both search modes, F7, the Search menu, and restoring the saved preference in a new App.

## [0.4.0] - 2026-10-20

News tab: all OctoPkg tabs are now available in the terminal.

### Added
- **News** tab (**5** or **Alt+5**), like OctoPkg: the 10 latest news items for the system variant (FreeBSD, NomadBSD, HardenedBSD, GhostBSD, or DragonFly BSD, detected from `uname -a` and `/etc/rc.conf.ghostbsd`, as in OctoPkg), with title, date, link, and description. **Ctrl+G** or File → Fetch latest news downloads it again.
- The feed is downloaded with base-system `fetch(1)`, which verifies the TLS certificate (OctoPkg uses `curl -k`, which does not), and is cached in `~/.config/octopkg-tui/news.xml`. On the first visit to the tab, cached items appear immediately while new items load in the background. If there is no network or the feed cannot be read, cached items remain visible with a warning. New items mark the tab with `*`.
- The RSS reader handles escaped HTML and CDATA, entities (`&amp;`, `&#233;`, etc.), and title-less FreeBSD feed notices ("New committer: ..."), which use the description as the title. Descriptions wrap to the terminal width.
- **Ctrl+F** also searches the News tab.
- `--news=freebsd|nomadbsd|hardenedbsd|ghostbsd|dragonfly` selects the feed when automatic detection is incorrect.
- Tests cover RSS parsing (including the FreeBSD feed format), variant detection, downloads with a fake fetch (cache, changes, failures, and invalid feeds), and keyboard-driven tab behavior. On FreeBSD, integration tests fetch the real feed.

## [0.3.1] - 2026-10-20

Packaging: the FreeBSD port.

### Added
- Port in `freebsd/` for `ports-mgmt/octopkg-tui` (OctoPkg's category): FTXUI as a library dependency, the GPLv2+ license, and a `pkg-message` (generated with `SUB_FILES`) containing an example `doas.conf` and Nerd Font guidance.
- `ci/freebsd-port.sh`: port validation in the four FreeBSD jobs, as in bectl-gui (`portlint -A`, `stage-qa`, `check-plist` with `DEVELOPER=yes`, package creation and installation, `--version`, and the installed binary in a pseudo-terminal).
- CI requires the `CMakeLists.txt` and `freebsd/Makefile` versions to match.
- [docs/RELEASING.en.md](docs/RELEASING.en.md): versioning process and instructions for submitting the port to the official ports tree.

## [0.3.0] - 2026-10-20

Themes, Preferences, and search within tabs.

### Added
- The 15 bectl-gui 1.2.0 themes with the same palettes: System (terminal colors, as before), OctoPKG (light), Catppuccin Frappé, Latte, Mocha, and Macchiato, Solarized and Selenized (dark and light), Dracula, Tokyo Night, Nord, Gruvbox Dark, and One Dark. Background, text, selection, borders, dialogs, and package-state colors follow the theme. Terminals without 24-bit color support receive 256- or 16-color approximations. **Alt+T** cycles themes, and the active theme appears in the status bar.
- **Preferences** (**Alt+,** or File → Preferences...): theme, language (system default, English, Portuguese, Spanish), and icons (Nerd Font, Unicode, ASCII), applied immediately and saved in `~/.config/octopkg-tui/config`. `--theme` option.
- **Ctrl+F**, like OctoPkg, searches text in Files, Output, and Help tabs case-insensitively. Results are highlighted; Enter/F3 and Shift+F3 (or the arrow keys) move to the next and previous match and scroll the tab; Esc closes search.
- Tests cover themes and generated colors, saved and restored Preferences, and search in all three tabs; the smoke test opens Preferences.

## [0.2.0] - 2026-10-20

Transactions: mark, install, remove, upgrade, and clean, like OctoPkg.

### Added
- OctoPkg marks: **+** (or Insert) to install and **-** (or Delete) to remove the selected package; press again to unmark. `pkg` and `xorg` cannot be removed, as in OctoPkg, and locked packages cannot be marked.
- **Actions** tab (3), showing packages to install and remove; **Ctrl+E** clears marks. The status bar shows the number marked.
- **Alt+M** applies the transaction (Ctrl+M from OctoPkg arrives as Enter). Before execution, the unprivileged preview (`pkg remove -n -R` and `pkg install -n -f`) displays affected packages and download size. If a removal also removes packages that depend on the selected ones, the dialog becomes a red **Warning**, focused on Cancel. It then runs OctoPkg's commands: `pkg remove -R -y` and `pkg install -f -y`.
- **Ctrl+U** installs upgrades (`pkg upgrade -y`) with the same preview; **Ctrl+K** synchronizes the database (`pkg update -f`); File → Clean local cache (`pkg clean -a -y`) asks for confirmation; Actions → Lock and Unlock run `pkg lock` and `pkg unlock`.
- Boot Environment before a transaction, like OctoPkg: when the system has Boot Environments (`bectl check`), the dialog offers to create one (`bectl create AAMMDD-hhmmss`), enabled by default for install and upgrade and disabled for removal. No shell is used, unlike OctoPkg: if Boot Environment creation fails, `pkg` is not run.
- doas/sudo, like bectl-tui: without a password prompt, an operation runs in the background and `pkg` output appears live in the Output tab; if doas/sudo asks for a password, the screen is suspended while it uses the terminal. `elevator` preference and `--elevator=doas|sudo|none` option; `--bectl` sets the bectl path.
- File, Actions, and Help menus (**F9**, **Alt+A**, **Alt+C**, **Alt+J**) show each item's shortcuts. Help → About shows octopkg-tui and pkg versions.
- Form, confirmation, and message dialogs ported from bectl-tui; `pkg` errors appear in a dialog, status, and Output tab.
- mc-style key bar (F1, F5, F6, F9, F10, +/-, Alt+M) and a Help tab listing all shortcuts.
- Tests cover previews and operations using fake pkg, doas, and bectl executables; keyboard-driven marks, dialogs, menus, and operations; on FreeBSD, integration installs, locks, unlocks, and removes real `figlet`, and the interface does the same in a pseudo-terminal through the Actions tab. The smoke test opens the menu and marks a package.

### Changed
- Quitting (q, F10, Ctrl+Q) is blocked only during an operation; an ongoing list refresh no longer prevents exit.
- After a failed operation, the error remains in the status bar until the next action instead of being replaced by "Ready." when the list refreshes.

## [0.1.0] - 2026-10-19

First release: read-only.

### Added
- Main FTXUI screen with the OctoPkg layout: package list (state, name, version, size, installed date), search field, **Info**, **Files**, **Output**, and **Help** tabs numbered as in OctoPkg (1, 2, 4, and 6), status bar, and key bar.
- **Installed** and **Repository** views (`i`, Alt+I, or `--remote`), like OctoPkg's local and remote search, with states for installed, installed as a dependency, outdated (with available version), newer than repository, locked, and not installed.
- Info tab with version, origin, repository, URL, licenses, maintainer, architecture, size, install date, dependencies, reverse dependencies, options, and description; Files tab lists installed package files (up to 2 MiB of output, like OctoPkg). Details load in the background.
- Output tab with list operations and outdated packages (`pkg upgrade -n`, without updating the catalog).
- Column sorting (version by numeric components, size in bytes, date, and state in OctoPkg order): click a header or press **F6**. The order is saved in `~/.config/octopkg-tui/config`.
- Selection follows a package when filtering, sorting, and refreshing; the table draws only visible rows, so tens of thousands of repository packages scroll without delay.
- Nerd Font icons (default), Unicode, or ASCII (`--icons`).
- OctoPkg shortcuts delivered by the terminal (Alt+1/2/4/6, F1, Ctrl+L, Ctrl+P, Enter, Ctrl+Q), with alternatives for the others (see [docs/KEYMAP.en.md](docs/KEYMAP.en.md)); mouse support for headers, rows, and tabs.
- English (`en_US`), Portuguese (`pt_BR`), and Spanish (`es_ES`) locales (`--lang`).
- Dependency-free C++17 core porting OctoPkg package-reading code (`Package`, `UnixCommand`); `pkg(8)` runs without a shell and in English (`LC_ALL=C`), as in OctoPkg.
- Tests for the core (pkg output parsing and a fake pkg), model, view, integration with real `pkg(8)`, and the interface in a pseudo-terminal; CI on Ubuntu (FTXUI 6.1.9, ASan, UBSan) and FreeBSD 14.5 and 15.1, amd64 and aarch64.
- Compatible with FTXUI 6.1.9 and 7.x: the first list refresh starts from inside the event loop because FTXUI 6 drops tasks posted before it.

[0.7.1]: https://github.com/cleberalves40/octopkg-tui/compare/v0.7.0...v0.7.1
[0.7.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/cleberalves40/octopkg-tui/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/cleberalves40/octopkg-tui/releases/tag/v0.1.0
