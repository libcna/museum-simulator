#pragma once

#include <memory>
#include <set>
#include <string>

#include "World/Catalogue.hpp"

/**
 * The visitor's progress (which cells they have been in, which unlocks directory quick travel) kept in a small text
 * file through CNA's StorageDevice and StorageContainer: the same API the *Title Files and Saved Data* room
 * demonstrates. Every failure (no storage, a corrupt file) is non-fatal: the museum simply starts with nothing visited.
 */
class ProgressStore {
public:
    explicit ProgressStore(bool enabled);
    ~ProgressStore();

    [[nodiscard]] bool Enabled() const { return enabled_; }
    /** Reads the visited cells (by room key, so the file survives changes to the museum's layout). */
    std::set<int> Load(const MuseumPlan& museum);
    /** Writes them. Returns false when storage is unavailable. */
    bool Save(const std::set<int>& visited, const MuseumPlan& museum);
    [[nodiscard]] const std::string& Where() const { return where_; }

private:
    bool Open();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool enabled_;
    std::string where_;
};
