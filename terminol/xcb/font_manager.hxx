// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef XCB__FONT_MANAGER__HXX
#define XCB__FONT_MANAGER__HXX

#include "terminol/xcb/font_set.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/common/config.hxx"
#include "terminol/support/pattern.hxx"

#include <set>
#include <map>
#include <memory>

class FontManager final : protected Uncopyable {
public:
    class I_Client {
    public:
        virtual void useFontSet(FontSet * fontSet, int delta) = 0;

    protected:
        ~I_Client() = default;
    };

private:
    using FontSetPtr = std::unique_ptr<FontSet>;

    const Config & _config;
    Basics &       _basics;

    int                       _delta = 0;        // Global delta
    std::map<I_Client *, int> _clients;          // client->size
    std::map<int, FontSetPtr> _fontSets;         // size->font-set
    bool                      _dispatch = false; // used to disallow re-entrance

public:
    FontManager(const Config & config, Basics & basics) : _config(config), _basics(basics) {}

    ~FontManager() {
        ASSERT(!_dispatch, );
        ASSERT(_clients.empty(), << "Clients remain at destruction");
    }

    FontSet * addClient(I_Client * client) {
        ASSERT(!_dispatch, );

        auto size = std::max(1, _config.fontSize + _delta);

        _clients.insert({client, size});
        auto iter = _fontSets.find(size);
        if (iter == _fontSets.end()) {
            iter = _fontSets.insert({size, std::make_unique<FontSet>(_config, _basics, size)})
                       .first;
            return iter->second.get();
        }
        else {
            return iter->second.get();
        }
    }

    void removeClient(I_Client * client) {
        ASSERT(!_dispatch, );

        auto iter = _clients.find(client);
        ASSERT(iter != _clients.end(), );
        _clients.erase(iter);

        purgeUnusedFonts();
    }

    void localDelta(I_Client * client, int delta) {
        if (_dispatch) { return; }

        ASSERT(_clients.find(client) != _clients.end(), );
        resizeClient(client, delta);
        purgeUnusedFonts();
    }

    void globalDelta(int delta) {
        if (_dispatch) { return; }

        for (auto & p : _clients) { resizeClient(p.first, delta); }

        purgeUnusedFonts();

        if (delta == 0) { _delta = 0; }
        else {
            _delta += delta;
        }
    }

protected:
    void resizeClient(I_Client * client, int delta) {
        auto iter1 = _clients.find(client);
        ASSERT(iter1 != _clients.end(), );
        auto old_size    = iter1->second;
        auto new_size    = delta == 0 ? _config.fontSize : old_size + delta;
        auto iter2       = _fontSets.find(new_size);
        auto total_delta = new_size - _config.fontSize;

        if (new_size < 1) { return; }

        iter1->second = new_size;

        _dispatch = true;
        ScopeGuard dispatchGuard([this]() { _dispatch = false; });

        if (iter2 == _fontSets.end()) {
            iter2 = _fontSets
                        .insert({new_size, std::make_unique<FontSet>(_config, _basics, new_size)})
                        .first;
            client->useFontSet(iter2->second.get(), total_delta);
        }
        else {
            client->useFontSet(iter2->second.get(), total_delta);
        }
    }

    void purgeUnusedFonts() {
        // Create a set of sizes and populate it with all the sizes that are in use.
        std::set<int> used;
        for (auto & p : _clients) { used.insert(p.second); }

        std::set<int> unused;
        for (auto & p : _fontSets) {
            if (used.find(p.first) == used.end()) { unused.insert(p.first); }
        }

        for (auto size : unused) {
            auto iter = _fontSets.find(size);
            ASSERT(iter != _fontSets.end(), );
            _fontSets.erase(iter);
        }
    }
};

#endif // XCB__FONT_MANAGER__HXX
