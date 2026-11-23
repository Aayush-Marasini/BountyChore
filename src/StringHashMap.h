// StringHashMap.h - custom separate-chaining hash table (string key -> V).
// Supports search, insert, update, erase, collision chains and resizing.
// Expected O(1) per operation while the load factor stays <= 0.75.
#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace bc {

template <typename V>
class StringHashMap {
public:
    explicit StringHashMap(std::size_t initialBuckets = 16)
        : buckets_(initialBuckets < 4 ? 4 : initialBuckets) {}

    std::size_t size() const { return size_; }
    bool contains(const std::string& key) const { return find(key) != nullptr; }

    V* find(const std::string& key) {
        for (auto& e : buckets_[indexFor(key, buckets_.size())])
            if (e.key == key) return &e.value;
        return nullptr;
    }
    const V* find(const std::string& key) const {
        for (const auto& e : buckets_[indexFor(key, buckets_.size())])
            if (e.key == key) return &e.value;
        return nullptr;
    }

    // Returns false (and changes nothing) if the key already exists.
    bool insert(const std::string& key, const V& value) {
        if (contains(key)) return false;
        if (static_cast<double>(size_ + 1) > 0.75 * static_cast<double>(buckets_.size()))
            rehash(buckets_.size() * 2);
        buckets_[indexFor(key, buckets_.size())].push_back(Entry{key, value});
        ++size_;
        return true;
    }
    // Returns false if the key does not exist.
    bool update(const std::string& key, const V& value) {
        V* v = find(key);
        if (!v) return false;
        *v = value;
        return true;
    }
    bool erase(const std::string& key) {
        auto& chain = buckets_[indexFor(key, buckets_.size())];
        for (auto it = chain.begin(); it != chain.end(); ++it) {
            if (it->key == key) {
                chain.erase(it);
                --size_;
                return true;
            }
        }
        return false;
    }
    void clear() {
        for (auto& c : buckets_) c.clear();
        size_ = 0;
    }

private:
    struct Entry { std::string key; V value; };

    static std::size_t indexFor(const std::string& key, std::size_t n) {
        std::uint64_t h = 1469598103934665603ULL; // FNV-1a
        for (unsigned char c : key) { h ^= c; h *= 1099511628211ULL; }
        return static_cast<std::size_t>(h % n);
    }
    void rehash(std::size_t newCount) {
        std::vector<std::list<Entry>> next(newCount);
        for (auto& chain : buckets_)
            for (auto& e : chain)
                next[indexFor(e.key, newCount)].push_back(std::move(e));
        buckets_.swap(next);
    }

    std::vector<std::list<Entry>> buckets_;
    std::size_t size_ = 0;
};

} // namespace bc
