#ifndef OCTOPKG_TUI_PROCESS_H
#define OCTOPKG_TUI_PROCESS_H

#include <string>
#include <utility>
#include <vector>

namespace octo {

struct ProcessResult {
    std::string out;
    std::string err;
    int status = -1;          // código de saída, se o processo terminou normalmente
    bool started = false;     // false: não foi possível executar argv[0]
    std::string startError;   // motivo, quando started == false
    bool timedOut = false;
    bool crashed = false;     // terminou por sinal
};

using Environment = std::vector<std::pair<std::string, std::string>>;

/**
 * Executa argv sem shell (fork + execvp), capturando stdout e stderr.
 * timeoutMs < 0: sem limite. env: variáveis definidas só no filho, como
 * LC_ALL=C para que a saída do pkg não dependa do idioma. A entrada do
 * filho é /dev/null.
 */
ProcessResult runProcess(const std::vector<std::string> &argv, int timeoutMs,
                         const Environment &env = {});

/** Procura um executável no PATH; devolve string vazia se não achar. */
std::string findExecutable(const std::string &name);

} // namespace octo

#endif
