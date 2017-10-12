// vi:noai:sw=4
// Copyright © 2016 David Bryant

#include "terminol/common/buffer2.hxx"
#include "terminol/common/simple_repository.hxx"
#include "terminol/support/debug.hxx"

#include <unistd.h>

int main() {
    SimpleRepository   repository;
    const CharSub      CS_US;
    const CharSubArray charSubArray{{{&CS_US, &CS_US, &CS_US, &CS_US}}};
    Buffer2 buffer(repository, 25, 80, 0, charSubArray);

    return 0;
}
