"""
Live Play-In-Editor test for the Milestone 1 mechanics, run INSIDE the open editor while a
2-player Listen Server PIE session is running (start PIE first, e.g. via the MCP StartPIE tool).

It drives the characters on the server world (server-authoritative), then checks what the
client world sees, so it covers replication as well as the game logic:
  - spirit platform visibility per machine (Bat's screen hides them, Saraa's shows them)
  - spirit platform collision for both characters
  - switch ignores Bat, responds to Saraa; bridge raise replicates and animates on the client
  - pit respawn, end zone needs both players, completion replicates to the client
It also reports the average editor frame rate while PIE runs.

Run from the editor console:
  py exec(open(r'<project>/Scripts/test_pie_live.py').read())
Output lines are tagged ECHO_PIE in Saved/Logs/OurLastEcho.log; the last line is PASS or FAIL.
"""
import unreal

gs = unreal.GameplayStatics
T = {"failures": 0, "elapsed": 0.0, "frames": 0, "step": 0, "next": 0.0, "handle": None}


def log(msg):
    unreal.log(f"ECHO_PIE: {msg}")


def check(ok, text):
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")


def actors(world, cls):
    return list(gs.get_all_actors_of_class(world, cls))


def first(world, cls):
    found = actors(world, cls)
    return found[0] if found else None


def drop(actor, x, y, top):
    """Places the actor above (x, y) and sweeps it down. True if something stopped it above `top`"""
    actor.set_actor_location(unreal.Vector(x, y, top + 400), False, True)
    actor.set_actor_location(unreal.Vector(x, y, top - 600), True, False)
    return actor.get_actor_location().z > top


def by_realm(world):
    found = {}
    for c in actors(world, unreal.OurLastEchoCharacter):
        found.setdefault("Saraa" if c.get_realm() == unreal.EchoRealm.SPIRIT else "Bat", c)
    return found.get("Bat"), found.get("Saraa")


def local_realm(world):
    pawn = gs.get_player_pawn(world, 0)
    return pawn.get_realm() if pawn else None


def platforms_visible(world):
    return [p.get_editor_property("mesh").is_visible() for p in actors(world, unreal.EchoSpiritPlatform)]


# ------------------------------------------------------------------ setup

pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
client = next((w for w in pie if not unreal.SystemLibrary.is_server(w)), None)
S = {}


def step_setup():
    log(f"PIE worlds: {len(pie)} (server={server is not None}, client={client is not None})")
    if not (server and client):
        check(False, "need a 2-player Listen Server PIE session")
        return False
    S["bat"], S["saraa"] = by_realm(server)
    S["switch"] = first(server, unreal.EchoSpiritSwitch)
    S["bridge"] = first(server, unreal.EchoRisingBridge)
    check(S["bat"] is not None and S["saraa"] is not None, "server has both Bat and Saraa")
    check(local_realm(server) == unreal.EchoRealm.LIVING, "host (server window) plays Bat")
    check(local_realm(client) == unreal.EchoRealm.SPIRIT, "client window plays Saraa")

    s_vis, c_vis = platforms_visible(server), platforms_visible(client)
    check(len(s_vis) == 4 and not any(s_vis), f"Bat's machine: spirit platforms hidden {s_vis}")
    check(len(c_vis) == 4 and all(c_vis), f"Saraa's machine: spirit platforms visible {c_vis}")

    c_bridge = first(client, unreal.EchoRisingBridge)
    check(not c_bridge.get_editor_property("raised"), "bridge starts lowered on the client")
    z = c_bridge.get_editor_property("bridge_mesh").get_editor_property("relative_location").z
    check(z < -500, f"client bridge mesh starts down in the pit (rel z={z:.0f})")
    return True


def step_collision_and_switch():
    bat, saraa = S["bat"], S["saraa"]
    for surface, x, y, top, bat_lands, saraa_lands in [
        ("start floor", -750, 0, 0, True, True),
        ("spirit platform 1", 250, -300, 0, False, True),
        ("spirit platform 3", 1050, -300, 60, False, True),
        ("far side floor", 2600, 0, 0, True, True),
    ]:
        check(drop(bat, x, y, top) == bat_lands, f"Bat   on {surface:18} -> {'lands' if bat_lands else 'falls through'}")
        check(drop(saraa, x, y, top) == saraa_lands, f"Saraa on {surface:18} -> {'lands' if saraa_lands else 'falls through'}")

    check(not drop(bat, 800, 300, 0), "Bat falls where the bridge will be (still lowered)")
    drop(bat, 1900, -300, 0)
    check(not S["switch"].get_editor_property("activated"), "Bat stepping on the switch does nothing")
    drop(saraa, 1900, -300, 0)
    check(S["switch"].get_editor_property("activated"), "Saraa stepping on the switch activates it")
    check(S["bridge"].get_editor_property("raised"), "switch raises the bridge on the server")

    # Into the pit: the respawn volume should send Bat home while we wait for the bridge
    bat.set_actor_location(unreal.Vector(500, 0, -700), False, True)


def step_after_raise():
    bat = S["bat"]
    check(bat.get_actor_location().x < -1000, f"Bat fell into the pit and respawned at the start (x={bat.get_actor_location().x:.0f})")

    c_switch = first(client, unreal.EchoSpiritSwitch)
    c_bridge = first(client, unreal.EchoRisingBridge)
    check(c_switch.get_editor_property("activated"), "switch press replicated to the client")
    check(c_bridge.get_editor_property("raised"), "bridge raise replicated to the client")
    z = c_bridge.get_editor_property("bridge_mesh").get_editor_property("relative_location").z
    check(abs(z + 15) < 2, f"client bridge mesh finished rising (rel z={z:.0f})")
    s_z = S["bridge"].get_editor_property("bridge_mesh").get_editor_property("relative_location").z
    check(abs(s_z + 15) < 2, f"server bridge mesh finished rising (rel z={s_z:.0f})")

    check(drop(bat, 800, 300, 0), "Bat lands on the risen bridge")
    check(not any(platforms_visible(server)), "spirit platforms still hidden on Bat's machine")

    drop(bat, 3100, -150, 0)
    check(not gs.get_game_state(server).is_milestone_complete(), "Bat alone in the end zone does not complete")
    drop(S["saraa"], 3100, 150, 0)
    check(gs.get_game_state(server).is_milestone_complete(), "Bat + Saraa in the end zone completes (server)")


def step_client_complete():
    check(gs.get_game_state(client).is_milestone_complete(), "milestone completion replicated to the client")


# (delay before running, function)
STEPS = [(1.0, step_setup), (0.5, step_collision_and_switch), (4.5, step_after_raise), (1.0, step_client_complete)]


def finish():
    unreal.unregister_slate_post_tick_callback(T["handle"])
    if T["elapsed"] > 0:
        log(f"editor frame rate during test: {T['frames'] / T['elapsed']:.1f} fps average over {T['elapsed']:.1f}s")
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
        check(False, f"{fn.__name__} crashed: {e!r}")
        ok = False
    T["step"] += 1
    if ok is False or T["step"] >= len(STEPS):
        finish()
        return
    T["next"] = T["elapsed"] + STEPS[T["step"]][0]


T["next"] = STEPS[0][0]
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
log("started")
