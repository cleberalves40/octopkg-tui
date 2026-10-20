// SPDX-License-Identifier: GPL-2.0-or-later
//
// Integração com o pkg(8): porte em C++ padrão da leitura de pacotes do
// OctoPkg (classes Package e UnixCommand), sem depender do Qt.
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#ifndef OCTOPKG_TUI_PKG_H
#define OCTOPKG_TUI_PKG_H

#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace octo {

/** Mesma ordem do PackageStatus do OctoPkg, que é a ordem da coluna de estado. */
enum class Status { Locked = 0, Installed, NotInstalled, Outdated, Newer };

struct Package {
    std::string name;
    std::string version;            // do repositório, na visão Repositório
    std::string origin;             // categoria/porte
    std::string repository;
    std::string comment;
    std::int64_t size = 0;          // tamanho instalado (flatsize), em bytes
    std::int64_t installedOn = 0;   // UNIX; 0 se não instalado
    bool installed = false;
    bool locked = false;
    bool automatic = false;         // instalado como dependência
    int requiredBy = 0;             // quantos pacotes dependem dele
    std::string installedVersion;   // versão instalada, se installed
    std::string newVersion;         // versão disponível, se desatualizado
    Status status = Status::NotInstalled;
};

struct Upgrade {
    std::string oldVersion;
    std::string newVersion;
};

/** Aba Informação e aba Arquivos de um pacote. */
struct Details {
    std::string name;
    std::string version;
    std::string origin;
    std::string repository;
    std::string maintainer;
    std::string www;
    std::string arch;
    std::string comment;
    std::string description;
    std::int64_t size = 0;
    std::int64_t installedOn = 0;
    bool installed = false;
    std::vector<std::string> licenses;
    std::vector<std::string> dependencies;   // nome-versão
    std::vector<std::string> requiredBy;     // nome-versão; só instalados
    std::vector<std::string> options;        // OPÇÃO=on|off
    std::vector<std::string> files;          // só instalados
    bool filesTruncated = false;
};

/**
 * Prévia de uma transação: a saída de "pkg install -n", "pkg remove -n" ou
 * "pkg upgrade -n", como o Package::getTargetUpgradeList e o
 * getTargetRemovalList do OctoPkg a leem.
 */
struct TransactionPreview {
    struct Section {
        std::string action;              // INSTALLED, UPGRADED, REINSTALLED, DOWNGRADED, REMOVED...
        std::vector<std::string> items;  // como o pkg as imprime: "curl: 8.9.1 -> 8.10.0"
    };
    std::vector<Section> sections;
    std::string downloadSize;            // "3 MiB", de "3 MiB to be downloaded."
    std::string spaceChange;             // "The operation will free 22 KiB."

    int count() const;
    /** Itens das seções de action (vazio: todas as que não são REMOVED). */
    std::vector<std::string> items(const std::string &action) const;
    std::vector<std::string> removed() const { return items("REMOVED"); }
};

/** Função que recebe a saída de uma operação à medida que ela roda. */
using OutputFn = std::function<void(const std::string &chunk)>;

// --- Leitura da saída (funções puras, testadas sem o pkg) ---

/** Linhas de "pkg query -a" com o formato de Manager::listInstalled(). */
std::vector<Package> parseInstalled(const std::string &output);
/** Linhas de "pkg rquery -a" com o formato de Manager::listRemote(). */
std::vector<Package> parseRemote(const std::string &output);
/**
 * Pacotes a atualizar na saída de "pkg upgrade -n": a seção "Installed
 * packages to be UPGRADED:", como o Package::getOutdatedStringList do OctoPkg.
 */
std::map<std::string, Upgrade> parseUpgradeDryRun(const std::string &output);
/** Primeiro registro da consulta de detalhes (campos e descrição). */
Details parseDetails(const std::string &output);

/** Marca desatualizados (e bloqueados) na lista de instalados. */
void applyUpgrades(std::vector<Package> &installed, const std::map<std::string, Upgrade> &upgrades);
/**
 * Visão Repositório: os pacotes dos repositórios, com o estado dos
 * instalados, mais os instalados que nenhum repositório tem. Pacotes de
 * mesmo nome em mais de um repositório aparecem uma vez (o primeiro).
 */
std::vector<Package> mergeRemote(const std::vector<Package> &remote, const std::vector<Package> &installed);

TransactionPreview parseTransaction(const std::string &output);
/** Nome do pacote num item da prévia ("curl: 8.9.1 -> 8.10.0" → "curl"; "py311-x-1.0 [r]" → "py311-x"). */
std::string itemName(const std::string &item);

/** Tamanho legível, como o Package::kbytesToSize do OctoPkg ("12.34 MiB"). */
std::string formatSize(std::int64_t bytes);
/** Data e hora local "AAAA-MM-DD HH:MM"; vazio para 0. */
std::string formatTime(std::int64_t unixTime);

/**
 * Nome aceito como argumento do pkg: não vazio, sem espaços nem caracteres de
 * controle e sem "-" inicial (seria lido como opção).
 */
bool validPackageName(const std::string &name);

/** Pacotes que o OctoPkg não deixa remover (Package::isForbidden). */
bool isForbidden(const std::string &name);
/** Nome de Boot Environment do OctoPkg para uma transação: "AAMMDD-hhmmss". */
std::string bootEnvironmentName(std::time_t when);

class Manager
{
public:
    /**
     * pkgPath: executável no lugar do pkg(8); vazio: /usr/local/sbin/pkg ou o
     * do PATH. elevator: comando que eleva privilégios nas operações ("doas",
     * "sudo"), ou vazio para executá-las diretamente (como root). bectlPath:
     * o bectl(8) dos Boot Environments; vazio: /sbin/bectl.
     */
    explicit Manager(std::string pkgPath = std::string(), std::string elevator = std::string(),
                     std::string bectlPath = std::string());

    std::vector<Package> listInstalled() const;
    std::vector<Package> listRemote() const;
    /**
     * Desatualizados, sem atualizar o catálogo (pkg upgrade -n -U). Uma falha
     * aqui não impede a listagem: devolve vazio e preenche warning.
     */
    std::map<std::string, Upgrade> listOutdated(std::string *warning = nullptr) const;
    Details details(const std::string &name, bool installed) const;
    /** pkg -v */
    std::string version() const;

    // --- Prévias, sem privilégios (dry-run, sem atualizar o catálogo) ---
    TransactionPreview previewInstall(const std::vector<std::string> &names) const;
    TransactionPreview previewRemove(const std::vector<std::string> &names) const;
    TransactionPreview previewUpgrade() const;

    // --- Operações, com elevação; os comandos são os do OctoPkg ---
    void install(const std::vector<std::string> &names, const OutputFn &out = nullptr) const;   // install -f -y
    void remove(const std::vector<std::string> &names, const OutputFn &out = nullptr) const;    // remove -R -y
    void upgrade(const OutputFn &out = nullptr) const;                                          // upgrade -y
    void update(const OutputFn &out = nullptr) const;                                           // update -f
    void clean(const OutputFn &out = nullptr) const;                                            // clean -a -y
    void lock(const std::string &name, const OutputFn &out = nullptr) const;                    // lock -y
    void unlock(const std::string &name, const OutputFn &out = nullptr) const;                  // unlock -y
    /** bectl create nome: o Boot Environment antes da transação, como no OctoPkg. */
    void createBootEnvironment(const std::string &name, const OutputFn &out = nullptr) const;
    /** O sistema usa Boot Environments ("bectl check")? */
    bool bootEnvironmentsAvailable() const;

    /** Linha de comando de uma operação, para a aba Saída ("doas pkg install -f -y curl"). */
    std::string commandLine(const std::vector<std::string> &args, bool bectl = false) const;

    const std::string &pkgPath() const { return m_pkgPath; }
    const std::string &elevator() const { return m_elevator; }
    const std::string &bectlPath() const { return m_bectlPath; }

    /** Limite da aba Arquivos, como no OctoPkg (2 MiB de saída). */
    static constexpr std::size_t kMaxFilesOutput = 2097152;

private:
    std::string query(const std::vector<std::string> &args) const;
    TransactionPreview dryRun(const std::vector<std::string> &args) const;
    void privileged(const std::vector<std::string> &args, const OutputFn &out, bool bectl = false) const;

    std::string m_pkgPath;
    std::string m_elevator;
    std::string m_bectlPath;
};

} // namespace octo

#endif
