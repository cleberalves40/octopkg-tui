// Tela: renderiza a interface em texto e confere o que aparece, e dirige a
// App com eventos de teclado, sem terminal.

#include "app.h"
#include "i18n.h"
#include "support.h"
#include "view.h"

#include <catch2/catch_test_macros.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <fstream>
#include <memory>
#include <set>

using namespace ftxui;
using octo::Status;

namespace {

octo::Package installed(const std::string &name, const std::string &version, std::int64_t size,
                        bool automatic = false)
{
    octo::Package p;
    p.name = name;
    p.version = version;
    p.installedVersion = version;
    p.size = size;
    p.installedOn = 1760000000;
    p.installed = true;
    p.automatic = automatic;
    p.comment = name + " comment";
    p.repository = "FreeBSD";
    p.status = Status::Installed;
    return p;
}

std::vector<octo::Package> sample()
{
    return {installed("curl", "8.9.1", 4 * 1024 * 1024), installed("ca_root_nss", "3.104", 1024 * 1024, true),
            installed("pkg", "2.3.1", 25 * 1024 * 1024)};
}

tui::Listing listing()
{
    tui::Listing l;
    l.installed = sample();
    l.outdated["curl"] = {"8.9.1", "8.10.0"};
    return l;
}

octo::Details curlDetails()
{
    octo::Details d;
    d.name = "curl";
    d.version = "8.9.1";
    d.origin = "ftp/curl";
    d.repository = "FreeBSD";
    d.maintainer = "sunpoet@FreeBSD.org";
    d.www = "https://curl.se/";
    d.licenses = {"MIT"};
    d.dependencies = {"ca_root_nss-3.104", "libnghttp2-1.63.0"};
    d.description = "Primeira linha.\nSegunda linha.";
    d.installed = true;
    d.files = {"/usr/local/bin/curl", "/usr/local/lib/libcurl.so.4"};
    return d;
}

std::string draw(const tui::ViewState &s, int width = 120, int height = 40)
{
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, tui::render(s, text("")));
    // Sem as sequências de cor e estilo, que o FTXUI intercala no texto.
    const std::string raw = screen.ToString();
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\x1b' && i + 1 < raw.size() && raw[i + 1] == '[') {
            i += 2;
            while (i < raw.size() && !(raw[i] >= '@' && raw[i] <= '~'))
                ++i;
            continue;
        }
        out += raw[i];
    }
    return out;
}

bool has(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

// CHECK de cada texto de uma lista, dizendo qual faltou.
#define CHECK_INFO(out, t)                                                                              \
    do {                                                                                                \
        INFO(t);                                                                                        \
        CHECK(has(out, t));                                                                             \
    } while (false)

struct AppFixture {
    TempDir dir;
    tui::App app;
    AppFixture()
        : app([this] {
              tui::Options o;
              o.pkgPath = dir.file("pkg-nao-usado");
              o.iconMode = "ascii";
              o.language = "en_US";
              o.settingsPath = dir.file("config");
              return o;
          }())
    {
        app.applyListing(listing());
    }
    std::string selected()
    {
        const tui::ViewState s = app.viewState();
        return s.hasCurrent ? s.current.name : std::string();
    }
};

} // namespace

TEST_CASE("tela mostra colunas, linhas e estado", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    const std::string out = draw(f.app.viewState());
    for (const char *t : {"OctoPkg", "[# Installed packages]", "Name ^", "Version", "Size", "Installed on", "curl",
                          "ca_root_nss", "4.00 MiB", "25.00 MiB", "Ready.", "3 package(s)", "1 outdated", "1 i Info",
                          "2 f Files", "3 a Actions", "4 > Output", "6 ? Help", "F1", "F5", "F9", "F10"})
        CHECK_INFO(out, t);
    // Ordem pelo nome; ca_root_nss é dependência ("d"), curl desatualizado ("U").
    CHECK(out.find("ca_root_nss") < out.find("curl"));
    CHECK(has(out, " d ca_root_nss"));
    CHECK(has(out, " U curl"));
    // Aba Informação do pacote selecionado, ainda sem os detalhes.
    CHECK(has(out, "Installed as a dependency"));
    CHECK(has(out, "Loading…"));
}

TEST_CASE("aba Informação com os detalhes", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);   // curl
    REQUIRE(f.selected() == "curl");
    f.app.applyDetails(f.app.currentDetailsKey(), curlDetails(), "");
    tui::ViewState s = f.app.viewState();
    std::string out = draw(s);
    for (const char *t : {"Outdated (version 8.10.0 is available)", "ftp/curl", "https://curl.se/", "MIT",
                          "sunpoet@FreeBSD.org", "ca_root_nss-3.104", "libnghttp2-1.63.0", "2 f Files (2)"})
        CHECK_INFO(out, t);
    // A descrição fica abaixo dos campos: aparece rolando a aba (Tab e setas).
    CHECK_FALSE(has(out, "Segunda linha."));
    s.contentFocused = true;
    s.contentScroll = 10;
    out = draw(s);
    for (const char *t : {"Primeira linha.", "Segunda linha.", "11/15"})
        CHECK_INFO(out, t);
    CHECK_FALSE(has(out, "ftp/curl"));
}

TEST_CASE("aba Arquivos e erro nos detalhes", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.applyDetails(f.app.currentDetailsKey(), curlDetails(), "");
    f.app.handleEvent(Event::Character('2'));
    std::string out = draw(f.app.viewState());
    CHECK(has(out, "/usr/local/lib/libcurl.so.4"));

    f.app.handleEvent(Event::ArrowDown);   // pkg, sem detalhes
    f.app.applyDetails(f.app.currentDetailsKey(), {}, "boom");
    out = draw(f.app.viewState());
    CHECK(has(out, "ERROR: boom"));
}

TEST_CASE("visão Repositório: não instalados e versão disponível", "[view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    std::vector<octo::Package> inst = sample();
    octo::applyUpgrades(inst, {{"curl", {"8.9.1", "8.10.0"}}});
    octo::Package vim;
    vim.name = "vim";
    vim.version = "9.1.0";
    vim.repository = "FreeBSD";
    octo::Package curl = inst[0];
    curl.version = "8.10.0";
    s.rows = octo::mergeRemote({curl, vim}, inst);
    s.remoteView = true;
    s.selected = 1;
    s.hasCurrent = true;
    s.current = s.rows[1];
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    CHECK(has(out, "[@ Repository]"));
    CHECK(has(out, " - vim"));
    CHECK(has(out, "8.10.0"));
    CHECK(has(out, "Not installed"));
}

TEST_CASE("ícones Nerd Font no modo padrão", "[view]")
{
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.iconMode = tui::IconMode::Nerd;
    const std::string out = draw(s);
    CHECK(has(out, ""));   // nf-fa-check_circle: instalado
    CHECK(has(out, ""));   // nf-fa-check_circle_o: dependência
    CHECK(has(out, ""));   // nf-fa-folder_open: aba Arquivos
}

TEST_CASE("tela em português", "[view]")
{
    i18n::setLocale("pt_BR");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.hasCurrent = true;
    s.current = s.rows[0];
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    for (const char *t : {"Nome", "Versão", "Tamanho", "Instalado em", "Informação", "Arquivos", "Saída", "Ajuda",
                          "Instalados", "Instalado", "Carregando…"})
        CHECK_INFO(out, t);
    i18n::setLocale("en_US");
}

TEST_CASE("F6 escolhe a coluna e Enter ordena, e a ordem é salva", "[app]")
{
    AppFixture f;
    REQUIRE(f.app.handleEvent(Event::F6));
    CHECK(f.app.viewState().sortChooserOpen);
    for (int i = 0; i < 2; ++i)   // Nome → Versão → Tamanho
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    tui::ViewState s = f.app.viewState();
    CHECK_FALSE(s.sortChooserOpen);
    CHECK(s.sort == model::SortSpec{model::Size, true});
    CHECK(s.rows.front().name == "ca_root_nss");
    CHECK(f.app.settings().intValue("sort.column", -1) == model::Size);

    // De novo na mesma coluna: inverte.
    f.app.handleEvent(Event::F6);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().sort == model::SortSpec{model::Size, false});
    CHECK(f.app.viewState().rows.front().name == "pkg");
    // "(ordem do pkg)": volta à ordem original.
    f.app.handleEvent(Event::F6);
    for (int i = 0; i < 8; ++i)
        f.app.handleEvent(Event::ArrowUp);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().rows.front().name == "curl");
}

TEST_CASE("seleção sobrevive à ordenação e ao filtro", "[app]")
{
    AppFixture f;
    CHECK(f.selected() == "ca_root_nss");
    f.app.handleEvent(Event::ArrowDown);
    CHECK(f.selected() == "curl");
    f.app.handleEvent(Event::F6);
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);   // por tamanho
    CHECK(f.selected() == "curl");
    f.app.filterText() = "pk";          // curl some: seleciona a primeira
    CHECK(f.selected() == "pkg");
    f.app.filterText().clear();
    CHECK(f.selected() == "pkg");
}

TEST_CASE("janela da tabela rola com a seleção", "[app]")
{
    AppFixture f;
    tui::Listing l;
    for (int i = 0; i < 100; ++i)
        l.installed.push_back(installed("p" + std::string(i < 10 ? "0" : "") + std::to_string(i), "1", i));
    f.app.applyListing(l);
    f.app.setTableHeight(10);
    tui::ViewState s = f.app.viewState();
    CHECK(s.rows.size() == 10);
    CHECK(s.rowCount == 100);
    f.app.handleEvent(Event::End);
    s = f.app.viewState();
    CHECK(s.firstRow == 90);
    CHECK(s.rows.back().name == "p99");
    CHECK(s.selected == 9);
    f.app.handleEvent(Event::PageUp);
    s = f.app.viewState();
    CHECK(s.current.name == "p90");
    CHECK(s.firstRow == 90);
    f.app.handleEvent(Event::ArrowUp);
    s = f.app.viewState();
    CHECK(s.firstRow == 89);
    CHECK(s.selected == 0);
}

TEST_CASE("atalhos de busca, abas e foco", "[app]")
{
    AppFixture f;
    CHECK(f.app.handleEvent(Event::Character('/')));
    CHECK(f.app.filterMode());
    CHECK_FALSE(f.app.handleEvent(Event::Character('x')));   // vai para o campo de filtro
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.filterMode());
    f.app.handleEvent(Event::Special("\x0c"));                 // Ctrl+L, como no OctoPkg
    CHECK(f.app.filterMode());
    f.app.handleEvent(Event::Special("\x10"));                 // Ctrl+P volta à lista
    CHECK_FALSE(f.app.filterMode());

    f.app.handleEvent(Event::Character('2'));
    CHECK(f.app.viewState().tab == tui::FilesTab);
    f.app.handleEvent(Event::Special("\x1b" "4"));             // Alt+4: Saída
    CHECK(f.app.viewState().tab == tui::OutputTab);
    f.app.handleEvent(Event::Special("\x1b" "6"));             // Alt+6: Ajuda
    CHECK(f.app.viewState().tab == tui::HelpTab);
    f.app.handleEvent(Event::Character('3'));                  // Ações
    CHECK(f.app.viewState().tab == tui::ActionsTab);
    f.app.handleEvent(Event::Special("\x1b" "5"));             // Notícias: ainda não existe
    CHECK(f.app.viewState().tab == tui::ActionsTab);
    CHECK(f.app.viewState().status == "This tab is not available yet.");
    f.app.handleEvent(Event::Return);                          // Enter: aba Informação
    CHECK(f.app.viewState().tab == tui::InfoTab);
    f.app.handleEvent(Event::F1);
    CHECK(f.app.viewState().tab == tui::HelpTab);

    // Tab: as setas rolam a aba em vez de mover a seleção.
    f.app.handleEvent(Event::Tab);
    CHECK(f.app.viewState().contentFocused);
    const std::string before = f.selected();
    (void)draw(f.app.viewState());
    f.app.handleEvent(Event::ArrowDown);
    CHECK(f.selected() == before);
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.viewState().contentFocused);
}

TEST_CASE("Saída lista os desatualizados; erro mantém a última lista", "[app]")
{
    AppFixture f;
    tui::ViewState s = f.app.viewState();
    bool found = false;
    for (const auto &l : s.log)
        found = found || l.find("curl  8.9.1 -> 8.10.0") != std::string::npos;
    CHECK(found);

    tui::Listing bad;
    bad.error = "boom";
    f.app.applyListing(bad);
    s = f.app.viewState();
    CHECK(s.error);
    CHECK(s.status == "ERROR: boom");
    CHECK(s.log.back().find("ERROR: boom") != std::string::npos);
    CHECK(s.totalPackages == 3);
}

// --- v0.2.0: marcas, transações, diálogos e menus ---

namespace {

/**
 * App com um pkg e um bectl falsos, sem elevador (como root). Cada prévia
 * (verbo -n) devolve o arquivo <verbo>-n.txt; cada operação sai com o código
 * de <verbo>.rc, se existir. Tudo vai para calls.log, na ordem.
 */
struct OpsFixture {
    TempDir dir;
    std::string log = dir.file("calls.log");
    std::unique_ptr<tui::App> app;

    explicit OpsFixture(bool bootEnvironments = false)
    {
        const std::string d = dir.path;
        std::ofstream(dir.file("installed.txt"))
            << "curl\t8.9.1\tftp/curl\t4572000\t1760000000\t0\t0\t1\tFreeBSD\tURLs\n"
               "git\t2.46.0\tdevel/git\t60000000\t1760000000\t0\t0\t0\tFreeBSD\tGit\n"
               "pkg\t2.3.1\tports-mgmt/pkg\t25000000\t1760000000\t0\t0\t0\tFreeBSD\tPackage manager\n"
               "zsh\t5.9\tshells/zsh\t9000000\t1760000000\t1\t0\t0\tFreeBSD\tZ shell\n";
        writeExecutable(dir.file("pkg"),
                        "#!/bin/sh\necho \"pkg $*\" >> '" + log + "'\n"
                        "D='" + d + "'\n"
                        "if [ \"$1\" = query ] && [ \"$2\" = -a ]; then cat \"$D/installed.txt\"; exit 0; fi\n"
                        // Como o pkg real: com -n, sai com 1 se houver o que fazer.
                        "if [ \"$2\" = -n ]; then\n"
                        "  [ -f \"$D/$1-n.txt\" ] || { echo \"pkg: sem prévia para $1\" >&2; exit 1; }\n"
                        "  cat \"$D/$1-n.txt\"; grep -q 'packages to be' \"$D/$1-n.txt\" && exit 1; exit 0\n"
                        "fi\n"
                        "echo \"executando $1\"\n"
                        "[ -f \"$D/$1.rc\" ] && { echo \"pkg: $1 falhou\" >&2; exit $(cat \"$D/$1.rc\"); }\n"
                        "exit 0\n");
        writeExecutable(dir.file("bectl"),
                        "#!/bin/sh\necho \"bectl $*\" >> '" + log + "'\n"
                        "[ \"$1\" = check ] && exit " + std::string(bootEnvironments ? "0" : "1") + "\n"
                        "[ -f '" + d + "/bectl.rc' ] && { echo 'bectl: sem espaço' >&2; exit 1; }\nexit 0\n");
        tui::Options o;
        o.pkgPath = dir.file("pkg");
        o.bectlPath = dir.file("bectl");
        o.elevator = "";
        o.iconMode = "ascii";
        o.language = "en_US";
        o.settingsPath = dir.file("config");
        app = std::make_unique<tui::App>(o);
        app->refresh();   // sem tela, roda na hora
    }

    void preview(const std::string &verb, const std::string &output) const
    {
        std::ofstream(dir.file(verb + "-n.txt")) << output;
    }
    void failNext(const std::string &verb) const { std::ofstream(dir.file(verb + ".rc")) << "3"; }
    // Chamadas que não são consultas (query, rquery, o upgrade -n da listagem e o bectl check).
    std::vector<std::string> operations() const
    {
        std::vector<std::string> out;
        for (const auto &l : readLines(log))
            if (l.rfind("pkg query", 0) != 0 && l.rfind("pkg rquery", 0) != 0 && l != "pkg upgrade -n -U"
                && l != "bectl check")
                out.push_back(l);
        return out;
    }
    void select(const std::string &name)
    {
        app->handleEvent(Event::Home);
        for (int i = 0; i < 10 && app->viewState().current.name != name; ++i)
            app->handleEvent(Event::ArrowDown);
        REQUIRE(app->viewState().current.name == name);
    }
    const tui::Dialog &dialog() const { return app->dialog(); }
    std::string screen() { return draw(app->viewState()); }
};

const char kInstallGit[] = "New packages to be INSTALLED:\n\tgit: 2.46.1 [FreeBSD]\n\n12 MiB to be downloaded.\n";
const char kRemoveCurl[] = "Installed packages to be REMOVED:\n\tcurl: 8.9.1\n";
const char kRemoveCurlAndGit[] = "Installed packages to be REMOVED:\n\tcurl: 8.9.1\n\tgit: 2.46.0\n";

} // namespace

TEST_CASE("+ e - marcam, como no OctoPkg, e Ctrl+E desfaz", "[app][actions]")
{
    OpsFixture f;
    f.select("curl");
    f.app->handleEvent(Event::Character('+'));
    CHECK(f.app->viewState().toInstall == std::set<std::string>{"curl"});
    f.app->handleEvent(Event::Character('-'));   // remover tira a marca de instalar
    tui::ViewState s = f.app->viewState();
    CHECK(s.toInstall.empty());
    CHECK(s.toRemove == std::set<std::string>{"curl"});
    f.app->handleEvent(Event::Character('-'));   // de novo: desmarca
    CHECK(f.app->viewState().toRemove.empty());

    f.select("git");
    f.app->handleEvent(Event::Insert);
    f.select("curl");
    f.app->handleEvent(Event::Delete);
    f.select("pkg");
    f.app->handleEvent(Event::Character('-'));   // protegido
    CHECK(f.app->viewState().status == "pkg cannot be removed.");
    f.select("zsh");
    f.app->handleEvent(Event::Character('+'));   // bloqueado
    CHECK(f.app->viewState().status == "zsh is locked.");

    s = f.app->viewState();
    CHECK(s.toInstall == std::set<std::string>{"git"});
    CHECK(s.toRemove == std::set<std::string>{"curl"});
    f.app->handleEvent(Event::Character('3'));
    const std::string out = f.screen();
    for (const char *t : {"3 a Actions (2)", "To be installed (1)", "To be removed (1)", " + git", " x curl", "+1",
                          "-1"})
        CHECK_INFO(out, t);
    CHECK(f.operations().empty());   // marcar não executa nada

    f.app->handleEvent(Event::Special("\x05"));   // Ctrl+E
    CHECK(f.app->viewState().toInstall.empty());
    CHECK(f.app->viewState().toRemove.empty());
}

TEST_CASE("Alt+M mostra a prévia, e OK instala e remove", "[app][actions]")
{
    OpsFixture f;
    f.preview("install", kInstallGit);
    f.preview("remove", kRemoveCurl);
    f.select("git");
    f.app->handleEvent(Event::Character('+'));
    f.select("curl");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));   // Alt+M

    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().title == "Confirmation");
    CHECK_FALSE(f.dialog().danger);
    CHECK(f.dialog().fields.empty());   // sem Boot Environments, sem a opção
    std::string out = f.screen();
    for (const char *t : {"The following package needs to be retrieved", "git: 2.46.1 [FreeBSD]",
                          "Total download size: 12 MiB", "The following package will be removed", "curl: 8.9.1",
                          "Confirm?", "[ OK ]", "[ Cancel ]"})
        CHECK_INFO(out, t);
    CHECK(f.operations() == std::vector<std::string>{"pkg remove -n -R curl", "pkg install -n -f -U git"});

    f.app->handleEvent(Event::Return);   // OK
    // Como no OctoPkg: remover e depois instalar.
    CHECK(f.operations() == std::vector<std::string>{"pkg remove -n -R curl", "pkg install -n -f -U git",
                                                    "pkg remove -R -y curl", "pkg install -f -y git"});
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "Command finished OK!");
    CHECK(f.dialog().message.find("Installed: git") != std::string::npos);
    CHECK(f.dialog().message.find("Removed: curl") != std::string::npos);
    tui::ViewState s = f.app->viewState();
    CHECK(s.toInstall.empty());
    CHECK(s.toRemove.empty());
    CHECK(s.tab == tui::OutputTab);
    bool output = false;
    for (const auto &l : s.log)
        output = output || l == "  executando install";
    CHECK(output);
    f.app->handleEvent(Event::Return);   // fecha a mensagem
    CHECK(f.dialog().kind == tui::Dialog::None);
}

TEST_CASE("remoção que leva dependentes: Aviso, foco em Cancelar", "[app][actions]")
{
    OpsFixture f;
    f.preview("remove", kRemoveCurlAndGit);
    f.select("curl");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().title == "Warning");
    CHECK(f.dialog().danger);
    CHECK(f.screen().find("The following 2 packages will be removed") != std::string::npos);
    f.app->handleEvent(Event::Return);   // Cancelar, o foco padrão
    CHECK(f.dialog().kind == tui::Dialog::None);
    CHECK(f.operations() == std::vector<std::string>{"pkg remove -n -R curl"});
    CHECK(f.app->viewState().toRemove == std::set<std::string>{"curl"});   // as marcas ficam
}

TEST_CASE("Boot Environment antes da transação, como no OctoPkg", "[app][actions]")
{
    OpsFixture f(true);
    f.preview("install", kInstallGit);
    f.select("git");
    f.app->handleEvent(Event::Character('+'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    REQUIRE(f.dialog().fields.size() == 1);
    CHECK(f.dialog().fields[0].checked);   // instalar: marcado
    CHECK(f.screen().find("[x] Create a boot environment before the transaction") != std::string::npos);

    SECTION("criado antes do pkg")
    {
        f.app->handleEvent(Event::Return);
        const auto ops = f.operations();
        REQUIRE(ops.size() == 3);
        CHECK(ops[0] == "pkg install -n -f -U git");
        CHECK(ops[1].rfind("bectl create ", 0) == 0);
        CHECK(ops[2] == "pkg install -f -y git");
        CHECK(f.dialog().message.find("Boot environment: ") != std::string::npos);
    }
    SECTION("desmarcado com Espaço")
    {
        f.app->handleEvent(Event::TabReverse);   // do OK para a opção
        f.app->handleEvent(Event::Character(' '));
        CHECK_FALSE(f.dialog().fields[0].checked);
        f.app->handleEvent(Event::Return);
        CHECK(f.operations().back() == "pkg install -f -y git");
        for (const auto &op : f.operations())
            CHECK(op.rfind("bectl create", 0) != 0);
    }
    SECTION("se o Boot Environment falha, o pkg não roda")
    {
        std::ofstream(f.dir.file("bectl.rc")) << "1";
        f.app->handleEvent(Event::Return);
        CHECK(f.operations().back().rfind("bectl create ", 0) == 0);
        REQUIRE(f.dialog().kind == tui::Dialog::Message);
        CHECK(f.dialog().title == "Error");
        CHECK(f.dialog().message == "bectl: sem espaço");
        CHECK(f.app->viewState().toInstall == std::set<std::string>{"git"});
    }
}

TEST_CASE("remoção não marca o Boot Environment, e falha do pkg vira erro", "[app][actions]")
{
    OpsFixture f(true);
    f.preview("remove", kRemoveCurl);
    f.failNext("remove");
    f.select("curl");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().fields.size() == 1);
    CHECK_FALSE(f.dialog().fields[0].checked);   // o OctoPkg desmarca na remoção
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "Error");
    CHECK(f.dialog().message == "pkg: remove falhou");
    const tui::ViewState s = f.app->viewState();
    CHECK(s.error);
    CHECK(s.toRemove == std::set<std::string>{"curl"});
}

TEST_CASE("sem marcas, Alt+M explica; prévia que falha vira erro", "[app][actions]")
{
    OpsFixture f;
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "No packages marked. Use + to install and - to remove.");
    f.app->handleEvent(Event::Escape);

    f.select("git");
    f.app->handleEvent(Event::Character('+'));   // install-n.txt não existe: o pkg falha
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "Error");
    CHECK(f.operations() == std::vector<std::string>{"pkg install -n -f -U git"});
}

TEST_CASE("Ctrl+U atualiza o sistema; Ctrl+K sincroniza o banco", "[app][actions]")
{
    OpsFixture f;
    SECTION("sem atualizações")
    {
        f.preview("upgrade", "Your packages are up to date.\n");
        f.app->handleEvent(Event::Special("\x15"));
        REQUIRE(f.dialog().kind == tui::Dialog::Message);
        CHECK(f.dialog().message == "There are no new updates available!");
    }
    SECTION("com atualizações")
    {
        f.preview("upgrade", "Installed packages to be UPGRADED:\n\tcurl: 8.9.1 -> 8.10.0\n\n3 MiB to be downloaded.\n");
        f.app->handleEvent(Event::Special("\x15"));
        REQUIRE(f.dialog().kind == tui::Dialog::Form);
        CHECK(f.dialog().title == "System upgrade");
        CHECK(f.screen().find("curl: 8.9.1 -> 8.10.0") != std::string::npos);
        f.app->handleEvent(Event::Return);
        CHECK(f.operations().back() == "pkg upgrade -y");
        CHECK(f.dialog().title == "Command finished OK!");
    }
    SECTION("Ctrl+K")
    {
        f.app->handleEvent(Event::Special("\x0b"));
        CHECK(f.operations() == std::vector<std::string>{"pkg update -f"});
        CHECK(f.app->viewState().tab == tui::OutputTab);
    }
}

TEST_CASE("menus: limpar o cache, bloquear e Sobre", "[app][menu]")
{
    OpsFixture f;
    f.app->handleEvent(Event::F9);
    REQUIRE(f.dialog().kind == tui::Dialog::Menu);
    std::string out = f.screen();
    for (const char *t : {"File", "Actions", "Help", "Check updates", "Ctrl+K", "Clean the local cache"})
        CHECK_INFO(out, t);
    for (int i = 0; i < 4; ++i)   // Atualizar, Instalados/Repositório, (sep), Procurar, Instalar, Limpar
        f.app->handleEvent(Event::ArrowDown);
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Confirm);
    CHECK(f.dialog().message == "Do you really want to clean the local package cache?");
    f.app->handleEvent(Event::Character('y'));
    CHECK(f.operations() == std::vector<std::string>{"pkg clean -a -y"});

    f.select("curl");
    f.app->handleEvent(Event::Special("\x1b" "c"));   // Alt+C: Ações
    REQUIRE(f.dialog().menu == 1);
    for (int i = 0; i < 4; ++i)   // Marcar instalar, remover, (sep), Aplicar, Cancelar, (sep), Bloquear
        f.app->handleEvent(Event::ArrowDown);
    f.app->handleEvent(Event::Return);
    CHECK(f.operations().back() == "pkg lock -y curl");

    f.app->handleEvent(Event::Special("\x1b" "j"));   // Alt+J: Ajuda
    f.app->handleEvent(Event::ArrowDown);
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "About");
    CHECK(f.dialog().message.find(std::string("octopkg-tui ") + OCTOPKG_TUI_VERSION) != std::string::npos);
}

TEST_CASE("diálogo em português", "[view][menu]")
{
    i18n::setLocale("pt_BR");
    tui::ViewState s;
    s.iconMode = tui::IconMode::Ascii;
    s.dialog.kind = tui::Dialog::Form;
    s.dialog.title = i18n::tr("Confirmation");
    s.dialog.message = i18n::tr("Confirm?");
    tui::DialogField be;
    be.kind = tui::DialogField::Check;
    be.label = i18n::tr("Create a boot environment before the transaction");
    be.checked = true;
    s.dialog.fields.push_back(be);
    const std::string out = draw(s);
    for (const char *t : {"Confirmação", "Confirma?", "[x] Criar um boot environment antes da transação", "[ OK ]",
                          "[ Cancelar ]", "F9Menu", "+/-Marcar"})
        CHECK_INFO(out, t);
    i18n::setLocale("en_US");
}
