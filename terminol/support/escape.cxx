// vi:noai:sw=4

#include "terminol/support/escape.hxx"

namespace {

const char * const ESC_TABLE[] = {
    "\033[0m",      // reset

    "\033[1m",      // bold
    "\033[2m",
    "\033[3m",
    "\033[4m",
    "\033[5m",
    "\033[7m",
    "\033[8m",

    "\033[22m",     // cl weight
    "\033[23m",
    "\033[24m",
    "\033[25m",
    "\033[27m",
    "\033[28m",
    "\033[29m",

    "\033[30m",     // fg black
    "\033[31m",     // fg red
    "\033[32m",     // fg green
    "\033[33m",
    "\033[34m",
    "\033[35m",
    "\033[36m",
    "\033[37m",

    "\033[40m",     // bg black
    "\033[41m",     // bg red
    "\033[42m",     // bg green
    "\033[43m",
    "\033[44m",
    "\033[45m",
    "\033[46m",
    "\033[47m"
};

} // namespace {anonymous}

std::ostream &
operator << (std::ostream & ost, Esc e) {
    ost << ESC_TABLE[static_cast<size_t>(e)];
    return ost;
}
