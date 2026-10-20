# Versões e envio do port

## Versionamento

O projeto segue o [versionamento semântico](https://semver.org/lang/pt-BR/), com tags
`vX.Y.Z`:

- **patch** (`v0.3.1`): correções e empacotamento, sem funcionalidades novas;
- **minor** (`v0.4.0`): funcionalidades novas, compatíveis;
- **major** (`v1.0.0`): mudanças incompatíveis.

A versão fica em dois arquivos, e o CI reprova se eles divergirem ou se o
`CHANGELOG.md` não tiver a entrada correspondente:

| Arquivo | Campo |
|:---|:---|
| `CMakeLists.txt` | `project(octopkg_tui VERSION X.Y.Z ...)`: a fonte da versão exibida no `--version` e no Sobre |
| `freebsd/Makefile` | `DISTVERSION=` |

## Publicando uma versão

1. Num branch `release-X.Y.Z`, altere os dois arquivos e registre as mudanças no
   `CHANGELOG.md`, no formato [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/),
   com o link de comparação no fim do arquivo.
2. Abra o pull request. O CI precisa passar no Ubuntu (com os testes também sob ASan e
   UBSan, e a interface num pseudo-terminal contra o FTXUI 6.1.9), nos quatro jobs
   FreeBSD Tier 1 (14.5 e 15.1, amd64 e aarch64), que incluem a integração com o
   `pkg(8)` real e a validação do port, e no FreeBSD 16.0-CURRENT/amd64. Antes de uma
   versão que mude o comportamento no sistema, rode também o 16.0-CURRENT/aarch64
   (Actions → FreeBSD 16-CURRENT → Run workflow), que leva mais tempo.
3. Faça o merge com um *merge commit*.
4. Depois do CI verde no commit de merge, crie a tag anotada nele e publique a release
   com o texto do CHANGELOG, a tag anterior para rollback e o link de comparação:

   ```sh
   git fetch origin
   git tag -a vX.Y.Z -m "octopkg-tui vX.Y.Z" origin/main
   git push origin vX.Y.Z
   ```

## O port

O port fica em `freebsd/` (`Makefile`, `pkg-descr` e `pkg-message.in`, com o exemplo de
`doas.conf`, que vai para `files/` no port) e é validado a cada pull request por `ci/freebsd-port.sh`, nas quatro
combinações de release e arquitetura:

- `portlint -A`, com o port numa árvore de ports em git, como antes de um commit na
  árvore oficial;
- `make stage stage-qa check-plist` com `DEVELOPER=yes`, que reprova os erros do
  `stage-qa` (por exemplo, uma biblioteca usada e não declarada);
- `make package`, a instalação do pacote com `pkg add`, o `--version` e o binário
  instalado num pseudo-terminal.

Como a tag só existe depois do merge, o distfile é gerado do commit em teste, com o nome
e o prefixo que o `USE_GITHUB` espera.

## Enviando o port ao FreeBSD

Para propô-lo na árvore oficial, como `ports-mgmt/octopkg-tui` (a categoria do
OctoPkg), depois de publicar a tag:

1. Numa máquina FreeBSD com a árvore de ports atualizada, copie `Makefile` e
   `pkg-descr` de `freebsd/` para `/usr/ports/ports-mgmt/octopkg-tui/` e
   `pkg-message.in` para `files/`, e gere o `distinfo` a partir do tarball da tag
   publicada:

   ```sh
   cd /usr/ports/ports-mgmt/octopkg-tui
   make makesum
   ```

2. Teste num ambiente limpo, nas arquiteturas que puder (o CI não usa o `poudriere`):

   ```sh
   portlint -AC
   poudriere testport -j <jail> -o ports-mgmt/octopkg-tui
   ```

3. Adicione `octopkg-tui` ao `SUBDIR` de `ports-mgmt/Makefile` e gere o patch com
   `git format-patch`, a partir da árvore de ports em git.
4. Abra um relatório em <https://bugs.freebsd.org/> (produto *Ports & Packages*,
   componente *Individual Port(s)*), com o título `[NEW PORT] ports-mgmt/octopkg-tui:
   Terminal front-end for pkg(8), derived from OctoPkg`, e anexe o patch e o log do
   `poudriere testport`.

O `MAINTAINER` do port é `cleber@bsd.com.br`, que recebe as notificações do relatório e
das atualizações futuras.
