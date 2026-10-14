---
area: scaling
type: added
audience: admins
title: On the journey, moved creatures' spells shrink with them and items follow the journey plan
---
A creature the journey moved still casts the numbers of its original level; its spell damage and harmful damage over
time are now cut by what a blow is worth at its new level against its original one, for its totems and summons too.
Heals, scripted lethal damage and percentage-of-health damage keep their size. Items follow the journey's plan: the
required level follows the journey, the item level follows that required level, Outland's and Northrend's top rewards
sit on the endgame ladder (`CoAContentScaling.Journey.Endgame.*.ItemLevels`), the old world's raid rewards stay as they
are. Instance item level requirements are read on the same ladder.
