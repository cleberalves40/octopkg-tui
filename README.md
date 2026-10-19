# octopkg-tui

[![Build & Test](https://github.com/cleberalves40/octopkg-tui/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/octopkg-tui/actions/workflows/build.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2%2B-blue.svg)](LICENSE)

Interface de terminal (TUI) para o [`pkg(8)`](https://man.freebsd.org/cgi/man.cgi?pkg(8)),
o gerenciador de pacotes do FreeBSD. É a contraparte de terminal do
[OctoPkg](https://github.com/aarnt/octopkg) (fork em
[cleberalves40/octopkg](https://github.com/cleberalves40/octopkg)), com as mesmas abas e
atalhos sempre que o terminal permite, e ícones [Nerd Fonts](https://www.nerdfonts.com/).

```
 OctoPkg — pkg(8) [# Instalados]                    / Filtrar por nome…
────────────────────────────────────────────────────────────────────────────────────
   Nome ^                        Versão                   Tamanho  Instalado em
────────────────────────────────────────────────────────────────────────────────────
 d ca_root_nss                   3.104                   1.02 MiB  2026-10-01 10:00
 I cmake                         3.31.6                 45.10 MiB  2026-10-01 10:02
 U curl                          8.9.1                   4.36 MiB  2026-10-01 10:00
 L pkg                           2.3.1                  24.80 MiB  2026-09-20 08:30
 I vim                           9.1.1000               38.90 MiB  2026-10-05 14:12
────────────────────────────────────────────────────────────────────────────────────
 1 i Informação   2 f Arquivos (34)   4 > Saída   6 ? Ajuda
curl  Command line tool and library for transferring data with URLs
Estado:           Desatualizado (a versão 8.10.0 está disponível)
Versão:           8.9.1
Origem:           ftp/curl
Repositório:      FreeBSD
URL:              https://curl.se/
────────────────────────────────────────────────────────────────────────────────────
Pronto.                                              5 pacote(s)  1 desatualizado(s)
F5 Atualizar  F6 Ordenar  ^L Buscar  i Repositório  1-6 Abas  F10 Sair
```

(com `--icons=ascii`: **I** instalado, **d** dependência, **U** desatualizado, **N** mais
novo que o repositório, **L** bloqueado, **-** não instalado.)

## Estado

**v0.1.0: somente leitura.** Lista os pacotes instalados e os dos repositórios, com os
estados do OctoPkg, busca, ordenação, detalhes, arquivos, saída e ajuda. Marcar,
instalar, remover, atualizar e limpar o cache (as abas Ações e a transação do OctoPkg)
chegam na v0.2.0. Veja o [CHANGELOG](CHANGELOG.md).

## Requisitos (FreeBSD)

```sh
pkg install ftxui cmake ninja
pkg install nerd-fonts-jetbrainsmono      # ícones; opcional
```

Se o `pkg` não encontrar o `ftxui` (o ramo `latest` do FreeBSD 15/aarch64 ficou sem ele em
outubro de 2026), instale do ramo `quarterly`: veja `ci/freebsd-ci.sh`.

Para os ícones, configure o terminal com a fonte **JetBrainsMono Nerd Font Mono**. Sem
uma fonte Nerd, use `--icons=unicode` ou `--icons=ascii`.

Ao contrário do OctoPkg, o octopkg-tui não depende do Qt: o FTXUI é um único pacote,
sem X11 nem Wayland. Ele serve, por exemplo, a um servidor acessado por SSH. Nesta
versão, só leitura, ele roda como usuário comum: as consultas ao `pkg` não pedem root.

## Compilação

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/octopkg-tui
```

Opções: `--remote` (começa na visão Repositório), `--icons=nerd|unicode|ascii`,
`--lang=en_US|pt_BR|es_ES`, `--config=ARQUIVO`, `--pkg=EXECUTÁVEL`, `--version`. As
preferências ficam em `~/.config/octopkg-tui/config`; hoje, a ordem da tabela.

## Teclado e mouse (v0.1.0)

| Tecla | Ação |
|:---|:---|
| <kbd>F5</kbd> / <kbd>Ctrl+R</kbd> | Atualizar a lista |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>PgUp</kbd> <kbd>PgDn</kbd> <kbd>Home</kbd> <kbd>End</kbd> (ou <kbd>j</kbd> <kbd>k</kbd>) | Mover a seleção |
| <kbd>Ctrl+L</kbd> / <kbd>/</kbd> | Buscar pelo nome (<kbd>Esc</kbd>, <kbd>Enter</kbd> ou <kbd>Ctrl+P</kbd> volta à lista) |
| <kbd>i</kbd> / <kbd>Alt+I</kbd> | Alternar entre os pacotes instalados e os dos repositórios |
| <kbd>F6</kbd> | Escolher a coluna de ordenação: de novo na mesma coluna inverte, e "(ordem do pkg)" volta à ordem original |
| Clique no cabeçalho | Ordenar: crescente, decrescente, ordem do pkg |
| <kbd>1</kbd> <kbd>2</kbd> <kbd>4</kbd> <kbd>6</kbd> / <kbd>Alt+1</kbd> <kbd>Alt+2</kbd> <kbd>Alt+4</kbd> <kbd>Alt+6</kbd> | Abas Informação, Arquivos, Saída, Ajuda, com os números do OctoPkg (<kbd>F1</kbd>: Ajuda) |
| <kbd>Enter</kbd> | Aba Informação do pacote selecionado |
| <kbd>Tab</kbd> | Rolar a aba com as setas (de novo, ou <kbd>Esc</kbd>, volta à lista) |
| <kbd>Ctrl+Q</kbd> / <kbd>F10</kbd> / <kbd>q</kbd> | Sair |

O mapa completo, com a correspondência para os atalhos do OctoPkg, está em
[docs/KEYMAP.md](docs/KEYMAP.md).

## Arquitetura

| Camada | Local | Responsabilidade |
|:---|:---|:---|
| Núcleo | `src/core/` | `pkg(8)` sem shell e em inglês (`LC_ALL=C`), leitura de `pkg query`, `pkg rquery` e `pkg upgrade -n`, estados dos pacotes, tamanhos, i18n e preferências. C++17 puro: um porte da leitura de pacotes do OctoPkg (`Package`, `UnixCommand`). |
| Modelo | `src/model/` | Filtro, ordenação (versão, tamanho, data, estado), seleção que acompanha o pacote e a janela visível da tabela |
| Interface | `src/tui/` | FTXUI: tela, teclado, mouse, listagem e detalhes em segundo plano |

As consultas usam formatos próprios do `pkg query`/`pkg rquery` (campos separados por
TAB), em vez de ler a saída para pessoas de `pkg info` e `pkg search` como o OctoPkg:
o resultado não depende do alinhamento das colunas nem da versão do pkg.

## Testes

```sh
cmake -B build -S . -DOCTOPKG_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/freebsd-e2e.sh build    # no FreeBSD: integração com o pkg real e a interface
sh tests/tui-smoke.sh build      # a interface num pseudo-terminal, com um pkg falso
```

- `test_core`: leitura da saída do pkg (incluindo a de `pkg upgrade -n`), os estados
  da visão Repositório, um pkg falso que confere os argumentos, a execução de
  processos e as preferências.
- `test_model`: filtro, ordenação, seleção e rolagem, com 40 mil pacotes.
- `test_view`: desenha a tela em texto e dirige a interface com eventos de teclado.
- `test_integration`: contra o `pkg(8)` real, só leitura.
- `tests/tui-smoke.sh`: a interface num pseudo-terminal com um pkg falso (lista,
  detalhes e saída); roda também no Ubuntu, contra o FTXUI 6.1.9.

O CI roda no Ubuntu (com ASan e UBSan) e no FreeBSD 14.5 e 15.1, em amd64 e aarch64,
incluindo a interface num pseudo-terminal.

## Licença

[GPL-2.0-or-later](LICENSE). O núcleo é um porte de código do OctoPkg, © Alexandre
Albuquerque Arnt, cujos fontes são GPL-2.0-or-later (o arquivo LICENSE do OctoPkg é a
LGPL-2.1, que também permite a redistribuição sob a GPL-2.0 ou posterior).
