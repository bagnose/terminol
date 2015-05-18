// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/text.hxx"
#include "terminol/common/simple_repository.hxx"
#include "terminol/common/para_cache.hxx"
#include "terminol/support/test.hxx"

#include <algorithm>

namespace {

void write(Text & text, utf8::Seq seq, int16_t & row, int16_t & col) {
    ASSERT(row < text.getRows() && col < text.getCols(), "row/col out of range.");

    if (seq.lead() == '\n') {
        if (row + 1 == text.getRows()) {
            text.addLine(false);
        }
        else {
            ++row;
        }

        col = 0;
    }
    else {
        text.setCell(row, col, Cell::utf8(seq));

        ++col;

        if (col == text.getCols()) {
            if (row + 1 == text.getRows()) {
                text.addLine(true);
            }
            else {
                text.makeContinued(row);
                ++row;
            }

            col = 0;
        }
    }
}

void write(Text & text, const std::string & str, int16_t & row, int16_t & col) {
    auto p = str;

    auto iter = str.begin();

    while (iter != str.end()) {
        utf8::Seq seq;

        auto length = utf8::leadLength(*iter);
        for (uint8_t i = 0; i != length; ++i) {
            seq.bytes[i] = *iter++;
        }

        write(text, seq, row, col);
    }
}

std::string read(const Text & text, int16_t row, int16_t col, int32_t count) {
    std::string result;

    for (int32_t i = 0; i != count; ++i) {
        auto cell = text.getCell(row, col);
        auto seq  = cell.seq;

        std::copy(seq.bytes, seq.bytes + utf8::leadLength(seq.lead()),
                  std::back_inserter(result));

        ++col;

        if (col == text.getCols()) {
            ++row;
            col = 0;
        }
    }

    return result;
}

SimpleRepository repository;
ParaCache        paraCache(repository);

void testBasics(Test & test) {
    Text text(repository, paraCache, 1, 8, 0);
    std::string input = "hello";

    int16_t row = 0;
    int16_t col = 0;
    write(text, input.c_str(), row, col);

    auto output = read(text, 0, 0, input.size());

    test.assertEqual(input, output, "Write/read '" + input + "'.");
}

void testStraddlingPara(Test & test) {
    Text text(repository, paraCache, 1, 24, 0);
    // Note, this dash in this string is utf-8.
    std::string input =
        "It was a dark and stormy night; the rain fell in torrents — except " \
        "at occasional intervals, when it was checked by a violent gust of wind " \
        "which swept up the streets (for it is in London that our scene lies), " \
        "rattling along the housetops, and fiercely agitating the scanty flame " \
        "of the lamps that struggled against the darkness.";

    int16_t row = 0;
    int16_t col = 0;
    write(text, input.c_str(), row, col);
    //text.dumpCurrent(std::cout, true);

    {
        std::string subInput = "It was a dark and stormy";
        auto output = read(text, -13, 0, subInput.size());
        test.assertEqual(subInput, output, "Write/read '" + subInput + "'.");
    }

    {
        std::string subInput = "t the darkness.";  // Determined empirically.
        auto output = read(text, 0, 0, subInput.size());
        test.assertEqual(subInput, output, "Write/read '" + subInput + "'.");
    }
}

void testHistorical(Test & test) {
    Text text(repository, paraCache, 1, 24, 1);
    int16_t row = 0;
    int16_t col = 0;
    write(text, "hello\nworld", row, col);
    text.dumpHistory(std::cout, true);
    text.dumpCurrent(std::cout, true);
}

void testRfind(Test & test) {
    Text text(repository, paraCache, 3, 10, 0);

    int16_t row = 0;
    int16_t col = 0;
    write(text, "hello\nworld\n", row, col);
    //text.dumpCurrent(std::cout, true);

    Regex        regex("o");
    Text::Marker marker = text.end();
    bool         ongoing;

    // 3rd line
    auto matches = text.rfind(regex, marker, ongoing);
    test.assert(ongoing, "Ongoing (3rd line).");
    test.assert(matches.empty(), "No matches (3rd line).");

    // 2nd line
    matches = text.rfind(regex, marker, ongoing);
    test.assert(ongoing, "Ongoing (2nd line).");
    test.assert(matches.size() == 1, "One match (2nd line).");
    test.assert(matches[0].row()    == 1 &&
                matches[0].col()    == 1 &&
                matches[0].length() == 1, "Match specifics (2nd line).");

    // 1st line
    matches = text.rfind(regex, marker, ongoing);
    test.assert(ongoing, "Ongoing (1st line).");
    test.assert(matches.size() == 1, "One match (1st line).");
    test.assert(matches[0].row()    == 0 &&
                matches[0].col()    == 4 &&
                matches[0].length() == 1, "Match specifics (1st line).");

    // hit the end
    matches = text.rfind(regex, marker, ongoing);
    test.assert(!ongoing, "Not ongoing.");
}

} // namespace {anonymous}

int main() {
    Test test("common/text");
    //test.run("basics", testBasics);
    test.run("historical", testHistorical);
    //test.run("straddling-para", testStraddlingPara);
    //test.run("rfind", testRfind);
    return test.rval();
}
