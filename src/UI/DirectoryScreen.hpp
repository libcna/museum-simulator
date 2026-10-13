#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Camera/InputState.hpp"
#include "Exhibits/Services.hpp"
#include "World/Catalogue.hpp"

/**
 * The full-screen museum directory: a tab per floor, a searchable list of every room and
 * the floor plan. Selecting a room and pressing Enter (or clicking it twice) quick-travels
 * there, on floors the visitor has already reached.
 */
class DirectoryScreen {
public:
    explicit DirectoryScreen(Services& services) : s_(services) {}

    void Open(int currentFloor, int currentCell);
    void Close() { open_ = false; }
    [[nodiscard]] bool IsOpen() const { return open_; }

    struct Result {
        bool close = false;
        std::optional<int> travelTo;   // cell index
    };
    /** Handles this frame's input while open. `screenW/H` is the back buffer size. */
    Result Update(const InputState& input, const std::set<int>& discovered, int screenW, int screenH);
    void Draw(const std::set<int>& discovered, int currentFloor, int currentCell, const Microsoft::Xna::Framework::Vector3& player,
              float yaw, int screenW, int screenH);

private:
    struct Entry {
        int cell;
        int wingHeader;   // -1 for room rows, else wing index (a heading row)
    };
    void Rebuild();
    [[nodiscard]] bool Unlocked(int cell, const std::set<int>& discovered) const;
    void MoveSelection(int delta);

    Services& s_;
    bool open_ = false;
    int floor_ = 0;
    int selected_ = -1;
    std::string query_;
    std::vector<Entry> entries_;
    int scroll_ = 0;
    int lastClickCell_ = -1;
    std::string message_;
};
