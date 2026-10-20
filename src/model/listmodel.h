#ifndef OCTOPKG_TUI_LISTMODEL_H
#define OCTOPKG_TUI_LISTMODEL_H

#include "pkg.h"

#include <string>
#include <vector>

/**
 * Filtro, ordenação e seleção da lista, independentes da interface, com as
 * colunas do OctoPkg. Testável sem desenhar a tela.
 */
namespace model {

enum Column { StateIcon = 0, Name, Version, Size, InstalledOn, ColumnCount };

struct SortSpec {
    int column = Name;       // -1: ordem do pkg
    bool ascending = true;

    bool operator==(const SortSpec &o) const { return column == o.column && ascending == o.ascending; }
};

/**
 * Onde o filtro procura, como o menu Busca do OctoPkg: no nome, ou no nome e
 * no comentário do pacote (o PackageModel::ctn_PACKAGE_DESCRIPTION_FILTER_NO_COLUMN
 * do OctoPkg, que junta os dois).
 */
enum class SearchMode { Name = 0, Description };

/** Clique no cabeçalho, como no bectl-tui: crescente, decrescente e sem ordem. */
SortSpec nextOnHeaderClick(SortSpec current, int column);

/**
 * Índices de all que passam no filtro (parte do nome, ou do nome e do
 * comentário, sem diferenciar maiúsculas), na ordem pedida. Ordenação
 * estável; a do estado segue a ordem do OctoPkg (bloqueado, instalado, não
 * instalado, desatualizado, mais novo).
 */
std::vector<int> visibleRows(const std::vector<octo::Package> &all, const std::string &filter, SortSpec sort,
                             SearchMode mode = SearchMode::Name);

/**
 * Posição a selecionar após atualizar, filtrar ou ordenar: a do pacote que
 * estava selecionado, se ainda visível; senão a primeira (ou -1, lista vazia).
 */
int rowToSelect(const std::vector<octo::Package> &all, const std::vector<int> &rows, const std::string &previous);

/** Primeira linha da janela de height linhas para que selected fique visível. */
int scrollOffset(int selected, int offset, int height, int count);

} // namespace model

#endif
