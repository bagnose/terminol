Features:

- UTF-8

- Scrollback (option for infinite)

- Scroll history de-dupe across terminal instances.

- Architect for tabs

- Linux centric, backend-agnostic. Port to Xlib, Wayland, etc

- Design for spawn from daemon process (ala urxvt)

- Resizing horizontally changes line-wrapping (urxvt vs xterm)

- Extensions, highlight text regions, clickable for actions

- Support alternative screen (change scrolling, unlike urxvt)

- Mini view, ala sublime edit

- Block select

- Hide pointer when typing.

Decisions:

- Write in C++11. Maybe port to D later.

- Extension language: lua? python?

- Backing store

  - None: Lowest memory usage. Flickery during redraw / updates.

  - Per terminal, always: Least flicker, highest memory usage.

  - Per terminal, while mapped: Less memory usage, greater delay on re-map.

  - Shared. Store is size of largest dimensions of all terminals.

    - Just used to eliminate flicker of update - not used for scrolling.

    - Used more extensively, e.g., use for scrolling if still valid for
      current terminal, etc.
      This is the most complex to implement.

Note:

- How to have a test-suite?

- yaourt -S st / yaourt -S st-git

- grab virtual boxes from: http://virtualboxes.org/images/centos/

Links:

- http://rtfm.etla.org/xterm/ctlseq.html

- http://www.vt100.net/docs/
