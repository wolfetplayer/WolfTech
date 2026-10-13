# Spline Movers

A curved, constant-speed alternative to chaining `path_corner`s with `func_train`.
Plain trains move in dead-straight legs with a hard direction change at every
corner; a spline leg bows smoothly between its endpoints instead, which is the
right tool for trams, boats, cable cars, or anything else that shouldn't visibly
"snap" direction mid-route.

Each leg is built from a pair of `info_train_spline_main` nodes and driven at
runtime by the `followspline` mapscript command. A route with more than one
curved leg is built by issuing several `followspline` commands in sequence
(the same way `gotomarker` is already used one leg at a time).

## Building a leg

1. Place two `info_train_spline_main` entities where the leg should start and end.
2. Give the start node a `targetname`, and set its `target` to the end node's `targetname`.
3. Optionally, place up to 4 ordinary `path_corner` entities off to the side of
   the straight line between the two nodes, and reference them from the start
   node via `control` / `control2` / `control3` / `control4`. The curve bows
   toward each control point in order; leave them out for a straight leg.

```
// a gentle curve from spline_a to spline_b, bowing toward corner_mid
{
"classname" "info_train_spline_main"
"targetname" "spline_a"
"target" "spline_b"
"control" "corner_mid"
"origin" "0 0 0"
}
{
"classname" "path_corner"
"targetname" "corner_mid"
"origin" "256 128 0"
}
{
"classname" "info_train_spline_main"
"targetname" "spline_b"
"origin" "512 0 0"
}
```

## The `followspline` script command

```
followspline <backward 0|1> <spline targetname> <speed> [roll <start> <end>] [wait]
```

- `backward` — `0` travels from the leg's start node toward its target, `1` reverses that.
- `<spline targetname>` — the `targetname` of the leg's **start** `info_train_spline_main` node.
- `<speed>` — world units per second along the curve.
- `roll <start> <end>` — optional banking: the mover's roll angle lerps from `start` to `end` over the leg.
- `wait` — if present, the command blocks until the leg finishes before the script continues; otherwise the script moves on immediately and the engine polls for arrival in the background (same as `gotomarker`).

Only works on an entity that is already an `ET_MOVER` brush (e.g. a `func_static`)
— place the command in that entity's own script block, not on the path nodes.

```
// func_static named "tram", scripted to glide along the "spline_a" -> "spline_b" leg
tram:
	followspline 0 spline_a 120 roll -5 5 wait
```

## Current limitations

- One leg per `followspline` call — chain multiple calls for a longer, multi-leg route.
- No multi-car/rider support — a leg moves exactly one mover; there's no equivalent
  of a train with several carriages trailing a fixed distance behind each other yet.
- Bots have no awareness of these movers — they won't path onto or across one.
