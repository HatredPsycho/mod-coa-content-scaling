---
area: scaling
type: added
audience: admins
title: On the journey, the random bots follow it and the ZoneLevels addon shows it
---
The module answers the core's bot level plan hook: random bots stop at the endgame level, ride and fly when the
journey's trainers teach it and hunt each zone at its journey levels, Outland and Northrend included. It needs the
`coa-scaling` branch of the CoA mod-playerbots fork. The ZoneLevels client addon (`client/AddOns/ZoneLevels`, from
mod-world-journey) is told at login the journey's zone levels, the Dungeon Finder's levels and the lines of gems and
enchantments as they are now (`CoAContentScaling.Journey.Addon.Enable`, `CoAContentScaling.Journey.Tooltips.Enable`).
