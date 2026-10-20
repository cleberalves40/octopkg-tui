#ifndef OCTOPKG_TUI_VIEW_H
#define OCTOPKG_TUI_VIEW_H

#include "icons.h"
#include "listmodel.h"
#include "news.h"
#include "pkg.h"
#include "theme.h"

#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <set>
#include <string>
#include <vector>

namespace tui {

/**
 * As abas do OctoPkg, com as teclas dele: Alt+1 Informação, Alt+2 Arquivos,
 * Alt+3 Ações, Alt+4 Saída, Alt+5 Notícias e Alt+6 Ajuda.
 */
enum Tab { InfoTab = 0, FilesTab, ActionsTab, OutputTab, NewsTab, HelpTab, TabCount };

/**
 * Disposição da tela, como o F10/F11 do OctoPkg (maximizar a lista ou as
 * abas); aqui, F11 ou Alt+Z passa de uma à outra.
 */
enum class Layout { Normal = 0, ListOnly, TabsOnly };

/** Tecla da aba ('1' a '6'). */
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
    model::SearchMode searchMode = model::SearchMode::Name;
    std::string category;              // filtro por categoria; vazio: todas
    bool categoryChooserOpen = false;  // F8
    std::vector<std::pair<std::string, int>> categories;   // as da visão, com a contagem
    int categoryIndex = 0;             // 0 = todas, 1… = categories
    bool remoteView = false;
    int tab = InfoTab;
    bool contentFocused = false;       // Tab: as setas rolam a aba
    int contentScroll = 0;
    int contentHeight = 12;
    int contentWidth = 96;
    Layout layout = Layout::Normal;             // largura útil das abas (as notícias quebram nela)
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
    std::string themeId = "system";
    // Busca na aba (Ctrl+F): o texto e a linha do resultado atual.
    bool findOpen = false;
    std::string findText;
    int findMatch = -1;                // linha do resultado atual; -1 sem resultado
    int findMatchIndex = 0;            // posição dele entre os resultados
    int findCount = 0;
    // Aba Notícias.
    std::string newsSource;            // "FreeBSD"; vazio se a variante não tem feed
    std::vector<octo::NewsItem> news;
    bool newsLoading = false;
    bool newsFromCache = false;
    bool newsUnseen = false;           // há notícias novas que ainda não foram vistas
    std::string newsError;
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

/**
 * Texto das linhas das abas em que Ctrl+F busca (Arquivos, Saída, Notícias e
 * Ajuda), uma por linha desenhada; vazio nas outras.
 */
std::vector<std::string> tabText(const ViewState &state);
/** Índices das linhas de lines que contêm needle, sem diferenciar maiúsculas. */
std::vector<int> findLines(const std::vector<std::string> &lines, const std::string &needle);

/** Tela completa. filter é o campo de filtro (componente Input ou texto). */
ftxui::Element render(const ViewState &state, ftxui::Element filter, HitBoxes *boxes = nullptr);

} // namespace tui

#endif
