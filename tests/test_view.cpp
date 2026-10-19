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
                          "2 f Files", "4 > Output", "6 ? Help", "F5", "F10"})
        CHECK_INFO(out, t);
    CHECK_FALSE(has(out, "3 Actions"));
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
    CHECK(has(out, "i Installed packages"));
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
    f.app.handleEvent(Event::Character('3'));                  // Ações: ainda não existe
    CHECK(f.app.viewState().tab == tui::HelpTab);
    CHECK(f.app.viewState().status == "This tab arrives in a later version.");
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
