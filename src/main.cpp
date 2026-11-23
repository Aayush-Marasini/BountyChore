// BountyChore - roommate chore board driven by a max-heap of live bounties.
// Terminal menu application. See README.md for build and usage.
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "Household.h"
#include "Storage.h"

using namespace bc;

namespace {

constexpr int64 kMaxInput = 1000000000;  // cap for typed point values

struct Cancelled {};    // user pressed Enter on an empty prompt
struct InputClosed {};  // stdin reached EOF

Household g_house;
bool g_dirty = false;
std::string g_dir = "data";

// ---------------------------------------------------------------- input helpers
std::string readRaw(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string s;
    if (!std::getline(std::cin, s)) throw InputClosed{};
    return trim(s);
}
// Blank input cancels the current command.
std::string ask(const std::string& prompt) {
    std::string s = readRaw(prompt);
    if (s.empty()) throw Cancelled{};
    return s;
}
int64 askInt(const std::string& prompt, int64 lo, int64 hi) {
    for (;;) {
        std::string s = ask(prompt);
        int64 v = 0;
        if (parseWhole(s, v) && v >= lo && v <= hi) return v;
        std::cout << "  Please enter a whole number from " << lo << " to " << hi
                  << " (blank to cancel).\n";
    }
}
std::string askId(const std::string& prompt) {
    for (;;) {
        std::string s = ask(prompt);
        if (validId(s)) return s;
        std::cout << "  IDs are 1-32 letters, digits, '-' or '_' (blank to cancel).\n";
    }
}
std::string askText(const std::string& prompt) {
    for (;;) {
        std::string s = ask(prompt);
        if (validText(s)) return s;
        std::cout << "  Enter 1-60 printable characters (blank to cancel).\n";
    }
}
int64 askUrgency() {
    for (;;) {
        std::string s = ask("Urgency (1.0, 1.5, 2.0 or 2.5): ");
        int64 v = 0;
        if (parseHalf(s, v) && v >= 2 && v <= 5) return v;
        std::cout << "  Urgency must be 1.0, 1.5, 2.0 or 2.5.\n";
    }
}
bool confirm(const std::string& prompt) {
    std::string s = readRaw(prompt + " (y/n): ");
    return s == "y" || s == "Y" || s == "yes" || s == "YES" || s == "Yes";
}
// Menu choice: returns -1 for anything that is not a number in range.
int askChoice(int lo, int hi) {
    std::string s = readRaw("Choose: ");
    int64 v = 0;
    if (parseWhole(s, v) && v >= lo && v <= hi) return static_cast<int>(v);
    std::cout << "  Invalid choice. Enter a number from " << lo << " to " << hi << ".\n";
    return -1;
}

// ---------------------------------------------------------------- display helpers
std::string urgencyText(int64 t2) { return formatHalf(t2); }

void printChore(const ChoreNode& c, const std::string& prefix = "") {
    std::cout << prefix << "[" << c.taskId << "] " << c.name << " (" << c.room << ")\n"
              << prefix << "    Base " << c.basePoints << " | Growth +" << c.dailyGrowth
              << "/day | Days unclaimed " << c.daysUnclaimed << " | Urgency x"
              << urgencyText(c.urgencyTimes2) << " | Tips " << c.tipPot << "\n"
              << prefix << "    Live bounty: " << formatHalf(c.bountyHalf) << " pts\n";
}
void printRoommate(const Roommate& r) {
    std::cout << "  [" << r.id << "] " << r.name << " | Balance " << formatHalf(r.balanceHalf)
              << " | Earned work " << formatHalf(r.earnedHalf) << " | Completed "
              << r.completed << "\n";
}

// ---------------------------------------------------------------- persistence UI
std::string askFolder() {
    std::string s = readRaw("Data folder [" + g_dir + "]: ");
    return s.empty() ? g_dir : s;
}
bool doSave() {
    std::string dir = askFolder();
    std::string err;
    if (!saveHousehold(dir, g_house, err)) {
        std::cout << "SAVE FAILED: " << err << "\nChanges are still unsaved.\n";
        return false;
    }
    g_dir = dir;
    g_dirty = false;
    std::cout << "Saved " << g_house.chores.size() << " chore(s), "
              << g_house.roommates.count() << " roommate(s), " << g_house.tips.size()
              << " tip record(s) to '" << dir << "'.\n";
    return true;
}
void doLoad() {
    while (g_dirty) {
        std::cout << "You have unsaved changes.\n  1. Save First\n  2. Discard and Load\n"
                     "  3. Cancel\n";
        int c = askChoice(1, 3);
        if (c == 3) { std::cout << "Load cancelled.\n"; return; }
        if (c == 2) break;
        if (c == 1) {
            if (doSave()) break;  // failed save: loop and ask again
        }
    }
    std::string dir = askFolder();
    Household next;
    std::string err;
    if (!loadHousehold(dir, next, err)) {
        std::cout << "LOAD REJECTED: " << err << "\nCurrent session was not changed.\n";
        return;
    }
    g_house = std::move(next);
    g_dir = dir;
    g_dirty = false;
    std::cout << "Loaded " << g_house.chores.size() << " chore(s), "
              << g_house.roommates.count() << " roommate(s), " << g_house.tips.size()
              << " tip record(s) from '" << dir << "'.\n";
}

// ---------------------------------------------------------------- commands: add
void addChore() {
    ChoreNode c;
    c.taskId = askId("New task ID: ");
    if (g_house.chores.findById(c.taskId)) {
        std::cout << "Error: task ID '" << c.taskId << "' already exists.\n";
        return;
    }
    c.name = askText("Chore name: ");
    c.room = askText("Room: ");
    c.basePoints = askInt("Base points (1-1000000000): ", 1, kMaxInput);
    c.dailyGrowth = askInt("Daily growth in points (0-1000000000): ", 0, kMaxInput);
    c.urgencyTimes2 = askUrgency();
    if (!g_house.chores.insert(c)) {
        std::cout << "Error: chore could not be added (value too large).\n";
        return;
    }
    g_dirty = true;
    auto stored = g_house.chores.findById(c.taskId);
    std::cout << "Chore added.\n";
    if (stored) printChore(*stored, "  ");
}
void addRoommate() {
    std::string id = askId("New roommate ID: ");
    if (g_house.roommates.exists(id)) {
        std::cout << "Error: roommate ID '" << id << "' already exists.\n";
        return;
    }
    std::string name = askText("Roommate name: ");
    g_house.roommates.add(id, name);
    g_dirty = true;
    std::cout << "Roommate added.\n";
}

// ---------------------------------------------------------------- commands: view
void showTop() {
    auto top = g_house.chores.peekMax();
    if (!top) { std::cout << "The board is empty - no active chores.\n"; return; }
    std::cout << "Top bounty:\n";
    printChore(*top, "  ");
}
void searchChore() {
    std::string id = askId("Task ID: ");
    auto c = g_house.chores.findById(id);
    if (!c) { std::cout << "No active chore with ID '" << id << "'.\n"; return; }
    printChore(*c, "  ");
    for (const auto& t : g_house.tips)
        if (t.taskId == id) std::cout << "    Tip: " << t.amount << " from " << t.donorId << "\n";
}
void showBoard() {
    auto list = g_house.chores.inPriorityOrder();
    if (list.empty()) { std::cout << "The board is empty - no active chores.\n"; return; }
    std::cout << "=== Bounty Board (highest first) ===\n";
    int rank = 1;
    for (const auto& c : list) {
        std::cout << "#" << rank++ << " ";
        printChore(c);
    }
}
void searchRoommate() {
    std::string id = askId("Roommate ID: ");
    const Roommate* r = g_house.roommates.find(id);
    if (!r) { std::cout << "No roommate with ID '" << id << "'.\n"; return; }
    printRoommate(*r);
}
void showStandings() {
    auto board = g_house.roommates.byBalanceDescending();
    if (board.empty()) { std::cout << "No roommates yet.\n"; return; }
    std::cout << "=== Balance Leaderboard (spendable points) ===\n";
    int rank = 1;
    for (const auto& r : board) {
        std::cout << "#" << rank++ << " ";
        printRoommate(r);
    }
    int64 total = 0;
    try {
        for (const auto& r : board) total = checkedAdd(total, r.earnedHalf);
    } catch (const std::overflow_error&) {
        std::cout << "\nFairness report unavailable (totals too large).\n";
        return;
    }
    double avg = static_cast<double>(total) / 2.0 / static_cast<double>(board.size());
    std::cout << "\n=== Fairness Report (earned work points) ===\n"
              << std::fixed << std::setprecision(2) << "House average: " << avg << "\n";
    for (const auto& r : board) {
        double earned = static_cast<double>(r.earnedHalf) / 2.0;
        std::cout << "  [" << r.id << "] " << r.name << " | Earned " << earned
                  << " | Delta " << std::showpos << (earned - avg) << std::noshowpos << "\n";
    }
    std::cout << std::defaultfloat;
    std::cout << "(A negative delta means fewer rewarded points than average; it does not "
                 "assign a chore.)\n";
}

// ---------------------------------------------------------------- commands: modify
void editChore() {
    std::string id = askId("Task ID to edit: ");
    auto cur = g_house.chores.findById(id);
    if (!cur) { std::cout << "No active chore with ID '" << id << "'.\n"; return; }
    printChore(*cur, "  ");
    std::cout << "  1. Name\n  2. Room\n  3. Base points\n  4. Daily growth\n  5. Urgency\n";
    int f = askChoice(1, 5);
    if (f < 0) return;
    ChoreNode u = *cur;
    switch (f) {
        case 1: u.name = askText("New name: "); break;
        case 2: u.room = askText("New room: "); break;
        case 3: u.basePoints = askInt("New base points (1-1000000000): ", 1, kMaxInput); break;
        case 4: u.dailyGrowth = askInt("New daily growth (0-1000000000): ", 0, kMaxInput); break;
        case 5: u.urgencyTimes2 = askUrgency(); break;
    }
    if (!g_house.chores.modifyChoreKey(u)) {
        std::cout << "Error: edit rejected (value too large). Nothing changed.\n";
        return;
    }
    g_dirty = true;
    std::cout << "Chore updated.\n";
    printChore(*g_house.chores.findById(id), "  ");
}
void addTip() {
    std::string donor = askId("Donor roommate ID: ");
    const Roommate* d = g_house.roommates.find(donor);
    if (!d) { std::cout << "No roommate with ID '" << donor << "'.\n"; return; }
    Roommate donorRec = *d;
    std::string taskId = askId("Chore ID to tip: ");
    auto chore = g_house.chores.findById(taskId);
    if (!chore) { std::cout << "No active chore with ID '" << taskId << "'.\n"; return; }
    std::cout << "  " << donorRec.name << " has " << formatHalf(donorRec.balanceHalf)
              << " pts available.\n";
    int64 amount = askInt("Tip amount in whole points (>= 1): ", 1, kMaxInput);
    int64 cost = amount * 2;  // amount <= 1e9, cannot overflow
    if (donorRec.balanceHalf < cost) {
        std::cout << "Error: insufficient balance. Nothing changed.\n";
        return;
    }
    ChoreNode u = *chore;
    try { u.tipPot = checkedAdd(u.tipPot, amount); }
    catch (const std::overflow_error&) { std::cout << "Error: tip pot too large.\n"; return; }
    // The heap update is the only step that can fail, so run it first.
    if (!g_house.chores.modifyChoreKey(u)) {
        std::cout << "Error: tip would overflow the bounty. Nothing changed.\n";
        return;
    }
    g_house.roommates.setPoints(donor, donorRec.earnedHalf, donorRec.balanceHalf - cost,
                                donorRec.completed);
    bool merged = false;
    for (auto& t : g_house.tips)
        if (t.taskId == taskId && t.donorId == donor) { t.amount += amount; merged = true; break; }
    if (!merged) g_house.tips.push_back(Tip{taskId, donor, amount});
    g_dirty = true;
    std::cout << "Tip recorded. New bounty: "
              << formatHalf(g_house.chores.findById(taskId)->bountyHalf) << " pts.\n";
}
void renameRoommate() {
    std::string id = askId("Roommate ID: ");
    if (!g_house.roommates.exists(id)) { std::cout << "No roommate with ID '" << id << "'.\n"; return; }
    std::string name = askText("New name: ");
    g_house.roommates.rename(id, name);
    g_dirty = true;
    std::cout << "Roommate renamed.\n";
}
void advanceDay() {
    if (g_house.chores.empty()) { std::cout << "No active chores to age.\n"; return; }
    try {
        g_house.chores.advanceDayAllChores();
    } catch (const std::overflow_error&) {
        std::cout << "Error: a bounty would overflow. No chores were changed.\n";
        return;
    }
    g_dirty = true;
    std::cout << "One day passed. All bounties recalculated.\n";
    showTop();
}

// ---------------------------------------------------------------- commands: remove
void removeTipsFor(const std::string& taskId) {
    std::vector<Tip> keep;
    for (auto& t : g_house.tips)
        if (t.taskId != taskId) keep.push_back(t);
    g_house.tips.swap(keep);
}
void completeTop() {
    auto top = g_house.chores.peekMax();
    if (!top) { std::cout << "The board is empty - nothing to complete.\n"; return; }
    std::cout << "Current top chore:\n";
    printChore(*top, "  ");
    std::string id = askId("Completing roommate ID: ");
    const Roommate* r = g_house.roommates.find(id);
    if (!r) { std::cout << "No roommate with ID '" << id << "'.\n"; return; }
    Roommate rec = *r;
    if (!confirm("Confirm " + rec.name + " finished '" + top->name + "'")) {
        std::cout << "Cancelled. Nothing changed.\n";
        return;
    }
    int64 newBalance, newEarned;
    try {
        newBalance = checkedAdd(rec.balanceHalf, top->bountyHalf);
        newEarned = checkedAdd(rec.earnedHalf, workHalf(*top));  // tips are not new work
    } catch (const std::overflow_error&) {
        std::cout << "Error: points would overflow. Nothing changed.\n";
        return;
    }
    g_house.chores.extractMax();
    g_house.roommates.setPoints(id, newEarned, newBalance, rec.completed + 1);
    removeTipsFor(top->taskId);
    g_dirty = true;
    std::cout << rec.name << " earned " << formatHalf(top->bountyHalf) << " pts (work value "
              << formatHalf(workHalf(*top)) << "). Balance is now " << formatHalf(newBalance)
              << ".\n";
}
void deleteChore() {
    std::string id = askId("Task ID to delete: ");
    auto c = g_house.chores.findById(id);
    if (!c) { std::cout << "No active chore with ID '" << id << "'.\n"; return; }
    printChore(*c, "  ");
    if (!confirm("Delete this chore (no points awarded, tips refunded)")) {
        std::cout << "Cancelled. Nothing changed.\n";
        return;
    }
    // Work out every refund first so the command is all-or-nothing.
    std::map<std::string, int64> refund;  // donor -> half units
    for (const auto& t : g_house.tips)
        if (t.taskId == id) refund[t.donorId] += t.amount * 2;
    std::vector<std::pair<Roommate, int64>> updates;
    for (const auto& [donor, half] : refund) {
        const Roommate* d = g_house.roommates.find(donor);
        if (!d) continue;  // cannot happen: roommates are never deleted
        try { updates.push_back({*d, checkedAdd(d->balanceHalf, half)}); }
        catch (const std::overflow_error&) {
            std::cout << "Error: a refund would overflow a balance. Nothing changed.\n";
            return;
        }
    }
    g_house.chores.removeById(id);
    for (const auto& [rec, newBal] : updates)
        g_house.roommates.setPoints(rec.id, rec.earnedHalf, newBal, rec.completed);
    removeTipsFor(id);
    g_dirty = true;
    std::cout << "Chore deleted. " << updates.size() << " donor(s) refunded.\n";
}

// ---------------------------------------------------------------- menus
void runGuarded(void (*fn)()) {
    try { fn(); }
    catch (const Cancelled&) { std::cout << "Cancelled.\n"; }
}

void addMenu() {
    std::cout << "\n-- Add --\n  1. Add Chore\n  2. Add Roommate\n  3. Back\n";
    switch (askChoice(1, 3)) {
        case 1: runGuarded(addChore); break;
        case 2: runGuarded(addRoommate); break;
        default: break;
    }
}
void viewMenu() {
    std::cout << "\n-- View / Search --\n  1. Top bounty\n  2. Search chore by task ID\n"
                 "  3. Full bounty board\n  4. Search roommate by ID\n"
                 "  5. Roommate standings and fairness report\n  6. Back\n";
    switch (askChoice(1, 6)) {
        case 1: showTop(); break;
        case 2: runGuarded(searchChore); break;
        case 3: showBoard(); break;
        case 4: runGuarded(searchRoommate); break;
        case 5: showStandings(); break;
        default: break;
    }
}
void modifyMenu() {
    std::cout << "\n-- Modify --\n  1. Edit chore\n  2. Add tip to chore\n"
                 "  3. Rename roommate\n  4. Advance one day\n  5. Back\n";
    switch (askChoice(1, 5)) {
        case 1: runGuarded(editChore); break;
        case 2: runGuarded(addTip); break;
        case 3: runGuarded(renameRoommate); break;
        case 4: advanceDay(); break;
        default: break;
    }
}
void removeMenu() {
    std::cout << "\n-- Complete / Delete --\n  1. Complete top chore\n  2. Delete chore\n"
                 "  3. Back\n";
    switch (askChoice(1, 3)) {
        case 1: runGuarded(completeTop); break;
        case 2: runGuarded(deleteChore); break;
        default: break;
    }
}
// Returns true if the program may exit.
bool exitMenu() {
    while (g_dirty) {
        std::cout << "You have unsaved changes.\n  1. Save and Exit\n  2. Exit Without Saving\n"
                     "  3. Cancel\n";
        int c = askChoice(1, 3);
        if (c == 3) return false;
        if (c == 2) break;
        if (c == 1) {
            if (doSave()) break;
        }
    }
    return true;
}

void mainLoop() {
    for (;;) {
        std::cout << "\n===== BountyChore =====" << (g_dirty ? "  [unsaved changes]" : "")
                  << "\n  1. Add\n  2. View / Search\n  3. Modify\n  4. Complete / Delete\n"
                     "  5. Save\n  6. Load\n  7. Exit\n";
        switch (askChoice(1, 7)) {
            case 1: addMenu(); break;
            case 2: viewMenu(); break;
            case 3: modifyMenu(); break;
            case 4: removeMenu(); break;
            case 5: doSave(); break;
            case 6: doLoad(); break;
            case 7:
                if (exitMenu()) { std::cout << "Goodbye!\n"; return; }
                break;
            default: break;
        }
    }
}

} // namespace

int main() {
    std::cout << "BountyChore - the roommate chore bounty board\n"
                 "(Press Enter on an empty prompt to cancel a command.)\n";
    try {
        mainLoop();
    } catch (const InputClosed&) {
        std::cout << "\nInput closed. Exiting"
                  << (g_dirty ? " (unsaved changes were discarded).\n" : ".\n");
    } catch (const Cancelled&) {
        // A blank answer inside a menu-level prompt; just leave.
        std::cout << "Exiting.\n";
    } catch (const std::exception& e) {
        std::cout << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
