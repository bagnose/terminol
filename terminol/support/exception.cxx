#include "terminol/support/exception.hxx"
#include "terminol/support/debug.hxx"

#include <sstream>

namespace {

    thread_local const char * global_file = nullptr;
    thread_local int          global_line = 0;

} // namespace

void Exception::set_thread_location(const char * file, int line) {
    ASSERT(file && line != 0, );
    global_file = file;
    global_line = line;
}

std::string Exception::get_thread_location() {
    if (global_file) {
        ASSERT(global_line != 0, );
        std::ostringstream ost;
        ost << global_file << ':' << global_line;
        global_file = nullptr;
        global_line = 0;
        return ost.str();
    }
    else {
        return "";
    }
}
