// SPDX-License-Identifier: GPL-2.0-or-later
//
// Porte da leitura de pacotes do OctoPkg (Package, UnixCommand).
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#include "pkg.h"
#include "i18n.h"
#include "process.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace octo {

namespace {

// O rquery de todos os pacotes leva alguns segundos num aarch64 emulado.
constexpr int kTimeoutMs = 120000;

// Separadores dos formatos de consulta: TAB entre os campos e, nos detalhes,
// RS (0x1e) no fim de cada registro, porque a descrição tem várias linhas.
constexpr char kSep = '\t';
constexpr char kRecordEnd = '\x1e';

// Visão Instalados: pkg query -a
const char kInstalledFormat[] = "%n\t%v\t%o\t%sb\t%t\t%k\t%a\t%#r\t%R\t%c";
// Visão Repositório: pkg rquery -a
const char kRemoteFormat[] = "%n\t%v\t%o\t%sb\t%R\t%c";
// Detalhes: o rquery não tem a data de instalação (%t), que vira 0.
const char kDetailsInstalled[] = "%n\t%v\t%o\t%R\t%m\t%w\t%q\t%sb\t%t\t%c\t%e\x1e";
const char kDetailsRemote[] = "%n\t%v\t%o\t%R\t%m\t%w\t%q\t%sb\t0\t%c\t%e\x1e";

[[noreturn]] void fail(const std::string &msg)
{
    throw std::runtime_error(msg);
}

std::string trim(const std::string &s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return std::string();
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> split(const std::string &s, char sep)
{
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        const size_t pos = s.find(sep, start);
        parts.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }
    return parts;
}

std::vector<std::string> lines(const std::string &s)
{
    std::vector<std::string> out;
    for (auto &l : split(s, '\n')) {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (!l.empty())
            out.push_back(l);
    }
    return out;
}

std::string join(const std::vector<std::string> &v, const std::string &sep)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
        out += (i ? sep : std::string()) + v[i];
    return out;
}

std::int64_t toInt(const std::string &s)
{
    char *end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    return (end && end != s.c_str()) ? static_cast<std::int64_t>(v) : 0;
}

// Os últimos campos voltam a ser um só: o comentário pode ter TAB.
std::vector<std::string> fields(const std::string &line, size_t count)
{
    std::vector<std::string> parts = split(line, kSep);
    if (parts.size() > count) {
        std::vector<std::string> rest(parts.begin() + static_cast<long>(count) - 1, parts.end());
        parts.resize(count - 1);
        parts.push_back(join(rest, "\t"));
    }
    return parts;
}

std::string defaultBectl()
{
    return ::access("/sbin/bectl", X_OK) == 0 ? std::string("/sbin/bectl") : findExecutable("bectl");
}

void requireNames(const std::vector<std::string> &names)
{
    if (names.empty())
        fail(i18n::tr("No packages selected."));
    for (const auto &n : names)
        if (!validPackageName(n))
            fail(i18n::trf("Invalid package name: %1", {n}));
}

// Mesmas variáveis das consultas, mas com UTF-8: a saída vai para a tela, e
// nomes de arquivos podem ter acentos.
const Environment kRunEnv{{"LANG", "C.UTF-8"}, {"LC_MESSAGES", "C"}, {"LC_ALL", "C.UTF-8"}};

std::string defaultPkg()
{
    // /usr/sbin/pkg é o bootstrap, que repassa ao /usr/local/sbin/pkg.
    if (::access("/usr/local/sbin/pkg", X_OK) == 0)
        return "/usr/local/sbin/pkg";
    const std::string found = findExecutable("pkg");
    return found.empty() ? std::string("pkg") : found;
}

} // namespace

std::vector<Package> parseInstalled(const std::string &output)
{
    std::vector<Package> result;
    for (const auto &line : lines(output)) {
        const auto f = fields(line, 10);
        if (f.size() < 10 || f[0].empty())
            continue;
        Package p;
        p.name = f[0];
        p.version = f[1];
        p.installedVersion = f[1];
        p.origin = f[2];
        p.size = toInt(f[3]);
        p.installedOn = toInt(f[4]);
        p.locked = f[5] == "1";
        p.automatic = f[6] == "1";
        p.requiredBy = static_cast<int>(toInt(f[7]));
        p.repository = f[8];
        p.comment = f[9];
        p.installed = true;
        p.status = p.locked ? Status::Locked : Status::Installed;
        result.push_back(std::move(p));
    }
    return result;
}

std::vector<Package> parseRemote(const std::string &output)
{
    std::vector<Package> result;
    for (const auto &line : lines(output)) {
        const auto f = fields(line, 6);
        if (f.size() < 6 || f[0].empty())
            continue;
        Package p;
        p.name = f[0];
        p.version = f[1];
        p.origin = f[2];
        p.size = toInt(f[3]);
        p.repository = f[4];
        p.comment = f[5];
        result.push_back(std::move(p));
    }
    return result;
}

std::map<std::string, Upgrade> parseUpgradeDryRun(const std::string &output)
{
    // "Installed packages to be UPGRADED:" seguido de linhas com TAB:
    //     curl: 8.9.1 -> 8.10.0 [FreeBSD]
    // A seção termina na próxima linha "... packages to be ...".
    std::map<std::string, Upgrade> result;
    bool inUpgrade = false;
    for (const auto &line : lines(output)) {
        if (line.find("packages to be") != std::string::npos) {
            inUpgrade = line.find("Installed packages to be UPGRADED:") != std::string::npos;
            continue;
        }
        if (!inUpgrade || line.find('\t') == std::string::npos)
            continue;
        std::vector<std::string> parts;
        for (const auto &p : split(trim(line), ' '))
            if (!p.empty())
                parts.push_back(p);
        if (parts.size() < 4 || parts[0].size() < 2 || parts[0].back() != ':' || parts[2] != "->")
            continue;
        result[parts[0].substr(0, parts[0].size() - 1)] = {parts[1], parts[3]};
    }
    return result;
}

Details parseDetails(const std::string &output)
{
    Details d;
    const std::string record = output.substr(0, output.find(kRecordEnd));
    const auto f = fields(record, 11);
    if (f.size() < 11)
        return d;
    d.name = trim(f[0]);
    d.version = f[1];
    d.origin = f[2];
    d.repository = f[3];
    d.maintainer = f[4];
    d.www = f[5];
    d.arch = f[6];
    d.size = toInt(f[7]);
    d.installedOn = toInt(f[8]);
    d.comment = f[9];
    d.description = trim(f[10]);
    return d;
}

int TransactionPreview::count() const
{
    int n = 0;
    for (const auto &sec : sections)
        n += static_cast<int>(sec.items.size());
    return n;
}

std::vector<std::string> TransactionPreview::items(const std::string &action) const
{
    std::vector<std::string> out;
    for (const auto &sec : sections)
        if (action.empty() ? sec.action != "REMOVED" : sec.action == action)
            out.insert(out.end(), sec.items.begin(), sec.items.end());
    return out;
}

TransactionPreview parseTransaction(const std::string &output)
{
    // Seções "<...> packages to be <AÇÃO>:" seguidas de itens com TAB; fora
    // delas, o tamanho a baixar e a mudança de espaço em disco.
    TransactionPreview p;
    TransactionPreview::Section *current = nullptr;
    for (const auto &line : lines(output)) {
        const size_t tb = line.find("packages to be ");
        if (tb != std::string::npos && line.back() == ':') {
            const size_t start = tb + std::string("packages to be ").size();
            p.sections.push_back({line.substr(start, line.size() - 1 - start), {}});
            current = &p.sections.back();
            continue;
        }
        if (line[0] == '\t' && current) {
            current->items.push_back(trim(line));
            continue;
        }
        current = nullptr;
        const size_t dl = line.find(" to be downloaded.");
        if (dl != std::string::npos)
            p.downloadSize = trim(line.substr(0, dl));
        else if (line.find("The operation will ") == 0)
            p.spaceChange = trim(line);
    }
    // Seções vazias (o pkg não as imprime, mas por garantia) não contam.
    std::vector<TransactionPreview::Section> kept;
    for (auto &sec : p.sections)
        if (!sec.items.empty())
            kept.push_back(std::move(sec));
    p.sections = std::move(kept);
    return p;
}

std::string itemName(const std::string &item)
{
    // "nome: versão..." (instalar, atualizar, remover) ou "nome-versão [repo]
    // (motivo)" (reinstalar), como o getTargetRemovalList do OctoPkg separa.
    const std::string t = trim(item);
    const size_t colon = t.find(": ");
    if (colon != std::string::npos)
        return t.substr(0, colon);
    const std::string first = t.substr(0, t.find(' '));
    const size_t dash = first.rfind('-');
    return dash == std::string::npos || dash == 0 ? first : first.substr(0, dash);
}

bool isForbidden(const std::string &name)
{
    return name == "pkg" || name == "xorg";
}

std::string bootEnvironmentName(std::time_t when)
{
    std::tm tm {};
    ::localtime_r(&when, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%y%m%d-%H%M%S", &tm);
    return buf;
}

void applyUpgrades(std::vector<Package> &installed, const std::map<std::string, Upgrade> &upgrades)
{
    for (auto &p : installed) {
        const auto it = upgrades.find(p.name);
        p.newVersion = it == upgrades.end() ? std::string() : it->second.newVersion;
        // Como no OctoPkg, o bloqueio prevalece sobre a atualização.
        if (p.locked)
            p.status = Status::Locked;
        else
            p.status = p.newVersion.empty() ? Status::Installed : Status::Outdated;
    }
}

std::vector<Package> mergeRemote(const std::vector<Package> &remote, const std::vector<Package> &installed)
{
    std::map<std::string, const Package *> byName;
    for (const auto &p : installed)
        byName[p.name] = &p;

    std::vector<Package> result;
    std::set<std::string> seen;
    for (const auto &r : remote) {
        if (!seen.insert(r.name).second)
            continue;
        Package p = r;
        const auto it = byName.find(r.name);
        if (it != byName.end()) {
            const Package &i = *it->second;
            p.installed = true;
            p.installedVersion = i.installedVersion;
            p.installedOn = i.installedOn;
            p.locked = i.locked;
            p.automatic = i.automatic;
            p.requiredBy = i.requiredBy;
            p.newVersion = i.newVersion;
            if (i.locked)
                p.status = Status::Locked;
            else if (!i.newVersion.empty())
                p.status = Status::Outdated;
            else if (i.installedVersion != r.version)
                p.status = Status::Newer;   // o pkg upgrade não o lista: o repositório é mais antigo
            else
                p.status = Status::Installed;
        }
        result.push_back(std::move(p));
    }
    for (const auto &i : installed)
        if (!seen.count(i.name))
            result.push_back(i);
    return result;
}

std::string formatSize(std::int64_t bytes)
{
    const double kb = static_cast<double>(bytes) / 1024.0;
    char buf[32];
    if (kb >= 1073741824.0)
        std::snprintf(buf, sizeof buf, "%.2f TiB", kb / 1073741824.0);
    else if (kb >= 1048576.0)
        std::snprintf(buf, sizeof buf, "%.2f GiB", kb / 1048576.0);
    else if (kb >= 1024.0)
        std::snprintf(buf, sizeof buf, "%.2f MiB", kb / 1024.0);
    else if (kb >= 1.0)
        std::snprintf(buf, sizeof buf, "%.2f KiB", kb);
    else
        std::snprintf(buf, sizeof buf, "%.2f Bytes", static_cast<double>(bytes));
    return buf;
}

std::string formatTime(std::int64_t unixTime)
{
    if (unixTime <= 0)
        return std::string();
    const std::time_t t = static_cast<std::time_t>(unixTime);
    std::tm tm {};
    ::localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

bool validPackageName(const std::string &name)
{
    if (name.empty() || name[0] == '-' || name.size() > 255)
        return false;
    for (unsigned char c : name)
        if (c <= ' ' || c == 0x7f)
            return false;
    return true;
}

Manager::Manager(std::string pkgPath, std::string elevator, std::string bectlPath)
    : m_pkgPath(pkgPath.empty() ? defaultPkg() : std::move(pkgPath))
    , m_elevator(std::move(elevator))
    , m_bectlPath(bectlPath.empty() ? defaultBectl() : std::move(bectlPath))
{
}

std::string Manager::query(const std::vector<std::string> &args) const
{
    std::vector<std::string> cmd{m_pkgPath};
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = join(cmd, " ");
    // Como o UnixCommand::performQuery do OctoPkg: saída em inglês, que é a
    // que o porte sabe ler.
    const ProcessResult r = runProcess(cmd, kTimeoutMs, {{"LANG", "C"}, {"LC_MESSAGES", "C"}, {"LC_ALL", "C"}});
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.timedOut)
        fail(i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kTimeoutMs), shown}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    if (r.status != 0) {
        std::string msg = trim(r.err).empty() ? trim(r.out) : trim(r.err);
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
    return r.out;
}

std::vector<Package> Manager::listInstalled() const
{
    return parseInstalled(query({"query", "-a", kInstalledFormat}));
}

std::vector<Package> Manager::listRemote() const
{
    return parseRemote(query({"rquery", "-U", "-a", kRemoteFormat}));
}

std::map<std::string, Upgrade> Manager::listOutdated(std::string *warning) const
{
    // "pkg upgrade -n" sai com 1 quando há o que atualizar: só a falha ao
    // executar conta como erro.
    const std::vector<std::string> cmd{m_pkgPath, "upgrade", "-n", "-U"};
    const ProcessResult r = runProcess(cmd, kTimeoutMs, {{"LANG", "C"}, {"LC_MESSAGES", "C"}, {"LC_ALL", "C"}});
    std::string problem;
    if (!r.started)
        problem = i18n::trf("Failed to start '%1': %2", {join(cmd, " "), r.startError});
    else if (r.timedOut)
        problem = i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kTimeoutMs), join(cmd, " ")});
    else if (r.crashed)
        problem = i18n::trf("Process crashed: %1", {join(cmd, " ")});
    else if (r.status != 0 && r.status != 1)
        problem = trim(r.err).empty() ? i18n::trf("Command failed with exit code %1", {std::to_string(r.status)})
                                      : trim(r.err);
    if (warning)
        *warning = problem;
    return problem.empty() ? parseUpgradeDryRun(r.out) : std::map<std::string, Upgrade>();
}

Details Manager::details(const std::string &name, bool installed) const
{
    if (!validPackageName(name))
        fail(i18n::trf("Invalid package name: %1", {name}));
    const std::string verb = installed ? "query" : "rquery";
    std::vector<std::string> base{verb};
    if (!installed)
        base.push_back("-U");
    const auto run = [&](const std::string &format) {
        std::vector<std::string> args = base;
        args.push_back(format);
        args.push_back(name);
        return query(args);
    };

    Details d = parseDetails(run(installed ? kDetailsInstalled : kDetailsRemote));
    if (d.name.empty())
        fail(i18n::trf("Package not found: %1", {name}));
    d.installed = installed;
    // Uma linha por item; com mais de um repositório, os itens se repetem.
    const auto list = [&](const std::string &format) {
        std::vector<std::string> out;
        std::set<std::string> seen;
        for (const auto &l : lines(run(format)))
            if (seen.insert(l).second)
                out.push_back(l);
        return out;
    };
    d.licenses = list("%L");
    d.dependencies = list("%dn-%dv");
    d.options = list("%Ok=%Ov");
    if (installed) {
        d.requiredBy = list("%rn-%rv");
        const std::string files = run("%Fp");
        if (files.size() > kMaxFilesOutput)
            d.filesTruncated = true;   // como no OctoPkg: pacote grande demais para a aba
        else
            d.files = lines(files);
    }
    return d;
}

std::string Manager::version() const
{
    return trim(query({"-v"}));
}

TransactionPreview Manager::dryRun(const std::vector<std::string> &args) const
{
    std::vector<std::string> cmd{m_pkgPath};
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = join(cmd, " ");
    const ProcessResult r = runProcess(cmd, kTimeoutMs, {{"LANG", "C"}, {"LC_MESSAGES", "C"}, {"LC_ALL", "C"}});
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.timedOut)
        fail(i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kTimeoutMs), shown}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    TransactionPreview p = parseTransaction(r.out);
    // Com -n, o pkg sai com 1 quando haveria o que fazer: só é erro sem prévia.
    if (r.status != 0 && !(r.status == 1 && p.count() > 0)) {
        std::string msg = trim(r.err).empty() ? trim(r.out) : trim(r.err);
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
    return p;
}

TransactionPreview Manager::previewInstall(const std::vector<std::string> &names) const
{
    requireNames(names);
    std::vector<std::string> args{"install", "-n", "-f", "-U"};
    args.insert(args.end(), names.begin(), names.end());
    return dryRun(args);
}

TransactionPreview Manager::previewRemove(const std::vector<std::string> &names) const
{
    requireNames(names);
    std::vector<std::string> args{"remove", "-n", "-R"};
    args.insert(args.end(), names.begin(), names.end());
    return dryRun(args);
}

TransactionPreview Manager::previewUpgrade() const
{
    return dryRun({"upgrade", "-n", "-U"});
}

std::string Manager::commandLine(const std::vector<std::string> &args, bool bectl) const
{
    std::vector<std::string> cmd;
    if (!m_elevator.empty())
        cmd.push_back(m_elevator);
    cmd.push_back(bectl ? m_bectlPath : m_pkgPath);
    cmd.insert(cmd.end(), args.begin(), args.end());
    return join(cmd, " ");
}

void Manager::privileged(const std::vector<std::string> &args, const OutputFn &out, bool bectl) const
{
    std::vector<std::string> cmd;
    if (!m_elevator.empty()) {
        const std::string tool = findExecutable(m_elevator);
        if (tool.empty())
            fail(i18n::trf("'%1' not found. Install it (pkg install %1) or run octopkg-tui as root.",
                           {m_elevator}));
        cmd.push_back(tool);
    }
    cmd.push_back(bectl ? m_bectlPath : m_pkgPath);
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = commandLine(args, bectl);
    // Sem limite de tempo: a espera inclui a senha do doas/sudo, e uma
    // atualização pode baixar centenas de megabytes.
    const ProcessResult r = runProcess(cmd, -1, kRunEnv, out);
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    if (r.status != 0) {
        std::string msg = trim(r.err);
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
}

void Manager::install(const std::vector<std::string> &names, const OutputFn &out) const
{
    requireNames(names);
    std::vector<std::string> args{"install", "-f", "-y"};
    args.insert(args.end(), names.begin(), names.end());
    privileged(args, out);
}

void Manager::remove(const std::vector<std::string> &names, const OutputFn &out) const
{
    requireNames(names);
    for (const auto &n : names)
        if (isForbidden(n))
            fail(i18n::trf("%1 cannot be removed.", {n}));
    std::vector<std::string> args{"remove", "-R", "-y"};
    args.insert(args.end(), names.begin(), names.end());
    privileged(args, out);
}

void Manager::upgrade(const OutputFn &out) const
{
    privileged({"upgrade", "-y"}, out);
}

void Manager::update(const OutputFn &out) const
{
    privileged({"update", "-f"}, out);
}

void Manager::clean(const OutputFn &out) const
{
    privileged({"clean", "-a", "-y"}, out);
}

void Manager::lock(const std::string &name, const OutputFn &out) const
{
    requireNames({name});
    privileged({"lock", "-y", name}, out);
}

void Manager::unlock(const std::string &name, const OutputFn &out) const
{
    requireNames({name});
    privileged({"unlock", "-y", name}, out);
}

void Manager::createBootEnvironment(const std::string &name, const OutputFn &out) const
{
    // Regras do libbe, como no bectl-tui: letras, dígitos e "_-.:", sem "-" inicial.
    bool ok = !name.empty() && name[0] != '-' && name.size() <= 255;
    for (unsigned char c : name)
        ok = ok && (std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':');
    if (!ok)
        fail(i18n::trf("Invalid boot environment name: %1", {name}));
    privileged({"create", name}, out, true);
}

bool Manager::bootEnvironmentsAvailable() const
{
    // Como o UnixCommand::isRootOnZFS do OctoPkg, mas com "bectl check": sai
    // com 0 quando o sistema usa Boot Environments. Não precisa de root.
    if (m_bectlPath.empty() || ::access(m_bectlPath.c_str(), X_OK) != 0)
        return false;
    const ProcessResult r = runProcess({m_bectlPath, "check"}, 10000);
    return r.started && !r.timedOut && r.status == 0;
}

} // namespace octo
