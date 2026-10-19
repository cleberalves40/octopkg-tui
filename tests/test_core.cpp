// Núcleo: leitura da saída do pkg (os formatos que o OctoPkg lê), chamadas ao
// pkg com um pkg falso, execução de processos e preferências. Roda em
// qualquer sistema.

#include "i18n.h"
#include "pkg.h"
#include "process.h"
#include "settings.h"
#include "support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>   // CHECK_THROWS_WITH
#include <cstdlib>
#include <stdexcept>

using octo::Status;

namespace {

bool startsWith(const std::string &s, const std::string &prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool contains(const std::string &s, const std::string &part)
{
    return s.find(part) != std::string::npos;
}

// Saída de "pkg upgrade -n" no FreeBSD 15.1 (pkg 2.x).
const char kUpgradeDryRun[] =
    "Updating FreeBSD repository catalogue...\n"
    "FreeBSD repository is up to date.\n"
    "All repositories are up to date.\n"
    "Checking for upgrades (4 candidates): .......... done\n"
    "Processing candidates (4 candidates): .......... done\n"
    "The following 4 package(s) will be affected (of 0 checked):\n"
    "\n"
    "New packages to be INSTALLED:\n"
    "\tlibidn2: 2.3.7 [FreeBSD]\n"
    "\n"
    "Installed packages to be UPGRADED:\n"
    "\tcurl: 8.9.1 -> 8.10.0 [FreeBSD]\n"
    "\tgit: 2.46.0 -> 2.46.1\n"
    "\n"
    "Installed packages to be REINSTALLED:\n"
    "\tpython311-3.11.9 [FreeBSD] (options changed)\n"
    "\n"
    "Number of packages to be installed: 1\n"
    "Number of packages to be upgraded: 2\n"
    "\n"
    "3 MiB to be downloaded.\n";

octo::Package installed(const std::string &name, const std::string &version, bool locked = false)
{
    octo::Package p;
    p.name = name;
    p.version = version;
    p.installedVersion = version;
    p.installed = true;
    p.locked = locked;
    p.installedOn = 1760000000;
    p.status = locked ? Status::Locked : Status::Installed;
    return p;
}

octo::Package remote(const std::string &name, const std::string &version, const std::string &repo = "FreeBSD")
{
    octo::Package p;
    p.name = name;
    p.version = version;
    p.repository = repo;
    return p;
}

} // namespace

TEST_CASE("lê a lista de instalados", "[parse]")
{
    const auto list = octo::parseInstalled(
        "curl\t8.9.1\tftp/curl\t4567890\t1760000000\t0\t0\t3\tFreeBSD\tCommand line tool for URLs\n"
        "pkg\t2.3.1\tports-mgmt/pkg\t25000000\t1750000000\t1\t1\t0\tFreeBSD\tPackage manager\twith tab\n"
        "linha inválida\n"
        "\n");
    REQUIRE(list.size() == 2);
    const auto &c = list[0];
    CHECK(c.name == "curl");
    CHECK(c.version == "8.9.1");
    CHECK(c.installedVersion == "8.9.1");
    CHECK(c.origin == "ftp/curl");
    CHECK(c.size == 4567890);
    CHECK(c.installedOn == 1760000000);
    CHECK_FALSE(c.locked);
    CHECK_FALSE(c.automatic);
    CHECK(c.requiredBy == 3);
    CHECK(c.repository == "FreeBSD");
    CHECK(c.comment == "Command line tool for URLs");
    CHECK(c.installed);
    CHECK(c.status == Status::Installed);
    const auto &p = list[1];
    CHECK(p.locked);
    CHECK(p.automatic);
    CHECK(p.status == Status::Locked);
    CHECK(p.comment == "Package manager\twith tab");
}

TEST_CASE("lê a lista dos repositórios", "[parse]")
{
    const auto list = octo::parseRemote("vim\t9.1.0\teditors/vim\t40000000\tFreeBSD\tImproved vi\n"
                                        "nano\t8.2\teditors/nano\t2000000\tFreeBSD\tEditor\n");
    REQUIRE(list.size() == 2);
    CHECK(list[0].name == "vim");
    CHECK(list[0].repository == "FreeBSD");
    CHECK(list[0].size == 40000000);
    CHECK_FALSE(list[0].installed);
    CHECK(list[0].status == Status::NotInstalled);
    CHECK(list[1].comment == "Editor");
}

TEST_CASE("lê os desatualizados de pkg upgrade -n", "[parse]")
{
    const auto up = octo::parseUpgradeDryRun(kUpgradeDryRun);
    REQUIRE(up.size() == 2);
    CHECK(up.at("curl").oldVersion == "8.9.1");
    CHECK(up.at("curl").newVersion == "8.10.0");
    CHECK(up.at("git").newVersion == "2.46.1");
    CHECK(octo::parseUpgradeDryRun("Your packages are up to date.\n").empty());
}

TEST_CASE("lê os detalhes e só o primeiro registro", "[parse]")
{
    const auto d = octo::parseDetails(
        "curl\t8.9.1\tftp/curl\tFreeBSD\tsunpoet@FreeBSD.org\thttps://curl.se/\tFreeBSD:15:amd64\t4567890\t"
        "1760000000\tCommand line tool\tcurl is a tool\nto transfer data.\n\x1e"
        "curl\t8.9.1\tftp/curl\toutro\t\t\t\t0\t0\t\t\x1e");
    CHECK(d.name == "curl");
    CHECK(d.repository == "FreeBSD");
    CHECK(d.maintainer == "sunpoet@FreeBSD.org");
    CHECK(d.www == "https://curl.se/");
    CHECK(d.arch == "FreeBSD:15:amd64");
    CHECK(d.size == 4567890);
    CHECK(d.installedOn == 1760000000);
    CHECK(d.comment == "Command line tool");
    CHECK(d.description == "curl is a tool\nto transfer data.");
    CHECK(octo::parseDetails("").name.empty());
}

TEST_CASE("desatualizados e bloqueados", "[status]")
{
    std::vector<octo::Package> list{installed("curl", "8.9.1"), installed("git", "2.46.0", true),
                                    installed("vim", "9.1")};
    octo::applyUpgrades(list, octo::parseUpgradeDryRun(kUpgradeDryRun));
    CHECK(list[0].status == Status::Outdated);
    CHECK(list[0].newVersion == "8.10.0");
    CHECK(list[1].status == Status::Locked);   // o bloqueio prevalece, como no OctoPkg
    CHECK(list[1].newVersion == "2.46.1");
    CHECK(list[2].status == Status::Installed);
    // Atualizar de novo sem desatualizados volta ao estado instalado.
    octo::applyUpgrades(list, {});
    CHECK(list[0].status == Status::Installed);
    CHECK(list[0].newVersion.empty());
}

TEST_CASE("visão Repositório junta o estado dos instalados", "[status]")
{
    std::vector<octo::Package> inst{installed("curl", "8.9.1"), installed("git", "2.46.0", true),
                                    installed("vim", "9.2"), installed("nano", "8.2"), installed("meu-local", "1.0")};
    octo::applyUpgrades(inst, octo::parseUpgradeDryRun(kUpgradeDryRun));
    const auto all = octo::mergeRemote({remote("curl", "8.10.0"), remote("git", "2.46.1"), remote("vim", "9.1"),
                                        remote("nano", "8.2"), remote("zsh", "5.9"), remote("zsh", "5.8", "outro")},
                                       inst);
    REQUIRE(all.size() == 6);
    const auto find = [&](const std::string &n) {
        for (const auto &p : all)
            if (p.name == n)
                return p;
        throw std::runtime_error("faltou " + n);
    };
    CHECK(find("curl").status == Status::Outdated);
    CHECK(find("curl").version == "8.10.0");          // a versão do repositório
    CHECK(find("curl").installedVersion == "8.9.1");
    CHECK(find("git").status == Status::Locked);
    CHECK(find("vim").status == Status::Newer);
    CHECK(find("nano").status == Status::Installed);
    CHECK(find("nano").installedOn == 1760000000);
    CHECK(find("zsh").status == Status::NotInstalled);
    CHECK(find("zsh").version == "5.9");               // o primeiro repositório
    CHECK(find("meu-local").status == Status::Installed);
}

TEST_CASE("tamanhos legíveis, como no OctoPkg", "[format]")
{
    CHECK(octo::formatSize(0) == "0.00 Bytes");
    CHECK(octo::formatSize(512) == "512.00 Bytes");
    CHECK(octo::formatSize(1536) == "1.50 KiB");
    CHECK(octo::formatSize(5LL * 1024 * 1024) == "5.00 MiB");
    CHECK(octo::formatSize(3LL * 1024 * 1024 * 1024) == "3.00 GiB");
    CHECK(octo::formatTime(0).empty());
    CHECK(octo::formatTime(1760000000).size() == 16);   // AAAA-MM-DD HH:MM
}

TEST_CASE("nomes de pacote aceitos como argumento", "[validation]")
{
    for (const char *ok : {"curl", "py311-requests", "libxml++", "p5-Foo-Bar", "R-cran-x", "gcc14"})
        CHECK(octo::validPackageName(ok));
    for (const char *bad : {"", "-a", "--all", "dois nomes", "tab\tname", "nl\nname"})
        CHECK_FALSE(octo::validPackageName(bad));
}

TEST_CASE("chama o pkg com os argumentos certos e em inglês", "[pkg]")
{
    TempDir dir;
    const std::string log = dir.file("calls.log");
    const std::string pkg = fakePkg(dir, log, R"(
case "$1" in
query)
  if [ "$2" = -a ]; then
    printf 'curl\t8.9.1\tftp/curl\t100\t1760000000\t0\t0\t0\tFreeBSD\tlang=%s\n' "$LC_ALL"
  else
    case "$2" in
    %n*) printf 'curl\t8.9.1\tftp/curl\tFreeBSD\tm@x\thttps://curl.se/\tFreeBSD:15:amd64\t100\t1760000000\tc\tdesc\n\036' ;;
    %L) echo MIT ;;
    %dn-%dv) printf 'ca_root_nss-3.1\nlibnghttp2-1.6\n' ;;
    %rn-%rv) echo git-2.46.0 ;;
    %Ok=%Ov) echo 'HTTP2=on' ;;
    %Fp) printf '/usr/local/bin/curl\n/usr/local/lib/libcurl.so\n' ;;
    esac
  fi ;;
rquery)
  printf 'vim\t9.1\teditors/vim\t200\tFreeBSD\tvi\n' ;;
upgrade)
  printf 'Installed packages to be UPGRADED:\n\tcurl: 8.9.1 -> 8.10.0\n'
  exit 1 ;;
-v) echo 2.3.1 ;;
esac)");
    octo::Manager mgr(pkg);

    const auto inst = mgr.listInstalled();
    REQUIRE(inst.size() == 1);
    CHECK(inst[0].comment == "lang=C");
    // pkg upgrade -n sai com 1 quando há atualizações: não é erro.
    std::string warning = "x";
    const auto up = mgr.listOutdated(&warning);
    CHECK(warning.empty());
    CHECK(up.at("curl").newVersion == "8.10.0");
    CHECK(mgr.listRemote().at(0).name == "vim");
    CHECK(mgr.version() == "2.3.1");

    const auto d = mgr.details("curl", true);
    CHECK(d.installed);
    CHECK(d.licenses == std::vector<std::string>{"MIT"});
    CHECK(d.dependencies.size() == 2);
    CHECK(d.requiredBy == std::vector<std::string>{"git-2.46.0"});
    CHECK(d.options == std::vector<std::string>{"HTTP2=on"});
    CHECK(d.files == std::vector<std::string>{"/usr/local/bin/curl", "/usr/local/lib/libcurl.so"});

    const auto calls = readLines(log);
    REQUIRE(calls.size() >= 4);
    CHECK(startsWith(calls[0], "query|-a|%n\t%v\t%o\t%sb\t%t\t%k\t%a\t%#r\t%R\t%c"));
    CHECK(calls[1] == "upgrade|-n|-U");
    CHECK(startsWith(calls[2], "rquery|-U|-a|%n\t"));
    bool filesQueried = false;
    for (const auto &c : calls)
        filesQueried = filesQueried || c == "query|%Fp|curl";
    CHECK(filesQueried);
}

TEST_CASE("detalhes de pacote não instalado usam rquery, sem arquivos", "[pkg]")
{
    TempDir dir;
    const std::string log = dir.file("calls.log");
    const std::string pkg = fakePkg(dir, log, R"(
case "$3" in
%n*) printf 'vim\t9.1\teditors/vim\tFreeBSD\tm@x\thttps://vim.org/\tFreeBSD:15:*\t200\t0\tvi\tVim\n\036' ;;
%dn-%dv) printf 'gettext-0.22\ngettext-0.22\n' ;;
esac)");
    octo::Manager mgr(pkg);
    const auto d = mgr.details("vim", false);
    CHECK_FALSE(d.installed);
    CHECK(d.installedOn == 0);
    CHECK(d.dependencies == std::vector<std::string>{"gettext-0.22"});   // repetidas entre repositórios
    CHECK(d.files.empty());
    for (const auto &c : readLines(log)) {
        CHECK(startsWith(c, "rquery|-U|"));
        CHECK_FALSE(contains(c, "%Fp"));
        CHECK_FALSE(contains(c, "%rn"));
    }
}

TEST_CASE("nome inválido é recusado antes do pkg", "[pkg]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    const std::string log = dir.file("calls.log");
    octo::Manager mgr(fakePkg(dir, log, "exit 0"));
    CHECK_THROWS_WITH(mgr.details("-a", true), "Invalid package name: -a");
    CHECK(readLines(log).empty());
}

TEST_CASE("erros do pkg são repassados", "[pkg]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    const std::string log = dir.file("calls.log");
    SECTION("mensagem do stderr")
    {
        octo::Manager mgr(fakePkg(dir, log, "echo 'pkg: No package(s) matching x' >&2; exit 70"));
        CHECK_THROWS_WITH(mgr.listInstalled(), "pkg: No package(s) matching x");
        std::string warning;
        CHECK(mgr.listOutdated(&warning).empty());
        CHECK(warning == "pkg: No package(s) matching x");
    }
    SECTION("status sem mensagem")
    {
        octo::Manager mgr(fakePkg(dir, log, "exit 3"));
        CHECK_THROWS_WITH(mgr.listRemote(), "Command failed with exit code 3");
    }
    SECTION("pacote não encontrado")
    {
        octo::Manager mgr(fakePkg(dir, log, "exit 0"));
        CHECK_THROWS_WITH(mgr.details("nada", true), "Package not found: nada");
    }
    SECTION("pkg ausente")
    {
        octo::Manager mgr("/nao/existe/pkg");
        try {
            mgr.listInstalled();
            FAIL("sem exceção");
        } catch (const std::runtime_error &e) {
            CHECK(startsWith(e.what(), "Failed to start '/nao/existe/pkg query -a"));
        }
    }
}

TEST_CASE("mensagens seguem o idioma", "[i18n]")
{
    i18n::setLocale("pt_BR");
    CHECK(i18n::trf("Invalid package name: %1", {"-a"}) == "Nome de pacote inválido: -a");
    i18n::setLocale("es_ES");
    CHECK(i18n::tr("Outdated") == "Desactualizado");
    i18n::setLocale("xx_YY");
    CHECK(i18n::locale() == "en_US");
    CHECK(i18n::tr("Outdated") == "Outdated");
}

TEST_CASE("execução de processos", "[process]")
{
    SECTION("captura stdout, stderr e status")
    {
        const auto r = octo::runProcess({"/bin/sh", "-c", "echo out; echo err >&2; exit 4"}, 5000);
        CHECK(r.started);
        CHECK(r.out == "out\n");
        CHECK(r.err == "err\n");
        CHECK(r.status == 4);
    }
    SECTION("ambiente do filho")
    {
        ::setenv("OCTOPKG_TUI_TEST_VAR", "pai", 1);
        const auto r = octo::runProcess({"/bin/sh", "-c", "echo \"$OCTOPKG_TUI_TEST_VAR $LC_ALL\""}, 5000,
                                        {{"OCTOPKG_TUI_TEST_VAR", "filho"}, {"LC_ALL", "C"}});
        CHECK(r.out == "filho C\n");
        CHECK(std::string(std::getenv("OCTOPKG_TUI_TEST_VAR")) == "pai");   // o pai não muda
        ::unsetenv("OCTOPKG_TUI_TEST_VAR");
    }
    SECTION("entrada é /dev/null")
    {
        const auto r = octo::runProcess({"/bin/sh", "-c", "cat; echo fim"}, 5000);
        CHECK(r.out == "fim\n");
    }
    SECTION("executável inexistente")
    {
        const auto r = octo::runProcess({"/nao/existe/pkg"}, 5000);
        CHECK_FALSE(r.started);
        CHECK_FALSE(r.startError.empty());
    }
    SECTION("tempo limite")
    {
        const auto r = octo::runProcess({"/bin/sh", "-c", "sleep 5"}, 200);
        CHECK(r.timedOut);
    }
    SECTION("término por sinal")
    {
        const auto r = octo::runProcess({"/bin/sh", "-c", "kill -9 $$"}, 5000);
        CHECK(r.crashed);
    }
}

TEST_CASE("preferências gravam e releem", "[settings]")
{
    TempDir dir;
    const std::string path = dir.file("sub/dir/config");
    {
        octo::Settings s(path);
        CHECK(s.value("x", "padrão") == "padrão");
        s.setValue("sort.column", 4);
        s.setValue("icons", "ascii");
        REQUIRE(s.save());
    }
    octo::Settings s(path);
    CHECK(s.intValue("sort.column", -1) == 4);
    CHECK(s.value("icons") == "ascii");
    CHECK(s.intValue("icons", 7) == 7);
}

TEST_CASE("trf substitui os argumentos uma vez", "[i18n]")
{
    i18n::setLocale("en_US");
    CHECK(i18n::trf("Failed to start '%1': %2", {"%2", "x"}) == "Failed to start '%2': x");
    i18n::setLocale("pt_BR");
    CHECK(i18n::trf("%1 package(s) installed.", {"3"}) == "3 pacote(s) instalado(s).");
    i18n::setLocale("en_US");
}
