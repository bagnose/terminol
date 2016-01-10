// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__BUFFER2__HXX
#define COMMON__BUFFER2__HXX

#include "terminol/common/data_types.hxx"
#include "terminol/common/repository_interface.hxx"
#include "terminol/common/para_cache.hxx"
#include "terminol/common/text.hxx"
#include "terminol/common/char_sub.hxx"

#include <deque>
#include <limits>
#include <unordered_map>
#include <mutex>
#include <iomanip>

class Buffer2 {

    // Cursor encompasses the state associated with a VT cursor.
    struct Cursor {
        Pos     pos;            // Current cursor position.
        Style   style;          // Current cursor style.
        bool    wrapNext;       // Flag indicating whether the next char wraps.
        CharSet charSet;        // Which CharSet is in use?

        Cursor() : pos(), style(), wrapNext(false), charSet(CharSet::G0) {}
    };

    struct SavedCursor {
        Cursor          cursor;
        const CharSub * charSub;

        SavedCursor() : cursor(), charSub(nullptr) {}
    };

    Text         _text;
    Cursor       _cursor;           // Current cursor.
    SavedCursor  _savedCursor;      // Saved cursor.
    CharSubArray _charSubs;
    //int16_t      _marginBegin;      // Index of first row in margin (inclusive).
    int16_t      _marginEnd;        // Index of last row in  margin (exclusive).

public:
    class I_Renderer {
    public:
        virtual void bufferDrawBg(Pos     pos,
                                  int16_t count,
                                  UColor  color) = 0;
        virtual void bufferDrawFg(Pos             pos,
                                  int16_t         count,
                                  UColor          color,
                                  AttrSet         attrs,
                                  const uint8_t * str,       // nul-terminated
                                  size_t          size) = 0;
        virtual void bufferDrawCursor(Pos             pos,
                                      UColor          fg,
                                      UColor          bg,
                                      AttrSet         attrs,
                                      const uint8_t * str,    // nul-terminated, count 1
                                      size_t          size,
                                      bool            wrapNext) = 0;

    protected:
        ~I_Renderer() {}
    };

    //
    //
    //

    Buffer2(I_Repository & repository, ParaCache & paraCache,
            int16_t rows, int16_t cols, uint32_t historyLimit,
            const CharSubArray & charSubArray) :
        _text(repository, paraCache, rows, cols, historyLimit),
        _charSubs(charSubArray)
    {}

    int16_t getRows() const { return _text.getRows(); }
    int16_t getCols() const { return _text.getCols(); }

    void write(utf8::Seq seq, bool autoWrap, bool UNUSED(insert)) {
        damageCell();

        auto cs = getCharSub(_cursor.charSet);
        cs->translate(seq);

        if (autoWrap && _cursor.wrapNext) {
            _cursor.wrapNext = false;

            if (_cursor.pos.row == _marginEnd - 1 || _cursor.pos.row < getRows() - 1) {
                _text.makeContinued(_cursor.pos.row);
            }
        }
    }

private:
    const CharSub * getCharSub(CharSet charSet) const {
        return _charSubs.get(charSet);
    }

    void damageCell() {
    }
};

#endif // COMMON__BUFFER2__HXX
