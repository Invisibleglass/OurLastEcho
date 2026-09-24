"""
Live Play-In-Editor test for Milestone 3 (spirit bow, echo platforms, The Climb). Run INSIDE the open editor
while a fresh 2-player Listen Server PIE session is running:
  py exec(open(r'<project>/Scripts/test_climb_live.py').read())
Output lines are tagged ECHO_CLIMB_TEST; the last line is PASS or FAIL.

Bat is the listen-server host, so the server world is his machine and the client world is Saraa's. Shots go
through the bow's own server path (FireAt) from where Bat stands, so walls and the rock screen really block them.
Saraa's climb is done with REAL jumps on her client: the test holds movement input toward the next platform and
presses jump at the platform edge, exactly like a player, and the server has to agree where she lands.
"""
import math

import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
C = {"ECHO_CLIMB_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_climb.py").read(), C)
G = C["G"]

gs = unreal.GameplayStatics
T = {"failures": 0, "checks": 0, "wait": 0.0, "elapsed": 0.0, "handle": None, "gen": None}
HALF_HEIGHT = 90.0


def log(msg):
    unreal.log(f"ECHO_CLIMB_TEST: {msg}")


def check(ok, text):
    T["checks"] += 1
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def v(x, y, z):
    return unreal.Vector(x, y, z)


def drop(actor, x, y, top, above=300.0):
    actor.set_actor_location(v(x, y, top + above), False, True)
    actor.set_actor_location(v(x, y, top - 800), True, False)
    return actor.get_actor_location().z - HALF_HEIGHT


def by_label(world, cls, prefix):
    return sorted((a for a in gs.get_all_actors_of_class(world, cls) if a.get_actor_label().startswith(prefix)),
                  key=lambda a: a.get_actor_label())


def top_of(platform):
    return platform.get_actor_location()


def place(actor, x, y, ground=None):
    """Stand a character on the floor at (x, y), facing along +X"""
    z = (G["ground_height"](x, y) if ground is None else ground) + HALF_HEIGHT + 5
    actor.set_actor_location(v(x, y, z), False, True)


def shoot(bow, target, wait_frames=True):
    return bow.fire_at(target)


def scenario():
    pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
    server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
    client = next((w for w in pie if not unreal.SystemLibrary.is_server(w)), None)
    if not (server and client):
        check(False, "need a 2-player Listen Server PIE session")
        return

    chars = {}
    for c in gs.get_all_actors_of_class(server, unreal.OurLastEchoCharacter):
        chars["Saraa" if c.get_realm() == unreal.EchoRealm.SPIRIT else "Bat"] = c
    bat, saraa = chars["Bat"], chars["Saraa"]
    saraa_c = gs.get_player_pawn(client, 0)
    bat_bow = bat.get_editor_property("spirit_bow")
    saraa_bow = saraa.get_editor_property("spirit_bow")

    P = by_label(server, unreal.EchoPlatform, "EchoPlatform_")
    Pc = by_label(client, unreal.EchoPlatform, "EchoPlatform_")
    check(len(P) == 7 and len(Pc) == 7, f"7 echo platforms on both machines ({len(P)}, {len(Pc)})")
    timed = [p for p in P if p.get_editor_property("timed")]
    check(len(timed) == 1 and "Timed" in timed[0].get_actor_label(), f"exactly one timed platform ({[p.get_actor_label() for p in timed]})")
    L = C["layout"]()

    # ---------------------------------------------------------------- the bow belongs to Bat
    check(bat_bow.can_use_bow() and not saraa_bow.can_use_bow(), "only Bat can use the spirit bow")
    bat_parts = len(bat.get_components_by_class(unreal.StaticMeshComponent))
    saraa_parts = len(saraa.get_components_by_class(unreal.StaticMeshComponent))
    check(bat_parts - saraa_parts == 6, f"Bat carries the 6-part placeholder bow, Saraa doesn't ({bat_parts} vs {saraa_parts} mesh components)")
    check(saraa_bow.fire_at(bat.get_actor_location()) is None, "the server refuses to fire Saraa's bow")

    imc = unreal.load_asset("/Game/Input/IMC_Bow.IMC_Bow")   # EditorAssetLibrary refuses to run during PIE
    keys = sorted(f"{m.get_editor_property('action').get_name()}:{m.get_editor_property('key').get_editor_property('key_name')}"
                  for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"))
    check(keys == ["IA_Aim:Gamepad_LeftTrigger", "IA_Aim:RightMouseButton", "IA_Fire:Gamepad_RightTrigger", "IA_Fire:LeftMouseButton"],
          f"IMC_Bow maps aim and fire for mouse and gamepad {keys}")

    # ---------------------------------------------------------------- aiming
    boom = bat.get_editor_property("camera_boom")
    move = bat.get_editor_property("character_movement")
    normal_arm = boom.get_editor_property("target_arm_length")
    bat_bow.set_aiming(True)
    yield 1.0
    arm = boom.get_editor_property("target_arm_length")
    check(bat_bow.is_aiming(), "Bat is aiming")
    check(abs(arm - bat_bow.get_editor_property("aim_camera_distance")) < 25, f"aim camera pulls in over the shoulder ({normal_arm:.0f} -> {arm:.0f} cm)")
    check(abs(move.get_editor_property("max_walk_speed") - bat_bow.get_editor_property("aim_walk_speed")) < 1, f"aiming slows Bat to {move.get_editor_property('max_walk_speed'):.0f} cm/s")
    check(bat.get_editor_property("use_controller_rotation_yaw"), "aiming Bat faces where he aims")
    bat_c = next(c for c in gs.get_all_actors_of_class(client, unreal.OurLastEchoCharacter) if c.get_realm() == unreal.EchoRealm.LIVING)
    check(bat_c.get_editor_property("spirit_bow").is_aiming(), "Saraa's machine sees Bat aiming (replicated)")

    fired1 = bat_bow.fire()
    fired2 = bat_bow.fire()
    check(fired1 and not fired2, f"cooldown: first shot fires, an immediate second shot doesn't ({fired1}, {fired2})")
    yield 0.2
    arrows_s = gs.get_all_actors_of_class(server, unreal.EchoArrow)
    arrows_c = gs.get_all_actors_of_class(client, unreal.EchoArrow)
    check(len(arrows_s) >= 1 and len(arrows_c) >= 1, f"Bat's arrow exists on both machines (server {len(arrows_s)}, client {len(arrows_c)})")
    if arrows_c:
        a0 = arrows_c[0].get_actor_location()
        yield 0.15
        a1 = arrows_c[0].get_actor_location() if unreal.SystemLibrary.is_valid(arrows_c[0]) else a0
        check((a1 - a0).length() > 50 or arrows_c[0].is_stuck(), f"the arrow flies on Saraa's machine ({(a1 - a0).length():.0f} cm in 0.15 s)")
    bat_bow.set_aiming(False)
    yield 1.0
    check(abs(boom.get_editor_property("target_arm_length") - normal_arm) < 25 and move.get_editor_property("max_walk_speed") > 400,
          "releasing aim restores the camera and walk speed")

    # ---------------------------------------------------------------- dormant platforms
    yield 0.5
    for p, pc in zip(P, Pc):
        if not check(p.is_outline_visible_locally() and not p.is_slab_visible_locally() and not pc.is_outline_visible_locally() and not pc.is_slab_visible_locally(),
                     f"{p.get_actor_label()} dormant: Bat sees the outline only, Saraa sees nothing"):
            break
    t1 = top_of(P[0])
    check(drop(bat, t1.x, t1.y, t1.z) < t1.z - 50, "nobody stands on a dormant platform: Bat falls through")
    check(drop(saraa, t1.x, t1.y, t1.z) < t1.z - 50, "nobody stands on a dormant platform: Saraa falls through")

    # ---------------------------------------------------------------- waking P1 from in front of it
    fx, fy = C["wall_xy"](L["stations"][0], 1600)
    place(bat, fx, fy)
    yield 0.3
    cues_s, cues_c = P[0].get_editor_property("local_awaken_cues"), Pc[0].get_editor_property("local_awaken_cues")
    check(bat_bow.fire_at(t1) is not None, "Bat shoots P1 from the canyon floor")
    yield 1.2
    check(P[0].is_awake() and Pc[0].is_awake(), "P1 is awake on both machines")
    check(Pc[0].is_slab_visible_locally() and not P[0].is_slab_visible_locally(), "awake P1: Saraa sees the blue slab, Bat doesn't")
    check(P[0].is_outline_visible_locally() and not Pc[0].is_outline_visible_locally(), "awake P1: Bat still sees a faint outline")
    check(P[0].get_editor_property("local_awaken_cues") > cues_s and Pc[0].get_editor_property("local_awaken_cues") > cues_c,
          "the wake-up glow and sound played on both machines")
    check(abs(drop(saraa, t1.x, t1.y, t1.z) - t1.z) < 15, "Saraa can stand on awake P1")
    check(drop(bat, t1.x, t1.y, t1.z) < t1.z - 50, "Bat still falls through awake P1")

    # ---------------------------------------------------------------- the rock screen: P6 only from behind it
    t6 = top_of(P[5])
    blocked = 0
    # Open floor squarely in front of the screen (the corridor mouth, where diagonal shots can sneak in, is the "spot")
    spots = [(L["stations"][5] - 150, 1800), (L["stations"][5], 1600), (L["stations"][5] + 200, 2200), (L["stations"][6], 1500)]
    for s, u in spots:
        x, y = C["wall_xy"](s, u)
        place(bat, x, y)
        yield 0.1
        bat_bow.fire_at(t6)
        yield 1.4
        blocked += 0 if P[5].is_awake() else 1
    check(blocked == len(spots), f"P6 can't be woken from the open floor in front of the screen ({blocked}/{len(spots)} shots blocked)")
    sx, sy = C["wall_xy"](L["screen_s0"] + 40, 250)
    place(bat, sx, sy)
    yield 0.1
    bat_bow.fire_at(t6)
    yield 1.4
    check(P[5].is_awake(), "P6 wakes when Bat shoots from the gap behind the screen")

    # ---------------------------------------------------------------- wake the rest (P3 is timed, so last)
    for index, spot_u in ((1, 1500), (3, 1500), (4, 1500)):
        x, y = C["wall_xy"](L["stations"][index], spot_u)
        place(bat, x, y)
        yield 0.1
        bat_bow.fire_at(top_of(P[index]))
        yield 1.3
    place(bat, sx, sy)
    yield 0.1
    bat_bow.fire_at(top_of(P[6]))
    yield 1.4
    check(all(P[i].is_awake() for i in (0, 1, 3, 4, 5, 6)), f"all permanent platforms awake {[P[i].is_awake() for i in range(7)]}")

    # ---------------------------------------------------------------- the timed platform
    x, y = C["wall_xy"](L["stations"][2], 1500)
    place(bat, x, y)
    yield 0.1
    bat_bow.fire_at(top_of(P[2]))
    yield 1.3
    duration = P[2].get_editor_property("awake_duration")
    check(P[2].is_awake() and Pc[2].is_awake(), f"timed P3 wakes ({duration:.0f} s)")
    remaining = Pc[2].get_remaining_awake_time()
    check(0 < remaining <= duration, f"Saraa's machine knows how long P3 has left ({remaining:.1f} s)")
    yield remaining + 0.8
    check(not P[2].is_awake() and not Pc[2].is_awake(), "timed P3 went dormant again on both machines")
    t3 = top_of(P[2])
    check(drop(saraa, t3.x, t3.y, t3.z) < t3.z - 50, "Saraa falls through P3 once it's dormant")
    check(all(P[i].is_awake() for i in (0, 1, 3, 4, 5, 6)), "the permanent platforms stayed awake")

    # ---------------------------------------------------------------- Saraa climbs with real jumps
    x, y = C["wall_xy"](L["stations"][2], 1500)
    place(bat, x, y)
    yield 0.1
    bat_bow.fire_at(top_of(P[2]))
    yield 1.0
    tops = [top_of(p) for p in P]
    saraa.set_actor_location(v(tops[0].x, tops[0].y, tops[0].z + HALF_HEIGHT + 5), False, True)
    yield 1.5   # let Saraa's client take the server's teleport before she starts running
    targets = tops[1:]
    ledge_x, ledge_y = C["wall_xy"](L["ledge_s0"] + 150, C["PLATFORMS"][-1][0])
    ledge_top = targets[-1].z + C["RISE"]
    targets.append(v(ledge_x, ledge_y, ledge_top))
    names = [p.get_actor_label() for p in P[1:]] + ["the ledge"]
    froms = tops[:]
    timed_top = top_of(P[2])
    timed_spot = C["wall_xy"](L["stations"][2], 1500)
    for name, target, frm in zip(names, targets, froms):
        # Up to 3 tries per hop, like a player would; a miss drops her to the floor, so she's put back on the previous step
        for attempt in range(1, 4):
            if (target - timed_top).length() < 1:
                # The timed platform: Bat wakes it right before she jumps (that's the coordination the brief asks for)
                place(bat, timed_spot[0], timed_spot[1])
                yield 0.1
                bat_bow.fire_at(timed_top)
                yield 1.0
            landed = yield from hop(saraa_c, saraa, target)
            if landed:
                break
            saraa.set_actor_location(v(frm.x, frm.y, frm.z + HALF_HEIGHT + 5), False, True)
            yield 1.5
        if not check(landed, f"Saraa jumps up to {name} (try {attempt}; feet at {saraa.get_actor_location().z - HALF_HEIGHT:.0f}, top {target.z:.0f})"):
            break

    # ---------------------------------------------------------------- the switch lowers the ramp for Bat
    switch = by_label(server, unreal.EchoSpiritSwitch, "Climb_Switch")[0]
    ramp = by_label(server, unreal.EchoRisingBridge, "Climb_Ramp")[0]
    ramp_c = by_label(client, unreal.EchoRisingBridge, "Climb_Ramp")[0]
    check(not ramp.is_raised(), "the ramp starts stowed")
    sw = switch.get_actor_location()
    drop(bat, sw.x, sw.y, sw.z)   # Bat can't reach it and it ignores him anyway
    yield 0.3
    check(not switch.get_editor_property("activated"), "the climb switch ignores Bat")
    saraa.set_actor_location(v(sw.x, sw.y, sw.z + HALF_HEIGHT + 20), False, True)
    yield 0.5
    check(switch.get_editor_property("activated") and ramp.is_raised(), "Saraa's switch lowers the ramp")
    yield 3.8
    check(ramp_c.is_raised() and ramp_c.get_raise_progress() > 0.99, f"the ramp finished lowering on Saraa's machine too ({ramp_c.get_raise_progress():.2f})")

    # Bat walks up the ramp from its foot
    # From 2.5 m beyond the ramp's foot, straight up it and 4 m onto the ledge (the ramp's +X runs down, away from the hinge)
    hinge = ramp.get_actor_location()
    down = ramp.get_actor_forward_vector()
    down = v(down.x, down.y, 0).normal()
    run = C["RAMP"]["run"]
    place(bat, hinge.x + down.x * (run + 250), hinge.y + down.y * (run + 250))
    yield 0.4
    goal = v(hinge.x - down.x * 400, hinge.y - down.y * 400, 0)
    walked = False
    for _ in range(400):
        loc = bat.get_actor_location()
        d = v(goal.x - loc.x, goal.y - loc.y, 0)
        if d.length() < 150:
            walked = True
            break
        bat.add_movement_input(d.normal(), 1.0, False)
        yield 0
    feet = bat.get_actor_location().z - HALF_HEIGHT
    check(walked and abs(feet - ledge_top) < 40, f"Bat walks up the ramp onto the ledge (feet {feet:.0f}, ledge {ledge_top:.0f})")

    # ---------------------------------------------------------------- the end zone on the ledge
    zone = by_label(server, unreal.EchoEndZone, "EndZone")[0]
    z = zone.get_actor_location()
    state_s, state_c = gs.get_game_state(server), gs.get_game_state(client)
    check(abs(z.z - ledge_top) < 5, f"the end zone is on the ledge ({z.z:.0f})")
    check(not state_s.is_milestone_complete(), "not complete before both are in the end zone")
    drop(bat, z.x - 150, z.y, z.z)
    drop(saraa, z.x + 150, z.y, z.z)
    yield 0.6
    check(state_s.is_milestone_complete() and state_c.is_milestone_complete(), "both players in the end zone on the ledge completes it (both machines)")

    # ---------------------------------------------------------------- checkpoint at the climb base
    cp = by_label(server, unreal.EchoCheckpoint, "Climb_Checkpoint")[0]
    want = cp.get_respawn_transform().translation
    saraa.set_actor_location(v(12000, G["centre_y"](12000), -2100), False, True)
    yield 3.0
    got = saraa.get_actor_location()
    check((got - want).length() < 150, f"Saraa falling after the checkpoint respawns at the climb base ({got.x:.0f}, {got.y:.0f})")

    # ---------------------------------------------------------------- debug command
    for p in P:
        if p.is_awake():
            p.sleep()
    yield 0.6
    sp_s = gs.get_all_actors_of_class(server, unreal.EchoSpiritPlatform)
    check(not any(s.get_editor_property("mesh").is_visible() for s in sp_s), "Bat can't see Saraa's spirit platforms normally")
    # From Python every call runs locally (the editor's script guard), so a client->server RPC can't be tested here:
    # request it on the host. Typing EchoShowAllPlatforms in either player's console uses the real RPC path.
    bat.request_show_all_platforms(True)
    yield 0.8
    check(state_s.is_debug_show_all_platforms() and state_c.is_debug_show_all_platforms(), "EchoShowAllPlatforms turns on for both machines")
    check(all(s.get_editor_property("mesh").is_visible() for s in sp_s), "debug: Bat now sees the spirit platforms")
    check(all(pc.is_outline_visible_locally() for pc in Pc), "debug: Saraa now sees every dormant echo platform")
    bat.request_show_all_platforms(False)
    yield 0.8
    check(not state_c.is_debug_show_all_platforms() and not any(pc.is_outline_visible_locally() for pc in Pc), "debug view turns off again")


def hop(saraa_c, saraa_s, target):
    """Real jump on Saraa's client: run toward the target, jump near the edge, keep steering until she lands"""
    start = saraa_c.get_actor_location()
    move = saraa_c.get_movement_component()
    jumped = False
    for frame in range(240):
        loc = saraa_c.get_actor_location()
        to = v(target.x - loc.x, target.y - loc.y, 0)
        if to.length() > 25:
            saraa_c.add_movement_input(to.normal(), 1.0, False)
        run = v(loc.x - start.x, loc.y - start.y, 0).length()
        if not jumped and run > 10:   # jump early in the run-up, with the whole platform still underfoot
            saraa_c.jump()
            jumped = True
        elif jumped and frame > 3 and not move.is_falling() and to.length() < 200:
            break
        yield 0
    saraa_c.stop_jumping()
    yield 0.5   # let the server catch up
    s = saraa_s.get_actor_location()
    return abs(s.z - HALF_HEIGHT - target.z) < 20 and v(s.x - target.x, s.y - target.y, 0).length() < 180


def on_tick(delta):
    T["elapsed"] += delta
    if T["wait"] > 0:
        T["wait"] -= delta
        return
    try:
        T["wait"] = next(T["gen"]) or 0.0
    except StopIteration:
        finish()
    except Exception as e:
        import traceback
        check(False, f"crashed: {e!r} {traceback.format_exc()}")
        finish()


def finish():
    unreal.unregister_slate_post_tick_callback(T["handle"])
    log(f"{T['checks']} checks in {T['elapsed']:.0f}s")
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")


T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
log("started")
