// SPDX-License-Identifier: GPL-2.0-or-later
//
// Notícias do sistema (aba Notícias): porte do utils::retrieveDistroNews e
// do parseDistroNews do OctoPkg, sem Qt.
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#ifndef OCTOPKG_TUI_NEWS_H
#define OCTOPKG_TUI_NEWS_H

#include <string>
#include <vector>

namespace octo {

/** As variantes do UnixCommand::getBSDFlavour do OctoPkg. */
enum class Flavour { FreeBSD, NomadBSD, HardenedBSD, GhostBSD, DragonFly, Unknown };

/** A variante a partir do "uname -a" e da existência de /etc/rc.conf.ghostbsd. */
Flavour detectFlavour(const std::string &unameAll, bool ghostbsdMarker);
/** A variante do sistema em uso. */
Flavour systemFlavour();
/** "freebsd", "nomadbsd", "hardenedbsd", "ghostbsd", "dragonfly" (ou vazio). */
Flavour flavourFromString(const std::string &s);
/** Nome para a tela ("FreeBSD"); vazio se desconhecida. */
std::string flavourName(Flavour f);
/** O feed RSS do OctoPkg para a variante (o do FreeBSD para a NomadBSD); vazio se não houver. */
std::string newsUrl(Flavour f);

struct NewsItem {
    std::string title;
    std::string link;
    std::string description;   // texto puro, sem as tags HTML
    std::string date;          // pubDate sem o fuso, como no OctoPkg
};

/** Os primeiros max itens de um feed RSS 2.0 (CDATA e entidades HTML decodificados). */
std::vector<NewsItem> parseRss(const std::string &xml, size_t max = 10);

struct NewsResult {
    std::vector<NewsItem> items;
    bool updated = false;      // o feed baixado é diferente do que estava no cache
    bool fromCache = false;    // não foi possível baixar: estes são os do cache
    std::string error;         // por que não foi possível baixar
};

/**
 * O feed e o cache em disco, como o ~/.config/octopkg/distro_rss.xml do
 * OctoPkg. O download usa o fetch(1) do sistema base, que confere o
 * certificado TLS (o OctoPkg usa "curl -k", que não confere).
 */
class NewsFeed
{
public:
    /** fetchPath vazio: /usr/bin/fetch. */
    explicit NewsFeed(std::string cachePath, std::string fetchPath = std::string());

    /** As notícias do cache, sem rede (vazio se ainda não há cache). */
    NewsResult cached() const;
    /** Baixa o feed da variante; se falhar, devolve o cache com o erro. */
    NewsResult refresh(Flavour flavour) const;

    const std::string &cachePath() const { return m_cachePath; }

private:
    std::string m_cachePath;
    std::string m_fetchPath;
};

} // namespace octo

#endif
