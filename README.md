# PEX-gtk3
built using gtk3.0, UI with glade, and drawings with cairo.

dependencies:
- gtk (>= 3.0, < 4.10)

### Currently it has:
- an okay layout that kinda lets you change sizes
- a text log that documents incoming messages and trader disconnects
  - text log shows most recent messages unless u scroll up by a certain amount then it stays there
- a bottom bar that lets you masquarade a (non-disconnected) trader
  - this always sends valid messages
  - can manually disconnect a trader too
- a quit button

### What it will have when I finish:
- a [bid ask spread chart](https://www.google.com/search?q=bid+ask+spread+chart) for every item
- something on the top left 
