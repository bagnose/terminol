// vi:noai:sw=4

#ifndef COMMON__CONFIG__HXX
#define COMMON__CONFIG__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/common/data_types.hxx"

class Config : protected Uncopyable {
private:
    // TODO
    // pointerfgColor
    // pointerBgColor
    // scrollbarStrategy: show/hide/auto

    // titleUpdateStrategy: replace, append, prepend, ignore

    // allow blink

    // flow control?

    std::string _fontName;
    int         _fontSize;
    std::string _termName;
    bool        _scrollWithHistory;
    bool        _scrollOnTtyOutput;
    bool        _scrollOnTtyKeyPress;
    bool        _scrollOnResize;
    bool        _scrollOnPaste;
    bool        _doubleBuffer;
    std::string _title;
    std::string _icon;
    std::string _chdir;
    size_t      _scrollBackHistory;
    bool        _unlimitedScrollBack;
    int         _framesPerSecond;
    bool        _traditionalWrapping;
    // Debugging support:
    bool        _traceTty;
    bool        _syncTty;
    //
    int16_t     _initialX;
    int16_t     _initialY;
    uint16_t    _initialRows;
    uint16_t    _initialCols;

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
    uint32_t    _doubleClickTimeout;

    std::string _socketPath;
    bool        _serverFork;

public:
    Config();

    void setColorScheme(const std::string & name);

    void setFontName(const std::string & val) { _fontName = val; }
    void setFontSize(int val) { _fontSize = val; }
    void setTermName(const std::string & val) { _termName = val; }
    void setScrollOnTtyOutput(bool val) { _scrollOnTtyOutput = val; }
    void setScrollOnTtyKeyPress(bool val) { _scrollOnTtyKeyPress = val; }
    void setScrollOnResize(bool val) { _scrollOnResize = val; }
    void setDoubleBuffer(bool val) { _doubleBuffer = val; }
    void setTitle(const std::string & val) { _title = val; }
    void setIcon(const std::string & val) { _icon = val; }
    void setChdir(const std::string & val) { _chdir = val; }
    void setScrollBackHistory(size_t val) { _scrollBackHistory = val; }
    void setTraceTty(bool val) { _traceTty = val; }
    void setSyncTty(bool val) { _syncTty = val; }
    void setServerFork(bool val) { _serverFork = val; }

    const std::string & getFontName() const { return _fontName; }
    int                 getFontSize() const { return _fontSize; }
    const std::string & getTermName() const { return _termName; }
    bool                getScrollWithHistory() const { return _scrollWithHistory; }
    bool                getScrollOnTtyOutput() const { return _scrollOnTtyOutput; }
    bool                getScrollOnTtyKeyPress() const { return _scrollOnTtyKeyPress; }
    bool                getScrollOnResize() const { return _scrollOnResize; }
    bool                getScrollOnPaste() const { return _scrollOnPaste; }
    bool                getDoubleBuffer() const { return _doubleBuffer; }
    const std::string & getTitle() const { return _title; }
    const std::string & getIcon() const { return _icon; }
    const std::string & getChdir() const { return _chdir; }
    size_t              getScrollBackHistory() const { return _scrollBackHistory; }
    bool                getUnlimitedScrollBack() const { return _unlimitedScrollBack; }
    int                 getFramesPerSecond() const { return _framesPerSecond; }
    bool                getTraditionalWrapping() const { return _traditionalWrapping; }
    bool                getTraceTty() const { return _traceTty; }
    bool                getSyncTty() const { return _syncTty; }

    int16_t             getInitialX()    const { return _initialX; }
    int16_t             getInitialY()    const { return _initialY; }
    uint16_t            getInitialRows() const { return _initialRows; }
    uint16_t            getInitialCols() const { return _initialCols; }

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

    uint32_t            getDoubleClickTimeout()    const { return _doubleClickTimeout; }

    const std::string & getSocketPath()            const { return _socketPath; }
    bool                getServerFork()            const { return _serverFork; }
};

#endif // COMMON__CONFIG__HXX
