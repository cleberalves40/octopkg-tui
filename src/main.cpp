#include "app.h"

#include <cstring>
#include <iostream>
#include <string>

namespace {

void usage()
{
    std::cout << "uso: octopkg-tui [--remote] [--icons=nerd|unicode|ascii] [--lang=en_US|pt_BR|es_ES]\n"
                 "                  [--elevator=doas|sudo|none] [--config=ARQUIVO] [--pkg=EXECUTÁVEL]\n"
                 "                  [--bectl=EXECUTÁVEL] [--version]\n"
                 "\n"
                 "Interface de terminal para o pkg(8), derivada do OctoPkg. --remote começa\n"
                 "na lista dos repositórios. As operações usam o doas (padrão, se não for root)\n"
                 "ou o sudo; --elevator=none executa o pkg diretamente. Os ícones \"nerd\"\n"
                 "(padrão) exigem uma fonte Nerd no terminal, como a JetBrainsMono Nerd Font\n"
                 "Mono (pkg install nerd-fonts-jetbrainsmono).\n";
}

bool option(const char *arg, const char *name, std::string *value)
{
    const size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) != 0 || arg[n] != '=')
        return false;
    *value = arg + n + 1;
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    tui::Options options;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
            usage();
            return 0;
        }
        if (!std::strcmp(a, "--version") || !std::strcmp(a, "-V")) {
            std::cout << "octopkg-tui " << OCTOPKG_TUI_VERSION << '\n';
            return 0;
        }
        if (!std::strcmp(a, "--remote")) {
            options.remote = true;
            continue;
        }
        if (option(a, "--elevator", &options.elevator)) {
            if (options.elevator == "none")
                options.elevator.clear();
            continue;
        }
        if (option(a, "--icons", &options.iconMode) || option(a, "--lang", &options.language)
            || option(a, "--config", &options.settingsPath) || option(a, "--pkg", &options.pkgPath)
            || option(a, "--bectl", &options.bectlPath))
            continue;
        std::cerr << "octopkg-tui: opção desconhecida: " << a << "\n\n";
        usage();
        return 2;
    }
    tui::App app(options);
    return app.run();
}
