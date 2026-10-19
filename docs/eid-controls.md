# Using the expanded descriptions

Walk near an item, card, pill, dice-room symbol, sacrifice spike or supported
special object. Its description appears in the same compact style as the
working item display. Unknown pills remain unknown, and concealed items stay
concealed.

In a sacrifice room, the panel tells you what the **next sacrifice** can give.
It uses the game's real counter, including when you return to the room.

To review what you are carrying:

- Hold **Map**.
- Keep Map held and press **shoot left/right** to move through your character,
  active items, pocket cards or pills, equipped trinkets and other held items.
- Release Map to return to nearby descriptions.
- If you have a full Bag of Crafting, its ready-to-craft item appears first.

The reminder now accepts handheld controllers and all eight connected
controller slots. It previously skipped handheld input entirely, which is why
holding Map could show nothing. A held direction moves one page per press;
release and press it again to keep browsing. In co-op, when the first player
releases Map while another player still holds it, the panel switches to that
other player's character and belongings.

These names refer to your game controls. If you changed your button bindings,
use whichever buttons perform those actions. The game still receives your
normal movement and shooting inputs while the panel is open.

Near a pedestal affected by **Flip**, hold Map to read the item on the other
side. Near a **Glitched Crown** pedestal, press Map to browse its possible
items; with Spindown Dice, browsing advances when you release Map. These
pedestal previews take priority over your held-item list. Move away from that
pedestal to review your inventory.

Long descriptions change pages automatically. Leave the same description open
to read its remaining pages.

This update also adds item quality, transformation progress, selected charge
information, character-specific Birthright descriptions, golden-trinket and
Mom's Box effects, and selected character/item interactions. Tainted Cain's
pedestals have salvage information, with the game's unlock restrictions kept.

The full PC mod still has features this version does not have. In particular,
there is **no full crafting recipe search**, no complete list of every possible
synergy, and no settings or language menu. The held-item list does not yet show
gulped trinkets, wisps or items absorbed by Void. The complete status list is
[eid-coverage.md](eid-coverage.md).

The item display and sacrifice-room descriptions were tested by the user on
the Switch. The Map fix passes tests that run the real reminder code against
simulated game input and inventory. This repaired control path still needs a
check during actual play after the next build is installed.
