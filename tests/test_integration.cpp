// Integração contra o pkg(8) real, somente leitura: os mesmos formatos de
// consulta que a interface usa, no banco de dados e no catálogo do sistema.
//
// tests/freebsd-e2e.sh define OCTOPKG_TEST_PKG com o caminho do pkg; sem a
// variável, os testes são ignorados. Espera um sistema com o pkg e os pacotes
// de compilação do projeto instalados (cmake, ninja, catch2, ftxui) e o
// catálogo do repositório atualizado.

#include "pkg.h"
#include "support.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <stdexcept>

namespace {

bool enabled()
{
    const char *cmd = std::getenv("OCTOPKG_TEST_PKG");
    return cmd && *cmd;
}

const octo::Manager &mgr()
{
    static octo::Manager m(std::getenv("OCTOPKG_TEST_PKG") ? std::getenv("OCTOPKG_TEST_PKG") : "");
    return m;
}

const std::vector<octo::Package> &installed()
{
    static const std::vector<octo::Package> list = mgr().listInstalled();
    return list;
}

const octo::Package &find(const std::vector<octo::Package> &list, const std::string &name)
{
    for (const auto &p : list)
        if (p.name == name)
            return p;
    throw std::runtime_error("pacote não encontrado: " + name);
}

bool contains(const std::vector<std::string> &v, const std::string &s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}

#define REQUIRE_PKG()                                                                                   \
    if (!enabled())                                                                                     \
    SKIP("OCTOPKG_TEST_PKG não definido; execute via tests/freebsd-e2e.sh")

} // namespace

TEST_CASE("lista os instalados", "[integration]")
{
    REQUIRE_PKG();
    REQUIRE(installed().size() >= 4);
    const octo::Package &p = find(installed(), "pkg");
    CHECK(p.origin == "ports-mgmt/pkg");
    CHECK(p.version == mgr().version());
    CHECK(p.size > 0);
    CHECK(p.installedOn > 0);
    CHECK_FALSE(p.repository.empty());
    CHECK_FALSE(p.comment.empty());
    CHECK(p.installed);
    // Os pacotes de compilação foram pedidos; ao menos uma dependência veio junto.
    CHECK_FALSE(find(installed(), "cmake").automatic);
    CHECK(std::any_of(installed().begin(), installed().end(), [](const octo::Package &x) { return x.automatic; }));
    CHECK(std::any_of(installed().begin(), installed().end(), [](const octo::Package &x) { return x.requiredBy > 0; }));
}

TEST_CASE("detalhes e arquivos de um instalado", "[integration]")
{
    REQUIRE_PKG();
    const octo::Details d = mgr().details("pkg", true);
    CHECK(d.name == "pkg");
    CHECK(d.origin == "ports-mgmt/pkg");
    CHECK_FALSE(d.description.empty());
    CHECK_FALSE(d.licenses.empty());
    CHECK_FALSE(d.arch.empty());
    CHECK(d.installedOn > 0);
    CHECK(contains(d.files, "/usr/local/sbin/pkg"));

    // Quem é requerido por outro pacote mostra a lista de quem depende dele.
    for (const auto &p : installed())
        if (p.requiredBy > 0) {
            const octo::Details r = mgr().details(p.name, true);
            CHECK(static_cast<int>(r.requiredBy.size()) == p.requiredBy);
            break;
        }
}

TEST_CASE("lista os desatualizados sem erro", "[integration]")
{
    REQUIRE_PKG();
    std::string warning;
    const auto up = mgr().listOutdated(&warning);
    CHECK(warning.empty());
    for (const auto &kv : up) {
        CHECK_FALSE(kv.second.oldVersion.empty());
        CHECK_FALSE(kv.second.newVersion.empty());
    }
}

TEST_CASE("lista os repositórios e junta com os instalados", "[integration]")
{
    REQUIRE_PKG();
    const auto remote = mgr().listRemote();
    REQUIRE(remote.size() > 1000);
    const auto all = octo::mergeRemote(remote, installed());
    CHECK(find(all, "pkg").installed);
    CHECK(find(all, "pkg").status != octo::Status::NotInstalled);

    const auto notInstalled = std::find_if(all.begin(), all.end(), [](const octo::Package &p) {
        return !p.installed && octo::validPackageName(p.name);
    });
    REQUIRE(notInstalled != all.end());
    const octo::Details d = mgr().details(notInstalled->name, false);
    CHECK(d.name == notInstalled->name);
    CHECK_FALSE(d.installed);
    CHECK(d.installedOn == 0);
    CHECK(d.files.empty());
}
