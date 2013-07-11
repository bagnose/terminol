terminol
========

A simple terminal emulator currently under heavy development.

Requires bub (bottom-up-build), xkb, xcb, pango, cairo and a C++11
compiler (uses clang by default - you may want to edit bub.cfg and
change CC/CXX to gcc/g++, respectively).

Brief build instructions:

    # Build only
    make MODE=debug BUILDDIR=build/debug
    # launch a standalone window:
    ./build/debug/dist/bin/terminol
    # or launch the daemon (FIFO is /tmp/terminols-${USER}, by default)
    ./build/debug/dist/bin/terminols
    # and start windows with:
    ./build/debug/dist/bin/terminolc

    # Install binaries
    make MODE=debug BUILDDIR=build/debug INSTALLDIR=/usr/local install

Noteworthy Features:

 - Reflowed text.

 - Scrollback history deduplication.

 - 24-bit colour support.

 - Client server model (run terminols and then terminolc to spawn windows).

 - Lean and mean.

To come:

 - Wayland support (designed with portable front-end).

 - Filtering (regex highlight of patterns with associated actions).

 - More...

You may wish to create a config file like the following:

    # ~/.config/terminol/config
    
    set serverFork true
    
    set colorScheme solarized-dark
    # other options:
    #   linux
    #   rxvt
    #   tango
    #   xterm
    #   zenburn-dark
    #   zenburn
    #   solarized-dark
    #   solarized-light

    set fontName MesloLGM
    set fontSize 11
    
    # termName
    # scrollWithHistory, scrollOnTtyOutput, scrollOnTtyKeyPress
    # scrollOnResize, scrollOnPaste
    # title, icon, chdir
    # scrollBackHistory, unlimitedScrollBack
    # syncTty, traceTty
    
    # set scrollBackHistory   1024
    # set resizeStrategy      preserve
    set unlimitedScrollBack true
    set resizeStrategy      reflow
    set reflowHistory       1024
    
    # set borderThickness 4
    # set borderColor #ffff00
    
    # Use these for compatibility with 'vttest':
    # set resizeStrategy      clip
    # set traditionalWrapping true
