#!/bin/sh
#
# Compila o octopkg-tui no FreeBSD e executa tests/freebsd-e2e.sh. Roda como
# root dentro da VM de CI, a partir da raiz do código-fonte.

set -eu

echo "== $(uname -srm), $(sysctl -n hw.ncpu) CPU(s), $(($(sysctl -n hw.physmem) / 1048576)) MB"

pkg install -y cmake ninja catch2 >/dev/null
# O ramo latest do FreeBSD 15/aarch64 ficou sem o ftxui (outubro de 2026); o
# quarterly o tem. Sem dependências, ele convive com os demais pacotes.
if ! pkg install -y ftxui >/dev/null 2>&1; then
	echo "== ftxui ausente no repositório padrão; instalando do quarterly"
	mkdir -p /usr/local/etc/pkg/repos
	cat >/usr/local/etc/pkg/repos/quarterly.conf <<'CONF'
quarterly: {
  url: "pkg+https://pkg.FreeBSD.org/${ABI}/quarterly",
  mirror_type: "srv",
  signature_type: "fingerprints",
  fingerprints: "/usr/share/keys/pkg",
  enabled: yes
}
CONF
	pkg update -q -r quarterly
	pkg install -y -r quarterly ftxui >/dev/null
fi

echo "== toolchain"
c++ --version | head -1
cmake --version | head -1
pkg query '%n-%v' ftxui catch2

echo "== build"
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DOCTOPKG_TUI_BUILD_TESTS=ON
if ! cmake --build build >build.log 2>&1; then
	cat build.log
	exit 1
fi
cat build.log
echo "== warnings do compilador: $(grep -c 'warning:' build.log || true)"
file build/octopkg-tui

sh tests/freebsd-e2e.sh build
