#!/bin/sh
#
# Valida o port (freebsd/) com o framework de ports do FreeBSD, como no
# bectl-gui: portlint, build, stage-qa, check-plist, geração do pacote,
# instalação do pacote e o binário instalado num pseudo-terminal. Roda como
# root na VM de CI, depois de ci/freebsd-ci.sh (que já instalou as
# dependências de build), a partir da raiz do código-fonte.
#
# O distfile é gerado do commit em teste, com o nome e o prefixo que o
# USE_GITHUB espera, e o distinfo é refeito para ele: a tag da versão só
# existe depois do merge. O freebsd/distinfo do repositório, o da última tag
# publicada, é conferido à parte (ci/port-distinfo.sh, no job Ubuntu).

set -eu

step() { echo "== $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

src=$(pwd)
ports=/var/tmp/ports
distdir=/var/tmp/distfiles
packages=/var/tmp/packages
port=$ports/ports-mgmt/octopkg-tui

pkg install -y git portlint >/dev/null
echo "ftxui: $(pkg query '%n-%v' ftxui); bibliotecas:"
pkg info -l ftxui | grep '/lib/libftxui' || true

# DEVELOPER=yes faz os erros do stage-qa reprovarem o build.
mk() {
	make -C "$port" PORTSDIR="$ports" DISTDIR="$distdir" PACKAGES="$packages" \
		BATCH=yes DEVELOPER=yes "$@"
}

step "árvore de ports (só o framework e as dependências diretas do port)"
rm -rf "$ports"
git clone -q --depth 1 --filter=blob:none --sparse \
	https://github.com/freebsd/freebsd-ports.git "$ports"
git -C "$ports" sparse-checkout set Mk Templates Keywords
mkdir -p "$port" "$distdir" "$packages"
mkdir -p "$port/files"
cp freebsd/Makefile freebsd/pkg-descr "$port/"
cp freebsd/pkg-message.in "$port/files/"
# Cada dependência é "arquivo-ou-pacote:origem[@flavor][:target]".
origins=$(mk -V FETCH_DEPENDS -V EXTRACT_DEPENDS -V BUILD_DEPENDS -V LIB_DEPENDS \
	-V RUN_DEPENDS -V PKG_DEPENDS | tr ' ' '\n' | sed -n 's/^[^:]*:\([^:@]*\).*/\1/p' | sort -u)
echo $origins
git -C "$ports" sparse-checkout add $origins

step "distfile a partir do commit em teste"
distfile=$(mk -V DISTFILES:C/:.*//)
prefix=$(basename "$(mk -V WRKSRC)")
echo "$distfile ($prefix/)"
git -c safe.directory="$src" -C "$src" archive --format=tar.gz \
	--prefix="$prefix/" -o "$distdir/$distfile" HEAD
mk makesum clean
cat "$port/distinfo"

# O portlint confere, como faria antes de um commit na árvore oficial, que o
# port está no git e sem diretório de trabalho.
git -C "$ports" add --sparse ports-mgmt/octopkg-tui

step "portlint"
# -A: todos os testes; FATAL encerra com status diferente de zero. O
# portlint lê a árvore de ports do ambiente, não do make.
PORTSDIR=$ports portlint -A "$port"

step "build, stage-qa e check-plist"
mk stage stage-qa check-plist

step "pacote"
mk package
pkgfile=$(mk -V PKGFILE)
ls -l "$pkgfile"
pkg info -F "$pkgfile" | sed -n '1,/^Description/p'
pkg info -D -F "$pkgfile"

step "instalação do pacote"
pkg add "$pkgfile"
pkg info -l octopkg-tui
bin=/usr/local/bin/octopkg-tui
[ -x "$bin" ] || fail "octopkg-tui não foi instalado"
# O binário tem a versão do código em teste (o DISTVERSION pode ser a da
# última tag publicada).
version=$(sed -n 's/^project(octopkg_tui VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ "$("$bin" --version)" = "octopkg-tui $version" ] || fail "--version não mostrou $version"

# O binário do pacote, num pseudo-terminal: lista os pacotes, carrega os
# detalhes do primeiro ("Origin:") e sai com "q", como em tests/freebsd-e2e.sh.
step "binário instalado (pseudo-terminal)"
work=$(mktemp -d /var/tmp/octopkg-tui-port.XXXXXX)
esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$@"
}
set +e
(
	for i in $(seq 1 120); do
		plain "$work/screen.raw" 2>/dev/null | grep -aq "Origin:" && break
		sleep 1
	done
	sleep 1
	printf q
) | TERM=xterm-256color timeout 240 script -F -q "$work/screen.raw" \
	"$bin" --icons=ascii --lang=en_US --config="$work/config" >/dev/null 2>&1
rc=$?
set -e
plain "$work/screen.raw" >"$work/screen.txt"
[ "$rc" -eq 0 ] || { tail -c 3000 "$work/screen.txt"; fail "octopkg-tui instalado terminou com status $rc"; }
grep -aq "Origin:" "$work/screen.txt" || { tail -c 3000 "$work/screen.txt"; fail "a lista não apareceu"; }
rm -rf "$work"

pkg delete -y octopkg-tui >/dev/null
step "port validado"
