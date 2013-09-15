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
    
    set server-fork true
    
    set color-scheme solarized-dark
    # other options:
    #   linux
    #   rxvt
    #   tango
    #   xterm
    #   zenburn-dark
    #   zenburn
    #   solarized-dark
    #   solarized-light
    
    set font-name "Meslo LG M"
    set font-size 14
    
    # term-name
    # scroll-with-history, scroll-on-tty-output, scroll-on-tty-key-press
    # scroll-on-resize, scroll-on-paste
    # title, icon, chdir
    # scroll-back-history, unlimited-scroll-back
    # sync-tty, trace-tty
    
    set unlimited-scroll-back true
    
    # set border-thickness 4
    # set border-color #ffff00
    
    # Use these for compatibility with 'vttest':
    # set traditional-wrapping true
    
    bindsym ctrl+0                  local-font-reset
    bindsym ctrl+minus              local-font-smaller
    bindsym ctrl+equal              local-font-bigger
    
    bindsym ctrl+shift+parenright   global-font-reset
    bindsym ctrl+shift+underscore   global-font-smaller
    bindsym ctrl+shift+plus         global-font-bigger
    
    bindsym ctrl+shift+C            copy-to-clipboard
    bindsym ctrl+shift+V            paste-from-clipboard
    
    bindsym shift+Up                scroll-up-line
    bindsym shift+Down              scroll-down-line
    bindsym shift+Page_Up           scroll-up-page
    bindsym shift+Page_Down         scroll-down-page
    bindsym shift+Home              scroll-top
    bindsym shift+End               scroll-bottom
    
    bindsym shift+F4                clear-history
    
    bindsym shift+F5                debug-global-tags
    bindsym shift+F6                debug-local-tags
    bindsym shift+F7                debug-history
    bindsym shift+F8                debug-active
    
    bindsym shift+F9                debug-modes
    bindsym shift+F10               debug-selection
    bindsym shift+F11               debug-stats
    bindsym shift+F12               debug-stats2
