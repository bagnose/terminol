// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/test.hxx"
#include "terminol/support/math.hxx"

int main() {
    Test test("support/math");

    test.assertEqual(0, divideRoundUp(0, 10),  "0 div-round-up 10.");
    test.assertEqual(1, divideRoundUp(1, 10),  "1 div-round-up 10.");
    test.assertEqual(1, divideRoundUp(10, 10), "10 div-round-up 10.");
    test.assertEqual(2, divideRoundUp(11, 10), "11 div-round-up 10.");
}
