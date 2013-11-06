// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/support/escape.hxx"

int main() {
    std::cout << SGR::RESET_ALL << "Normal" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::BOLD << "Bold" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::ITALIC << "Italic" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::BOLD << SGR::ITALIC << "Bold/Italic" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::FAINT << "Faint" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::UNDERLINE << "Underline" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::BLINK_SLOW << "Blink slow" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::BLINK_RAPID << "Blink rapid" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::INVERSE << "Inverse" << SGR::RESET_ALL << std::endl;
    std::cout << SGR::CONCEAL << "Conceal" << SGR::RESET_ALL << std::endl;

    return 0;
}
