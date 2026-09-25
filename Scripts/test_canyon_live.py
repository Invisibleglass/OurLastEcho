"""
Live Play-In-Editor test for the Milestone 2 canyon, run INSIDE the open editor while a 2-player
Listen Server PIE session is running (like test_pie_live.py, but in its own PIE session because it
moves both characters all over the level).

Checks, for both Bat (Pawn) and Saraa (SpiritPawn), on the server world:
  - both stand on the landscape floor, the overlook shelf, its ramp and a PCG rock
  - the boundary volumes stop them at the wall faces, even high above the rim
    (so nobody can climb or jump out), and at both ends of the canyon
  - falling below the floor anywhere in the canyon respawns them (canyon kill volume)
  - no PCG rock sits on the Spirit Path
It also reports the average editor frame rate with both PIE worlds running.

Run from the editor console:
  py exec(open(r'<project>/Scripts/test_canyon_live.py').read())
Output lines are tagged ECHO_CANYON_TEST; the last line is PASS or FAIL.
"""
import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"

# Layout functions (centre_y, half_width, ground_height, ...) from the builder, without building
G = {"ECHO_CANYON_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_canyon.py").read(), G)

gs = unreal.GameplayStatics
T = {"failures": 0, "elapsed": 0.0, "frames": 0, "step": 0, "next": 0.0, "handle": None}
S = {}


def log(msg):
    unreal.log(f"ECHO_CANYON_TEST: {msg}")


def check(ok, text):
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")


def drop(actor, x, y, top, above=400.0):
    """Sweeps the actor down onto (x, y) from above `top`. Returns the resting Z"""
    actor.set_actor_location(unreal.Vector(x, y, top + above), False, True)
    actor.set_actor_location(unreal.Vector(x, y, top - 800), True, False)
    return actor.get_actor_location().z


def sweep(actor, start, end):
    """Teleports to start, sweeps toward end. Returns the final location"""
    actor.set_actor_location(unreal.Vector(*start), False, True)
    actor.set_actor_location(unreal.Vector(*end), True, False)
    return actor.get_actor_location()


def step_setup():
    pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
    server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
    if not server or len(pie) < 2:
        check(False, "need a 2-player Listen Server PIE session")
        return False
    S["world"] = server
    for c in gs.get_all_actors_of_class(server, unreal.OurLastEchoCharacter):
        S.setdefault("Saraa" if c.get_realm() == unreal.EchoRealm.SPIRIT else "Bat", c)
    check("Bat" in S and "Saraa" in S, "server has both Bat and Saraa")
    return "Bat" in S and "Saraa" in S


def step_surfaces():
    cy = G["centre_y"]
    for who in ("Bat", "Saraa"):
        ch = S[who]
        for x in (-9000.0, 6000.0, 17000.0):   # clear of the ravine and of The Crossing's chasm (Milestone 4)
            ground = G["ground_height"](x, cy(x))
            z = drop(ch, x, cy(x), ground)
            check(abs(z - 97 - ground) < 30, f"{who:5} stands on the canyon floor at x={x:.0f} (feet {z - 97:.0f}, ground {ground:.0f})")

        shelf_y = G["half_width"](1, 3700) - 450
        z = drop(ch, 3700, shelf_y, 900)
        check(abs(z - 97 - 900) < 30, f"{who:5} stands on the overlook shelf (feet {z - 97:.0f}, top 900)")

        top = G["face_point"](1, 4400, -330.0)
        foot = G["face_point"](1, 7000, -330.0)
        mx, my = (top[0] + foot[0]) / 2, (top[1] + foot[1]) / 2
        z = drop(ch, mx, my, 450)
        check(300 < z - 97 < 600, f"{who:5} stands on the overlook ramp halfway up (feet {z - 97:.0f}, expect ~450)")

    # The tallest round boulder out on the open floor: spheres have a centred pivot, so dropping
    # onto the instance position is guaranteed to hit the rock (and away from the walls, nothing else)
    cy = G["centre_y"]
    rocks = [r for r in S.get("rocks", []) if abs(r[1] - cy(r[0])) < 900 and r[4] == "Sphere"]
    check(bool(rocks), f"found open-floor round boulders to stand on ({len(rocks)})")
    if rocks:
        rx, ry, rz, top, _ = max(rocks, key=lambda r: r[3] - G["ground_height"](r[0], r[1]))
        ground = G["ground_height"](rx, ry)
        z = drop(S["Saraa"], rx, ry, top + 200)
        check(z - 97 > ground + 40, f"Saraa stands on a PCG rock (feet {z - 97:.0f}, ground {ground:.0f})")
        z = drop(S["Bat"], rx, ry, top + 200)
        check(z - 97 > ground + 40, f"Bat   stands on a PCG rock (feet {z - 97:.0f}, ground {ground:.0f})")


# The boundary is a chain of straight 18 m boxes at 14 m stations, so along curves and where the
# canyon widens it sits up to ~1.5 m off the ideal face line - still well inside the bottom rock
# stratum (32 m deep). A sweep that gets further than this past the face means a gap.
MAX_PAST_FACE = 250.0


def step_bounds():
    cy, hw = G["centre_y"], G["half_width"]
    # Every 5 m along the whole canyon, end cap to end cap
    stations = [x for x in range(int(G["X_START_END"]) + 500, int(G["X_FAR_END"]) - 400, 500)]
    for who in ("Bat", "Saraa"):
        ch = S[who]
        worst = {}
        for side in (-1, 1):
            for x in stations:
                nx, ny = G["outward"](side, x)
                w = hw(side, x)
                for z in (1500.0, 9000.0):   # partway up the cliff, and above every rim
                    start = (x, cy(x), z)
                    end = (x + nx * (w + 5000), cy(x) + ny * (w + 5000), z)
                    loc = sweep(ch, start, end)
                    reached = (loc.x - x) * nx + (loc.y - cy(x)) * ny
                    key = ("L" if side < 0 else "R", z)
                    if reached - w > worst.get(key, (-1e9, 0))[0]:
                        worst[key] = (reached - w, x)
        for (side, z), (over, x) in sorted(worst.items()):
            check(over < MAX_PAST_FACE,
                  f"{who:5} held by the {side} wall at z={z:.0f} at all {len(stations)} stations "
                  f"(furthest: {over:.0f} cm past the face line at x={x}, limit {MAX_PAST_FACE:.0f})")

        for label, x_end, d in (("start", G["X_START_END"], -1), ("far", G["X_FAR_END"], 1)):
            for z_off in (200.0, 9000.0):
                x0 = x_end - d * 3000
                z = G["ground_height"](x0, cy(x0)) + z_off if z_off < 1000 else z_off
                loc = sweep(ch, (x0, cy(x0), z), (x_end + d * 5000, cy(x_end + d * 5000), z))
                past = (loc.x - x_end) * d
                check(past < 0, f"{who:5} stopped at the {label} end at height {z_off:.0f} ({past:.0f} cm past the end line, must be < 0)")


def step_kill_volume_drop():
    cy = G["centre_y"]
    S["Bat"].set_actor_location(unreal.Vector(12000, cy(12000), -2100), False, True)
    S["Saraa"].set_actor_location(unreal.Vector(-12000, cy(-12000), -2100), False, True)


def step_kill_volume_check():
    for who in ("Bat", "Saraa"):
        loc = S[who].get_actor_location()
        # Their respawn point: the start, or the last checkpoint they passed (Milestone 3's climb base)
        want = S[who].get_respawn_transform().translation
        check((loc - want).length() < 150 and loc.z > 0,
              f"{who:5} fell below the canyon floor far away and respawned at their respawn point ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")


def collect_rocks(world):
    """(x, y, z, top, mesh name) of every PCG rock instance in the given world"""
    rocks = []
    for vol in gs.get_all_actors_of_class(world, unreal.PCGVolume):
        for ism in vol.get_components_by_class(unreal.InstancedStaticMeshComponent):
            mesh = ism.get_editor_property("static_mesh")
            box = mesh.get_bounding_box() if mesh else None
            for i in range(ism.get_instance_count()):
                t = ism.get_instance_transform(i, True)
                half = (box.max.z - box.min.z) / 2 * t.scale3d.z if box else 50.0
                rocks.append((t.translation.x, t.translation.y, t.translation.z, t.translation.z + half, mesh.get_name() if mesh else ""))
    return rocks


def step_rocks():
    rocks = collect_rocks(S["world"])
    S["rocks"] = rocks
    check(len(rocks) > 200, f"PCG rocks present in the game world: {len(rocks)}")
    on_path = [r for r in rocks if -2000 < r[0] < 4100 and abs(r[1]) < 1100]
    check(not on_path, f"no rocks on the Spirit Path (found {len(on_path)})")



STEPS = [
    (1.0, step_setup),
    (0.2, step_rocks),
    (0.2, step_surfaces),
    (0.2, step_bounds),
    (0.2, step_kill_volume_drop),
    (3.0, step_kill_volume_check),
]


def finish():
    unreal.unregister_slate_post_tick_callback(T["handle"])
    log(f"editor frame rate while testing (2 PIE worlds): {T['frames'] / max(T['elapsed'], 0.001):.1f} fps average over {T['elapsed']:.1f}s")
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")


def on_tick(delta):
    T["elapsed"] += delta
    T["frames"] += 1
    if T["elapsed"] < T["next"]:
        return
    delay, fn = STEPS[T["step"]]
    try:
        ok = fn()
    except Exception as e:
        import traceback
        check(False, f"{fn.__name__} crashed: {e!r} {traceback.format_exc()}")
        ok = False
    T["step"] += 1
    if ok is False or T["step"] >= len(STEPS):
        finish()
        return
    T["next"] = T["elapsed"] + STEPS[T["step"]][0]


# Note: the editor throttles itself to a few fps when it isn't the foreground window. For a
# meaningful fps figure, turn off Editor Preferences > Performance > "Use Less CPU when in
# Background" (bThrottleCPUWhenNotForeground) while this runs.
T["next"] = STEPS[0][0]
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
log("started")
