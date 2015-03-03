// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/escape.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

#include <iostream>
using namespace std;

#include <unistd.h>

void fillChar(int rows, int cols, char c) {
    for (int i = 0; i != rows; ++i) {
        cout << CsiEsc::CUP(i + 1, 1);

        for (int j = 0; j != cols; ++j) {
            cout << c;
        }
    }
    cout << flush;
}

int main() {
    cout << CsiEsc::DECCOLM(CsiEsc::Cols::_80);
    fillChar(24, 80, 'X');
    cout << CsiEsc::CUP(2, 1);
    cout << CsiEsc::DECSTBM(2, 23);
    cout << flush;

    usleep(1000000);

    /*
    cout << CsiEsc::DECCOLM(CsiEsc::Cols::_80) << flush;
    usleep(1000000);
    cout << CsiEsc::DECCOLM(CsiEsc::Cols::_132) << flush;
    usleep(1000000);
    cout << CsiEsc::DECCOLM(CsiEsc::Cols::_80) << flush;
    */

    return 0;
}
