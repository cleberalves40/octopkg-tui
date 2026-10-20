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
#include <ftxui/screen/terminal.hpp>
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
              // Sem rede nos testes: a variante é fixa e o fetch é o falso de fetch.sh.
              o.newsFlavour = "freebsd";
              o.fetchPath = dir.file("fetch.sh");
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
                          "2 f Files", "3 a Actions", "4 > Output", "5 n News", "6 ? Help", "F1", "F5", "F9", "F10"})
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
    f.app.handleEvent(Event::Special("\x1b" "5"));             // Notícias
    CHECK(f.app.viewState().tab == tui::NewsTab);
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
        o.newsFlavour = "freebsd";
        o.fetchPath = dir.file("fetch-nao-existe");
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
    REQUIRE(f.dialog().menu == 2);   // Arquivo, Busca, Ações, Ajuda
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

// --- v0.3.0: temas, preferências e busca nas abas ---

namespace {

// A tela com as sequências de cor, para conferir as cores do tema.
std::string drawRaw(const tui::ViewState &s, int width = 120, int height = 40)
{
    Terminal::SetColorSupport(Terminal::Color::TrueColor);
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, tui::render(s, text("")));
    return screen.ToString();
}

} // namespace

TEST_CASE("os temas do bectl-gui, em ciclo", "[theme]")
{
    const auto &list = tui::themes();
    REQUIRE(list.size() == 15);
    CHECK(std::string(list.front().id) == "system");
    CHECK(list.front().system);
    std::set<std::string> ids;
    for (const auto &t : list)
        ids.insert(t.id);
    CHECK(ids.size() == list.size());
    for (const char *id : {"octopkg", "frappe", "latte", "mocha", "macchiato", "solarized_dark", "solarized_light",
                           "selenized_dark", "selenized_light", "dracula", "tokyo_night", "nord", "gruvbox",
                           "one_dark"})
        CHECK(ids.count(id));
    CHECK(std::string(tui::theme("nao-existe").id) == "system");
    CHECK(std::string(tui::nextTheme("system").id) == "octopkg");
    CHECK(std::string(tui::nextTheme("one_dark").id) == "system");
}

TEST_CASE("tema pinta o fundo, o texto e a seleção", "[theme][view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.iconMode = tui::IconMode::Ascii;
    s.themeId = "nord";
    const std::string raw = drawRaw(s);
    CHECK(raw.find("48;2;46;52;64") != std::string::npos);    // fundo #2e3440
    CHECK(raw.find("38;2;216;222;233") != std::string::npos); // texto #d8dee9
    CHECK(raw.find("48;2;129;161;193") != std::string::npos); // seleção #81a1c1
    CHECK(draw(s).find("Theme: Nord") != std::string::npos);

    s.themeId = "system";
    const std::string plain = drawRaw(s);
    CHECK(plain.find("48;2;") == std::string::npos);          // sem cores próprias
    CHECK(draw(s).find("Theme: System") != std::string::npos);
}

TEST_CASE("Alt+T troca o tema e o salva", "[theme][app]")
{
    AppFixture f;
    CHECK(f.app.viewState().themeId == "system");
    f.app.handleEvent(Event::Special("\x1b" "t"));
    CHECK(f.app.viewState().themeId == "octopkg");
    CHECK(f.app.settings().value("theme") == "octopkg");
    CHECK(f.app.viewState().status == "Theme: OctoPKG (claro)");
    for (int i = 0; i < 14; ++i)
        f.app.handleEvent(Event::Special("\x1b" "t"));
    CHECK(f.app.viewState().themeId == "system");
}

TEST_CASE("Preferências: tema, idioma e ícones, aplicados e salvos", "[app][preferences]")
{
    AppFixture f;
    f.app.handleEvent(Event::Special("\x1b" ","));   // Alt+,
    REQUIRE(f.app.viewState().dialog.kind == tui::Dialog::Form);
    const tui::Dialog d = f.app.viewState().dialog;
    CHECK(d.title == "Preferences");
    REQUIRE(d.fields.size() == 3);
    CHECK(d.fields[0].choices.size() == 15);
    CHECK(d.focus == 0);
    std::string out = draw(f.app.viewState());
    for (const char *t : {"Theme:", "System (terminal colors)", "Language:", "Icons:", "ASCII"})
        CHECK_INFO(out, t);

    for (int i = 0; i < 12; ++i)   // Tema: system → … → nord
        f.app.handleEvent(Event::ArrowRight);
    f.app.handleEvent(Event::Tab);                   // Idioma
    f.app.handleEvent(Event::ArrowRight);            // en_US
    f.app.handleEvent(Event::ArrowRight);            // pt_BR
    f.app.handleEvent(Event::Tab);                   // Ícones
    f.app.handleEvent(Event::ArrowLeft);             // ASCII → Unicode
    f.app.handleEvent(Event::Return);

    const tui::ViewState s = f.app.viewState();
    CHECK(s.dialog.kind == tui::Dialog::None);
    CHECK(s.themeId == "nord");
    CHECK(s.iconMode == tui::IconMode::Unicode);
    CHECK(i18n::locale() == "pt_BR");
    CHECK(s.status == "Preferências salvas.");
    CHECK(f.app.settings().value("theme") == "nord");
    CHECK(f.app.settings().value("language") == "pt_BR");
    CHECK(f.app.settings().value("icons") == "unicode");
    // Relido do arquivo, numa nova App.
    tui::Options o;
    o.pkgPath = f.dir.file("pkg-nao-usado");
    o.settingsPath = f.dir.file("config");
    tui::App again(o);
    CHECK(again.viewState().themeId == "nord");
    CHECK(again.viewState().iconMode == tui::IconMode::Unicode);
    i18n::setLocale("en_US");
}

TEST_CASE("Ctrl+F busca nas abas Arquivos, Saída e Ajuda", "[app][find]")
{
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);   // curl
    octo::Details d = curlDetails();
    d.files = {"/usr/local/bin/curl", "/usr/local/include/curl/curl.h", "/usr/local/lib/libcurl.so.4",
               "/usr/local/share/man/man1/curl.1.gz"};
    f.app.applyDetails(f.app.currentDetailsKey(), d, "");

    // Na aba Informação, não há onde buscar.
    f.app.handleEvent(Event::Special("\x06"));
    CHECK_FALSE(f.app.viewState().findOpen);
    CHECK(f.app.viewState().status == "Find works in the Files, Output, News and Help tabs.");

    f.app.handleEvent(Event::Character('2'));
    f.app.handleEvent(Event::Special("\x06"));
    REQUIRE(f.app.viewState().findOpen);
    for (char c : std::string("LIB"))   // sem diferenciar maiúsculas
        f.app.handleEvent(Event::Character(c));
    tui::ViewState s = f.app.viewState();
    CHECK(s.findCount == 1);
    CHECK(s.findMatch == 2);
    CHECK(draw(s).find("Find: LIB") != std::string::npos);
    CHECK(draw(s).find("1/1") != std::string::npos);

    f.app.handleEvent(Event::Backspace);
    f.app.handleEvent(Event::Backspace);
    f.app.handleEvent(Event::Backspace);
    for (char c : std::string("curl"))
        f.app.handleEvent(Event::Character(c));
    s = f.app.viewState();
    CHECK(s.findCount == 4);
    CHECK(s.findMatch == 0);
    f.app.handleEvent(Event::F3);
    CHECK(f.app.viewState().findMatch == 1);
    f.app.handleEvent(Event::Special("\x1b[1;2R"));   // Shift+F3
    f.app.handleEvent(Event::Special("\x1b[1;2R"));
    CHECK(f.app.viewState().findMatch == 3);          // volta ao último, em ciclo
    for (char c : std::string("zzz"))
        f.app.handleEvent(Event::Character(c));
    CHECK(f.app.viewState().findCount == 0);
    CHECK(draw(f.app.viewState()).find("Not found") != std::string::npos);
    // q é texto da busca, não sair.
    CHECK(f.app.handleEvent(Event::Character('q')));
    CHECK(f.app.viewState().findText == "curlzzzq");
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.viewState().findOpen);
    CHECK(f.app.viewState().findText.empty());

    // Na Ajuda, encontra o próprio atalho.
    f.app.handleEvent(Event::F1);
    f.app.handleEvent(Event::Special("\x06"));
    for (char c : std::string("Ctrl+F"))
        f.app.handleEvent(Event::Character(c));
    CHECK(f.app.viewState().findCount >= 1);
}

// --- v0.4.0: aba Notícias ---

TEST_CASE("aba Notícias: cache na hora, Ctrl+G baixa de novo, e a busca funciona", "[app][news]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    std::ofstream(f.dir.file("feed.xml"))
        << "<rss><channel><item><title>FreeBSD 15.1-RELEASE Available</title>"
           "<link>https://www.freebsd.org/releases/15.1R/announce/</link>"
           "<description>The FreeBSD Release Engineering Team is pleased to announce the availability of "
           "FreeBSD 15.1-RELEASE, the second release of the stable/15 branch, with many improvements "
           "and new features for everyone.</description><pubDate>Tue, 02 Jun 2026 00:00:00 +0000</pubDate>"
           "</item><item><title>Ports status</title><description>ok</description></item>"
           "<item><description>New committer: Boris Lytochkin (src)</description>"
           "<pubDate>02 Oct 26 00:00 UTC</pubDate></item></channel></rss>";
    writeExecutable(f.dir.file("fetch.sh"), "#!/bin/sh\necho \"$*\" >> '" + f.dir.file("fetch.log") + "'\n"
                                            "while [ $# -gt 1 ]; do [ \"$1\" = -o ] && out=$2; shift; done\n"
                                            "cp '" + f.dir.file("feed.xml") + "' \"$out\"\n");

    f.app.handleEvent(Event::Character('5'));
    tui::ViewState s = f.app.viewState();
    CHECK(s.tab == tui::NewsTab);
    REQUIRE(s.news.size() == 3);   // sem tela, o download roda na hora
    CHECK(readLines(f.dir.file("fetch.log")).size() == 1);
    std::string out = draw(s);
    for (const char *t : {"FreeBSD news", "FreeBSD 15.1-RELEASE Available  —  Tue, 02 Jun 2026 00:00:00",
                          "https://www.freebsd.org/releases/15.1R/announce/", "The FreeBSD Release Engineering",
                          "New committer: Boris Lytochkin (src)  —  02 Oct 26 00:00 UTC"})
        CHECK_INFO(out, t);
    // A descrição longa é quebrada na largura da aba.
    s.contentWidth = 40;
    for (const auto &l : tui::tabText(s))
        if (l.rfind("https://", 0) != 0 && l.find("  —  ") == std::string::npos)
            CHECK(l.size() <= 40);

    // Voltar à aba não baixa de novo; Ctrl+G, sim.
    f.app.handleEvent(Event::Character('1'));
    f.app.handleEvent(Event::Character('5'));
    CHECK(readLines(f.dir.file("fetch.log")).size() == 1);
    f.app.handleEvent(Event::Special("\x07"));   // Ctrl+G
    CHECK(readLines(f.dir.file("fetch.log")).size() == 2);

    f.app.handleEvent(Event::Special("\x06"));   // Ctrl+F
    for (char c : std::string("stable/15"))
        f.app.handleEvent(Event::Character(c));
    CHECK(f.app.viewState().findCount == 1);
    f.app.handleEvent(Event::Escape);

    // O cache ficou ao lado das preferências.
    CHECK(readFile(f.dir.file("news.xml")).find("Ports status") != std::string::npos);
}

TEST_CASE("aba Notícias sem rede mostra o aviso", "[app][news]")
{
    i18n::setLocale("en_US");
    AppFixture f;   // fetch.sh não existe
    f.app.handleEvent(Event::Special("\x1b" "5"));
    const tui::ViewState s = f.app.viewState();
    CHECK(s.news.empty());
    CHECK_FALSE(s.newsError.empty());
    const std::string out = draw(s);
    CHECK(out.find("WARNING: Failed to start") != std::string::npos);
    CHECK(out.find("No news yet. Ctrl+G gets the latest news.") != std::string::npos);
}

// --- v0.5.0: busca por descrição ---

TEST_CASE("F7 alterna a busca por nome e por descrição, e a escolha fica salva", "[app][search]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.filterText() = "transfer";   // está no comentário do curl, não no nome
    CHECK(f.app.viewState().rowCount == 0);

    f.app.handleEvent(Event::F7);
    tui::ViewState s = f.app.viewState();
    CHECK(s.searchMode == model::SearchMode::Description);
    CHECK(s.status == "Search by description (name and comment).");
    CHECK(f.app.settings().value("search") == "description");
    f.app.filterText() = "comment";    // sample(): comment = nome + " comment"
    s = f.app.viewState();
    CHECK(s.rowCount == 3);
    f.app.filterText() = "pkg comment";
    s = f.app.viewState();
    REQUIRE(s.rowCount == 1);
    CHECK(s.current.name == "pkg");
    CHECK(draw(s).find("Description:") != std::string::npos);

    f.app.handleEvent(Event::F7);
    CHECK(f.app.viewState().searchMode == model::SearchMode::Name);
    CHECK(f.app.viewState().rowCount == 0);   // "pkg comment" não está no nome
    CHECK(f.app.settings().value("search") == "name");
}

TEST_CASE("menu Busca, com o modo atual marcado", "[app][search][menu]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::Special("\x1b" "b"));   // Alt+B
    REQUIRE(f.app.viewState().dialog.kind == tui::Dialog::Menu);
    CHECK(f.app.viewState().dialog.menu == 1);
    std::string out = draw(f.app.viewState());
    CHECK(out.find("• By name") != std::string::npos);
    f.app.handleEvent(Event::ArrowDown);   // Por descrição
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().searchMode == model::SearchMode::Description);
    f.app.handleEvent(Event::Special("\x1b" "b"));
    out = draw(f.app.viewState());
    CHECK(out.find("• By description") != std::string::npos);
    f.app.handleEvent(Event::Escape);

    // Relido do arquivo, numa nova App.
    tui::Options o;
    o.pkgPath = f.dir.file("pkg-nao-usado");
    o.settingsPath = f.dir.file("config");
    o.newsFlavour = "freebsd";
    tui::App again(o);
    CHECK(again.viewState().searchMode == model::SearchMode::Description);
}

// --- v0.6.0: filtro por categoria ---

namespace {

tui::Listing categorized()
{
    tui::Listing l;
    const char *items[][2] = {{"vim", "editors/vim"},   {"nano", "editors/nano"}, {"curl", "ftp/curl"},
                              {"wget", "ftp/wget"},     {"zsh", "shells/zsh"},    {"bash", "shells/bash"},
                              {"pkg", "ports-mgmt/pkg"}};
    for (const auto &i : items) {
        octo::Package p = installed(i[0], "1.0", 1024);
        p.origin = i[1];
        l.installed.push_back(p);
    }
    return l;
}

} // namespace

TEST_CASE("F8 escolhe a categoria, que combina com o filtro", "[app][category]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.applyListing(categorized());
    f.app.handleEvent(Event::F8);
    tui::ViewState s = f.app.viewState();
    REQUIRE(s.categoryChooserOpen);
    CHECK(s.categoryIndex == 0);   // (todas), a atual
    CHECK(s.categories.size() == 4);
    std::string out = draw(s);
    for (const char *t : {" Category ", "• (all categories)", "editors", "ftp", "ports-mgmt", "shells", "1/5"})
        CHECK_INFO(out, t);

    f.app.handleEvent(Event::Character('s'));   // salta para shells
    CHECK(f.app.viewState().categoryIndex == 4);
    f.app.handleEvent(Event::Character('e'));   // e para editors
    CHECK(f.app.viewState().categoryIndex == 1);
    f.app.handleEvent(Event::Return);
    s = f.app.viewState();
    CHECK_FALSE(s.categoryChooserOpen);
    CHECK(s.category == "editors");
    CHECK(s.status == "Category: editors");
    CHECK(s.rowCount == 2);
    CHECK(draw(s).find("[# Installed packages · editors]") != std::string::npos);

    f.app.filterText() = "na";
    CHECK(f.app.viewState().rowCount == 1);   // nano; "na" no nome, só em editors
    f.app.filterText().clear();

    // De novo: a atual vem marcada e em foco; Home e Enter voltam a todas.
    f.app.handleEvent(Event::F8);
    s = f.app.viewState();
    CHECK(s.categoryIndex == 1);
    CHECK(draw(s).find("• editors") != std::string::npos);
    f.app.handleEvent(Event::Home);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().category.empty());
    CHECK(f.app.viewState().rowCount == 7);
}

TEST_CASE("categorias pelo menu Busca, e sem pacotes não quebra", "[app][category][menu]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.applyListing(categorized());
    f.app.handleEvent(Event::Special("\x1b" "b"));   // menu Busca
    for (int i = 0; i < 2; ++i)                     // Por nome, Por descrição, (sep), Filtrar por categoria
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    REQUIRE(f.app.viewState().categoryChooserOpen);
    f.app.handleEvent(Event::End);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().category == "shells");
    f.app.handleEvent(Event::Special("\x1b" "b"));
    for (int i = 0; i < 3; ++i)                     // … Todas as categorias
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().category.empty());

    // Sem pacotes, a lista de categorias fica vazia.
    AppFixture empty;
    empty.app.applyListing(tui::Listing{});
    empty.app.handleEvent(Event::F8);
    REQUIRE(empty.app.viewState().categoryChooserOpen);
    empty.app.handleEvent(Event::Character('x'));   // nada para saltar
    empty.app.handleEvent(Event::Escape);
    CHECK_FALSE(empty.app.viewState().categoryChooserOpen);
}
