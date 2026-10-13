#include "MuseumGame.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv)
{
    try {
        MuseumGame game(MuseumOptions::Parse(argc, argv));
        game.Run();
        return 0;
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "cna-3d-museum: %s\n", failure.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "cna-3d-museum: unknown error\n");
        return 1;
    }
}
