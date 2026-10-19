#!/bin/sh
#
# Testes no FreeBSD: os de núcleo, modelo e tela; o de integração contra o
# pkg(8) real, só leitura; e a interface num pseudo-terminal, nas visões
# Instalados e Repositório. Espera os pacotes de compilação instalados e o
# catálogo do repositório atualizado (ci/freebsd-ci.sh cuida disso).
#
# uso: tests/freebsd-e2e.sh [diretório-de-build]

set -eu

build=$(cd "${1:-build}" && pwd)

step() { echo "== $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

for t in test_core test_model test_view; do
	step "$t"
	"$build/$t"
done

pkgbin=/usr/local/sbin/pkg
step "test_integration ($($pkgbin -v))"
OCTOPKG_TEST_PKG=$pkgbin "$build/test_integration"

work=$(mktemp -d /var/tmp/octopkg-tui-e2e.XXXXXX)
trap 'rm -rf "$work"' EXIT

# A interface lista os pacotes e sai com "q", num pseudo-terminal. O "q" só é
# enviado quando o texto esperado aparece na tela: num aarch64 emulado a
# listagem demora, e uma espera fixa não basta. script -F grava a tela à
# medida que ela é desenhada; plain() tira as sequências de escape que o FTXUI
# intercala no texto.
esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$1"
}

# ui <nome> <texto que indica a tela pronta> [opções...]
ui() {
	name=$1 ready=$2
	shift 2
	set +e
	(
		for i in $(seq 1 120); do
			plain "$work/$name.raw" 2>/dev/null | grep -aq "$ready" && break
			sleep 1
		done
		printf q
	) | TERM=xterm-256color timeout 240 script -F -q "$work/$name.raw" \
		"$build/octopkg-tui" --icons=ascii --lang=en_US --config="$work/config" "$@" \
		>/dev/null 2>&1
	rc=$?
	set -e
	[ "$rc" -eq 0 ] || fail "octopkg-tui ($name) terminou com status $rc"
	plain "$work/$name.raw" >"$work/$name.txt"
}

count=$($pkgbin query -a %n | wc -l | tr -d ' ')

# Pronta quando a aba Informação do primeiro pacote carregou os detalhes.
step "interface: instalados (pseudo-terminal)"
ui installed "Origin:"
for text in "OctoPkg" "Installed packages" "$count package(s)" "Origin:" "Repository:"; do
	grep -aq "$text" "$work/installed.txt" || fail "a tela (instalados) não mostrou: $text"
done

step "interface: repositório (pseudo-terminal)"
ui remote "@ Repository]" --remote
grep -aq "@ Repository]" "$work/remote.txt" || fail "a tela (repositório) não mostrou a visão Repositório"

"$build/octopkg-tui" --version

step "todos os testes passaram"
