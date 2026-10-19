#ifndef OCTOPKG_TUI_VIEW_H
#define OCTOPKG_TUI_VIEW_H

#include "icons.h"
#include "listmodel.h"
#include "pkg.h"

#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <string>
#include <vector>

namespace tui {

/**
 * Abas do OctoPkg presentes nesta versão. As teclas são as do OctoPkg
 * (Alt+1 Informação, Alt+2 Arquivos, Alt+4 Saída, Alt+6 Ajuda); Ações (3) e
 * Notícias (5) ainda não existem.
 */
enum Tab { InfoTab = 0, FilesTab, OutputTab, HelpTab, TabCount };

/** Tecla da aba ('1', '2', '4' ou '6'). */
char tabKey(int tab);

/** Tudo o que a tela mostra; montado pela App e desenhado por render(). */
struct ViewState {
    std::vector<octo::Package> rows;   // só as linhas da janela da tabela
    int firstRow = 0;                  // posição de rows[0] na lista filtrada
    int rowCount = 0;                  // linhas da lista filtrada
    int selected = -1;                 // índice em rows; -1 fora da janela
    bool hasCurrent = false;
    octo::Package current;             // pacote selecionado
    bool detailsReady = false;         // details corresponde a current
    octo::Details details;
    std::string detailsError;
    model::SortSpec sort;
    bool remoteView = false;
    int tab = InfoTab;
    bool contentFocused = false;       // Tab: as setas rolam a aba
    int contentScroll = 0;
    int contentHeight = 12;
    std::vector<std::string> log;      // aba Saída, com horário
    std::string status;
    bool busy = false;
    bool error = false;
    int totalPackages = 0;
    int outdatedCount = 0;
    IconMode iconMode = IconMode::Nerd;
    bool sortChooserOpen = false;
    int sortChooserIndex = 0;          // 0 = ordem do pkg, 1… = colunas
};

/** Posições na tela, para o mouse, e o tamanho do conteúdo da aba. */
struct HitBoxes {
    std::array<ftxui::Box, model::ColumnCount> header{};
    std::vector<ftxui::Box> rows;      // de ViewState::rows
    std::array<ftxui::Box, TabCount> tabs{};
    int contentLines = 0;              // linhas da aba atual, para limitar a rolagem
};

/** Rótulo da coluna. */
std::string columnTitle(int column);
/** Estado do pacote por extenso ("Outdated (version 2.0 is available)"). */
std::string statusText(const octo::Package &p);

/** Tela completa. filter é o campo de filtro (componente Input ou texto). */
ftxui::Element render(const ViewState &state, ftxui::Element filter, HitBoxes *boxes = nullptr);

} // namespace tui

#endif
