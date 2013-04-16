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


Improvements:

- UTF-8 handling. Rejection of bad sequences and resynchronisation.
  State machine approach?
  Two states: stream/untrusted  --  trusted/verified.

- Maybe interlocutor is doing too much in places and is inconsistent.
  For example, it dispatches control chars as-is, but it interprets
  escape codes and translates them into higher level ops.

- When reading from tty, don't read indefinitely. Need a scheme
  based on maximum time or maximum data (UTF-8 or any?) before
  we return so that a redraw is possible.

- Separate horizontal cursor control from vertical cursor control?
  Sometimes it one is abs where the other is rel, for example LF.
