# External Item Descriptions source data

The generated description text and item names come from **External Item
Descriptions** by **wofsauge and contributors**:

- Source: https://github.com/wofsauge/External-Item-Descriptions
- Pinned revision: `8b59d00c6828e333294639ad2b2b6875eae821cb`
- Original credits: `UPSTREAM-CREDITS.md`, copied unchanged from that revision.
- Imported files: `descriptions/ab+/en_us.lua`, `descriptions/rep/en_us.lua`,
  and `descriptions/names/en_us.lua`.

No LICENSE or COPYING file was present in that EID revision. These local
generated files retain upstream attribution and do not assert a new license
over the descriptions. No files have been published or uploaded by this work.

# Native-port research reference

The source of **IsaacExternalItemDescriptionsiOS** by **emp0ry** was reviewed to
compare native behavior and identify fields worth checking in the Switch build:

- Source: https://github.com/emp0ry/IsaacExternalItemDescriptionsiOS
- Pinned revision: `5e3cfa5a75163c7f7cbf19f33f5769f500122f51`
- Its bridge code is MIT licensed; its EID description data is separately
  attributed. Its author-permission statement concerns its own bundled data.

The new portable importer and C++ files in this directory were written for this
Switch project. They do not embed the iOS bridge or UIKit code. iOS offsets are
research leads, not validated Switch addresses.
