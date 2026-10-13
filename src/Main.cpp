#include "Catalogue/Inventory.hpp"
#include "Catalogue/ReferenceViews.hpp"
#include "MuseumGame.hpp"

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    try {
        const MuseumOptions options = MuseumOptions::Parse(argc, argv);
        if (options.showHelp) {
            std::fputs(MuseumOptions::Usage(), stdout);
            return 0;
        }
        // Data-only commands: they need no window, device or content, and their exit status is the result.
        if (options.listRooms) {
            for (const RoomDef& r : TheMuseum().rooms)
                if (r.kind == CellKind::Room) std::printf("%s\n", r.key.c_str());
            return 0;
        }
        if (options.listTurnedRooms) {
            for (const RoomDef& r : TheMuseum().rooms)
                if (r.kind == CellKind::Room && r.Turned()) std::printf("%s\n", r.key.c_str());
            return 0;
        }
        if (options.mapMarkdown || options.coverageMarkdown) {
            std::fputs((options.mapMarkdown ? MapAsMarkdown(TheMuseum()) : CoverageAsMarkdown(TheMuseum())).c_str(), stdout);
            return 0;
        }
        if (options.listViews) {
            for (const ReferenceView& v : ReferenceViews()) std::printf("%s\t%s\t%s\n", v.name, v.room, v.shows);
            return 0;
        }
        if (options.validate || options.inventory) {
            const MuseumPlan& museum = TheMuseum();
            const std::vector<std::string> errors = ValidateMuseum(museum);
            for (const std::string& e : errors) std::fprintf(stderr, "invalid: %s\n", e.c_str());
            if (options.inventory) std::fputs(InventoryAsJson(museum).c_str(), stdout);
            else std::fputs(InventoryAsText(museum).c_str(), stdout);
            std::fprintf(stderr, "%s\n", errors.empty() ? "museum structure: ok" : "museum structure: INVALID");
            return errors.empty() ? 0 : 1;
        }
        MuseumGame game(options);
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
