# Versioning and port submission

[**English**](RELEASING.en.md) | [Português](RELEASING.md)

## Versioning

The project follows [Semantic Versioning](https://semver.org/) with `vX.Y.Z` tags:

- **patch** (`v0.3.1`): fixes and packaging, with no new features;
- **minor** (`v0.4.0`): new backward-compatible features;
- **major** (`v1.0.0`): incompatible changes.

The version is recorded in two files. CI fails if they differ or if the corresponding `CHANGELOG.md` entry is missing:

| File | Field |
|:---|:---|
| `CMakeLists.txt` | `project(octopkg_tui VERSION X.Y.Z ...)`: source for the version shown by `--version` and About |
| `freebsd/Makefile` | `DISTVERSION=` |

## Publishing a release

1. On a `release-X.Y.Z` branch, update both version files and record the changes in `CHANGELOG.md` (Portuguese) and `CHANGELOG.en.md` (English), using [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), with comparison links at the end of both files.
2. Open a pull request. CI must pass on Ubuntu (including ASan and UBSan tests and the interface in a pseudo-terminal against FTXUI 6.1.9), on all four FreeBSD Tier 1 jobs (14.5 and 15.1, amd64 and aarch64), which include integration with real `pkg(8)` and port validation, and on FreeBSD 16.0-CURRENT/amd64. Before a release that changes system behavior, also run 16.0-CURRENT/aarch64 (Actions → FreeBSD 16-CURRENT → Run workflow); this takes longer.
3. Merge using a *merge commit*.
4. After CI passes on the merge commit, create an annotated tag on it and publish the release with the English changelog text, the previous tag for rollback, and the comparison link. The Portuguese notes can be included alongside it:

   ```sh
   git fetch origin
   git tag -a vX.Y.Z -m "octopkg-tui vX.Y.Z" origin/main
   git push origin vX.Y.Z
   ```

## The port

The port lives in `freebsd/` (`Makefile`, `pkg-descr`, and `pkg-message.in`, with the example `doas.conf` that is installed under the port's `files/` directory). It is validated on every pull request by `ci/freebsd-port.sh` on all four FreeBSD release and architecture combinations:

- `portlint -A`, with the port in a git-based ports tree, as before a commit to the official tree;
- `make stage stage-qa check-plist` with `DEVELOPER=yes`, which fails on stage-qa errors (for example, an undeclared library dependency);
- `make package`, package installation with `pkg add`, `--version`, and the installed binary in a pseudo-terminal.

Because the tag only exists after the merge, the distfile is generated from the commit under test, using the name and prefix expected by `USE_GITHUB`.

## Submitting the port to FreeBSD

To propose it for the official tree as `ports-mgmt/octopkg-tui` (the OctoPkg category), after publishing the tag:

1. On a FreeBSD machine with an up-to-date ports tree, copy `Makefile` and `pkg-descr` from `freebsd/` to `/usr/ports/ports-mgmt/octopkg-tui/`, copy `pkg-message.in` to `files/`, and generate `distinfo` from the published tag tarball:

   ```sh
   cd /usr/ports/ports-mgmt/octopkg-tui
   make makesum
   ```

2. Test in a clean environment on as many architectures as possible (CI does not use poudriere):

   ```sh
   portlint -AC
   poudriere testport -j <jail> -o ports-mgmt/octopkg-tui
   ```

3. Add `octopkg-tui` to `SUBDIR` in `ports-mgmt/Makefile` and generate the patch with `git format-patch` from the git-based ports tree.
4. File a report at <https://bugs.freebsd.org/> (product *Ports & Packages*, component *Individual Port(s)*), titled `[NEW PORT] ports-mgmt/octopkg-tui: Terminal front-end for pkg(8), derived from OctoPkg`, and attach the patch and `poudriere testport` log.

The port's `MAINTAINER` is `cleber@bsd.com.br`, who will receive notifications for the report and future updates.
