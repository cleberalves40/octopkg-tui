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
    {"Filter by name…", "Filtrar por nome…", "Filtrar por nombre…"},
    {"Sort by", "Ordenar por", "Ordenar por"},
    {"(pkg order)", "(ordem do pkg)", "(orden de pkg)"},

    {"Refresh", "Atualizar", "Actualizar"},
    {"Sort", "Ordenar", "Ordenar"},
    {"Exit", "Sair", "Salir"},
    {"Move", "Mover", "Mover"},
    {"Refresh the package list", "Atualizar a lista de pacotes", "Actualizar la lista de paquetes"},
    {"Find a package in the package list", "Buscar um pacote na lista", "Buscar un paquete en la lista"},
    {"Go back to the package list", "Voltar à lista de pacotes", "Volver a la lista de paquetes"},
    {"Switch between installed and repository packages", "Alternar entre os pacotes instalados e os do repositório", "Alternar entre los paquetes instalados y los del repositorio"},
    {"Choose the sort column (again: reverse; (pkg order): original order)", "Escolher a coluna de ordenação (de novo: inverter; (ordem do pkg): ordem original)", "Elegir la columna de orden (otra vez: invertir; (orden de pkg): orden original)"},
    {"Show this help page", "Mostrar esta página de ajuda", "Mostrar esta página de ayuda"},
    {"Show the Info tab of the selected package", "Mostrar a aba Informação do pacote selecionado", "Mostrar la pestaña Información del paquete seleccionado"},
    {"Scroll the tab with the arrow keys", "Rolar a aba com as setas", "Desplazar la pestaña con las flechas"},
    {"Click a column header to sort", "Clique no cabeçalho de uma coluna para ordenar", "Haga clic en el encabezado de una columna para ordenar"},

    {"Actions", "Ações", "Acciones"},
    {"To be installed", "A instalar", "A instalar"},
    {"To be removed", "A remover", "A eliminar"},
    {"No packages marked. Use + to install and - to remove.", "Nenhum pacote marcado. Use + para instalar e - para remover.", "Ningún paquete marcado. Use + para instalar y - para eliminar."},
    {"Alt+M: apply   Ctrl+E: cancel", "Alt+M: aplicar   Ctrl+E: cancelar", "Alt+M: aplicar   Ctrl+E: cancelar"},
    {"%1 is locked.", "%1 está bloqueado.", "%1 está bloqueado."},
    {"%1 cannot be removed.", "%1 não pode ser removido.", "%1 no se puede eliminar."},
    {"Only installed packages can be removed.", "Só pacotes instalados podem ser removidos.", "Solo se pueden eliminar paquetes instalados."},
    {"Select an installed package.", "Selecione um pacote instalado.", "Seleccione un paquete instalado."},
    {"No packages selected.", "Nenhum pacote selecionado.", "Ningún paquete seleccionado."},
    {"Collecting transaction data…", "Coletando os dados da transação…", "Recopilando los datos de la transacción…"},
    {"The following package needs to be retrieved", "O seguinte pacote precisa ser obtido", "El siguiente paquete debe obtenerse"},
    {"The following %1 packages need to be retrieved", "Os seguintes %1 pacotes precisam ser obtidos", "Los siguientes %1 paquetes deben obtenerse"},
    {"The following package will be removed", "O seguinte pacote será removido", "El siguiente paquete será eliminado"},
    {"The following %1 packages will be removed", "Os seguintes %1 pacotes serão removidos", "Los siguientes %1 paquetes serán eliminados"},
    {"… and %1 more (see the Output tab)", "… e mais %1 (veja a aba Saída)", "… y %1 más (vea la pestaña Salida)"},
    {"Total download size: %1", "Tamanho total do download: %1", "Tamaño total de la descarga: %1"},
    {"Nothing to do: the marked packages are already in this state.", "Nada a fazer: os pacotes marcados já estão nesse estado.", "Nada que hacer: los paquetes marcados ya están en ese estado."},
    {"Confirm?", "Confirma?", "¿Confirma?"},
    {"Confirmation", "Confirmação", "Confirmación"},
    {"Warning", "Aviso", "Aviso"},
    {"Transaction:", "Transação:", "Transacción:"},
    {"Create a boot environment before the transaction", "Criar um boot environment antes da transação", "Crear un boot environment antes de la transacción"},
    {"Name: %1", "Nome: %1", "Nombre: %1"},
    {"Installing selected packages...", "Instalando os pacotes selecionados...", "Instalando los paquetes seleccionados..."},
    {"Removing selected packages...", "Removendo os pacotes selecionados...", "Eliminando los paquetes seleccionados..."},
    {"Installing updates...", "Instalando as atualizações...", "Instalando las actualizaciones..."},
    {"Synchronizing the package database…", "Sincronizando o banco de dados de pacotes…", "Sincronizando la base de datos de paquetes…"},
    {"Cleaning the local cache...", "Limpando o cache local...", "Limpiando la caché local..."},
    {"Locking…", "Bloqueando…", "Bloqueando…"},
    {"Unlocking…", "Desbloqueando…", "Desbloqueando…"},
    {"Command finished OK!", "Comando concluído com sucesso!", "¡Comando terminado correctamente!"},
    {"Details in the Output tab.", "Detalhes na aba Saída.", "Detalles en la pestaña Salida."},
    {"Boot environment: %1", "Boot environment: %1", "Boot environment: %1"},
    {"Installed: %1", "Instalados: %1", "Instalados: %1"},
    {"Removed: %1", "Removidos: %1", "Eliminados: %1"},
    {"System upgrade", "Atualização do sistema", "Actualización del sistema"},
    {"There are no new updates available!", "Não há novas atualizações disponíveis!", "¡No hay nuevas actualizaciones disponibles!"},
    {"Clean the local cache", "Limpar o cache local", "Limpiar la caché local"},
    {"Do you really want to clean the local package cache?", "Deseja realmente limpar o cache local de pacotes?", "¿Desea realmente limpiar la caché local de paquetes?"},
    {"Running: %1", "Executando: %1", "Ejecutando: %1"},
    {"%1 may ask for your password.", "O %1 pode pedir a sua senha.", "%1 puede pedir su contraseña."},
    {"'%1' not found. Install it (pkg install %1) or run octopkg-tui as root.", "'%1' não encontrado. Instale-o (pkg install %1) ou execute o octopkg-tui como root.", "'%1' no encontrado. Instálelo (pkg install %1) o ejecute octopkg-tui como root."},
    {"Invalid boot environment name: %1", "Nome de Boot Environment inválido: %1", "Nombre de Boot Environment no válido: %1"},
    {"Error", "Erro", "Error"},

    {"OK", "OK", "Aceptar"},
    {"Cancel", "Cancelar", "Cancelar"},
    {"Yes", "Sim", "Sí"},
    {"No", "Não", "No"},
    {"Tab / ↑ ↓: field   Space: check   ← →: choose   Enter: confirm   Esc: cancel", "Tab / ↑ ↓: campo   Espaço: marcar   ← →: escolher   Enter: confirmar   Esc: cancelar", "Tab / ↑ ↓: campo   Espacio: marcar   ← →: elegir   Enter: confirmar   Esc: cancelar"},
    {"File", "Arquivo", "Archivo"},
    {"Menu", "Menu", "Menú"},
    {"Mark", "Marcar", "Marcar"},
    {"Apply", "Aplicar", "Aplicar"},
    {"Check updates", "Procurar atualizações", "Buscar actualizaciones"},
    {"Install updates", "Instalar as atualizações", "Instalar las actualizaciones"},
    {"Mark for installation", "Marcar para instalar", "Marcar para instalar"},
    {"Mark for removal", "Marcar para remover", "Marcar para eliminar"},
    {"Lock", "Bloquear", "Bloquear"},
    {"Unlock", "Desbloquear", "Desbloquear"},
    {"Keyboard shortcuts", "Atalhos de teclado", "Atajos de teclado"},
    {"About", "Sobre", "Acerca de"},
    {"Based on OctoPkg, © Alexandre Albuquerque Arnt", "Baseado no OctoPkg, © Alexandre Albuquerque Arnt", "Basado en OctoPkg, © Alexandre Albuquerque Arnt"},
    {"This tab is not available yet.", "Esta aba ainda não está disponível.", "Esta pestaña aún no está disponible."},
    {"Mark the selected package for installation", "Marcar o pacote selecionado para instalar", "Marcar el paquete seleccionado para instalar"},
    {"Mark the selected package for removal", "Marcar o pacote selecionado para remover", "Marcar el paquete seleccionado para eliminar"},
    {"Apply: install and remove the marked packages", "Aplicar: instalar e remover os pacotes marcados", "Aplicar: instalar y eliminar los paquetes marcados"},
    {"Cancel: clear the marks", "Cancelar: desfazer as marcas", "Cancelar: deshacer las marcas"},
    {"Check updates (pkg update)", "Procurar atualizações (pkg update)", "Buscar actualizaciones (pkg update)"},
    {"Install updates (pkg upgrade)", "Instalar as atualizações (pkg upgrade)", "Instalar las actualizaciones (pkg upgrade)"},
    {"Menus File, Actions and Help", "Menus Arquivo, Ações e Ajuda", "Menús Archivo, Acciones y Ayuda"},
    {"Tabs Info, Files, Actions, Output and Help", "Abas Informação, Arquivos, Ações, Saída e Ajuda", "Pestañas Información, Archivos, Acciones, Salida y Ayuda"},

    {"Preferences", "Preferências", "Preferencias"},
    {"Preferences…", "Preferências…", "Preferencias…"},
    {"Preferences: theme, language and icons", "Preferências: tema, idioma e ícones", "Preferencias: tema, idioma e iconos"},
    {"Preferences saved.", "Preferências salvas.", "Preferencias guardadas."},
    {"Could not save %1", "Não foi possível salvar %1", "No se pudo guardar %1"},
    {"Theme:", "Tema:", "Tema:"},
    {"Language:", "Idioma:", "Idioma:"},
    {"Icons:", "Ícones:", "Iconos:"},
    {"System", "Sistema", "Sistema"},
    {"System default", "Padrão do sistema", "Predeterminado del sistema"},
    {"System (terminal colors)", "Sistema (cores do terminal)", "Sistema (colores del terminal)"},
    {"Nerd Fonts need a Nerd Font in the terminal.", "Nerd Fonts exige uma fonte Nerd no terminal.", "Nerd Fonts requiere una fuente Nerd en el terminal."},
    {"Theme: %1", "Tema: %1", "Tema: %1"},
    {"Next theme", "Próximo tema", "Siguiente tema"},
    {"Find:", "Buscar:", "Buscar:"},
    {"Not found", "Não encontrado", "No encontrado"},
    {"Enter/F3: next  Shift+F3: previous  Esc: close", "Enter/F3: próximo  Shift+F3: anterior  Esc: fechar", "Enter/F3: siguiente  Shift+F3: anterior  Esc: cerrar"},
    {"Find text in the Files, Output and Help tabs (F3: next, Shift+F3: previous)", "Buscar texto nas abas Arquivos, Saída e Ajuda (F3: próximo, Shift+F3: anterior)", "Buscar texto en las pestañas Archivos, Salida y Ayuda (F3: siguiente, Shift+F3: anterior)"},
    {"Find works in the Files, Output and Help tabs.", "A busca funciona nas abas Arquivos, Saída e Ajuda.", "La búsqueda funciona en las pestañas Archivos, Salida y Ayuda."},
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
