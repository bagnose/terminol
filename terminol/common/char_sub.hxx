// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__CHAR_SUB__HXX
#define COMMON__CHAR_SUB__HXX

#include "terminol/common/utf8.hxx"

// CharSub (or Character-Substitution) is a translation layer used to
// support different character sets, e.g. US versus UK. CharSub uses a
// table to translate ASCII characters into UTF-8 sequences. If there is
// no entry for the ASCII character then it remains unchanged.
class CharSub {
    const utf8::Seq * _seqs;
    size_t            _offset;
    size_t            _size;
    bool              _special;

public:
    // Construct a dummy CharSub - one that does no actual substitution.
    CharSub() : _seqs(nullptr), _offset(0), _size(0), _special(false) {}

    // Construct a CharSub with a substitution table. Note 'seqs' must be
    // static data.
    CharSub(const utf8::Seq * seqs, size_t offset, size_t size, bool special = false) :
        _seqs(seqs), _offset(offset), _size(size), _special(special) {}

    // Bold and italic attributes are disabled from 'special' char subs.
    bool isSpecial() const { return _special; }

    // If the UTF-8 sequence is the ASCII subset then translate it if there is
    // an entry in the table.
    void translate(utf8::Seq & seq) const {
        if (utf8::leadLength(seq.lead()) == utf8::Length::L1) {
            auto ascii = seq.lead();
            if (ascii >= _offset && ascii < _offset + _size) {
                seq = _seqs[ascii - _offset];
            }
        }
    }
};

// CharSet (or Character-Set) enumerates the CharSub registers. Terminol supports
// two active CharSubs.
enum class CharSet { G0, G1, G2, G3 };

struct CharSubArray {
    CharSubArray(const CharSub * g0,
                 const CharSub * g1,
                 const CharSub * g2,
                 const CharSub * g3) : charSubs { g0, g1, g2, g3 } {}

    const CharSub * charSubs[4];

    void set(CharSet set_, const CharSub * sub) {
        charSubs[static_cast<int>(set_)] = sub;
    }

    const CharSub * get(CharSet set_) const {
        return charSubs[static_cast<int>(set_)];
    }
};

#endif // COMMON__CHAR_SUB__HXX
