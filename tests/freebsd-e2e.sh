#!/bin/sh
#
# Testes no FreeBSD: os de núcleo, modelo e tela; o de integração contra o
# pkg(8) real (inclusive instalar e remover o figlet); e a interface num
# pseudo-terminal, nas visões Instalados e Repositório, instalando e removendo
# o figlet pela aba Ações. Precisa de root e espera os pacotes de compilação
# instalados e o catálogo do repositório atualizado (ci/freebsd-ci.sh cuida
# disso).
#
# uso: tests/freebsd-e2e.sh [diretório-de-build]

set -eu

build=$(cd "${1:-build}" && pwd)

step() { echo "== $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }
# Falha da interface: mostra o fim do que foi desenhado, para o diagnóstico.
uifail() {
	echo "-- fim da tela ($1):" >&2
	tail -c 6000 "$work/$1.txt" | tr -s ' ' >&2
	fail "$2"
}

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
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$@"
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
	plain "$work/$name.raw" >"$work/$name.txt"
	[ "$rc" -eq 0 ] || uifail "$name" "octopkg-tui ($name) terminou com status $rc"
}

count=$($pkgbin query -a %n | wc -l | tr -d ' ')

# Pronta quando a aba Informação do primeiro pacote carregou os detalhes.
step "interface: instalados (pseudo-terminal)"
ui installed "Origin:"
for text in "OctoPkg" "Installed packages" "$count package(s)" "Origin:" "Repository:"; do
	grep -aq "$text" "$work/installed.txt" || uifail installed "a tela (instalados) não mostrou: $text"
done

step "interface: repositório (pseudo-terminal)"
ui remote "@ Repository]" --remote
grep -aq "@ Repository]" "$work/remote.txt" || uifail remote "a tela (repositório) não mostrou a visão Repositório"

# Uma transação pela interface: na visão Repositório, busca o figlet, marca
# com +, aplica (Alt+M) e confirma; depois o remove com -. Cada tecla só vai
# quando a tela mostra o resultado da anterior. Como root, não há doas.
step "interface: instalar e remover pela aba Ações (pseudo-terminal)"
$pkgbin query %n figlet >/dev/null 2>&1 && fail "o figlet já estava instalado"
# send manda a tecla e marca até onde a tela já foi gravada; wait_for só olha
# o que foi desenhado depois disso, então um texto de antes (outro pacote
# selecionado) não conta.
from=1
send() {
	from=$(($(wc -c <"$work/tx.raw" 2>/dev/null || echo 0) + 1))
	printf -- "$1"
}
wait_for() {
	for i in $(seq 1 240); do
		tail -c +"$from" "$work/tx.raw" 2>/dev/null | plain | grep -aq "$1" && return 0
		sleep 1
	done
}
set +e
(
	wait_for "@ Repository]"
	send '/figlet\r'
	sleep 2
	send '+'
	wait_for "3 a Actions (1)"
	send '\033m'
	wait_for "needs to be retrieved"
	send '\r'
	wait_for "Installed: figlet"
	# A lista é refeita depois da transação; só então o figlet aparece
	# instalado ("I", com --icons=ascii) e pode ser marcado para remover.
	send '\r'
	wait_for "I figlet"
	send '-'
	wait_for "x figlet"
	send '\033m'
	wait_for "will be removed"
	send '\r'
	wait_for "Removed: figlet"
	send '\r'
	wait_for "- figlet"
	send q
) | TERM=xterm-256color timeout 900 script -F -q "$work/tx.raw" \
	"$build/octopkg-tui" --remote --icons=ascii --lang=en_US --config="$work/config" >/dev/null 2>&1
rc=$?
set -e
plain "$work/tx.raw" >"$work/tx.txt"
[ "$rc" -eq 0 ] || uifail tx "octopkg-tui (transação) terminou com status $rc"
for text in "needs to be retrieved" "Installed: figlet" "will be removed" "Removed: figlet"; do
	grep -aq "$text" "$work/tx.txt" || uifail tx "a tela (transação) não mostrou: $text"
done
$pkgbin query %n figlet >/dev/null 2>&1 && fail "o figlet não foi removido"

"$build/octopkg-tui" --version

step "todos os testes passaram"
