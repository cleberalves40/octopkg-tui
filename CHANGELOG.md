# Changelog

Todas as mudanças relevantes do octopkg-tui são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

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

[0.1.0]: https://github.com/cleberalves40/octopkg-tui/releases/tag/v0.1.0
