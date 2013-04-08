// vi:noai:sw=4

#include "terminol/buffer.hxx"
#include "terminol/common.hxx"

void testRawBuffer() {
    size_t size = 4;

    RawBuffer buffer(size, 2 * size);

    buffer.insertChar(Char::ascii('x'), 0, 0);
    buffer.insertChar(Char::ascii('y'), 0, 1);
    buffer.insertChar(Char::ascii('z'), 0, 2);

    buffer.insertChar(Char::ascii('a'), 2, 0);
    buffer.insertChar(Char::ascii('b'), 2, 1);
    buffer.insertChar(Char::ascii('c'), 2, 2);
    buffer.insertChar(Char::ascii('d'), 2, 3);

    buffer.insertChar(Char::ascii('l'), 3, 0);
    buffer.insertChar(Char::ascii('s'), 3, 1);
    buffer.insertChar(Char::ascii(' '), 3, 2);
    buffer.insertChar(Char::ascii('-'), 3, 3);
    buffer.insertChar(Char::ascii('a'), 3, 4);
    buffer.insertChar(Char::ascii('l'), 3, 5);

    for (int i = 0; i < 8; ++i) {
        dumpRawBuffer(buffer);
        buffer.addLine();
    }
}

void testWrappedBuffer() {
#if 0
    size_t size = 4;

    WrappedBuffer buffer(80, size, 2 * size);

    buffer.insertChar(Char::ascii('x'), 0, 0);
    buffer.insertChar(Char::ascii('y'), 0, 1);
    buffer.insertChar(Char::ascii('z'), 0, 2);

    buffer.insertChar(Char::ascii('a'), 2, 0);
    buffer.insertChar(Char::ascii('b'), 2, 1);
    buffer.insertChar(Char::ascii('c'), 2, 2);
    buffer.insertChar(Char::ascii('d'), 2, 3);

    buffer.insertChar(Char::ascii('l'), 3, 0);
    buffer.insertChar(Char::ascii('s'), 3, 1);
    buffer.insertChar(Char::ascii(' '), 3, 2);
    buffer.insertChar(Char::ascii('-'), 3, 3);
    buffer.insertChar(Char::ascii('a'), 3, 4);
    buffer.insertChar(Char::ascii('l'), 3, 5);

    for (int i = 0; i < 8; ++i) {
        dumpWrappedBuffer(buffer);
        buffer.addLine();
    }
#endif
    WrappedBuffer buffer(4, 1, 1);

    buffer.insertChar(Char::ascii('a'), 0, 0);
    buffer.insertChar(Char::ascii('b'), 0, 1);
    buffer.insertChar(Char::ascii('c'), 0, 2);
    buffer.insertChar(Char::ascii('d'), 0, 3);

    dumpWrappedBuffer(buffer);
    buffer.setWrapCol(2);
    dumpWrappedBuffer(buffer);
    buffer.setWrapCol(1);
    dumpWrappedBuffer(buffer);
    buffer.setWrapCol(4);
    dumpWrappedBuffer(buffer);
}

int main() {
    //testRawBuffer();
    testWrappedBuffer();

    return 0;
}
