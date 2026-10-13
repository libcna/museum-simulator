#pragma once

#include <string>
#include <vector>

/**
 * A named, reproducible viewpoint: the room's own arrival pose moved `advance` metres forward. The same list
 * drives the visual regression tool (tools/reference.sh, `--view NAME`) and the Reference Scenes room, so a
 * view looked at by hand is exactly the view that is compared against its stored reference image.
 */
struct ReferenceView {
    const char* name;
    const char* room;       // key of the room the view is in
    float advance;          // metres to walk forward from the arrival pose
    const char* shows;      // one line: what to look at
};

[[nodiscard]] const std::vector<ReferenceView>& ReferenceViews();
[[nodiscard]] const ReferenceView* FindReferenceView(const std::string& name);
