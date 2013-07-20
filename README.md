terminol
========

A simple terminal emulator currently under heavy development.

Requires xkbcommon, xcb, pango, cairo and a C++11 compiler.

Brief build instructions (see notes below on setting up a configuration file
or your fonts might suck):

    # Build
    ./configure ./build debug gnu
    cd build
    make
    # launch a standalone window:
    ./dist/bin/terminol
    # or launch the daemon (FIFO is /tmp/terminols-${USER}, by default)
    ./dist/bin/terminols
    # and start windows with:
    ./dist/bin/terminolc

    # Install binaries
    make INSTALLDIR=/usr/local install

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
    #
    # Search order:
    #   ${XDG_CONFIG_HOME}/terminol/config
    #   ${XDG_CONFIG_DIRS...}/terminol/config
    #   ${HOME}/.config/terminol/config
    
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
