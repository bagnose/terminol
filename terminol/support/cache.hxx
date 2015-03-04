// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__CACHE__HXX
#define SUPPORT__CACHE__HXX

#include "terminol/support/debug.hxx"

#include <unordered_map>
#include <cstddef>

// LRU cache container
template <typename Key, typename T> class Cache {
    struct Link {
        Link *prev;
        Link *next;

        Link() : prev(this), next(this) {}
        Link(const Link & link) : prev(this), next(this) {
            ASSERT(link.single(), "");
        }

        // After this call, this->next==link.
        void insert(Link & link) {
            link.prev  = this;
            link.next  = next;
            next->prev = &link;
            next       = &link;
        }

        void extract() {
            prev->next = next;
            next->prev = prev;
            next = this;
            prev = this;
        }

        bool single() const {
            if (prev == next) {
                ASSERT(prev == this, "");
                return true;
            }
            else {
                return false;
            }
        }
    };

    struct Entry {
        Entry(const T & t_) : t(t_) {}
        T     t;
        Link  link;
    };

    static Entry & linkToEntry(Link & link) {
        auto linkOffset = offsetof(Entry, link);
        auto bytePtr    = reinterpret_cast<uint8_t *>(&link);
        auto & entry    = *reinterpret_cast<Entry *>(bytePtr - linkOffset);
        return entry;
    }

    typedef std::unordered_map<Key, Entry> Map;

    static Key & entryToKey(Entry & entry) {
        typedef typename Map::value_type Pair;
        auto keyOffset   = offsetof(Pair, first);
        auto entryOffset = offsetof(Pair, second);
        auto bytePtr     = reinterpret_cast<uint8_t *>(&entry);
        auto & key       = *reinterpret_cast<Key *>(bytePtr - (entryOffset - keyOffset));
        return key;
    }

    Link   _sentinel;       // next is newest, prev is oldest
    Map    _map;
    size_t _maxEntries;

public:
    typedef Key                     key_type;
    typedef T                       mapped_type;
    typedef std::pair<const Key, T> value_type;
    typedef value_type             &reference;
    typedef const value_type       &const_reference;
    typedef value_type             *pointer;
    typedef const value_type       *const_pointer;
    typedef size_t                  size_type;
    typedef ptrdiff_t               difference_type;

    class iterator {
        Link * _link;
    public:
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef std::pair<const Key, T>         value_type;
        typedef ptrdiff_t                       difference_type;
        typedef value_type                     *pointer;
        typedef value_type                     &reference;

        explicit iterator(Link * link) : _link(link) {}

        pointer operator->() {
            Key & key = entryToKey(linkToEntry(*_link));
            return reinterpret_cast<pointer>(&key);
        }

        reference operator*() {
            Key & key = entryToKey(linkToEntry(*_link));
            return reinterpret_cast<reference>(key);
        }

        iterator &operator++() {
            _link = _link->next;
            return *this;
        }

        iterator operator++(int) {
            iterator rval(*this);
            operator++();
            return rval;
        }

        iterator &operator--() {
            _link = _link->prev;
            return *this;
        }

        iterator operator--(int) {
            iterator rval(*this);
            operator--();
            return rval;
        }

        friend bool operator == (iterator lhs, iterator rhs) {
            return lhs._link == rhs._link;
        }

        friend bool operator != (iterator lhs, iterator rhs) {
            return !(lhs == rhs);
        }
    };

    class reverse_iterator {
        iterator _iterator;
    public:
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef std::pair<const Key, T>         value_type;
        typedef ptrdiff_t                       difference_type;
        typedef value_type                     *pointer;
        typedef value_type                     &reference;

        explicit reverse_iterator(Link * link) : _iterator(link) {}

        iterator base() const { return _iterator; }

        pointer operator->() {
            return operator->(_iterator);
        }

        reference operator*() {
            return operator*(_iterator);
        }

        iterator &operator++() {
            --_iterator;
            return *this;
        }

        iterator operator++(int) {
            iterator rval(*this);
            operator++();
            return rval;
        }

        iterator &operator--() {
            ++_iterator;
            return *this;
        }

        iterator operator--(int) {
            iterator rval(*this);
            operator--();
            return rval;
        }

        friend bool operator == (reverse_iterator lhs, reverse_iterator rhs) {
            return lhs._link == rhs._link;
        }

        friend bool operator != (reverse_iterator lhs, reverse_iterator rhs) {
            return !(lhs == rhs);
        }
    };

    iterator begin() {
        return iterator(_sentinel.next);
    }

    iterator end() {
        return iterator(&_sentinel);
    }

    reverse_iterator rbegin() {
        return iterator(_sentinel.prev);
    }

    reverse_iterator rend() {
        return iterator(&_sentinel);
    }

    explicit Cache(size_t maxEntries = 0) : _maxEntries(maxEntries) {}

    iterator insert(const Key & key, const T & t) {
        if (_maxEntries != 0 && _map.size() == _maxEntries) {
            removeOldest();
        }

        auto pair = _map.emplace(key, t);
        ASSERT(pair.second, "");

        auto & entry = pair.first->second;
        _sentinel.insert(entry.link);

        return iterator(&entry.link);
    }

    iterator erase(iterator iter) {
        auto & entry = iter->second;
        Link * next  = entry.link.next;
        entry.link.extract();
        return iterator(next);
    }

    iterator find(const Key & key) {
        auto iter = _map.find(key);

        if (iter == _map.end()) {
            return end();
        }
        else {
            auto & entry = iter->second;
            makeNewest(entry);
            return iterator(&entry.link);
        }
    }

private:
    void removeOldest() {
        ASSERT(!_sentinel.single(), "");
        auto & entry = linkToEntry(*_sentinel.prev);
        entry.link.extract();
        auto & key = entryToKey(entry);
        _map.erase(key);
    }

    void makeNewest(Entry & entry) {
        entry.link.extract();
        _sentinel.insert(entry.link);
    }
};

#endif // SUPPORT__CACHE__HXX
