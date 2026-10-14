---
area: scaling
type: added
audience: admins
title: The Journey preset moves creatures, quests, instances and the Dungeon Finder, with a level window
---
With `CoAContentScaling.Progression.Mode = Journey` the loaded creature templates take the journey's levels at
startup - by the map most of their spawns are on, heroics and raids at the endgame - with gold and, on the way, the old
world's base statistics. Quests take the levels of the map whoever starts them stands on, raised to their equipment
reward's level. Instances open at their journey entry, the Dungeon Finder offers every dungeon from there to the cap,
and Outland, Northrend and their normal dungeons scale for a character. The level window
(`CoAContentScaling.Journey.Window.*`) places a lifted open-world creature by its place in its zone. Needs the level
window and scaling scope hooks of the `coa-scaling` core branch.
