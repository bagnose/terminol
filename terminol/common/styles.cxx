// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/escape.hxx"

int main() {
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << "Normal"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::BOLD) << "Bold"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::ITALIC) << "Italic"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::BOLD) << CsiEsc::SGR(CsiEsc::StockSGR::ITALIC)
              << "Bold/Italic" << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::FAINT) << "Faint"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::UNDERLINE) << "Underline"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::BLINK_SLOW) << "Blink slow"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::BLINK_RAPID) << "Blink rapid"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::INVERSE) << "Inverse"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;
    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::CONCEAL) << "Conceal"
              << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL) << std::endl;

    return 0;
}
