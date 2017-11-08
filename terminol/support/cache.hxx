// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__CACHE__HXX
#define SUPPORT__CACHE__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"

#include <unordered_map>
#include <cstddef>

// LRU cache container
template <typename Key, typename T>
class Cache : protected Uncopyable {
    struct Link {
        Link * prev;
        Link * next;

        Link() noexcept : prev(this), next(this) {}
        Link(Link && UNUSED(link)) noexcept : prev(this), next(this) {}
        Link(const Link & UNUSED(link)) noexcept : prev(this), next(this) {}

        // After this call, link->next==this
        void insert(Link & link) noexcept {
            link.next = this;
            link.prev = prev;
            prev->next = &link;
            prev = &link;
        }

        void extract() noexcept {
            prev->next = next;
            next->prev = prev;
            next = this;
            prev = this;
        }

        bool single() const noexcept {
            if (prev == next) {
                ASSERT(prev == this, << "Invalid link.");
                return true;
            }
            else {
                return false;
            }
        }
    };

    struct Entry {
        T    t;
        Link link;

        explicit Entry(const T & t_) : t(t_) {}
    };

    static Entry & linkToEntry(Link & link) noexcept {
        auto linkOffset = offsetof(Entry, link);
        auto bytePtr    = reinterpret_cast<uint8_t *>(&link);
        auto & entry    = *reinterpret_cast<Entry *>(bytePtr - linkOffset);
        return entry;
    }

    static Key & entryToKey(Entry & entry) noexcept {
        using Pair = typename Map::value_type;
        auto keyOffset   = offsetof(Pair, first);
        auto entryOffset = offsetof(Pair, second);
        auto bytePtr     = reinterpret_cast<uint8_t *>(&entry);
        auto & key       = *reinterpret_cast<Key *>(bytePtr - (entryOffset - keyOffset));
        return key;
    }

    using Map = std::unordered_map<Key, Entry>;

    Link _sentinel;       // next is oldest, prev is newest
    Map  _map;

public:
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = std::pair<const Key, T>;
    using reference       = value_type &;
    using const_reference = const value_type &;
    using pointer         = value_type *;
    using const_pointer   = const value_type *;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    //
    //
    //

    class iterator {
        friend class Cache;
        Link * _link;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = std::pair<const Key, T>;
        using difference_type   = ptrdiff_t;
        using pointer           = value_type *;
        using reference         = value_type &;

        explicit iterator(Link * link) noexcept : _link(link) {}

        pointer operator -> () noexcept {
            Key & key = entryToKey(linkToEntry(*_link));
            return reinterpret_cast<pointer>(&key);
        }

        reference operator * () noexcept {
            Key & key = entryToKey(linkToEntry(*_link));
            return reinterpret_cast<reference>(key);
        }

        iterator & operator ++ () noexcept {
            _link = _link->next;
            return *this;
        }

        iterator operator ++ (int) noexcept {
            iterator rval(*this);
            operator++();
            return rval;
        }

        iterator & operator -- () noexcept {
            _link = _link->prev;
            return *this;
        }

        iterator operator -- (int) noexcept {
            iterator rval(*this);
            operator--();
            return rval;
        }

        friend bool operator == (iterator lhs, iterator rhs) noexcept {
            return lhs._link == rhs._link;
        }

        friend bool operator != (iterator lhs, iterator rhs) noexcept {
            return !(lhs == rhs);
        }
    };

    //
    //
    //

    class reverse_iterator {
        iterator _iterator;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = std::pair<const Key, T>;
        using difference_type   = ptrdiff_t;
        using pointer           = value_type *;
        using reference         = value_type &;

        explicit reverse_iterator(Link * link) noexcept : _iterator(link) {}

        iterator base() const noexcept {
            auto iter = _iterator;
            --iter;
            return iter;
        }

        pointer operator -> () noexcept {
            return _iterator.operator->();
        }

        reference operator * () noexcept {
            return _iterator.operator*();
        }

        reverse_iterator & operator++() noexcept {
            --_iterator;
            return *this;
        }

        reverse_iterator operator ++ (int) noexcept {
            iterator rval(*this);
            operator++();
            return rval;
        }

        reverse_iterator & operator -- () noexcept {
            ++_iterator;
            return *this;
        }

        reverse_iterator operator -- (int) noexcept {
            iterator rval(*this);
            operator--();
            return rval;
        }

        friend bool operator == (reverse_iterator lhs, reverse_iterator rhs) noexcept {
            return lhs._link == rhs._link;
        }

        friend bool operator != (reverse_iterator lhs, reverse_iterator rhs) noexcept {
            return !(lhs == rhs);
        }
    };

    //
    //
    //

    iterator begin() noexcept {
        return iterator(_sentinel.next);
    }

    iterator end() noexcept {
        return iterator(&_sentinel);
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(_sentinel.prev);
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(&_sentinel);
    }

    Cache() = default;
    Cache(Cache &&) noexcept = default;

    iterator insert(const Key & key, T && t) {
        auto pair = _map.emplace(key, Entry(t));
        ASSERT(pair.second, << "Duplicate key.");

        auto & entry = pair.first->second;
        _sentinel.insert(entry.link);

        return iterator(&entry.link);
    }

    iterator erase(iterator iter) {
        auto & entry = linkToEntry(*iter._link);
        Link * next  = entry.link.next;
        entry.link.extract();
        return iterator(next);
    }

    iterator find(const Key & key) noexcept {
        auto iter = _map.find(key);

        if (iter == _map.end()) {
            return end();
        }
        else {
            auto & entry = iter->second;
            entry.link.extract();
            _sentinel.insert(entry.link);
            return iterator(&entry.link);
        }
    }

    T & at(const Key & key) {
        auto iter = find(key);
        THROW_UNLESS(iter != end(), std::out_of_range("Cache"));
        return iter->second;
    }

    bool empty() const noexcept {
        return _map.empty();
    }

    size_t size() const noexcept {
        return _map.size();
    }
};

#endif // SUPPORT__CACHE__HXX
