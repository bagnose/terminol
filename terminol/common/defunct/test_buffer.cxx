// vi:noai:sw=4

#include "terminol/common/buffer.hxx"
#include "terminol/support/support.hxx"

void testRawBuffer() {
    size_t size = 4;

    RawBuffer buffer(size, 2 * size);

    buffer.insertCell(Cell::ascii('x'), 0, 0);
    buffer.insertCell(Cell::ascii('y'), 0, 1);
    buffer.insertCell(Cell::ascii('z'), 0, 2);

    buffer.insertCell(Cell::ascii('a'), 2, 0);
    buffer.insertCell(Cell::ascii('b'), 2, 1);
    buffer.insertCell(Cell::ascii('c'), 2, 2);
    buffer.insertCell(Cell::ascii('d'), 2, 3);

    buffer.insertCell(Cell::ascii('l'), 3, 0);
    buffer.insertCell(Cell::ascii('s'), 3, 1);
    buffer.insertCell(Cell::ascii(' '), 3, 2);
    buffer.insertCell(Cell::ascii('-'), 3, 3);
    buffer.insertCell(Cell::ascii('a'), 3, 4);
    buffer.insertCell(Cell::ascii('l'), 3, 5);

    for (int i = 0; i < 8; ++i) {
        dumpRawBuffer(buffer);
        buffer.addLine();
    }
}

void testWrappedBuffer() {
#if 0
    size_t size = 4;

    WrappedBuffer buffer(80, size, 2 * size);

    buffer.insertCell(Cell::ascii('x'), 0, 0);
    buffer.insertCell(Cell::ascii('y'), 0, 1);
    buffer.insertCell(Cell::ascii('z'), 0, 2);

    buffer.insertCell(Cell::ascii('a'), 2, 0);
    buffer.insertCell(Cell::ascii('b'), 2, 1);
    buffer.insertCell(Cell::ascii('c'), 2, 2);
    buffer.insertCell(Cell::ascii('d'), 2, 3);

    buffer.insertCell(Cell::ascii('l'), 3, 0);
    buffer.insertCell(Cell::ascii('s'), 3, 1);
    buffer.insertCell(Cell::ascii(' '), 3, 2);
    buffer.insertCell(Cell::ascii('-'), 3, 3);
    buffer.insertCell(Cell::ascii('a'), 3, 4);
    buffer.insertCell(Cell::ascii('l'), 3, 5);

    for (int i = 0; i < 8; ++i) {
        dumpWrappedBuffer(buffer);
        buffer.addLine();
    }
#endif
    WrappedBuffer buffer(4, 1, 1);

    buffer.insertCell(Cell::ascii('a'), 0, 0);
    buffer.insertCell(Cell::ascii('b'), 0, 1);
    buffer.insertCell(Cell::ascii('c'), 0, 2);
    buffer.insertCell(Cell::ascii('d'), 0, 3);

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
