// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__TEXT__HXX
#define COMMON__TEXT__HXX

#include "terminol/common/para_cache.hxx"

#include <deque>

class Text {
    class Line {
    public:
        Line(uint32_t index, uint32_t seqnum, bool continued) :
            _index(index), _seqnum(seqnum), _continued(continued ? 1 : 0) {}

        static uint32_t maxSeqnum() {
            return std::numeric_limits<uint32_t>::max() >> 1;
        }

        uint32_t getIndex()    const { return _index; }
        uint32_t getSeqnum()   const { return _seqnum; }
        bool     isContinued() const { return _continued != 0; }

        void setIndexSeqnum(uint32_t index, uint32_t seqnum) {
            _index  = index;
            _seqnum = seqnum;
        }

        void setContinued(bool continued) {
            _continued = static_cast<uint32_t>(continued);
        }

        void decrementIndex() {
            --_index;
        }

        void incrementIndex() {
            ++_index;
        }

        void dump(std::ostream & ost, uint32_t indexSubtraction) const {
            ost << "index=" << _index - indexSubtraction
                << " seqnum=" << _seqnum << " "
                << (isContinued() ? "(cont)" : "") << std::endl;
        }

    private:
        uint32_t _index;
        uint32_t _seqnum    : 31;
        uint32_t _continued : 1;   // True if this line is continued on the next line?
    };

    static_assert(sizeof(Line) == 8, "Line is 8 bytes");

    //
    //
    //

    I_Repository                & _repository;
    ParaCache                   & _paraCache;

    std::deque<I_Repository::Tag> _historyTags;         // Paragraph history as tags.
    uint32_t                      _poppedHistoryTags;   // Incremented for every _historyTags.pop_front();
    std::deque<Line>              _historyLines;

    std::deque<Para>              _currentParas;        // Current paragraphs.
    uint32_t                      _poppedCurrentParas;  // Incremented for every _currentParas.pop_front();
    uint32_t                      _straddlingLines;     // Number of lines that straddle history and current.
    std::deque<Line>              _currentLines;

    int16_t                       _cols;
    //uint32_t                      _historyTagLimit;

public:
    class Marker {
        friend class Text;
        bool     _valid = false;
        int32_t  _row;
        bool     _current;      // current vs historical
        uint32_t _index;
    };

    Marker begin() const;
    Marker end() const;

    //
    //
    //

    Text(I_Repository & repository,
         ParaCache    & paraCache,
         int16_t        rows,
         int16_t        cols,
         uint32_t       historyLimit);

    ~Text();

    int16_t getRows() const;

    int16_t getCols() const;

    void setCell(int16_t row, int16_t col, const Cell & cell);

    void insertCell(int16_t row, int16_t col, const Cell & cell);

    Cell getCell(int16_t row, int16_t col) const;

    void scrollDown(int16_t rowBegin, int16_t rowEnd, int16_t n);

    void scrollUp(int16_t rowBegin, int16_t rowEnd, int16_t n);

    void addLine(bool continuation);

    void makeContinued(int16_t row);

    void makeUncontinued(int16_t row);

    void resize(int16_t UNUSED(rows), int16_t UNUSED(cols), const std::vector<Marker *> & UNUSED(points) = {}) {
    }

    void dumpHistory(std::ostream & ost, bool decorate = false) const;

    void dumpCurrent(std::ostream & ost, bool decorate = false) const;

    void dumpDetail(std::ostream & ost, bool blah);

    class Match {
        friend class Text;
        bool     _valid = false;
        int32_t  _row;
        int16_t  _col;
        bool     _current;
        uint32_t _index;
        uint32_t _offsetBegin;
        uint32_t _offsetEnd;

    public:
        int32_t  row()    const { return _row; }
        int16_t  col()    const { return _col; }
        uint32_t length() const { return _offsetEnd - _offsetBegin; }
    };

    std::vector<Match> rfind(const Regex & regex, Marker & marker, bool & ongoing);

    //
    //
    //

    class I_Visitor {
    public:
        virtual void visitStyled(int32_t row, int16_t colBegin, int16_t colEnd,
                                 const Style & style, const Para & para, int32_t pos) = 0;

        virtual void visitUnstyled(int32_t row, int16_t colBegin, int16_t colEnd,
                                   const Para & para, int32_t pos) = 0;

    protected:
        ~I_Visitor() {}
    };

    //
    //
    //

    void visitStyled(int32_t rowBegin, int16_t colBegin,
                     int32_t rowEnd,   int16_t colEnd,
                     I_Visitor & visitor);

    void visitUnstyled(int32_t rowBegin, int16_t colBegin,
                       int32_t rowEnd,   int16_t colEnd,
                       I_Visitor & visitor);

protected:
    void cleanStraddling();
};

#endif // COMMON__TEXT__HXX
