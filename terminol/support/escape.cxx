// vi:noai:sw=4

#include "terminol/support/escape.hxx"

namespace {

const char * const SGR_TABLE[] = {
    "\033[0m",      // reset all

    "\033[1m",      // bold
    "\033[2m",      // faint
    "\033[3m",      // italic
    "\033[4m",      // underline
    "\033[5m",      // blink slow
    "\033[6m",      // blink rapid
    "\033[7m",      // inverse / negative
    "\033[8m",      // conceal

    "\033[22m",     // clear weight
    "\033[23m",     // clear slant
    "\033[24m",     // clear underline
    "\033[25m",     // clear blink
    "\033[27m",     // clear inverse
    "\033[28m",     // clear conceal

    "\033[30m",     // fg black
    "\033[31m",     // fg red
    "\033[32m",     // fg green
    "\033[33m",     // fg yellow
    "\033[34m",     // fg blue
    "\033[35m",     // fg magenta
    "\033[36m",     // fg cyan
    "\033[37m",     // fg white

    "\033[39m",     // fg default

    "\033[40m",     // bg black
    "\033[41m",     // bg red
    "\033[42m",     // bg green
    "\033[43m",     // bg yellow
    "\033[44m",     // bg blue
    "\033[45m",     // bg magenta
    "\033[46m",     // bg cyan
    "\033[47m",     // bg white

    "\033[49m",     // bg default

    "\033[90m",     // fg bright black
    "\033[91m",     // fg bright red
    "\033[92m",     // fg bright green
    "\033[93m",     // fg bright yellow
    "\033[94m",     // fg bright blue
    "\033[95m",     // fg bright magenta
    "\033[96m",     // fg bright cyan
    "\033[97m",     // fg bright white

    "\033[100m",    // bg bright black
    "\033[101m",    // bg bright red
    "\033[102m",    // bg bright green
    "\033[103m",    // bg bright yellow
    "\033[104m",    // bg bright blue
    "\033[105m",    // bg bright magenta
    "\033[106m",    // bg bright cyan
    "\033[107m"     // bg bright white
};

} // namespace {anonymous}

std::ostream &
operator << (std::ostream & ost, SGR sgr) {
    ost << SGR_TABLE[static_cast<size_t>(sgr)];
    return ost;
}
