# octopkg-tui

[![Build & Test](https://github.com/cleberalves40/octopkg-tui/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/octopkg-tui/actions/workflows/build.yml)
[![FreeBSD 16-CURRENT](https://github.com/cleberalves40/octopkg-tui/actions/workflows/freebsd16.yml/badge.svg)](https://github.com/cleberalves40/octopkg-tui/actions/workflows/freebsd16.yml)
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
 + curl                          8.9.1                   4.36 MiB  2026-10-01 10:00
 L pkg                           2.3.1                  24.80 MiB  2026-09-20 08:30
 I vim                           9.1.1000               38.90 MiB  2026-10-05 14:12
────────────────────────────────────────────────────────────────────────────────────
 1 i Informação   2 f Arquivos (34)   3 a Ações (1)   4 > Saída   6 ? Ajuda
curl  Command line tool and library for transferring data with URLs
Estado:           Desatualizado (a versão 8.10.0 está disponível)
Versão:           8.9.1
Origem:           ftp/curl
Repositório:      FreeBSD
URL:              https://curl.se/
────────────────────────────────────────────────────────────────────────────────────
Pronto.                                          +1  5 pacote(s)  1 desatualizado(s)
F1Ajuda  F5Atualizar  F6Ordenar  F9Menu  F10Sair  +/-Marcar  M-mAplicar
```

(com `--icons=ascii`: **I** instalado, **d** dependência, **U** desatualizado, **N** mais
novo que o repositório, **L** bloqueado, **-** não instalado; **+** marcado para
instalar e **x** para remover.)

## Estado

**v0.7.2.** Lista os pacotes instalados e os dos repositórios, com detalhes e
arquivos, busca por nome ou por descrição (**F7**), filtra por categoria (**F8**) e
maximiza a lista ou as abas (**F11**); marca pacotes para instalar (**+**) e remover (**-**), mostra a prévia da
transação e a executa (**Alt+M**), instala as atualizações (**Ctrl+U**), sincroniza o
banco (**Ctrl+K**), limpa o cache e bloqueia ou desbloqueia pacotes, com os comandos
do OctoPkg e, se o sistema usa Boot Environments, a opção de criar um antes. Tem os
15 temas do bectl-gui (**Alt+T**), Preferências (**Alt+,**) e a busca dentro das abas
(**Ctrl+F**), e a aba Notícias do OctoPkg (**5**, **Ctrl+G**). Tem um port FreeBSD
validado no CI. Veja o [CHANGELOG](CHANGELOG.md).

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
sem X11 nem Wayland. Ele serve, por exemplo, a um servidor acessado por SSH.

## Privilégios

A interface roda como usuário comum: listar, ver detalhes e calcular a prévia de uma
transação não pedem root. Só as operações (`pkg install`, `remove`, `upgrade`,
`update`, `clean`, `lock`, `unlock` e o `bectl create` do Boot Environment) passam
pelo **doas** (padrão) ou pelo **sudo**, sem shell. Se o doas/sudo não precisar de
senha, a operação roda em segundo plano e a saída aparece ao vivo na aba Saída; se
precisar, a tela é suspensa enquanto ele pede a senha no terminal. Como root, o `pkg`
roda direto.

Um `/usr/local/etc/doas.conf` para o grupo `wheel`, pedindo a senha uma vez a cada
alguns minutos:

```
permit persist :wheel cmd /usr/local/sbin/pkg
permit persist :wheel cmd /sbin/bectl
```

(O `bectl check`, que decide se a opção do Boot Environment aparece, roda como
usuário.)

Para usar o sudo: `--elevator=sudo`, ou `elevator=sudo` em
`~/.config/octopkg-tui/config`.

## Instalação pelo port

O port fica em `freebsd/` e é validado no CI. Ele compila a **última versão
publicada** (a tag no GitHub), com o `distinfo` dela; enquanto ele não está na
árvore oficial, a partir de um clone deste repositório, numa máquina FreeBSD com a
árvore de ports em `/usr/ports`:

```sh
mkdir -p /usr/ports/ports-mgmt/octopkg-tui/files
cp freebsd/Makefile freebsd/pkg-descr freebsd/distinfo /usr/ports/ports-mgmt/octopkg-tui/
cp freebsd/pkg-message.in /usr/ports/ports-mgmt/octopkg-tui/files/
cd /usr/ports/ports-mgmt/octopkg-tui && make install clean
```

Sem a árvore de ports (`/usr/ports/Mk`), o caminho é a compilação abaixo.

O envio à árvore oficial, como `ports-mgmt/octopkg-tui`, está em
[docs/RELEASING.md](docs/RELEASING.md).

## Compilação

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/octopkg-tui
```

Opções: `--remote` (começa na visão Repositório), `--icons=nerd|unicode|ascii`,
`--lang=en_US|pt_BR|es_ES`, `--theme=TEMA`, `--news=VARIANTE`, `--elevator=doas|sudo|none`,
`--config=ARQUIVO`, `--pkg=EXECUTÁVEL`, `--bectl=EXECUTÁVEL`, `--version`. As
preferências ficam em `~/.config/octopkg-tui/config`: a ordem da tabela, o modo de busca, o
tema, o idioma e os ícones (pelo diálogo de Preferências, **Alt+,**), e `elevator`, definido
à mão.

Temas, os mesmos do bectl-gui: `system` (as cores do terminal), `octopkg`, `frappe`,
`latte`, `mocha`, `macchiato`, `solarized_dark`, `solarized_light`, `selenized_dark`,
`selenized_light`, `dracula`, `tokyo_night`, `nord`, `gruvbox` e `one_dark`. Eles usam
cores de 24 bits; num terminal com menos, o FTXUI as aproxima.

## Teclado e mouse

| Tecla | Ação |
|:---|:---|
| <kbd>F5</kbd> / <kbd>Ctrl+R</kbd> | Atualizar a lista |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>PgUp</kbd> <kbd>PgDn</kbd> <kbd>Home</kbd> <kbd>End</kbd> (ou <kbd>j</kbd> <kbd>k</kbd>) | Mover a seleção |
| <kbd>Ctrl+L</kbd> / <kbd>/</kbd> | Buscar pelo nome, ou pela descrição (<kbd>Esc</kbd>, <kbd>Enter</kbd> ou <kbd>Ctrl+P</kbd> volta à lista) |
| <kbd>F7</kbd> | Buscar por nome ou por descrição (nome e comentário), como o menu Busca do OctoPkg |
| <kbd>F8</kbd> | Filtrar por categoria do port (editors, www, devel…); uma letra salta na lista |
| <kbd>i</kbd> / <kbd>Alt+I</kbd> | Alternar entre os pacotes instalados e os dos repositórios |
| <kbd>+</kbd> / <kbd>Insert</kbd> | Marcar o pacote para instalar (de novo: desmarca) |
| <kbd>-</kbd> / <kbd>Delete</kbd> | Marcar o pacote para remover |
| <kbd>Alt+M</kbd> | Aplicar: prévia, confirmação e a transação |
| <kbd>Ctrl+E</kbd> | Desfazer as marcas |
| <kbd>Ctrl+U</kbd> / <kbd>Ctrl+K</kbd> | Instalar as atualizações / sincronizar o banco |
| <kbd>F9</kbd> / <kbd>Alt+A</kbd> <kbd>Alt+B</kbd> <kbd>Alt+C</kbd> <kbd>Alt+J</kbd> | Menus Arquivo, Busca, Ações e Ajuda |
| <kbd>Ctrl+F</kbd> | Buscar texto nas abas Arquivos, Saída, Notícias e Ajuda (<kbd>F3</kbd> / <kbd>Shift+F3</kbd>: próximo / anterior; <kbd>Esc</kbd> fecha) |
| <kbd>Ctrl+G</kbd> | Buscar as últimas notícias do sistema (aba Notícias) |
| <kbd>Alt+,</kbd> / <kbd>Alt+T</kbd> | Preferências (tema, idioma, ícones) / próximo tema |
| <kbd>F6</kbd> | Escolher a coluna de ordenação: de novo na mesma coluna inverte, e "(ordem do pkg)" volta à ordem original |
| Clique no cabeçalho | Ordenar: crescente, decrescente, ordem do pkg |
| <kbd>1</kbd>…<kbd>6</kbd> / <kbd>Alt+1</kbd>…<kbd>Alt+6</kbd> | Abas Informação, Arquivos, Ações, Saída, Notícias e Ajuda, com os números do OctoPkg (<kbd>F1</kbd>: Ajuda) |
| <kbd>Enter</kbd> | Aba Informação do pacote selecionado |
| <kbd>Tab</kbd> | Rolar a aba com as setas (de novo, ou <kbd>Esc</kbd>, volta à lista) |
| <kbd>F11</kbd> / <kbd>Alt+Z</kbd> | Maximizar a lista, depois as abas, depois voltar às duas |
| <kbd>Ctrl+Q</kbd> / <kbd>F10</kbd> / <kbd>q</kbd> | Sair (não durante uma operação) |

O mapa completo, com a correspondência para os atalhos do OctoPkg, está em
[docs/KEYMAP.md](docs/KEYMAP.md).

## Arquitetura

| Camada | Local | Responsabilidade |
|:---|:---|:---|
| Núcleo | `src/core/` | `pkg(8)` sem shell e em inglês (`LC_ALL=C`), leitura de `pkg query`, `pkg rquery` e das prévias (`-n`), estados dos pacotes, as operações com doas/sudo e a saída ao vivo, Boot Environments, as notícias (variante, `fetch(1)`, RSS e cache), tamanhos, i18n e preferências. C++17 puro: um porte de `Package`, `UnixCommand`, das transações e das notícias do OctoPkg. |
| Modelo | `src/model/` | Filtro, ordenação (versão, tamanho, data, estado), seleção que acompanha o pacote e a janela visível da tabela |
| Interface | `src/tui/` | FTXUI: tela, temas, teclado, mouse, marcas, diálogos, menus e busca nas abas; listagem, detalhes, prévias e operações em segundo plano |

As consultas usam formatos próprios do `pkg query`/`pkg rquery` (campos separados por
TAB), em vez de ler a saída para pessoas de `pkg info` e `pkg search` como o OctoPkg:
o resultado não depende do alinhamento das colunas nem da versão do pkg.

## Testes

```sh
cmake -B build -S . -DOCTOPKG_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/freebsd-e2e.sh build    # no FreeBSD, como root: integração com o pkg real e a interface
sh tests/tui-smoke.sh build      # a interface num pseudo-terminal, com um pkg falso
```

- `test_core`: leitura da saída do pkg (incluindo as prévias de instalar, remover e
  atualizar), os estados da visão Repositório, um pkg, um doas e um bectl falsos que
  conferem os argumentos de cada operação, as notícias (RSS, variante e um fetch
  falso), a execução de processos e as preferências.
- `test_model`: filtro, ordenação, seleção e rolagem, com 40 mil pacotes.
- `test_view`: desenha a tela em texto e dirige a interface com eventos de teclado:
  marcas, prévias, diálogos, menus, operações, temas, Preferências e a busca nas abas.
- `test_integration`: contra o `pkg(8)` real; instala, bloqueia, desbloqueia e remove
  o `figlet`, e baixa o feed de notícias do FreeBSD com o `fetch(1)`.
- `tests/tui-smoke.sh`: a interface num pseudo-terminal com um pkg falso (lista,
  detalhes, menu, marca, Preferências e saída); roda também no Ubuntu, contra o FTXUI
  6.1.9.
- `tests/freebsd-e2e.sh`: além dos testes acima, instala e remove o `figlet` pela
  interface, num pseudo-terminal.

O CI roda no Ubuntu (com ASan e UBSan) e no FreeBSD 14.5 e 15.1, em amd64 e aarch64,
incluindo a interface num pseudo-terminal e a validação do port
(`ci/freebsd-port.sh`: portlint, stage-qa, pacote e o binário instalado). O
FreeBSD 16.0-CURRENT, que nenhuma action oferece, roda os mesmos testes na imagem
oficial de snapshot, no QEMU (`ci/freebsd16-vm.sh`): o amd64 em cada pull request e
as duas arquiteturas toda semana e no disparo manual.

## Licença

[GPL-2.0-or-later](LICENSE). O núcleo é um porte de código do OctoPkg, © Alexandre
Albuquerque Arnt, cujos fontes são GPL-2.0-or-later (o arquivo LICENSE do OctoPkg é a
LGPL-2.1, que também permite a redistribuição sob a GPL-2.0 ou posterior).
