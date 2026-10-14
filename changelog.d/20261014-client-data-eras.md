---
area: scaling
type: changed
audience: admins
title: Content eras come from the client data, and the expansion switches moved into this module
---
The generated content census, the TBC and WotLK content packs and the core patch files are gone. The era of a map comes
from `Map.dbc`, a quest's from the map of its zone, and instance sizes from `MapDifficulty.dbc`; the raid tiers are kept
in `InstanceProfile.cpp`. `CoATBC.Enable` and `CoAWotLK.Enable` are now `CoAContentScaling.TBC.Enable` and
`CoAContentScaling.WotLK.Enable`. Only dungeons and raids carry a combat budget; the open world keeps the base statistics
of the level a creature is moved to. The Dungeon Finder composition modes, `.lfgmode`, `.lfgchallenge`, `.coascale lfg`,
the CoALFGMode addon protocol and `character_coa_lfg_settings` are removed; `.coascale census` is now `.coascale eras`.
The module builds against the hook layer of the `coa-scaling` core branch.
