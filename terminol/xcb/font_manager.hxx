// vi:noai:sw=4

#ifndef XCB__FONT_MANAGER__HXX
#define XCB__FONT_MANAGER__HXX

#include "terminol/xcb/font_set.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/common/config.hxx"
#include "terminol/support/pattern.hxx"

#include <set>
#include <map>

class FontManager : protected Uncopyable {
public:
    class I_Client {
    public:
        virtual void useFontSet(FontSet * fontSet, int delta) throw () = 0;

    protected:
        I_Client() {}
        ~I_Client() {}
    };

private:
    const Config             & _config;
    Basics                   & _basics;

    int                        _delta;      // Global delta
    std::map<I_Client *, int>  _clients;
    std::map<int, FontSet *>   _fontSets;
    bool                       _dispatch;

public:
    FontManager(const Config & config, Basics & basics) throw (FontSet::Error) :
        _config(config),
        _basics(basics),
        _delta(0),
        _clients(),
        _fontSets(),
        _dispatch(false)
    {
        auto fontSet = new FontSet(_config, _basics, _delta);
        _fontSets.insert(std::make_pair(_delta, fontSet));
        ASSERT(_fontSets.find(_delta) != _fontSets.end(), "");
    }

    virtual ~FontManager() {
        ASSERT(!_dispatch, "");

        for (auto p : _fontSets) {
            delete p.second;
        }

        ASSERT(_clients.empty(), "Clients remain at destruction.");
    }

    FontSet * addClient(I_Client * client) {
        ASSERT(!_dispatch, "");

        _clients.insert(std::make_pair(client, _delta));
        auto iter = _fontSets.find(_delta);
        ASSERT(iter != _fontSets.end(), "");
        return iter->second;
    }

    void removeClient(I_Client * client) {
        ASSERT(!_dispatch, "");

        auto iter = _clients.find(client);
        ASSERT(iter != _clients.end(), "");
        _clients.erase(iter);
    }

    void localDelta(I_Client * client, int delta) {
        if (_dispatch) { return; }

        ASSERT(!_dispatch, "");

        ASSERT(_clients.find(client) != _clients.end(), "");
        resizeClient(client, delta);
        purgeUnusedFonts();
    }

    void globalDelta(int delta) {
        if (_dispatch) { return; }
        ASSERT(!_dispatch, "");

        bool success = true;
        for (auto & p : _clients) {
            success = resizeClient(p.first, delta) && success;
        }
        purgeUnusedFonts();

        if (success) { _delta += delta; }
    }

protected:
    bool resizeClient(I_Client * client, int delta) {
        try {
            auto iter1 = _clients.find(client);
            ASSERT(iter1 != _clients.end(), "");
            auto old_delta = iter1->second;
            auto new_delta = delta == 0 ? 0 : old_delta + delta;

            auto iter2 = _fontSets.find(new_delta);

            _dispatch = true;
            auto dispatchGuard = scopeGuard([&] { _dispatch = false; });

            if (iter2 == _fontSets.end()) {
                auto fontSet = new FontSet(_config, _basics, new_delta);
                _fontSets.insert(std::make_pair(new_delta, fontSet));
                client->useFontSet(fontSet, new_delta);
            }
            else {
                client->useFontSet(iter2->second, new_delta);
            }

            iter1->second = new_delta;      // Only update now that load has succeeded.
            return true;
        }
        catch (const FontSet::Error & error) {
            ERROR(error.message);
            return false;
        }
    }

    void purgeUnusedFonts() {
        std::set<int> used;
        used.insert(_delta);
        for (auto & p : _clients) { used.insert(p.second); }

        std::set<int> unused;
        for (auto & p : _fontSets) {
            if (p.first == _delta) { continue; }
            if (used.find(p.first) == used.end()) {
                unused.insert(p.first);
            }
        }

        ASSERT(used.find(_delta) != used.end(), "");

        for (auto delta : unused) {
            auto iter = _fontSets.find(delta);
            ASSERT(iter != _fontSets.end(), "");
            delete iter->second;
            _fontSets.erase(iter);
        }

        ASSERT(_fontSets.find(_delta) != _fontSets.end(), "");
    }
};

#endif // XCB__FONT_MANAGER__HXX
