// SPDX-License-Identifier: GPL-2.0-or-later
//
// Porte do utils::retrieveDistroNews e do parseDistroNews do OctoPkg.
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#include "news.h"
#include "i18n.h"
#include "process.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace octo {

namespace {

constexpr int kFetchTimeoutMs = 60000;

std::string readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// mkdir -p do diretório de path.
void makeParentDirs(const std::string &path)
{
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos || slash == 0)
        return;
    const std::string dir = path.substr(0, slash);
    size_t pos = 0;
    while ((pos = dir.find('/', pos + 1)) != std::string::npos)
        ::mkdir(dir.substr(0, pos).c_str(), 0755);
    ::mkdir(dir.c_str(), 0755);
}

void appendUtf8(std::string &out, unsigned long cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// &amp; &lt; &gt; &quot; &apos; &nbsp; &#NNN; &#xHH;
std::string decodeEntities(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        const size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10) {
            out += s[i];
            continue;
        }
        const std::string name = s.substr(i + 1, semi - i - 1);
        if (name == "amp")
            out += '&';
        else if (name == "lt")
            out += '<';
        else if (name == "gt")
            out += '>';
        else if (name == "quot")
            out += '"';
        else if (name == "apos")
            out += '\'';
        else if (name == "nbsp")
            out += ' ';
        else if (name.size() > 1 && name[0] == '#') {
            const bool hex = name[1] == 'x' || name[1] == 'X';
            char *end = nullptr;
            const unsigned long cp = std::strtoul(name.c_str() + (hex ? 2 : 1), &end, hex ? 16 : 10);
            if (!end || *end != '\0') {
                out += s.substr(i, semi - i + 1);
            } else {
                appendUtf8(out, cp);
            }
        } else {
            out += s.substr(i, semi - i + 1);   // desconhecida: fica como está
        }
        i = semi;
    }
    return out;
}

// Texto puro: a descrição pode vir como HTML escapado (&lt;p&gt;) ou dentro de
// CDATA; decodifica, tira as tags, decodifica as entidades que estavam no
// HTML e junta os espaços.
std::string plainText(const std::string &value)
{
    const std::string html = decodeEntities(value);
    std::string noTags;
    bool inTag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];
        // Uma tag começa com letra, "/" ou "!" logo depois do "<"; "3 < 4" fica.
        const char next = i + 1 < html.size() ? html[i + 1] : '\0';
        if (c == '<' && !inTag && (std::isalpha(static_cast<unsigned char>(next)) || next == '/' || next == '!'))
            inTag = true;
        else if (c == '>' && inTag) {
            inTag = false;
            noTags += ' ';
        } else if (!inTag)
            noTags += c;
    }
    const std::string decoded = decodeEntities(noTags);
    std::string out;
    bool space = false;
    for (char c : decoded) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = !out.empty();
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += c;
    }
    return out;
}

// Conteúdo de <tag>…</tag> dentro de item, com CDATA desembrulhado.
std::string element(const std::string &item, const std::string &tag)
{
    const std::string open = "<" + tag;
    size_t start = 0;
    for (;;) {
        start = item.find(open, start);
        if (start == std::string::npos)
            return std::string();
        const char next = start + open.size() < item.size() ? item[start + open.size()] : '\0';
        if (next == '>' || next == ' ' || next == '\t' || next == '\n')
            break;   // não confunde <link> com <linkx>
        start += open.size();
    }
    const size_t gt = item.find('>', start);
    if (gt == std::string::npos)
        return std::string();
    if (item[gt - 1] == '/')
        return std::string();   // <tag/>
    const size_t end = item.find("</" + tag + ">", gt);
    if (end == std::string::npos)
        return std::string();
    std::string value = item.substr(gt + 1, end - gt - 1);
    std::string out;
    size_t pos = 0;
    while (pos < value.size()) {
        const size_t cdata = value.find("<![CDATA[", pos);
        if (cdata == std::string::npos) {
            out += value.substr(pos);
            break;
        }
        out += value.substr(pos, cdata - pos);
        const size_t close = value.find("]]>", cdata);
        if (close == std::string::npos) {
            out += value.substr(cdata + 9);
            break;
        }
        // O conteúdo do CDATA já é texto: as entidades nele são escapadas para
        // sobreviver ao decodeEntities() de plainText().
        std::string raw = value.substr(cdata + 9, close - cdata - 9);
        std::string escaped;
        for (char c : raw)
            escaped += c == '&' ? std::string("&amp;") : std::string(1, c);
        out += escaped;
        pos = close + 3;
    }
    return out;
}

} // namespace

Flavour detectFlavour(const std::string &unameAll, bool ghostbsdMarker)
{
    // Mesma ordem do UnixCommand::getBSDFlavour do OctoPkg.
    if (ghostbsdMarker)
        return Flavour::GhostBSD;
    std::string lower;
    for (char c : unameAll)
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.find("nomadbsd") != std::string::npos)
        return Flavour::NomadBSD;
    if (unameAll.find("FreeBSD HBSD") != std::string::npos)
        return Flavour::HardenedBSD;
    if (unameAll.find("FreeBSD") != std::string::npos)
        return Flavour::FreeBSD;
    if (unameAll.find("DragonFly") != std::string::npos)
        return Flavour::DragonFly;
    return Flavour::Unknown;
}

Flavour systemFlavour()
{
    const ProcessResult r = runProcess({"uname", "-a"}, 5000);
    return detectFlavour(r.out, ::access("/etc/rc.conf.ghostbsd", F_OK) == 0);
}

Flavour flavourFromString(const std::string &s)
{
    if (s == "freebsd")
        return Flavour::FreeBSD;
    if (s == "nomadbsd")
        return Flavour::NomadBSD;
    if (s == "hardenedbsd")
        return Flavour::HardenedBSD;
    if (s == "ghostbsd")
        return Flavour::GhostBSD;
    if (s == "dragonfly")
        return Flavour::DragonFly;
    return Flavour::Unknown;
}

std::string flavourName(Flavour f)
{
    switch (f) {
    case Flavour::FreeBSD:     return "FreeBSD";
    case Flavour::NomadBSD:    return "NomadBSD";
    case Flavour::HardenedBSD: return "HardenedBSD";
    case Flavour::GhostBSD:    return "GhostBSD";
    case Flavour::DragonFly:   return "DragonFly BSD";
    default:                   return std::string();
    }
}

std::string newsUrl(Flavour f)
{
    // As URLs do utils::retrieveDistroNews do OctoPkg; a do GhostBSD em https.
    switch (f) {
    case Flavour::FreeBSD:
    case Flavour::NomadBSD:    return "https://www.freebsd.org/news/feed.xml";
    case Flavour::HardenedBSD: return "https://www.hardenedbsd.org/rss.xml";
    case Flavour::GhostBSD:    return "https://www.ghostbsd.org/rss.xml";
    case Flavour::DragonFly:   return "https://www.dragonflydigest.com/feed";
    default:                   return std::string();
    }
}

std::vector<NewsItem> parseRss(const std::string &xml, size_t max)
{
    std::vector<NewsItem> items;
    size_t pos = 0;
    while (items.size() < max) {
        size_t start = xml.find("<item", pos);
        while (start != std::string::npos && start + 5 < xml.size() && xml[start + 5] != '>'
               && xml[start + 5] != ' ')
            start = xml.find("<item", start + 5);   // <items>, <itemx>...
        if (start == std::string::npos)
            break;
        const size_t end = xml.find("</item>", start);
        if (end == std::string::npos)
            break;
        const std::string item = xml.substr(start, end - start);
        NewsItem n;
        n.title = plainText(element(item, "title"));
        n.link = plainText(element(item, "link"));
        n.description = plainText(element(item, "description"));
        // Como o OctoPkg: a data sem o fuso ("Mon, 20 Oct 2026 10:00:00 +0000").
        n.date = plainText(element(item, "pubDate"));
        const size_t plus = n.date.find_last_of("+-");
        if (plus != std::string::npos && plus > 0 && n.date[plus - 1] == ' ')
            n.date = n.date.substr(0, plus - 1);
        if (!n.title.empty() || !n.description.empty())
            items.push_back(std::move(n));
        pos = end + 7;
    }
    return items;
}

NewsFeed::NewsFeed(std::string cachePath, std::string fetchPath)
    : m_cachePath(std::move(cachePath))
    , m_fetchPath(fetchPath.empty() ? std::string("/usr/bin/fetch") : std::move(fetchPath))
{
}

NewsResult NewsFeed::cached() const
{
    NewsResult r;
    r.items = parseRss(readFile(m_cachePath));
    r.fromCache = true;
    return r;
}

NewsResult NewsFeed::refresh(Flavour flavour) const
{
    const std::string url = newsUrl(flavour);
    if (url.empty()) {
        NewsResult r = cached();
        r.error = i18n::tr("News are available on FreeBSD, NomadBSD, HardenedBSD, GhostBSD and DragonFly BSD.");
        return r;
    }
    makeParentDirs(m_cachePath);
    const std::string tmp = m_cachePath + ".tmp";
    // fetch(1): -q sem progresso, -T tempo limite em segundos, -o destino.
    const ProcessResult p = runProcess({m_fetchPath, "-q", "-T", "30", "-o", tmp, url}, kFetchTimeoutMs);
    std::string error;
    if (!p.started)
        error = i18n::trf("Failed to start '%1': %2", {m_fetchPath, p.startError});
    else if (p.timedOut)
        error = i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kFetchTimeoutMs), m_fetchPath});
    else if (p.status != 0) {
        std::string msg = p.err;
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == ' '))
            msg.pop_back();
        error = msg.empty() ? i18n::trf("Command failed with exit code %1", {std::to_string(p.status)}) : msg;
    }
    const std::string fresh = error.empty() ? readFile(tmp) : std::string();
    // Só substitui o cache por um feed que dê para ler.
    if (error.empty() && parseRss(fresh, 1).empty())
        error = i18n::tr("The news feed could not be read.");
    if (!error.empty()) {
        std::remove(tmp.c_str());
        NewsResult r = cached();
        r.error = error;
        return r;
    }
    NewsResult r;
    r.updated = fresh != readFile(m_cachePath);
    if (std::rename(tmp.c_str(), m_cachePath.c_str()) != 0) {
        r.error = i18n::trf("Could not save %1", {m_cachePath});
        std::remove(tmp.c_str());
    }
    r.items = parseRss(fresh);
    return r;
}

} // namespace octo
