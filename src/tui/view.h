#ifndef OCTOPKG_TUI_VIEW_H
#define OCTOPKG_TUI_VIEW_H

#include "icons.h"
#include "listmodel.h"
#include "pkg.h"

#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <set>
#include <string>
#include <vector>

namespace tui {

/**
 * Abas do OctoPkg presentes nesta versão, com as teclas dele: Alt+1
 * Informação, Alt+2 Arquivos, Alt+3 Ações, Alt+4 Saída e Alt+6 Ajuda. Notícias
 * (5) ainda não existe.
 */
enum Tab { InfoTab = 0, FilesTab, ActionsTab, OutputTab, HelpTab, TabCount };

/** Tecla da aba ('1', '2', '3', '4' ou '6'). */
char tabKey(int tab);

struct DialogField {
    enum Kind { Text, Check, Choice } kind = Text;
    std::string label;
    std::string text;                    // Text
    bool checked = false;                // Check
    std::vector<std::string> choices;    // Choice
    int choice = 0;
    std::string hint;                    // linha de ajuda abaixo do campo
};

/**
 * Diálogo modal, como no bectl-tui: formulário (a confirmação de uma
 * transação, com a opção de criar um Boot Environment), confirmação
 * (Sim/Não), mensagem, ou os menus (F9).
 */
struct Dialog {
    enum Kind { None, Form, Confirm, Message, Menu } kind = None;
    std::string title;
    std::string message;                 // Form, Confirm e Message
    std::vector<DialogField> fields;
    // Form: índice do campo, ou fields.size() = OK e fields.size() + 1 =
    // Cancelar. Confirm: 0 = Sim, 1 = Não.
    int focus = 0;
    bool danger = false;                 // destrutivo: em vermelho, foco em Não/Cancelar
    // Menu: títulos, itens (rótulo, atalho) e posição.
    std::vector<std::string> menus;
    std::vector<std::vector<std::pair<std::string, std::string>>> items;
    int menu = 0;
    int item = 0;
};

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
    std::set<std::string> toInstall;   // marcados com +, como no OctoPkg
    std::set<std::string> toRemove;    // marcados com -
    Dialog dialog;
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
