#ifndef OCTOPKG_TUI_APP_H
#define OCTOPKG_TUI_APP_H

#include "icons.h"
#include "listmodel.h"
#include "pkg.h"
#include "settings.h"
#include "view.h"

#include <atomic>
#include <functional>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
// FTXUI 7 define ScreenInteractive como apelido de ftxui::App; o cabeçalho
// real evita conflito com uma declaração antecipada.
#include <ftxui/component/screen_interactive.hpp>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace tui {

struct Options {
    std::string pkgPath;         // vazio: pkg(8) do sistema
    std::string language;        // vazio: preferência salva ou ambiente
    std::string iconMode;        // vazio: preferência salva ou "nerd"
    std::string settingsPath;    // vazio: ~/.config/octopkg-tui/config
    bool remote = false;         // começa na visão Repositório
    std::string elevator = "-";  // "-": preferência salva, ou doas (vazio se root)
    std::string bectlPath;       // vazio: /sbin/bectl
};

/** Resultado de uma listagem, aplicado na thread da interface. */
struct Listing {
    std::vector<octo::Package> installed;
    std::map<std::string, octo::Upgrade> outdated;
    bool hasRemote = false;
    std::vector<octo::Package> remote;
    std::string error;
    std::string warning;         // falha só na lista de desatualizados
};

class App
{
public:
    explicit App(const Options &options);
    ~App();

    /** Laço da interface em tela cheia; devolve o código de saída. */
    int run();

    // Para os testes: o estado que a tela mostraria e o tratamento de eventos.
    // Sem tela (run() não chamado), prévias e operações rodam na hora.
    ViewState viewState();
    bool handleEvent(const ftxui::Event &event);
    void applyListing(const Listing &listing);
    void applyDetails(const std::string &key, const octo::Details &details, const std::string &error);
    /** Chave do cache de detalhes do pacote selecionado ("I:nome" ou "R:nome"). */
    std::string currentDetailsKey();
    bool filterMode() const { return m_filterMode; }
    std::string &filterText() { return m_filter; }
    const octo::Settings &settings() const { return m_settings; }
    void setTableHeight(int rows) { m_tableHeight = rows; }
    const Dialog &dialog() const { return m_dialog; }
    void refresh();

private:
    using Op = std::function<void()>;
    using FormOp = std::function<void(const std::vector<DialogField> &)>;
    /** Uma operação elevada: recebe para onde mandar a saída. */
    using Work = std::function<void(const octo::OutputFn &)>;
    /** Trabalho sem privilégios fora da interface; devolve o que aplicar nela. */
    using Task = std::function<Op()>;

    // Ações do OctoPkg.
    void markInstall();
    void markRemove();
    void clearMarks();
    void actApply();
    void actUpgrade();
    void actUpdate();
    void actClean();
    void actLock(bool lock);
    void actAbout();
    void openMenu(int menu);
    void confirmTransaction(const octo::TransactionPreview &removal, const octo::TransactionPreview &install,
                            const std::vector<std::string> &toRemove, const std::vector<std::string> &toInstall);

    /** Prévia (dry-run) em segundo plano, com o status "Collecting transaction data…". */
    void runTask(const std::string &status, const Task &task);
    /**
     * Executa work (comandos elevados). Sem senha a pedir, roda em segundo
     * plano, com a saída na aba Saída; se o doas/sudo precisar da senha,
     * devolve o terminal enquanto isso. Depois, onSuccess e uma nova listagem.
     */
    void runOperation(const std::string &status, const std::string &command, const Work &work,
                      const Op &onSuccess);
    void finishOperation(const std::string &error, const Op &onSuccess);
    bool passwordNeeded();
    void appendOutput(const std::string &chunk);
    bool bootEnvironments();

    void showMessage(const std::string &title, const std::string &message);
    void showConfirm(const std::string &title, const std::string &message, bool danger, const Op &onYes);
    void showForm(const std::string &title, const std::string &message, std::vector<DialogField> fields,
                  bool danger, FormOp onAccept);
    bool handleDialogEvent(const ftxui::Event &event);
    bool refuseWhileBusy();
    const octo::Package *current();

    void toggleView();
    void requestDetails();
    void log(const std::string &message);
    void setSort(model::SortSpec sort);
    void move(int delta);
    void scrollContent(int delta);
    void setTab(int tab);
    bool handleMouse(const ftxui::Event &event);
    void quit();
    const std::vector<int> &rows();
    void rebuild();
    void layout();

    octo::Settings m_settings;
    octo::Manager m_mgr;
    IconMode m_iconMode;

    std::vector<octo::Package> m_installed;    // com os desatualizados marcados
    std::vector<octo::Package> m_remote;
    bool m_hasRemote = false;
    bool m_remoteView = false;
    std::vector<octo::Package> m_all;          // a visão atual
    int m_outdated = 0;

    // Linhas filtradas e ordenadas, refeitas só quando algo muda.
    std::vector<int> m_rows;
    std::string m_rowsFilter;
    model::SortSpec m_rowsSort{-2, true};
    bool m_rowsDirty = true;

    std::string m_filter;
    model::SortSpec m_sort;
    std::string m_selectedName;
    std::string m_shownName;                   // pacote da última tela, para zerar a rolagem
    int m_offset = 0;
    int m_tableHeight = 1000;
    int m_contentHeight = 12;
    int m_tab = InfoTab;
    bool m_contentFocused = false;
    int m_contentScroll = 0;
    bool m_filterMode = false;
    bool m_started = false;                    // primeira listagem já disparada
    bool m_sortChooser = false;
    int m_sortChooserIndex = 0;
    std::set<std::string> m_toInstall;
    std::set<std::string> m_toRemove;
    std::string m_partial;                     // linha da Saída ainda sem "\n"
    int m_beAvailable = -1;                    // bectl check: -1 ainda não testado
    std::string m_operationError;              // fica no status até a próxima ação

    Dialog m_dialog;
    Op m_onAccept;                             // Confirm
    FormOp m_onForm;                           // Form
    std::vector<std::vector<Op>> m_menuActions;

    std::map<std::string, octo::Details> m_details;   // cache, por currentDetailsKey()
    std::map<std::string, std::string> m_detailErrors;
    std::atomic<bool> m_detailBusy{false};
    std::thread m_detailWorker;

    std::vector<std::string> m_log;
    std::string m_status;
    bool m_error = false;
    std::atomic<bool> m_busy{false};
    bool m_inOperation = false;                // operação elevada: não dá para sair
    std::thread m_worker;

    ftxui::ScreenInteractive *m_screen = nullptr;
    HitBoxes m_boxes;
    ftxui::Component m_input;
};

} // namespace tui

#endif
