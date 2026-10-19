# Planetarium Chance native port

This component translates the lifecycle and HUD behavior of **Planetarium
Chance** by **Sectimus and contributors** from Lua into portable C++.

- Upstream: https://github.com/Sectimus/isaac-planetarium-chance
- Revision: `b724ac3641e89e0aaa8b43a301ccb168a07d1d6a`
- Upstream version: 11.1
- License: GNU General Public License, version 3. The unmodified license text is
  retained in `LICENSE`.

The native game's existing `Level::GetPlanetariumChance` remains responsible for
its item, skipped-treasure-room, visited-planetarium and stage calculations.
This port does not replace that game function with a guessed formula.

Port additions include bounded native reads, run/floor identity caching,
unknown-state handling, a validated eligibility-record format, and portable
regression tests. No game code or assets are contained in this source directory.
