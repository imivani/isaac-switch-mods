# Iteration 4

This revision addresses the user's invisible Breakfast and Psy Fly pictures and
Time Machine regression, and adds the requested menus and small-map cleanup.
Local drawing, controller, item-placement and installation tests have passed.
The user has now confirmed that the installed update works after testing on the Switch. The final build and deployment records state
which exact files were checked and installed.

## Changes

- Corrected the artwork paths in all 98 item animations. If an animation cannot
  load, its normal game picture remains visible instead of disappearing.
- Time Machine now recognizes extinguished fires. Burning fires and other
  unsafe situations still prevent acceleration. Its Mods page explains why it
  is currently waiting or shows its current speed.
- Clean Minimap hides the frame around the small map; the map itself and the
  expanded map keep their normal drawing.
- The old six-item shortcut opens an item picker containing the 721 actual
  items in this version. The list is alphabetical, has type filters, and shows
  the selected item's description. A places one free pedestal at a time.
- The main menu has an X: Mods shortcut after selecting a save. Ten features
  can be switched on or off. The same list is available from the item picker.
  Switching Enhanced Boss Bars off restores the game's normal health bars.

The menu pauses gameplay while open. Close it and release the controls to
resume. A disconnected controller closes the menu. Mod choices last until
Isaac is fully closed; they are not saved between launches.

See [controls](item-menu.md), [console checks](iteration-4-testing.md),
[map and boss-bar evidence](clean-map-port.md), and
[backup and installation behavior](refined-installer.md).

## Evidence and limits

The item-menu tests exercise both its logic and the actual native callbacks;
the description renderer checks all 721 entries. Animation checks cover the
corrected file paths, loaded-picture fallback, and actual drawing callbacks.
Time Machine tests reproduce the original extinguished-fire failure. The
installer's 93 local cases exercise the actual transaction with simulated
transfers, including restoration after failure. These are local checks, not
evidence that the new code has run on the Switch.

The resource update changes the two copies of 98 animation files and removes
six blank boss-bar replacements so the stock bars can return when requested.
The remaining 892 resource files are unchanged. The previous installed pack
is archived before replacement. Antibirth music, saves, and the original game
dump are outside this update's replacement operation.

This is a native implementation for the user's exact Repentance 1.7.9b dump.
It does not enable arbitrary PC Lua mods or claim complete upstream EID parity.
