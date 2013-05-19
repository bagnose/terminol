// vi:noai:sw=4

#ifndef COMMON__CONFIG__HXX
#define COMMON__CONFIG__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"

class Config : protected Uncopyable {
public:
    struct Color {
        double r, g, b;
    };

private:
    static const char * SOLARIZED_DARK_COLORS[16];
    static const char * SOLARIZED_LIGHT_COLORS[16];
    static const Color  STANDARD_COLORS[16];
    static const Color  TWEAKED_COLORS[16];

    // TODO
    // pointerfgColor
    // pointerBgColor
    // scrollbarStrategy: show/hide/auto

    // title
    // titleUpdateStrategy: replace, append, prepend, ignore

    // allow blink

    // flow control?

    std::string _fontName;
    std::string _geometryString;
    std::string _termName;
    bool        _scrollWithHistory;
    bool        _scrollOnTtyOutput;
    bool        _scrollOnTtyKeyPress;
    bool        _doubleBuffer;
    std::string _title;
    std::string _chdir;
    size_t      _scrollBackHistory;
    bool        _unlimitedScrollBack;
    // Debugging support:
    bool        _traceTty;
    bool        _syncTty;

    Color       _fgColor;
    Color       _bgColor;
    Color       _systemColors[16];

    bool        _customCursorFillColor;
    Color       _cursorFillColor;
    bool        _customCursorTextColor;
    Color       _cursorTextColor;

    Color       _scrollbarFgColor;
    Color       _scrollbarBgColor;
    int         _scrollbarWidth;

    Color       _borderColor;
    int         _borderThickness;

public:
    Config();

    void setFontName(const std::string & val) { _fontName = val; }
    void setGeometryString(const std::string & val) { _geometryString = val; }
    void setTermName(const std::string & val) { _termName = val; }
    void setScrollOnTtyOutput(bool val) { _scrollOnTtyOutput = val; }
    void setScrollOnTtyKeyPress(bool val) { _scrollOnTtyKeyPress = val; }
    void setDoubleBuffer(bool val) { _doubleBuffer = val; }
    void setTitle(const std::string & val) { _title = val; }
    void setChdir(const std::string & val) { _chdir = val; }
    void setScrollBackHistory(size_t val) { _scrollBackHistory = val; }
    void setTraceTty(bool val) { _traceTty = val; }
    void setSyncTty(bool val) { _syncTty = val; }

    const std::string & getFontName() const { return _fontName; }
    const std::string & getGeometryString() const { return _geometryString; }
    const std::string & getTermName() const { return _termName; }
    bool                getScrollWithHistory() const { return _scrollWithHistory; }
    bool                getScrollOnTtyOutput() const { return _scrollOnTtyOutput; }
    bool                getScrollOnTtyKeyPress() const { return _scrollOnTtyKeyPress; }
    bool                getDoubleBuffer() const { return _doubleBuffer; }
    const std::string & getTitle() const { return _title; }
    const std::string & getChdir() const { return _chdir; }
    size_t              getScrollBackHistory() const { return _scrollBackHistory; }
    bool                getUnlimitedScrollBack() const { return _unlimitedScrollBack; }
    bool                getTraceTty() const { return _traceTty; }
    bool                getSyncTty() const { return _syncTty; }

    int16_t             getX()    const { return -1; }
    int16_t             getY()    const { return -1; }
    uint16_t            getRows() const { return 24; }
    uint16_t            getCols() const { return 80; }

    const Color &       getFgColor() const { return _fgColor; }
    const Color &       getBgColor() const { return _bgColor; }

    const Color &       getSystemColor(uint8_t index) const {
        ASSERT(index <= 16, "");
        return _systemColors[index];
    }

    bool                getCustomCursorFillColor() const { return _customCursorFillColor; }
    const Color &       getCursorFillColor()       const { return _cursorFillColor; }
    bool                getCustomCursorTextColor() const { return _customCursorTextColor; }
    const Color &       getCursorTextColor()       const { return _cursorTextColor; }

    const Color &       getScrollbarFgColor()      const { return _scrollbarFgColor; }
    const Color &       getScrollbarBgColor()      const { return _scrollbarBgColor; }
    int                 getScrollbarWidth()        const { return _scrollbarWidth; }

    const Color &       getBorderColor()           const { return _borderColor; }
    int                 getBorderThickness()       const { return _borderThickness; }

protected:
    static Color decodeHexColor(const std::string & hexColor);
};

#endif // COMMON__CONFIG__HXX
