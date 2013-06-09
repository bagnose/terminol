// vi:noai:sw=4

#include "terminol/support/debug.hxx"
#include "terminol/support/escape.hxx"

int main() {
    std::cout << Esc::RESET_ALL << "Normal" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::BOLD << "Bold" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::ITALIC << "Italic" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::BOLD << Esc::ITALIC << "Bold/Italic" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::FAINT << "Faint" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::UNDERLINE << "Underline" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::BLINK_SLOW << "Blink slow" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::BLINK_RAPID << "Blink rapid" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::INVERSE << "Inverse" << Esc::RESET_ALL << std::endl;
    std::cout << Esc::CONCEAL << "Conceal" << Esc::RESET_ALL << std::endl;

    return 0;
}
