---
area: scaling
type: fixed
audience: admins
title: On the journey, early spawns and quest rewards no longer keep authored levels
---
Creatures the transports, battlefields and outdoor PvP spawn before the world is initialized were made from the
authored templates and kept their authored levels (2690 on a fresh start); they are made again from the journey's.
A quest's equipment reward is read at its journey level, so an Outland quest with a reward made for 60 no longer climbs
back to 60. The module also warns at startup when CoA.CreatureScaling.Enable is on beside content scaling.
