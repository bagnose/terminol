terminol
========

A simple terminal emulator currently under heavy development.

Implemented:

 - UTF-8 support

 - 24-bit fg/bg

 - Scrollback de-duplication

 - Infinite scrollback

 - XCB front-end

 - Client/server model (spawn multiple windows from single process)

Major features yet to be implemented:

 - Configuration file support (currently hard-coded in config.{h,c}xx)

 - Text re-flow

 - Filtering support

Minor features yet to be implemented:

 - Hide pointer when typing

 - Selection enhancements

   - Block select

   - Selection expansion with cut-chars

 - Shared backing store

 - Unix domain sockets for client/server (currently uses cheapo named pipes)

 - UTF-8 substitution fonts (or does pango take care of that automatically?)
