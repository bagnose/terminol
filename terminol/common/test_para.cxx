// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/para.hxx"
#include "terminol/support/test.hxx"

// This overload is declared in namespace 'std' to satisfy clang,
// see http://clang.llvm.org/compatibility.html#dep_lookup
//
// Otherwise we get:
//   call to function 'operator<<' that is neither visible
//   in the template definition nor found by argument-dependent lookup
//
// Note, extending namespace 'std' is considered bad, but this is just
// a unit test. An alternative to extending 'std' is to declare the function
// *before* #including para.hxx (which pulls in stringify()).
namespace std {

ostream & operator << (ostream & ost, const vector<uint8_t> & vec);

ostream & operator << (ostream & ost, const vector<uint8_t> & vec) {
    for (auto c : vec) {
        ost << c;
    }
    return ost;
}

} // namespace std

namespace {

utf8::Seq encode(const char * ch) noexcept {
    utf8::Seq seq;
    auto length = utf8::leadLength(ch[0]);

    for (uint8_t i = 0; i != length; ++i) {
        seq.bytes[i] = ch[i];
    }

    return seq;
}

std::vector<uint8_t> vec(const char * str) {
    std::vector<uint8_t> rval;

    while (*str != '\0') {
        rval.push_back(*str);
        ++str;
    }

    return rval;
}

void test1(Test & test) {
    // Construct an empty Para and populate it.

    Para para;
    test.enforceEqual(para.getLength(), 0u, "");

    para.setCell(0, Cell::utf8(encode(u8"<")));
    test.enforceEqual(para.getString(), vec(u8"<"), "");
    test.enforceEqual(para.getLength(), 1u, "");

    para.setCell(2, Cell::utf8(encode(u8">")));
    test.enforceEqual(para.getString(), vec(u8"< >"), "");
    test.enforceEqual(para.getLength(), 3u, "");

    para.setCell(0, Cell::utf8(encode(u8"≤")));
    test.enforceEqual(para.getString(), vec(u8"≤ >"), "");
    test.enforceEqual(para.getLength(), 3u, "");

    para.setCell(2, Cell::utf8(encode(u8"≥")));
    test.enforceEqual(para.getString(), vec(u8"≤ ≥"), "");
    test.enforceEqual(para.getLength(), 3u, "");

    para.truncate(1);
    test.enforceEqual(para.getLength(), 1u, "");
}

void test2(Test & test) {
    {
        Para para;
        para.setCell(0, Cell::ascii('a'));
        test.enforceEqual(para.getString(), vec("a"), "");
        para.insertCell(0, 1, Cell::ascii('b'));
        test.enforceEqual(para.getString(), vec("b"), "");
    }

    {
        Para para;
        para.setCell(0, Cell::ascii('a'));
        para.setCell(1, Cell::ascii('b'));
        test.enforceEqual(para.getString(), vec("ab"), "");
        para.insertCell(0, 1, Cell::ascii('c'));
        test.enforceEqual(para.getString(), vec("cb"), "");
    }

    {
        Para para;
        para.setCell(0, Cell::ascii('a'));
        para.setCell(1, Cell::ascii('b'));
        para.setCell(2, Cell::ascii('c'));
        test.enforceEqual(para.getString(), vec("abc"), "");
        para.insertCell(1, 2, Cell::ascii('d'));
        test.enforceEqual(para.getString(), vec("adc"), "");
    }
}

void test3(Test & test) {
    // Construct a Para from styles and string.

    std::vector<Style> styles(8);
    std::vector<uint8_t> string = vec(u8"òőło-ȯụŏ");

    Para para(styles, string);
    test.enforceEqual(para.getString(), string, "");

    auto b = para.getStringAtOffset(0);
    auto e = para.getStringAtOffset(8);

    test.enforceEqual(std::vector<uint8_t>(b, e), string, "");
}

void test4(Test & test) {
#if DEBUG
    struct Violation {
        static void terminateHandler() {
            throw Violation();
        }
    };

    // Construct a para from inconsistent styles and string (should fail).

    auto oldHandler = setTerminate(&Violation::terminateHandler);
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

    test.enforce(success, "");
#endif
}

} // namespace {anonymous}

int main() {
    Test test("common/para");
    test.run("1", &test1);
    test.run("2", &test2);
    test.run("3", &test3);
    test.run("4", &test4);

    return 0;
}
