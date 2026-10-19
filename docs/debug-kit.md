# Deliberate item shortcut

**Historical instructions for iteration 3.** In iteration 4 (`combined-refined`),
this same shortcut opens the [item selection menu](item-menu.md). It no longer
automatically creates the six items described below.

In an unpaused, cleared room, hold **both stick buttons and D-pad Down together for two seconds**. The short progress message changes to a count of spawned items.

The six free pedestals are Brimstone, Tammy's Head, Sacred Heart, Holy Mantle, Pyromaniac and Car Battery. Pick up whichever items you want. The shortcut does not replace your inventory or enable invincibility. This set contains no self-damaging weapons; existing items or character mechanics can still change the result.

Release all three buttons before using it again. A ten-second cooldown prevents immediate repeats. Pausing, changing rooms or changing players cancels an unfinished hold. An already held chord when the mod starts does not activate it. Co-op partners cannot combine buttons from different controllers.

Pedestals use the game's own creation and empty-position routines. The adapter rechecks empty floor and nearby entities before each placement. If the room has insufficient space, the message reports how many of the six were actually placed. Nothing happens in an uncleared room or while controls are disabled.

Native input evidence: `evidence/eid-reminder-input/device/constants.json` and `evidence/iteration3/native-core/3f9b6c.asm.txt`. Device IDs are 1 through 8 and 33 (handheld). Physical buttons are 10/13 for stick clicks and 3 for D-pad Down. The device ID is an identifier, never an array index or bit shift.

Calls and item IDs are guarded against the exact original NRO and item XML. SpawnEx takes its final explicit unsigned argument on the stack; FindFreePickupSpawnPosition returns the two float components in s0/s1. Elapsed milliseconds come from the game's native clock, so Time Machine cannot shorten the hold.

Host tests exercise the exact hold/cooldown state machine. Actual button handling, placement and gameplay still require the console test.
