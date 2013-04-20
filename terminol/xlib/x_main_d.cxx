// vi:noai:sw=4

#include "terminol/xlib/x_basics.hxx"
#include "terminol/xlib/x_window.hxx"
#include "terminol/xlib/x_color_set.hxx"
#include "terminol/xlib/x_key_map.hxx"
#include "terminol/xlib/x_font_set.hxx"
#include "terminol/common/support.hxx"

#include <string>

#include <unistd.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>

#include <map>

class I_Spawner {
public:
    virtual void spawn() = 0;

protected:
    ~I_Spawner() {}
};


class Server : protected Uncopyable {
    I_Spawner & _spawner;
public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Server(I_Spawner & spawner) throw (Error);
    ~Server();

    // Select for read only.
    int getFd();

    void animate();
};

class MainLoop :
    protected I_Spawner,
    protected Uncopyable
{
    typedef std::map<int, X_Window *> X_Windows;

    X_Basics   _basics;
    X_ColorSet _colorSet;
    X_KeyMap   _keyMap;
    X_FontSet  _fontSet;
    X_Windows  _windows;
    Server     _server;

public:
    MainLoop(const std::string & fontName,
             const std::string & UNUSED(term))
        throw (X_Basics::Error, X_ColorSet::Error, X_FontSet::Error) :
        _basics(),
        _colorSet(_basics.display(), _basics.visual(), _basics.colormap()),
        _keyMap(),
        _fontSet(_basics.display(), fontName),
        _windows(),
        _server(*this)
    {
    }

    virtual ~MainLoop() {}

protected:
};

int main() {
    return 0;
}
