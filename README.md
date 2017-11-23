# Terminol #

A simple Linux/X11 VT220 terminal emulator featuring:

 - reflowed unicode text

 - enormous (millions of lines) history support and deduplication

 - client-server mode (optional)

 - user-defined key-bindings

 - 24-bit color

 - lean and mean implementation

# Quickstart #

Obtain the terminol source code, either by downloading and unpacking a package file,
or by cloning the repository:

    git clone https://github.com/bagnose/terminol.git
    cd terminol

Build terminol (this requires pcre, xkbcommon, xcb, pango, cairo and a C++11 compiler):

    # Establish a debug/GCC build directory (run 'configure' without any arguments
    # to see other options):
    ./configure ../terminol-debug-gnu debug gnu
    cd ../terminol-debug-gnu
    # (edit Makefile and set any variables, refer to top of common.mak)
    make

Create a configuration file (see doc/sample-config).
At the bare minimum you need to specify the font:

    mkdir -p ~/.config/terminol
    cat << EOF > ~/.config/terminol/config
    set font-name "Meslo LG M"
    set font-size 14
    EOF

Launch terminol:

    # Launch a standalone window:
    ./dist/bin/terminol
    
    # or launch the server/daemon:
    ./dist/bin/terminols
    
    # and start windows with:
    ./dist/bin/terminolc

Install terminol:

    # This will copy the terminol binaries into ${INSTALLDIR}/bin:
    make INSTALLDIR=/usr/local install

# Upcoming Features #

 - highlighting and actions for user defined (regex) patterns

 - reverse search
