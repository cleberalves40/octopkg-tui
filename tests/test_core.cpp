// Núcleo: leitura da saída do pkg (os formatos que o OctoPkg lê), chamadas ao
// pkg com um pkg falso, execução de processos e preferências. Roda em
// qualquer sistema.

#include "i18n.h"
#include "news.h"
#include "pkg.h"
#include "process.h"
#include "settings.h"
#include "support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>   // CHECK_THROWS_WITH
#include <cstdlib>
#include <ctime>
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

// --- v0.2.0: transações ---

namespace {

// "pkg install -n" do FreeBSD 15.1 (pkg 2.x), com um conflito removido.
const char kInstallDryRun[] =
    "Updating FreeBSD repository catalogue...\n"
    "FreeBSD repository is up to date.\n"
    "All repositories are up to date.\n"
    "Checking integrity... done (1 conflicting)\n"
    "The following 3 package(s) will be affected (of 0 checked):\n"
    "\n"
    "Installed packages to be REMOVED:\n"
    "\tvim-tiny: 9.1.0\n"
    "\n"
    "New packages to be INSTALLED:\n"
    "\tvim: 9.1.1000 [FreeBSD]\n"
    "\tlibsodium: 1.0.20 [FreeBSD]\n"
    "\n"
    "Number of packages to be removed: 1\n"
    "Number of packages to be installed: 2\n"
    "\n"
    "The operation will require 35 MiB more space.\n"
    "8 MiB to be downloaded.\n";

// "pkg remove -n -R": quem depende de curl também sai.
const char kRemoveDryRun[] =
    "Checking integrity... done (0 conflicting)\n"
    "Deinstallation has been requested for the following 2 packages (of 0 packages in the universe):\n"
    "\n"
    "Installed packages to be REMOVED:\n"
    "\tcurl: 8.9.1\n"
    "\tgit: 2.46.0\n"
    "\n"
    "Number of packages to be removed: 2\n"
    "\n"
    "The operation will free 50 MiB.\n";

// Elevador falso: registra e executa o comando, como o doas faria.
std::string fakeElevator(const TempDir &dir, const std::string &log)
{
    const std::string path = dir.file("doas-falso");
    writeExecutable(path, "#!/bin/sh\necho \"doas $*\" >> '" + log + "'\nexec \"$@\"\n");
    return path;
}

std::string fakeBectl(const TempDir &dir, const std::string &log, int checkStatus, int createStatus = 0)
{
    const std::string path = dir.file("bectl-falso");
    writeExecutable(path, "#!/bin/sh\necho \"bectl $*\" >> '" + log + "'\n"
                          "[ \"$1\" = check ] && exit " + std::to_string(checkStatus) + "\n"
                          "[ \"$1\" = create ] && { echo \"bectl: falhou\" >&2; exit " + std::to_string(createStatus) +
                          "; }\nexit 0\n");
    return path;
}

} // namespace

TEST_CASE("lê a prévia de instalar, remover e atualizar", "[parse]")
{
    const auto ins = octo::parseTransaction(kInstallDryRun);
    REQUIRE(ins.sections.size() == 2);
    CHECK(ins.sections[0].action == "REMOVED");
    CHECK(ins.removed() == std::vector<std::string>{"vim-tiny: 9.1.0"});
    CHECK(ins.items("") == std::vector<std::string>{"vim: 9.1.1000 [FreeBSD]", "libsodium: 1.0.20 [FreeBSD]"});
    CHECK(ins.count() == 3);
    CHECK(ins.downloadSize == "8 MiB");

    const auto rem = octo::parseTransaction(kRemoveDryRun);
    CHECK(rem.removed().size() == 2);
    CHECK(rem.items("").empty());
    CHECK(rem.spaceChange == "The operation will free 50 MiB.");
    CHECK(rem.downloadSize.empty());

    const auto up = octo::parseTransaction(kUpgradeDryRun);
    CHECK(up.items("INSTALLED").size() == 1);
    CHECK(up.items("UPGRADED").size() == 2);
    CHECK(up.items("REINSTALLED") == std::vector<std::string>{"python311-3.11.9 [FreeBSD] (options changed)"});
    CHECK(up.items("").size() == 4);
    CHECK(up.downloadSize == "3 MiB");
    CHECK(octo::parseTransaction("Your packages are up to date.\n").count() == 0);
}

TEST_CASE("nome do pacote nos itens da prévia", "[parse]")
{
    CHECK(octo::itemName("curl: 8.9.1 -> 8.10.0 [FreeBSD]") == "curl");
    CHECK(octo::itemName("vim-tiny: 9.1.0") == "vim-tiny");
    CHECK(octo::itemName("python311-3.11.9 [FreeBSD] (options changed)") == "python311");
    CHECK(octo::itemName("py311-pip-24.0") == "py311-pip");
}

TEST_CASE("pacotes protegidos e nome do Boot Environment", "[validation]")
{
    CHECK(octo::isForbidden("pkg"));
    CHECK(octo::isForbidden("xorg"));
    CHECK_FALSE(octo::isForbidden("curl"));
    const std::string be = octo::bootEnvironmentName(std::time(nullptr));
    REQUIRE(be.size() == 13);   // AAMMDD-hhmmss, como no OctoPkg
    CHECK(be[6] == '-');
}

TEST_CASE("prévias como usuário, sem elevação", "[pkg]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    const std::string log = dir.file("calls.log");
    std::ofstream(dir.file("install.txt")) << kInstallDryRun;
    std::ofstream(dir.file("remove.txt")) << kRemoveDryRun;
    const std::string pkg = fakePkg(dir, log, "case \"$1\" in\n"
                                              "install) cat '" + dir.file("install.txt") + "'; exit 1 ;;\n"
                                              "remove) cat '" + dir.file("remove.txt") + "' ;;\n"
                                              "upgrade) echo 'Your packages are up to date.' ;;\n"
                                              "esac");
    octo::Manager mgr(pkg, fakeElevator(dir, log));
    // Com -n, sair com 1 havendo o que fazer não é erro.
    CHECK(mgr.previewInstall({"vim"}).count() == 3);
    CHECK(mgr.previewRemove({"curl"}).removed().size() == 2);
    CHECK(mgr.previewUpgrade().count() == 0);
    const auto calls = readLines(log);
    CHECK(calls == std::vector<std::string>{"install|-n|-f|-U|vim", "remove|-n|-R|curl", "upgrade|-n|-U"});
    CHECK_THROWS_WITH(mgr.previewInstall({}), "No packages selected.");
    CHECK_THROWS_WITH(mgr.previewRemove({"-a"}), "Invalid package name: -a");
}

TEST_CASE("prévia que falha mostra a mensagem do pkg", "[pkg]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    const std::string log = dir.file("calls.log");
    octo::Manager mgr(fakePkg(dir, log, "echo \"pkg: No packages available to install matching 'nada'\" >&2; exit 1"));
    CHECK_THROWS_WITH(mgr.previewInstall({"nada"}), "pkg: No packages available to install matching 'nada'");
}

TEST_CASE("operações chamam o pkg pelo elevador, com os comandos do OctoPkg", "[pkg]")
{
    TempDir dir;
    const std::string log = dir.file("calls.log");
    const std::string pkg = fakePkg(dir, log, "echo \"saída de $1\"");
    octo::Manager mgr(pkg, fakeElevator(dir, log), fakeBectl(dir, log, 0));
    std::string out;
    const octo::OutputFn collect = [&out](const std::string &c) { out += c; };
    mgr.install({"curl", "git"}, collect);
    mgr.remove({"vim"}, collect);
    mgr.upgrade(collect);
    mgr.update(collect);
    mgr.clean(collect);
    mgr.lock("curl", collect);
    mgr.unlock("curl", collect);
    mgr.createBootEnvironment("261020-101500", collect);
    CHECK(mgr.bootEnvironmentsAvailable());

    std::vector<std::string> pkgCalls, elevated;
    for (const auto &c : readLines(log))
        (startsWith(c, "doas ") ? elevated : pkgCalls).push_back(c);
    CHECK(pkgCalls == std::vector<std::string>{"install|-f|-y|curl|git", "remove|-R|-y|vim", "upgrade|-y",
                                               "update|-f", "clean|-a|-y", "lock|-y|curl", "unlock|-y|curl",
                                               "bectl create 261020-101500", "bectl check"});
    CHECK(elevated.size() == 8);   // tudo menos o bectl check
    CHECK(startsWith(elevated[0], "doas " + pkg + " install -f -y curl git"));
    CHECK(contains(elevated[7], "bectl-falso create 261020-101500"));
    CHECK(contains(out, "saída de install"));
    CHECK(contains(out, "saída de clean"));
    CHECK(mgr.commandLine({"install", "-f", "-y", "curl"}).find(" install -f -y curl") != std::string::npos);
}

TEST_CASE("operações recusam o que o OctoPkg recusa e repassam erros", "[pkg]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    const std::string log = dir.file("calls.log");
    SECTION("pacote protegido e nomes inválidos, sem chamar o pkg")
    {
        octo::Manager mgr(fakePkg(dir, log, "exit 0"));
        CHECK_THROWS_WITH(mgr.remove({"pkg"}), "pkg cannot be removed.");
        CHECK_THROWS_WITH(mgr.install({"-f"}), "Invalid package name: -f");
        CHECK_THROWS_WITH(mgr.createBootEnvironment("-x"), "Invalid boot environment name: -x");
        CHECK_THROWS_WITH(mgr.createBootEnvironment("com espaço"), "Invalid boot environment name: com espaço");
        CHECK(readLines(log).empty());
    }
    SECTION("mensagem do stderr")
    {
        octo::Manager mgr(fakePkg(dir, log, "echo 'pkg: Insufficient privileges' >&2; exit 77"));
        CHECK_THROWS_WITH(mgr.upgrade(), "pkg: Insufficient privileges");
    }
    SECTION("elevador ausente")
    {
        octo::Manager mgr(fakePkg(dir, log, "exit 0"), "doas-que-nao-existe");
        CHECK_THROWS_WITH(mgr.clean(),
                          "'doas-que-nao-existe' not found. Install it (pkg install doas-que-nao-existe) or run "
                          "octopkg-tui as root.");
    }
    SECTION("Boot Environments: bectl check")
    {
        CHECK_FALSE(octo::Manager(fakePkg(dir, log, "exit 0"), "", fakeBectl(dir, log, 1)).bootEnvironmentsAvailable());
        CHECK_FALSE(octo::Manager(fakePkg(dir, log, "exit 0"), "", "/nao/existe/bectl").bootEnvironmentsAvailable());
    }
}

TEST_CASE("a saída chega aos poucos", "[process]")
{
    std::vector<std::string> chunks;
    const auto r = octo::runProcess({"/bin/sh", "-c", "echo um; sleep 0.3; echo dois >&2"}, 5000, {},
                                    [&chunks](const std::string &c) { chunks.push_back(c); });
    REQUIRE(chunks.size() >= 2);
    CHECK(chunks.front() == "um\n");
    CHECK(r.out == "um\n");
    CHECK(r.err == "dois\n");
}

// --- v0.4.0: notícias ---

namespace {

// Trecho no formato do https://www.freebsd.org/news/feed.xml: descrição em HTML
// escapado, CDATA, entidades e um <link> em cada item.
const char kFeed[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<rss version=\"2.0\"><channel><title>FreeBSD News Flash</title>\n"
    "<link>https://www.freebsd.org/news/newsflash/</link>\n"
    "<item><title>FreeBSD 15.1-RELEASE Available</title>\n"
    "<link>https://www.freebsd.org/releases/15.1R/announce/</link>\n"
    "<description>&lt;p&gt;FreeBSD 15.1-RELEASE is now   available.\n Please be sure to check the "
    "&lt;a href=\"x\"&gt;Release Notes&lt;/a&gt; &amp;amp; Errata.&lt;/p&gt;</description>\n"
    "<pubDate>Tue, 02 Jun 2026 00:00:00 +0000</pubDate></item>\n"
    "<item><title><![CDATA[Ports & Packages: Q3 report]]></title>\n"
    "<link>https://www.freebsd.org/status/</link>\n"
    "<description><![CDATA[<b>Status</b> report &amp; more]]></description>\n"
    "<pubDate>Mon, 05 Oct 2026 10:00:00 -0300</pubDate></item>\n"
    "<item><title>Caf&#233; &#x2014; 3 &lt; 4</title><description>x</description></item>\n"
    "</channel></rss>\n";

std::string feed(int items)
{
    std::string x = "<rss><channel>";
    for (int i = 0; i < items; ++i)
        x += "<item><title>N" + std::to_string(i) + "</title><description>d</description></item>";
    return x + "</channel></rss>";
}

// fetch(1) falso: grava em -o o arquivo de origem e registra os argumentos.
std::string fakeFetch(const TempDir &dir, const std::string &source, int status = 0)
{
    const std::string path = dir.file("fetch-" + std::to_string(status));
    writeExecutable(path, "#!/bin/sh\necho \"$*\" >> '" + dir.file("fetch.log") + "'\n"
                          "while [ $# -gt 1 ]; do [ \"$1\" = -o ] && out=$2; shift; done\n"
                          "[ " + std::to_string(status) + " -ne 0 ] && { echo 'fetch: Not Found' >&2; exit " +
                          std::to_string(status) + "; }\ncp '" + source + "' \"$out\"\n");
    return path;
}

} // namespace

TEST_CASE("lê o feed RSS como o OctoPkg", "[news]")
{
    const auto items = octo::parseRss(kFeed);
    REQUIRE(items.size() == 3);
    CHECK(items[0].title == "FreeBSD 15.1-RELEASE Available");
    CHECK(items[0].link == "https://www.freebsd.org/releases/15.1R/announce/");
    CHECK(items[0].description
          == "FreeBSD 15.1-RELEASE is now available. Please be sure to check the Release Notes & Errata.");
    CHECK(items[0].date == "Tue, 02 Jun 2026 00:00:00");   // sem o fuso, como no OctoPkg
    CHECK(items[1].title == "Ports & Packages: Q3 report");
    CHECK(items[1].description == "Status report & more");
    CHECK(items[1].date == "Mon, 05 Oct 2026 10:00:00");
    CHECK(items[2].title == "Café — 3 < 4");
    CHECK(items[2].link.empty());
    CHECK(octo::parseRss(feed(15)).size() == 10);   // as 10 últimas
    CHECK(octo::parseRss(feed(15), 3).size() == 3);
    CHECK(octo::parseRss("<html>não é um feed</html>").empty());
}

TEST_CASE("variante do sistema e o feed de cada uma", "[news]")
{
    using octo::Flavour;
    CHECK(octo::detectFlavour("FreeBSD host 15.1-RELEASE FreeBSD 15.1-RELEASE amd64", false) == Flavour::FreeBSD);
    CHECK(octo::detectFlavour("FreeBSD host 14.3-STABLE FreeBSD HBSD amd64", false) == Flavour::HardenedBSD);
    CHECK(octo::detectFlavour("FreeBSD nomad 14.2 NomadBSD amd64", false) == Flavour::NomadBSD);
    CHECK(octo::detectFlavour("FreeBSD ghost 14.1 FreeBSD amd64", true) == Flavour::GhostBSD);
    CHECK(octo::detectFlavour("DragonFly df 6.4-RELEASE DragonFly v6.4.0", false) == Flavour::DragonFly);
    CHECK(octo::detectFlavour("Darwin mac 25.0.0 Darwin Kernel", false) == Flavour::Unknown);
    CHECK(octo::newsUrl(Flavour::FreeBSD) == "https://www.freebsd.org/news/feed.xml");
    CHECK(octo::newsUrl(Flavour::NomadBSD) == octo::newsUrl(Flavour::FreeBSD));
    CHECK(octo::newsUrl(Flavour::Unknown).empty());
    CHECK(octo::flavourFromString("ghostbsd") == Flavour::GhostBSD);
    CHECK(octo::flavourName(Flavour::DragonFly) == "DragonFly BSD");
}

TEST_CASE("baixa o feed com o fetch, guarda e avisa o que mudou", "[news]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    std::ofstream(dir.file("feed.xml")) << kFeed;
    const std::string cache = dir.file("sub/news.xml");
    octo::NewsFeed news(cache, fakeFetch(dir, dir.file("feed.xml")));
    CHECK(news.cached().items.empty());   // ainda sem cache

    octo::NewsResult r = news.refresh(octo::Flavour::FreeBSD);
    CHECK(r.error.empty());
    CHECK(r.updated);
    CHECK_FALSE(r.fromCache);
    REQUIRE(r.items.size() == 3);
    CHECK(readFile(cache) == kFeed);
    CHECK(readLines(dir.file("fetch.log")).front()
          == "-q -T 30 -o " + cache + ".tmp https://www.freebsd.org/news/feed.xml");

    // O mesmo feed de novo: nada mudou.
    r = news.refresh(octo::Flavour::FreeBSD);
    CHECK_FALSE(r.updated);
    CHECK(news.cached().items.size() == 3);
}

TEST_CASE("sem rede ou com feed inválido, fica o cache", "[news]")
{
    i18n::setLocale("en_US");
    TempDir dir;
    std::ofstream(dir.file("news.xml")) << kFeed;   // o cache de antes
    std::ofstream(dir.file("ruim.xml")) << "<html>erro do proxy</html>";
    SECTION("fetch falha")
    {
        octo::NewsFeed news(dir.file("news.xml"), fakeFetch(dir, dir.file("feed.xml"), 1));
        const octo::NewsResult r = news.refresh(octo::Flavour::FreeBSD);
        CHECK(r.error == "fetch: Not Found");
        CHECK(r.fromCache);
        CHECK(r.items.size() == 3);
    }
    SECTION("feed que não dá para ler não substitui o cache")
    {
        octo::NewsFeed news(dir.file("news.xml"), fakeFetch(dir, dir.file("ruim.xml")));
        const octo::NewsResult r = news.refresh(octo::Flavour::FreeBSD);
        CHECK(r.error == "The news feed could not be read.");
        CHECK(r.items.size() == 3);
        CHECK(readFile(dir.file("news.xml")) == kFeed);
    }
    SECTION("variante sem feed")
    {
        octo::NewsFeed news(dir.file("news.xml"), fakeFetch(dir, dir.file("feed.xml")));
        const octo::NewsResult r = news.refresh(octo::Flavour::Unknown);
        CHECK_FALSE(r.error.empty());
        CHECK(readLines(dir.file("fetch.log")).empty());   // nem tentou baixar
    }
}
