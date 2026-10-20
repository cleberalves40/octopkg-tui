#!/bin/sh
#
# Teste de fumaça da interface num pseudo-terminal, com um pkg falso: ela
# precisa listar os pacotes, carregar os detalhes do selecionado, abrir o menu
# (F9), marcar um pacote (+) e sair com "q". Funciona com o script(1) do BSD e com o do util-linux. Não precisa de
# root nem do pkg(8).
#
# uso: tests/tui-smoke.sh [diretório-de-build]

set -eu

build=$(cd "${1:-build}" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/octopkg-tui-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT

cat >"$work/pkg" <<'EOF'
#!/bin/sh
case "$1" in
query)
	if [ "$2" = -a ]; then
		printf 'curl\t8.9.1\tftp/curl\t4572000\t1760000000\t0\t0\t1\tFreeBSD\tTransfer data with URLs\n'
		printf 'tela-ok\t1.0\tmisc/tela-ok\t1024\t1760000000\t0\t1\t0\tFreeBSD\tSmoke test\n'
		exit 0
	fi
	case "$2" in
	%n*) printf '%s\t8.9.1\tftp/curl\tFreeBSD\tme@example.org\thttps://curl.se/\tFreeBSD:15:amd64\t4572000\t1760000000\tc\tdescription\n\036' "$3" ;;
	%Fp) echo /usr/local/bin/curl ;;
	esac ;;
upgrade)
	printf 'Installed packages to be UPGRADED:\n\tcurl: 8.9.1 -> 8.10.0\n'
	exit 1 ;;
esac
EOF
chmod +x "$work/pkg"

esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$1"
}

cmd="$build/octopkg-tui --pkg=$work/pkg --icons=ascii --lang=en_US --config=$work/config"
# Espera o texto aparecer na tela (até 60 s).
wait_for() {
	for i in $(seq 1 60); do
		plain "$work/screen.raw" 2>/dev/null | grep -aq "$1" && return 0
		sleep 1
	done
}
keys() {
	wait_for "ftp/curl"
	printf '\033[20~'   # F9
	wait_for "Check updates"
	printf '\033'
	sleep 1
	printf '+'
	wait_for "Actions (1)"
	printf q
}
set +e
if script --version 2>/dev/null | grep -q util-linux; then
	keys | TERM=xterm-256color timeout 120 script -q -f -e -c "$cmd" "$work/screen.raw" >/dev/null 2>&1
else
	# shellcheck disable=SC2086
	keys | TERM=xterm-256color timeout 120 script -F -q "$work/screen.raw" $cmd >/dev/null 2>&1
fi
rc=$?
set -e

plain "$work/screen.raw" >"$work/screen.txt"
[ "$rc" -eq 0 ] || { tail -c 4000 "$work/screen.txt"; echo "FAIL: octopkg-tui terminou com status $rc" >&2; exit 1; }
for text in "Installed packages" "tela-ok" "2 package(s)" "1 outdated" "Origin:" "ftp/curl" "Check updates" \
	"Actions (1)"; do
	grep -aq "$text" "$work/screen.txt" || {
		tail -c 4000 "$work/screen.txt"
		echo "FAIL: a tela não mostrou: $text" >&2
		exit 1
	}
done
echo "== interface ok"
