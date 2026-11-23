// ChoreMaxHeap.h - array-backed binary max-heap of chores (primary data structure).
// The root always holds the chore with the highest live bounty.
// A TaskIndexMap (hash table) maps task ID -> current array index so a chore can be
// found, changed or removed without scanning; every swap updates the map.
#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Common.h"
#include "StringHashMap.h"

namespace bc {

struct ChoreNode {
    std::string taskId;
    std::string name;
    std::string room;
    int64 basePoints = 0;     // B  (whole points, > 0)
    int64 dailyGrowth = 0;    // R  (whole points per day, >= 0)
    int64 daysUnclaimed = 0;  // D
    int64 urgencyTimes2 = 2;  // U*2: 2=1.0x, 3=1.5x, 4=2.0x, 5=2.5x
    int64 tipPot = 0;         // T  (whole points)
    int64 bountyHalf = 0;     // cached live bounty in half-point units
};

// Work value (B + D*R) * U in half-point units (tips excluded). Throws on overflow.
inline int64 workHalf(const ChoreNode& c) {
    int64 grown = checkedAdd(c.basePoints, checkedMul(c.daysUnclaimed, c.dailyGrowth));
    return checkedMul(grown, c.urgencyTimes2);
}

using TaskIndexMap = StringHashMap<std::size_t>;

class ChoreMaxHeap {
public:
    ChoreMaxHeap() = default;

    // Bulk build with Floyd's O(n) algorithm. Throws std::invalid_argument on
    // duplicate IDs and std::overflow_error if a bounty overflows.
    explicit ChoreMaxHeap(std::vector<ChoreNode> chores) {
        for (auto& c : chores) {
            if (indexByID.contains(c.taskId))
                throw std::invalid_argument("duplicate task ID " + c.taskId);
            c.bountyHalf = computeBounty(c);
            indexByID.insert(c.taskId, heap.size());
            heap.push_back(std::move(c));
        }
        buildMaxHeap();
    }

    std::size_t size() const { return heap.size(); }
    bool empty() const { return heap.empty(); }

    // O(log n). False on duplicate ID or arithmetic overflow (nothing changes).
    bool insert(ChoreNode chore) {
        if (indexByID.contains(chore.taskId)) return false;
        try { chore.bountyHalf = computeBounty(chore); }
        catch (const std::overflow_error&) { return false; }
        heap.push_back(std::move(chore));
        std::size_t i = heap.size() - 1;
        indexByID.insert(heap[i].taskId, i);
        heapifyUp(i);
        return true;
    }

    // O(1)
    std::optional<ChoreNode> peekMax() const {
        if (heap.empty()) return std::nullopt;
        return heap[0];
    }

    // Expected O(1) through the hash table.
    std::optional<ChoreNode> findById(const std::string& id) const {
        const std::size_t* idx = indexByID.find(id);
        if (!idx) return std::nullopt;
        return heap[*idx];
    }

    // Replaces the record with the same task ID, recomputes its bounty and repairs
    // the heap. False if the ID is unknown or the bounty would overflow (no change).
    bool modifyChoreKey(ChoreNode updated) {
        const std::size_t* idx = indexByID.find(updated.taskId);
        if (!idx) return false;
        std::size_t i = *idx;
        try { updated.bountyHalf = computeBounty(updated); }
        catch (const std::overflow_error&) { return false; }
        ChoreNode old = heap[i];
        heap[i] = std::move(updated);
        if (higherPriority(heap[i], old)) heapifyUp(i);
        else heapifyDown(i);
        return true;
    }

    // O(log n)
    std::optional<ChoreNode> extractMax() {
        if (heap.empty()) return std::nullopt;
        ChoreNode top = heap[0];
        swapAndReindex(0, heap.size() - 1);
        heap.pop_back();
        indexByID.erase(top.taskId);
        if (!heap.empty()) heapifyDown(0);
        return top;
    }

    // O(log n)
    std::optional<ChoreNode> removeById(const std::string& id) {
        const std::size_t* idx = indexByID.find(id);
        if (!idx) return std::nullopt;
        std::size_t target = *idx;
        ChoreNode removed = heap[target];
        std::size_t last = heap.size() - 1;
        if (target != last) swapAndReindex(target, last);
        heap.pop_back();
        indexByID.erase(id);
        if (target < heap.size()) {
            if (target > 0 && higherPriority(heap[target], heap[(target - 1) / 2]))
                heapifyUp(target);
            else
                heapifyDown(target);
        }
        return removed;
    }

    // Ages every chore by one day (all-or-nothing on overflow), then rebuilds in O(n).
    void advanceDayAllChores() {
        std::vector<ChoreNode> next = heap;
        for (auto& c : next) {
            c.daysUnclaimed = checkedAdd(c.daysUnclaimed, 1);
            c.bountyHalf = computeBounty(c);
        }
        heap.swap(next);
        buildMaxHeap();
    }

    // Fully ranked copy (extract repeatedly from a copy of the heap), O(n log n).
    std::vector<ChoreNode> inPriorityOrder() const {
        ChoreMaxHeap copy = *this;
        std::vector<ChoreNode> out;
        out.reserve(heap.size());
        while (auto c = copy.extractMax()) out.push_back(*c);
        return out;
    }

    // Records in raw heap-array order (used for saving).
    std::vector<ChoreNode> snapshot() const { return heap; }

private:
    static int64 computeBounty(const ChoreNode& c) {
        return checkedAdd(workHalf(c), checkedMul(c.tipPot, 2));
    }
    // Higher bounty first; tie -> waiting longer; tie -> smaller task ID.
    static bool higherPriority(const ChoreNode& a, const ChoreNode& b) {
        if (a.bountyHalf != b.bountyHalf) return a.bountyHalf > b.bountyHalf;
        if (a.daysUnclaimed != b.daysUnclaimed) return a.daysUnclaimed > b.daysUnclaimed;
        return a.taskId < b.taskId;
    }
    void swapAndReindex(std::size_t i, std::size_t j) {
        if (i == j) return;
        std::swap(heap[i], heap[j]);
        indexByID.update(heap[i].taskId, i);
        indexByID.update(heap[j].taskId, j);
    }
    void heapifyUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!higherPriority(heap[i], heap[parent])) break;
            swapAndReindex(i, parent);
            i = parent;
        }
    }
    void heapifyDown(std::size_t i) {
        const std::size_t n = heap.size();
        for (;;) {
            std::size_t best = i, l = 2 * i + 1, r = 2 * i + 2;
            if (l < n && higherPriority(heap[l], heap[best])) best = l;
            if (r < n && higherPriority(heap[r], heap[best])) best = r;
            if (best == i) break;
            swapAndReindex(i, best);
            i = best;
        }
    }
    void buildMaxHeap() {
        if (heap.size() < 2) return;
        for (std::size_t i = heap.size() / 2; i-- > 0;) heapifyDown(i);
    }

    std::vector<ChoreNode> heap;
    TaskIndexMap indexByID;
};

} // namespace bc
