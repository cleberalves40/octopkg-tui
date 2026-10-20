# Atalhos: OctoPkg → octopkg-tui

O octopkg-tui mantém os atalhos do OctoPkg 0.4.2 sempre que o terminal os entrega ao
programa. Alguns não chegam, ou chegam iguais a outra tecla, e ganham uma alternativa:

- o terminal envia <kbd>Ctrl+I</kbd> como <kbd>Tab</kbd> e <kbd>Ctrl+M</kbd> como
  <kbd>Enter</kbd>;
- <kbd>Ctrl+Shift+letra</kbd> chega igual a <kbd>Ctrl+letra</kbd>;
- <kbd>Alt+tecla</kbd> chega como <kbd>Esc</kbd> seguido da tecla; no Terminal do macOS,
  ative "Usar Option como Meta".

As abas mantêm os números do OctoPkg, de 1 a 6.

| Ação | OctoPkg | octopkg-tui | Disponível |
|:---|:---|:---|:---|
| Aba Informação / Arquivos / Saída / Ajuda | Alt+1 / Alt+2 / Alt+4 / Alt+6 | **1 2 4 6** ou **Alt+1 Alt+2 Alt+4 Alt+6** | v0.1.0 |
| Ajuda | F1 | **F1** | v0.1.0 |
| Buscar um pacote na lista | Ctrl+L | **Ctrl+L** ou **/** (<kbd>Esc</kbd> ou <kbd>Enter</kbd> volta à lista) | v0.1.0 |
| Voltar à lista de pacotes | Ctrl+P | **Ctrl+P** ou **Esc** | v0.1.0 |
| Pacotes instalados / do repositório | Ctrl+I | **i** ou **Alt+I** | v0.1.0 |
| Aba Informação do pacote selecionado | Enter | **Enter** | v0.1.0 |
| Ordenar | clique no cabeçalho | clique no cabeçalho ou **F6** (escolher a coluna, como no htop) | v0.1.0 |
| Atualizar a lista | — | **F5** / **Ctrl+R** | v0.1.0 |
| Rolar a aba atual | — (mouse) | **Tab**, depois as setas, PgUp, PgDn | v0.1.0 |
| Sair | Ctrl+Q | **Ctrl+Q**, **F10** ou **q** (não durante uma operação) | v0.1.0 |
| Marcar para instalar / remover | + / − | **+** / **−**, ou **Insert** / **Delete** | v0.2.0 |
| Aba Ações | Alt+3 | **3** ou **Alt+3** | v0.2.0 |
| Aplicar | Ctrl+M | **Alt+M** (Ctrl+M é Enter) | v0.2.0 |
| Cancelar (desfazer as marcas) | Ctrl+E | **Ctrl+E** | v0.2.0 |
| Procurar atualizações (pkg update) | Ctrl+K | **Ctrl+K** | v0.2.0 |
| Instalar as atualizações (pkg upgrade) | Ctrl+U | **Ctrl+U** | v0.2.0 |
| Limpar o cache (pkg clean) | Ctrl+Shift+C | menu Arquivo (Ctrl+Shift+C chega como Ctrl+C, que encerra o FTXUI) | v0.2.0 |
| Bloquear / desbloquear | menu de contexto | menu Ações | v0.2.0 |
| Menus | barra de menus | **F9** ou **Alt+A** (Arquivo), **Alt+C** (Ações), **Alt+J** (Ajuda), como no bectl-tui | v0.2.0 |
| Buscar texto nas abas Arquivos, Saída, Notícias e Ajuda | Ctrl+F (e F3 / Shift+F3 na barra de busca) | **Ctrl+F**; **Enter**/**F3** e **Shift+F3** (ou ↓ ↑) vão ao próximo e ao anterior, **Esc** fecha | v0.3.0 |
| Preferências (tema, idioma, ícones) | — (o bectl-gui usa Ctrl+,) | **Alt+,** (Ctrl+, não chega pelo terminal), ou Arquivo → Preferências… | v0.3.0 |
| Próximo tema | — (o bectl-gui usa Ctrl+Shift+T) | **Alt+T** | v0.3.0 |
| Aba Notícias / buscar as últimas notícias | Alt+5 / Ctrl+G | **5** ou **Alt+5** / **Ctrl+G** | v0.4.0 |

Nos diálogos: <kbd>Tab</kbd> ou as setas mudam o campo, <kbd>Espaço</kbd> marca a
opção de criar um Boot Environment, <kbd>Enter</kbd> confirma e <kbd>Esc</kbd>
cancela; nas confirmações, <kbd>y</kbd>/<kbd>s</kbd> e <kbd>n</kbd> também valem.

Sem equivalente no terminal: F4 (abrir um terminal no diretório) e F6 (gerenciador de
arquivos) do OctoPkg; F6 ordena, como no bectl-tui. F10 e F11 (maximizar a lista ou as
abas) ficam para depois: F10 sai, como no htop e no mc.

O FTXUI coloca o terminal em modo cru, então <kbd>Ctrl+Q</kbd> chega ao programa e não
retoma o terminal (controle de fluxo XON/XOFF).
