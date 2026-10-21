#!/bin/sh
#
# Aponta o port (freebsd/) para uma versão publicada: ajusta o DISTVERSION
# do freebsd/Makefile e gera o freebsd/distinfo a partir do tarball que o
# GitHub serve para a tag, o mesmo que o USE_GITHUB baixa. Roda em qualquer
# sistema com sh, curl e sha256sum (ou shasum).
#
# O distinfo não pode vir no próprio release: o tarball da tag contém o
# freebsd/distinfo, e o checksum mudaria ao incluí-lo. Por isso o port
# acompanha a última tag publicada, num PR depois de cada release (veja
# docs/RELEASING.md).
#
# uso: ci/port-distinfo.sh X.Y.Z [--check]
#   --check: não altera nada; confere se freebsd/distinfo bate com a tag.

set -eu

version=$1
mode=${2:-write}
account=cleberalves40
name=octopkg-tui
distfile=$account-$name-v${version}_GH0.tar.gz
url="https://codeload.github.com/$account/$name/tar.gz/v$version?dummy=/"

fail() { echo "FAIL: $*" >&2; exit 1; }
sha256() {
	if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
	elif command -v sha256 >/dev/null 2>&1; then sha256 -q "$1"
	else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/port-distinfo.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
code=$(curl -sSL -o "$tmp/$distfile" -w '%{http_code}' "$url") || fail "não foi possível baixar $url"
[ "$code" = 200 ] || fail "a tag v$version não está publicada (HTTP $code)"
# O diretório no tarball é o WRKSRC do port: octopkg-tui-X.Y.Z/.
top=$(tar -tzf "$tmp/$distfile" | head -1)
[ "$top" = "$name-$version/" ] || fail "o tarball começa por $top, não por $name-$version/"
sum=$(sha256 "$tmp/$distfile")
size=$(wc -c <"$tmp/$distfile" | tr -d ' ')

if [ "$mode" = --check ]; then
	grep -qx "SHA256 ($distfile) = $sum" freebsd/distinfo || fail "freebsd/distinfo não tem o SHA256 de $distfile ($sum)"
	grep -qx "SIZE ($distfile) = $size" freebsd/distinfo || fail "freebsd/distinfo não tem o SIZE de $distfile ($size)"
	echo "freebsd/distinfo confere com a tag v$version: SHA256 $sum, $size bytes"
	exit 0
fi

sed -i.bak "s/^DISTVERSION=.*/DISTVERSION=	$version/" freebsd/Makefile && rm -f freebsd/Makefile.bak
{
	echo "TIMESTAMP = $(date +%s)"
	echo "SHA256 ($distfile) = $sum"
	echo "SIZE ($distfile) = $size"
} >freebsd/distinfo
echo "port em $version:"
cat freebsd/distinfo
