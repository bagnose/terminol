// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/para.hxx"

namespace {

utf8::Seq encode(const char *ch) {
    utf8::Seq seq;
    auto length = utf8::leadLength(ch[0]);

    for (uint8_t i = 0; i != length; ++i) {
        seq.bytes[i] = ch[i];
    }

    return seq;
}

void enforce(const Para & para, const uint8_t *string) {
    auto     p   = string;
    uint32_t pos = 0;

    while (*p != '\0') {
        utf8::Seq seq;
        auto length = utf8::leadLength(*p);

        for (uint8_t i = 0; i != length; ++i) {
            seq.bytes[i] = *p++;
        }

        auto cell = para.getCell(pos);

        ENFORCE(seq == cell.seq, pos << ": '" << seq << "' != '" << cell.seq << "'");
        ++pos;
    }
}

struct Violation {};

void terminateHandler() {
    throw Violation();
}

} // namespace

int main() {
    {
        // Construct an empty Para and populate it.

        Para para;
        ENFORCE(para.getLength() == 0, "");

        para.setCell(0, Cell::utf8(encode(u8"<")));
        enforce(para, reinterpret_cast<const uint8_t *>(u8"<"));
        ENFORCE(para.getLength() == 1, "");

        para.setCell(2, Cell::utf8(encode(u8">")));
        enforce(para, reinterpret_cast<const uint8_t *>(u8"< >"));
        ENFORCE(para.getLength() == 3, "");

        para.setCell(0, Cell::utf8(encode(u8"≤")));
        enforce(para, reinterpret_cast<const uint8_t *>(u8"≤ >"));
        ENFORCE(para.getLength() == 3, "");

        para.setCell(2, Cell::utf8(encode(u8"≥")));
        enforce(para, reinterpret_cast<const uint8_t *>(u8"≤ ≥"));
        ENFORCE(para.getLength() == 3, "");

        //ENFORCE(para.getCell(2) == Cell(Style(), encode(u8"≥")), "");
        para.truncate(1);
        ENFORCE(para.getLength() == 1, "");
    }

    {
        // Construct a Para from styles and string.

        std::vector<Style> styles(8);
        std::vector<uint8_t> string;
        auto str = reinterpret_cast<const uint8_t *>(u8"òőło-ȯụŏ");     // 8 code points
        std::copy(str, str + strlen(reinterpret_cast<const char *>(str)), std::back_inserter(string));

        Para para(styles, string);
        enforce(para, str);

        auto b = para.getStringAtOffset(0);
        auto e = para.getStringAtOffset(8);

        ENFORCE(std::equal(b, e, str), "");
    }

#if DEBUG
    {
        // Construct a para from inconsistent styles and string (should fail).

        auto oldHandler = setTerminate(&terminateHandler);
        auto success = false;

        try {
            std::vector<Style> styles(1);
            std::vector<uint8_t> string;
            Para(styles, string);
        }
        catch (const Violation &) {
            success = true;
        }

        setTerminate(oldHandler);

        ENFORCE(success, "");
    }
#endif

    return 0;
}
