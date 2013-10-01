# Terminol #

A simple Linux/X11 VT220 terminal emulator featuring:

 - reflowed unicode text

 - enormous (millions of lines) history support and deduplication

 - client-server mode (optional)

 - user-defined key-bindings

 - 24-bit color

 - lean and mean implementation

# Quickstart #

Build terminol (this requires pcre, xkbcommon, xcb, pango, cairo and a C++11 compiler).

    ./configure ./build-dir debug gnu
    cd build-dir
    make

Create a configuration file (see doc/sample-config).
At the bare minimum you need to specify the font:

    mkdir ~/.config/terminol
    cat << EOF > ~/.config/terminol/config
    set font-name                   "Meslo LG M"
    set font-size                   14
    EOF

Launch terminol

    # Launch a standalone window:
    ./dist/bin/terminol
    
    # or launch the server/daemon
    ./dist/bin/terminols
    
    # and start windows with:
    ./dist/bin/terminolc

Install terminol

    make INSTALLDIR=/usr/local install

# TODO #

 - highlighting and actions for user defined (regex) patterns

 - reverse search

# FAQ #

 * Why another terminal emulator?

 * How do I write a config file?

   See the annotated doc/sample-config
