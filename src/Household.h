// Household.h - the whole session state: active chores, roommates, outstanding tips.
#pragma once
#include <string>
#include <vector>
#include "ChoreMaxHeap.h"
#include "RoommateAVL.h"

namespace bc {

// One donor's outstanding tip on one chore (whole points).
struct Tip {
    std::string taskId;
    std::string donorId;
    int64 amount = 0;
};

struct Household {
    ChoreMaxHeap chores;
    Roommates roommates;
    std::vector<Tip> tips;
};

} // namespace bc
