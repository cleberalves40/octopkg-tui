#include "i18n.h"

#include <cstdlib>
#include <map>

namespace i18n {

namespace {

std::string g_locale = "en_US";

using Catalog = std::map<std::string, std::string>;

// { inglês (chave, o texto do OctoPkg quando existe), pt_BR, es_ES }
struct Entry {
    const char *en;
    const char *pt;
    const char *es;
};

const Entry kEntries[] = {
    {"Failed to start '%1': %2", "Falha ao iniciar '%1': %2", "No se pudo iniciar '%1': %2"},
    {"Command timed out (%1 ms): %2", "Comando excedeu o tempo limite (%1 ms): %2", "El comando excedió el tiempo límite (%1 ms): %2"},
    {"Process crashed: %1", "Processo terminou com crash: %1", "El proceso terminó abruptamente: %1"},
    {"Command failed with exit code %1", "Comando falhou com código %1", "El comando falló con código %1"},
    {"Invalid package name: %1", "Nome de pacote inválido: %1", "Nombre de paquete no válido: %1"},
    {"Package not found: %1", "Pacote não encontrado: %1", "Paquete no encontrado: %1"},

    {"Status", "Estado", "Estado"},
    {"Name", "Nome", "Nombre"},
    {"Version", "Versão", "Versión"},
    {"Size", "Tamanho", "Tamaño"},
    {"Installed on", "Instalado em", "Instalado el"},
    {"Installed", "Instalado", "Instalado"},
    {"Installed as a dependency", "Instalado como dependência", "Instalado como dependencia"},
    {"Outdated", "Desatualizado", "Desactualizado"},
    {"(version %1 is available)", "(a versão %1 está disponível)", "(la versión %1 está disponible)"},
    {"Newer than the repository", "Mais novo que o do repositório", "Más nuevo que el del repositorio"},
    {" (newer installed version is %1)", " (a versão instalada, mais nova, é %1)", " (la versión instalada, más nueva, es %1)"},
    {"Locked", "Bloqueado", "Bloqueado"},
    {"Not installed", "Não instalado", "No instalado"},
    {"Installed packages", "Instalados", "Instalados"},
    {"Repository", "Repositório", "Repositorio"},

    {"Info", "Informação", "Información"},
    {"Files", "Arquivos", "Archivos"},
    {"Output", "Saída", "Salida"},
    {"Help", "Ajuda", "Ayuda"},
    {"Installed version", "Versão instalada", "Versión instalada"},
    {"Origin", "Origem", "Origen"},
    {"URL", "URL", "URL"},
    {"Licenses", "Licenças", "Licencias"},
    {"Maintainer", "Mantenedor", "Mantenedor"},
    {"Architecture", "Arquitetura", "Arquitectura"},
    {"Installed size", "Tamanho instalado", "Tamaño instalado"},
    {"Depends on", "Depende de", "Depende de"},
    {"Required by", "Requerido por", "Requerido por"},
    {"Options", "Opções", "Opciones"},
    {"Select a package to see details.", "Selecione um pacote para ver os detalhes.", "Seleccione un paquete para ver los detalles."},
    {"Loading…", "Carregando…", "Cargando…"},
    {"Files are listed for installed packages only.", "Os arquivos só são listados para pacotes instalados.", "Los archivos solo se listan para paquetes instalados."},
    {"This package has too many files to be listed.", "Este pacote tem arquivos demais para listar.", "Este paquete tiene demasiados archivos para listar."},
    {"No files.", "Nenhum arquivo.", "Ningún archivo."},
    {"No packages.", "Nenhum pacote.", "Ningún paquete."},

    {"Ready.", "Pronto.", "Listo."},
    {"Listing installed packages…", "Listando os pacotes instalados…", "Listando los paquetes instalados…"},
    {"Listing repository packages…", "Listando os pacotes dos repositórios…", "Listando los paquetes de los repositorios…"},
    {"%1 package(s) installed.", "%1 pacote(s) instalado(s).", "%1 paquete(s) instalado(s)."},
    {"%1 package(s) in the repositories.", "%1 pacote(s) nos repositórios.", "%1 paquete(s) en los repositorios."},
    {"Outdated packages (%1):", "Pacotes desatualizados (%1):", "Paquetes desactualizados (%1):"},
    {"%1 package(s)", "%1 pacote(s)", "%1 paquete(s)"},
    {"%1 outdated", "%1 desatualizado(s)", "%1 desactualizado(s)"},
    {"ERROR: %1", "ERRO: %1", "ERROR: %1"},
    {"WARNING: %1", "AVISO: %1", "AVISO: %1"},
    {"Wait for the current operation to finish.", "Aguarde o fim da operação em andamento.", "Espere a que termine la operación en curso."},
    {"This tab arrives in a later version.", "Esta aba chega numa versão futura.", "Esta pestaña llega en una versión futura."},
    {"Filter by name…", "Filtrar por nome…", "Filtrar por nombre…"},
    {"Sort by", "Ordenar por", "Ordenar por"},
    {"(pkg order)", "(ordem do pkg)", "(orden de pkg)"},

    {"Refresh", "Atualizar", "Actualizar"},
    {"Sort", "Ordenar", "Ordenar"},
    {"Find", "Buscar", "Buscar"},
    {"Tabs", "Abas", "Pestañas"},
    {"Exit", "Sair", "Salir"},
    {"Move", "Mover", "Mover"},
    {"Refresh the package list", "Atualizar a lista de pacotes", "Actualizar la lista de paquetes"},
    {"Find a package in the package list", "Buscar um pacote na lista", "Buscar un paquete en la lista"},
    {"Go back to the package list", "Voltar à lista de pacotes", "Volver a la lista de paquetes"},
    {"Switch between installed and repository packages", "Alternar entre os pacotes instalados e os do repositório", "Alternar entre los paquetes instalados y los del repositorio"},
    {"Choose the sort column (again: reverse; (pkg order): original order)", "Escolher a coluna de ordenação (de novo: inverter; (ordem do pkg): ordem original)", "Elegir la columna de orden (otra vez: invertir; (orden de pkg): orden original)"},
    {"Tabs Info, Files, Output and Help", "Abas Informação, Arquivos, Saída e Ajuda", "Pestañas Información, Archivos, Salida y Ayuda"},
    {"Show this help page", "Mostrar esta página de ajuda", "Mostrar esta página de ayuda"},
    {"Show the Info tab of the selected package", "Mostrar a aba Informação do pacote selecionado", "Mostrar la pestaña Información del paquete seleccionado"},
    {"Scroll the tab with the arrow keys", "Rolar a aba com as setas", "Desplazar la pestaña con las flechas"},
    {"Click a column header to sort", "Clique no cabeçalho de uma coluna para ordenar", "Haga clic en el encabezado de una columna para ordenar"},
    {"This version is read-only: installing and removing packages arrive in v0.2.0.", "Esta versão é só de leitura: instalar e remover pacotes chega na v0.2.0.", "Esta versión es de solo lectura: instalar y eliminar paquetes llega en la v0.2.0."},
};

const Catalog &catalog(const std::string &loc)
{
    static const Catalog pt = [] {
        Catalog c;
        for (const auto &e : kEntries)
            c[e.en] = e.pt;
        return c;
    }();
    static const Catalog es = [] {
        Catalog c;
        for (const auto &e : kEntries)
            c[e.en] = e.es;
        return c;
    }();
    static const Catalog none;
    if (loc == "pt_BR")
        return pt;
    if (loc == "es_ES")
        return es;
    return none;
}

} // namespace

void initFromEnvironment()
{
    for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char *v = std::getenv(var);
        if (!v || !*v)
            continue;
        const std::string value(v);
        if (value.rfind("pt", 0) == 0)
            setLocale("pt_BR");
        else if (value.rfind("es", 0) == 0)
            setLocale("es_ES");
        else
            setLocale("en_US");
        return;
    }
    setLocale("en_US");
}

void setLocale(const std::string &loc)
{
    g_locale = (loc == "pt_BR" || loc == "es_ES") ? loc : "en_US";
}

std::string locale()
{
    return g_locale;
}

std::vector<std::string> availableLocales()
{
    return {"en_US", "pt_BR", "es_ES"};
}

std::string tr(const char *key)
{
    const Catalog &c = catalog(g_locale);
    const auto it = c.find(key);
    return it == c.end() ? std::string(key) : it->second;
}

std::string trf(const char *key, std::initializer_list<std::string> args)
{
    const std::string pattern = tr(key);
    const std::vector<std::string> values(args);
    std::string out;
    out.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '%' && i + 1 < pattern.size() && pattern[i + 1] >= '1' && pattern[i + 1] <= '9') {
            const size_t n = static_cast<size_t>(pattern[i + 1] - '1');
            if (n < values.size()) {
                out += values[n];
                ++i;
                continue;
            }
        }
        out += pattern[i];
    }
    return out;
}

} // namespace i18n
