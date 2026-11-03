# Shortcuts: OctoPkg to octopkg-tui

[**English**](KEYMAP.en.md) | [Português](KEYMAP.md)

Whenever the terminal delivers them to the application, octopkg-tui keeps the OctoPkg 0.4.2 shortcuts. Some keys do not arrive, or arrive indistinguishable from another key, so octopkg-tui provides alternatives:

- The terminal sends <kbd>Ctrl+I</kbd> as <kbd>Tab</kbd> and <kbd>Ctrl+M</kbd> as <kbd>Enter</kbd>.
- <kbd>Ctrl+Shift+letter</kbd> arrives the same as <kbd>Ctrl+letter</kbd>.
- <kbd>Alt+key</kbd> arrives as <kbd>Esc</kbd> followed by the key. In macOS Terminal, enable "Use Option as Meta".

The tabs keep OctoPkg's numbers, from 1 to 6.

| Action | OctoPkg | octopkg-tui | Available since |
|:---|:---|:---|:---|
| Info / Files / Output / Help tab | Alt+1 / Alt+2 / Alt+4 / Alt+6 | **1 2 4 6** or **Alt+1 Alt+2 Alt+4 Alt+6** | v0.1.0 |
| Help | F1 | **F1** | v0.1.0 |
| Search for a package in the list | Ctrl+L | **Ctrl+L** or **/** (<kbd>Esc</kbd> or <kbd>Enter</kbd> returns to the list) | v0.1.0 |
| Search by name / description | Search menu | **F7** toggles; Search menu (**Alt+B**) | v0.5.0 |
| Filter by category | — (OctoPkg groups, F9, are not available in pkg) | **F8**; Search menu | v0.6.0 |
| Maximize the list / tabs | F10 / F11 | **F11** or **Alt+Z**, cycling through list only, tabs only, and both (F10 quits) | v0.7.0 |
| Return to the package list | Ctrl+P | **Ctrl+P** or **Esc** | v0.1.0 |
| Installed / repository packages | Ctrl+I | **i** or **Alt+I** | v0.1.0 |
| Info tab for the selected package | Enter | **Enter** | v0.1.0 |
| Sort | Click a column header | Click a column header or **F6** (choose the column, like htop) | v0.1.0 |
| Refresh the list | — | **F5** / **Ctrl+R** | v0.1.0 |
| Scroll the current tab | — (mouse) | **Tab**, then arrow keys, PgUp, PgDn | v0.1.0 |
| Quit | Ctrl+Q | **Ctrl+Q**, **F10**, or **q** (not during an operation) | v0.1.0 |
| Mark for installation / removal | + / − | **+** / **−**, or **Insert** / **Delete** | v0.2.0 |
| Actions tab | Alt+3 | **3** or **Alt+3** | v0.2.0 |
| Apply | Ctrl+M | **Alt+M** (Ctrl+M is Enter) | v0.2.0 |
| Cancel (undo package marks) | Ctrl+E | **Ctrl+E** | v0.2.0 |
| Check for updates (pkg update) | Ctrl+K | **Ctrl+K** | v0.2.0 |
| Install upgrades (pkg upgrade) | Ctrl+U | **Ctrl+U** | v0.2.0 |
| Clean the cache (pkg clean) | Ctrl+Shift+C | File menu (Ctrl+Shift+C arrives as Ctrl+C, which exits FTXUI) | v0.2.0 |
| Lock / unlock | Context menu | Actions menu | v0.2.0 |
| Menus | Menu bar | **F9** or **Alt+A** (File), **Alt+B** (Search, v0.5.0), **Alt+C** (Actions), **Alt+J** (Help), as in bectl-tui | v0.2.0 |
| Search text in Files, Output, News, and Help tabs | Ctrl+F (F3 / Shift+F3 in search bar) | **Ctrl+F**; **Enter**/**F3** and **Shift+F3** (or ↓ ↑) go to next and previous; **Esc** closes | v0.3.0 |
| Preferences (theme, language, icons) | — (bectl-gui uses Ctrl+,) | **Alt+,** (Ctrl+, is not delivered by the terminal), or File → Preferences... | v0.3.0 |
| Next theme | — (bectl-gui uses Ctrl+Shift+T) | **Alt+T** | v0.3.0 |
| News tab / fetch latest news | Alt+5 / Ctrl+G | **5** or **Alt+5** / **Ctrl+G** | v0.4.0 |

In dialogs, <kbd>Tab</kbd> or the arrow keys change the field, <kbd>Space</kbd> selects the option to create a Boot Environment, <kbd>Enter</kbd> confirms, and <kbd>Esc</kbd> cancels. In confirmations, <kbd>y</kbd>/<kbd>s</kbd> and <kbd>n</kbd> are also accepted.

Not available in the terminal: OctoPkg's F4 (open a terminal in the directory) and F6 (file manager); F6 sorts, as in bectl-tui. F10 quits, as in htop and mc, and maximizing the list or tabs uses F11 (or Alt+Z).

FTXUI puts the terminal in raw mode, so <kbd>Ctrl+Q</kbd> reaches the application instead of resuming terminal flow control (XON/XOFF).
