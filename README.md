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

 - 24-bit color support.

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
    
    #set server-fork                 true
    #set socket-path                 /tmp/my-terminols.socket
    
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
    
    set font-name                   "Meslo LG M"
    set font-size                   14
    
    #set term-name                   xterm-256color
    #set scroll-with-history         false
    #set scroll-on-tty-output        false
    #set scroll-on-tty-key-press     true
    #set scroll-on-resize            false
    #set scroll-on-paste             false
    #set title                       ""
    #set icon                        ""
    #set chdir                       ""
    #set scroll-back-history         1048576
    #set frames-per-second           50
    #set sync-tty                    false
    #set trace-tty                   false
    #set initial-x                   -1
    #set initial-y                   -1
    #set initial-rows                24
    #set initial-columns             80
    
    set unlimited-scroll-back true
    
    # By default the selection is drawn with swapped fg/bg.
    # This can be overridden:
    #set select-fg-color             #000000
    #set select-bg-color             #55ccff
    
    # Similary, by default the cursor drawn with swapped fg/bg.
    # This can be overridden:
    #set cursor-text-color           #000000
    #set cursor-fill-color           #55ccff
    
    # By default the scrollbar fg is:
    #set scrollbar-fg-color          #7f7f7f
    # And bg is taken from the theme's background.
    #set scrollbar-bg-color          #030303
    #set scrollbar-width             6
    
    # set border-thickness 1
    # By default the border color is taken from the theme.
    #set border-color #ffff00
    
    #set double-click-timeout        400
    
    # Use these for compatibility with 'vttest':
    #set traditional-wrapping true
    
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
