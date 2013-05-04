terminol
========

A simple terminal emulator

BUGS:

 - vim command 'dd'

TODO:

 - Shortcuts

   - Paste

 - Damage: extend simple buffer to store damage as operations are performed on it.
   The damage will be expressed as damageBegin/damageEnd, for each line.

   Have a --no-damage flag to make updates redraw the entire buffer for debugging.

 - Make the terminal or the buffer drive the rendering (instead of the window driving it).

 - Selection support

   - Modes

     - Regular

     - Single line (e.g. hold SHIFT and selection is constrained to single line)

     - Block (e.g. CTRL and selection is constrained to block - trailing space is trimmed)

   - Primary buffer (automatic)

   - Clipboard (triggered by shortcut, e.g. CTRL-SHIFT-C)

 - Paste

   - Middle button paste (from primary buffer)

   - Clipboard (e.g. CTRL-SHIFT-V)

 - Alternative screen

 - Scroll-back

 - Text re-flow

 - Fancy stuff
