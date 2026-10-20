# Changelog

Todas as mudanças relevantes do octopkg-tui são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [0.4.0] - 2026-10-20

A aba Notícias: agora todas as abas do OctoPkg estão no terminal.

### Adicionado
- Aba **Notícias** (**5** ou **Alt+5**), como no OctoPkg: as 10 últimas notícias da
  variante do sistema (FreeBSD, NomadBSD, HardenedBSD, GhostBSD ou DragonFly BSD,
  detectada pelo `uname -a` e pelo `/etc/rc.conf.ghostbsd`, como no OctoPkg), com
  título, data, link e descrição. **Ctrl+G** ou Arquivo → Buscar as últimas notícias
  baixa de novo.
- O feed é baixado com o `fetch(1)` do sistema base, que confere o certificado TLS
  (o OctoPkg usa `curl -k`, que não confere), e fica em
  `~/.config/octopkg-tui/news.xml`: na primeira visita à aba, as notícias salvas
  aparecem na hora e as novas chegam em segundo plano. Sem rede, ou com um feed que
  não dá para ler, ficam as salvas, com o aviso. Notícias novas marcam a aba com `*`.
- O leitor de RSS trata HTML escapado e CDATA, entidades (`&amp;`, `&#233;`...) e
  os avisos sem título do feed do FreeBSD ("New committer: ..."), que usam a
  descrição como título. A descrição é quebrada na largura do terminal.
- **Ctrl+F** busca também na aba Notícias.
- Opção `--news=freebsd|nomadbsd|hardenedbsd|ghostbsd|dragonfly`, para escolher o
  feed quando a detecção não acerta.
- Testes: o leitor de RSS (incluindo o formato do feed do FreeBSD), a detecção da
  variante, o download com um fetch falso (cache, mudança, falha e feed inválido) e
  a aba dirigida por teclas; no FreeBSD, a integração baixa o feed de verdade.

## [0.3.1] - 2026-10-20

Empacotamento: o port FreeBSD.

### Adicionado
- Port em `freebsd/`, para `ports-mgmt/octopkg-tui` (a categoria do OctoPkg): o
  FTXUI como dependência de biblioteca, a licença GPLv2+ e um `pkg-message` (gerado
  com `SUB_FILES`) com o `doas.conf` de exemplo e a fonte Nerd para os ícones.
- `ci/freebsd-port.sh`: validação do port nos quatro jobs FreeBSD, como no bectl-gui
  (`portlint -A`, `stage-qa` e `check-plist` com `DEVELOPER=yes`, o pacote, a
  instalação, o `--version` e o binário instalado num pseudo-terminal).
- `docs/RELEASING.md`: o processo de versão e os passos para enviar o port à árvore
  oficial do FreeBSD.
- CI: a versão do `CMakeLists.txt` e a do `freebsd/Makefile` precisam ser iguais.

## [0.3.0] - 2026-10-20

Temas, Preferências e busca nas abas.

### Adicionado
- Os 15 temas do bectl-gui 1.2.0, com as mesmas paletas: Sistema (as cores do
  terminal, como antes), OctoPKG (claro), Catppuccin Frappé, Latte, Mocha e
  Macchiato, Solarized e Selenized (escuro e claro), Dracula, Tokyo Night, Nord,
  Gruvbox Dark e One Dark. Fundo, texto, seleção, bordas, diálogos e as cores dos
  estados seguem o tema; num terminal sem cores de 24 bits, o FTXUI as reduz para
  256 ou 16. **Alt+T** passa ao próximo tema, e o tema aparece na barra de status.
- **Preferências** (**Alt+,** ou Arquivo → Preferências…): tema, idioma (padrão do
  sistema, English, Português, Español) e ícones (Nerd Fonts, Unicode, ASCII),
  aplicados na hora e salvos em `~/.config/octopkg-tui/config`. Opção `--theme`.
- **Ctrl+F**, como no OctoPkg: busca texto nas abas Arquivos, Saída e Ajuda, sem
  diferenciar maiúsculas; os resultados ficam destacados, e Enter/F3 e Shift+F3
  (ou as setas) vão ao próximo e ao anterior, rolando a aba; Esc fecha.
- Testes: os temas e as cores que eles produzem, as Preferências salvas e relidas,
  e a busca nas três abas; o teste de fumaça abre as Preferências.

## [0.2.0] - 2026-10-20

Transações: marcar, instalar, remover, atualizar e limpar, como no OctoPkg.

### Adicionado
- Marcas do OctoPkg: **+** (ou Insert) para instalar e **-** (ou Delete) para
  remover o pacote selecionado; de novo, desmarca. O `pkg` e o `xorg` não podem ser
  removidos, como no OctoPkg, e pacotes bloqueados não são marcados.
- Aba **Ações** (3), com os pacotes a instalar e a remover; **Ctrl+E** desfaz as
  marcas. A barra de status mostra quantos estão marcados.
- **Alt+M** aplica (o Ctrl+M do OctoPkg chega como Enter). Antes, a prévia sem
  privilégios (`pkg remove -n -R` e `pkg install -n -f`) vai a um diálogo com os
  pacotes afetados e o tamanho do download; quando a remoção leva pacotes que
  dependem dos marcados, o diálogo vira **Aviso**, em vermelho, com o foco em
  Cancelar. Depois, `pkg remove -R -y` e `pkg install -f -y`, os comandos do
  OctoPkg.
- **Ctrl+U** instala as atualizações (`pkg upgrade -y`), com a mesma prévia;
  **Ctrl+K** sincroniza o banco (`pkg update -f`); Arquivo → Limpar o cache local
  (`pkg clean -a -y`), com confirmação; Ações → Bloquear e Desbloquear (`pkg lock`
  e `pkg unlock`).
- Boot Environment antes da transação, como no OctoPkg: com o sistema em Boot
  Environments (`bectl check`), o diálogo oferece criar um (`bectl create
  AAMMDD-hhmmss`), marcado ao instalar e atualizar e desmarcado ao remover. Sem
  shell, ao contrário do OctoPkg: se o Boot Environment falhar, o `pkg` não roda.
- doas/sudo, como no bectl-tui: sem senha a pedir, a operação roda em segundo
  plano e a saída do `pkg` aparece ao vivo na aba Saída; se o doas/sudo pedir
  senha, a tela é suspensa enquanto ele usa o terminal. Preferência `elevator` e
  opção `--elevator=doas|sudo|none`; `--bectl` para o caminho do bectl.
- Menus Arquivo, Ações e Ajuda (**F9**, **Alt+A**, **Alt+C**, **Alt+J**), com os
  atalhos de cada item; Ajuda → Sobre mostra as versões do octopkg-tui e do pkg.
- Diálogos de formulário, confirmação e mensagem, portados do bectl-tui; erros do
  `pkg` aparecem num diálogo, no status e na aba Saída.
- Barra de teclas no estilo do mc (F1, F5, F6, F9, F10, +/-, Alt+M) e a aba Ajuda
  com todos os atalhos.
- Testes: as prévias e as operações contra um pkg, um doas e um bectl falsos; cada
  marca, diálogo, menu e operação dirigidos por teclas; no FreeBSD, a integração
  instala, bloqueia, desbloqueia e remove o `figlet` de verdade, e a interface faz o
  mesmo num pseudo-terminal, pela aba Ações; o teste de fumaça abre o menu e marca
  um pacote.

### Alterado
- Sair (q, F10, Ctrl+Q) é recusado só durante uma operação; uma listagem em
  andamento não impede mais a saída.
- Depois de uma operação que falhou, o erro continua na barra de status até a
  próxima ação, em vez de ser trocado por "Pronto." pela nova listagem.

## [0.1.0] - 2026-10-19

Primeira versão: somente leitura.

### Adicionado
- Tela principal em FTXUI com a estrutura do OctoPkg: lista de pacotes (estado,
  nome, versão, tamanho, instalado em), campo de busca, abas **Informação**,
  **Arquivos**, **Saída** e **Ajuda**, com os números do OctoPkg (1, 2, 4 e 6),
  barra de status e barra de teclas.
- Visões **Instalados** e **Repositório** (`i`, Alt+I ou `--remote`), como a busca
  local e remota do OctoPkg: os estados instalado, instalado como dependência,
  desatualizado (com a versão disponível), mais novo que o repositório, bloqueado
  e não instalado.
- Aba Informação com versão, origem, repositório, URL, licenças, mantenedor,
  arquitetura, tamanho, data de instalação, dependências, quem depende do pacote,
  opções e descrição; aba Arquivos com os arquivos do pacote instalado (até 2 MiB
  de saída, como no OctoPkg). Os detalhes são lidos em segundo plano.
- Aba Saída com as listagens e os pacotes desatualizados (`pkg upgrade -n`, sem
  atualizar o catálogo).
- Ordenação por coluna (versão por partes numéricas, tamanho em bytes, data e
  estado na ordem do OctoPkg): clique no cabeçalho ou **F6**. A ordem é salva em
  `~/.config/octopkg-tui/config`.
- Seleção que acompanha o pacote ao filtrar, ordenar e atualizar; a tabela desenha
  só as linhas visíveis, e as dezenas de milhares de pacotes do repositório
  rolam sem atraso.
- Ícones Nerd Fonts (padrão), Unicode ou ASCII (`--icons`).
- Atalhos do OctoPkg que o terminal entrega (Alt+1/2/4/6, F1, Ctrl+L, Ctrl+P,
  Enter, Ctrl+Q) e alternativas para os demais (veja `docs/KEYMAP.md`); mouse no
  cabeçalho, nas linhas e nas abas.
- Idiomas en_US, pt_BR e es_ES (`--lang`).
- Núcleo em C++17 sem dependências: porte da leitura de pacotes do OctoPkg
  (`Package`, `UnixCommand`), com o `pkg(8)` executado sem shell e em inglês
  (`LC_ALL=C`), como o OctoPkg faz.
- Testes: núcleo (leitura da saída do pkg e um pkg falso), modelo, tela,
  integração contra o `pkg(8)` real e a interface num pseudo-terminal; CI no
  Ubuntu (FTXUI 6.1.9, ASan e UBSan) e no FreeBSD 14.5 e 15.1, em amd64 e
  aarch64.
- Compatível com o FTXUI 6.1.9 e 7.x: a primeira listagem parte de dentro do
  laço de eventos, porque o FTXUI 6 descarta tarefas enviadas antes dele.

[0.4.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/cleberalves40/octopkg-tui/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/cleberalves40/octopkg-tui/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/cleberalves40/octopkg-tui/releases/tag/v0.1.0
