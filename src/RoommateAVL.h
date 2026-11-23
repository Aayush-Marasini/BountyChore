// RoommateAVL.h - roommate records in an AVL tree ordered by (balance, ID), plus a
// "Roommates" book that pairs the tree with a hash index (roommate ID -> current key).
#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Common.h"
#include "StringHashMap.h"

namespace bc {

struct Roommate {
    std::string id;
    std::string name;
    int64 earnedHalf = 0;   // earned work points (half units)
    int64 balanceHalf = 0;  // spendable balance (half units)
    int64 completed = 0;    // chores completed
};

// Ordering key: balance first, ID second, so equal balances stay distinct.
struct RoommateKey {
    int64 balanceHalf;
    std::string id;
};
inline bool keyLess(const RoommateKey& a, const RoommateKey& b) {
    if (a.balanceHalf != b.balanceHalf) return a.balanceHalf < b.balanceHalf;
    return a.id < b.id;
}
inline RoommateKey keyOf(const Roommate& r) { return RoommateKey{r.balanceHalf, r.id}; }

class RoommateAVL {
public:
    std::size_t size() const { return count_; }

    bool insert(const Roommate& r) {
        bool added = false;
        insertNode(root_, r, added);
        if (added) ++count_;
        return added;
    }
    bool erase(const RoommateKey& key) {
        bool removed = false;
        eraseNode(root_, key, removed);
        if (removed) --count_;
        return removed;
    }
    Roommate* find(const RoommateKey& key) {
        Node* n = root_.get();
        while (n) {
            RoommateKey k = keyOf(n->rec);
            if (keyLess(key, k)) n = n->left.get();
            else if (keyLess(k, key)) n = n->right.get();
            else return &n->rec;
        }
        return nullptr;
    }
    const Roommate* minNode() const {
        const Node* n = root_.get();
        if (!n) return nullptr;
        while (n->left) n = n->left.get();
        return &n->rec;
    }
    const Roommate* maxNode() const {
        const Node* n = root_.get();
        if (!n) return nullptr;
        while (n->right) n = n->right.get();
        return &n->rec;
    }
    // Ascending (lowest balance first).
    std::vector<Roommate> inOrder() const {
        std::vector<Roommate> out;
        walk(root_.get(), out, false);
        return out;
    }
    // Descending (highest balance first) via reverse in-order traversal.
    std::vector<Roommate> reverseInOrder() const {
        std::vector<Roommate> out;
        walk(root_.get(), out, true);
        return out;
    }
    int height() const { return heightOf(root_); }

private:
    struct Node;
    using NodePtr = std::unique_ptr<Node>;
    struct Node {
        Roommate rec;
        NodePtr left, right;
        int height = 1;
        explicit Node(const Roommate& r) : rec(r) {}
    };

    static int heightOf(const NodePtr& n) { return n ? n->height : 0; }
    static void refresh(Node& n) {
        n.height = 1 + std::max(heightOf(n.left), heightOf(n.right));
    }
    static void rotateRight(NodePtr& y) {
        NodePtr x = std::move(y->left);
        y->left = std::move(x->right);
        refresh(*y);
        x->right = std::move(y);
        refresh(*x);
        y = std::move(x);
    }
    static void rotateLeft(NodePtr& x) {
        NodePtr y = std::move(x->right);
        x->right = std::move(y->left);
        refresh(*x);
        y->left = std::move(x);
        refresh(*y);
        x = std::move(y);
    }
    static void rebalance(NodePtr& n) {
        refresh(*n);
        int bf = heightOf(n->left) - heightOf(n->right);
        if (bf > 1) {
            if (heightOf(n->left->left) < heightOf(n->left->right)) rotateLeft(n->left);
            rotateRight(n);
        } else if (bf < -1) {
            if (heightOf(n->right->right) < heightOf(n->right->left)) rotateRight(n->right);
            rotateLeft(n);
        }
    }
    static void insertNode(NodePtr& n, const Roommate& r, bool& added) {
        if (!n) { n = std::make_unique<Node>(r); added = true; return; }
        RoommateKey k = keyOf(r), nk = keyOf(n->rec);
        if (keyLess(k, nk)) insertNode(n->left, r, added);
        else if (keyLess(nk, k)) insertNode(n->right, r, added);
        else return; // duplicate key
        rebalance(n);
    }
    static Roommate removeMin(NodePtr& n) {
        if (n->left) {
            Roommate r = removeMin(n->left);
            rebalance(n);
            return r;
        }
        Roommate r = std::move(n->rec);
        NodePtr right = std::move(n->right);
        n = std::move(right);
        return r;
    }
    static void eraseNode(NodePtr& n, const RoommateKey& key, bool& removed) {
        if (!n) return;
        RoommateKey nk = keyOf(n->rec);
        if (keyLess(key, nk)) eraseNode(n->left, key, removed);
        else if (keyLess(nk, key)) eraseNode(n->right, key, removed);
        else {
            removed = true;
            if (!n->left) { NodePtr t = std::move(n->right); n = std::move(t); }
            else if (!n->right) { NodePtr t = std::move(n->left); n = std::move(t); }
            else n->rec = removeMin(n->right);
        }
        if (n) rebalance(n);
    }
    static void walk(const Node* n, std::vector<Roommate>& out, bool reverse) {
        if (!n) return;
        walk(reverse ? n->right.get() : n->left.get(), out, reverse);
        out.push_back(n->rec);
        walk(reverse ? n->left.get() : n->right.get(), out, reverse);
    }

    NodePtr root_;
    std::size_t count_ = 0;
};

// AVL tree + hash index. The index maps roommate ID -> current balance, i.e. the
// AVL key, so any roommate can be located in O(log m) from the ID alone.
class Roommates {
public:
    std::size_t count() const { return tree_.size(); }
    bool exists(const std::string& id) const { return keyIndex_.contains(id); }

    bool add(const std::string& id, const std::string& name) {
        Roommate r;
        r.id = id;
        r.name = name;
        return addRecord(r);
    }
    bool addRecord(const Roommate& r) {
        if (keyIndex_.contains(r.id)) return false;
        if (!tree_.insert(r)) return false;
        keyIndex_.insert(r.id, r.balanceHalf);
        return true;
    }
    // Copy of the record, or nullptr-equivalent via optional-like pointer semantics.
    const Roommate* find(const std::string& id) {
        const int64* bal = keyIndex_.find(id);
        if (!bal) return nullptr;
        return tree_.find(RoommateKey{*bal, id});
    }
    // Changes points: removes the old AVL key, updates the record, reinserts it.
    bool setPoints(const std::string& id, int64 earnedHalf, int64 balanceHalf, int64 completed) {
        const int64* bal = keyIndex_.find(id);
        if (!bal) return false;
        Roommate* r = tree_.find(RoommateKey{*bal, id});
        if (!r) return false;
        Roommate copy = *r;
        tree_.erase(RoommateKey{*bal, id});
        copy.earnedHalf = earnedHalf;
        copy.balanceHalf = balanceHalf;
        copy.completed = completed;
        tree_.insert(copy);
        keyIndex_.update(id, balanceHalf);
        return true;
    }
    // Renaming keeps the key (balance, id) so no restructuring is needed.
    bool rename(const std::string& id, const std::string& newName) {
        const int64* bal = keyIndex_.find(id);
        if (!bal) return false;
        Roommate* r = tree_.find(RoommateKey{*bal, id});
        if (!r) return false;
        r->name = newName;
        return true;
    }
    std::vector<Roommate> byBalanceDescending() const { return tree_.reverseInOrder(); }
    std::vector<Roommate> all() const { return tree_.inOrder(); }
    const Roommate* richest() const { return tree_.maxNode(); }

private:
    RoommateAVL tree_;
    StringHashMap<int64> keyIndex_;
};

} // namespace bc
