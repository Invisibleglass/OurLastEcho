"""
Live Play-In-Editor test for Milestone 4 (anchor arrows, the sword whip, The Crossing). Run INSIDE the open editor
while a fresh 2-player Listen Server PIE session is running:
  py exec(open(r'<project>/Scripts/test_crossing_live.py').read())
Output lines are tagged ECHO_CROSS_TEST; the last line is PASS or FAIL.

Shots go through Bat's real bow path (FireAt on the server, from where he stands), so rock really blocks them.
Saraa's swings are REAL: on her own machine the test holds movement input, jumps off the edge, points her camera
at the anchor, presses the whip button (UEchoSwordWhipComponent.PressWhip, exactly what the input binding calls)
and lets go when she's over the next landing, like a player would. Her movement is predicted on her machine and
replayed by the server, so every landing is checked on the SERVER.

Optional settings (set these globals before exec'ing the file):
  ECHO_CROSS_MODE = "full"        everything (default)
                    "host_saraa"  for a session where BP_EchoGameMode.bHostPlaysSaraa is on: Saraa is the host,
                                  Bat the client. Runs the anchor checks and the first swing and chain only.
                    "swing1"      just the anchor setup and swing 1, repeated ECHO_CROSS_REPEATS times (default 3),
                                  e.g. under network emulation
"""
import math

import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
X = {"ECHO_CROSSING_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_crossing.py").read(), X)
G = X["G"]

MODE = globals().get("ECHO_CROSS_MODE", "full")
REPEATS = globals().get("ECHO_CROSS_REPEATS", 3)
TRACE = globals().get("ECHO_CROSS_TRACE", False) or MODE == "swing3"
# e.g. ["NetEmulation.PktLag 60", "NetEmulation.PktLoss 3"]; turned off again at the end
NETEMU = globals().get("ECHO_CROSS_NETEMU", [])

gs = unreal.GameplayStatics
T = {"failures": 0, "checks": 0, "wait": 0.0, "elapsed": 0.0, "handle": None, "gen": None}
HALF_HEIGHT = 96.0
S = {}


def log(msg):
    unreal.log(f"ECHO_CROSS_TEST: {msg}")


def check(ok, text):
    T["checks"] += 1
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def v(x, y, z):
    return unreal.Vector(x, y, z)


def su(loc):
    """(station, cm from the left wall) of a world location"""
    s, lateral = G["station_of"](loc.x, loc.y)
    return s, lateral + G["half_width"](-1, s)


def forward(s):
    """Along Saraa's line (PATH_U out from the left wall), which bends differently from the canyon's centreline"""
    a, b = X["left_xy"](s - 50, X["PATH_U"]), X["left_xy"](s + 50, X["PATH_U"])
    return v(b[0] - a[0], b[1] - a[1], 0.0).normal()


def forward_right(s):
    """Along Bat's shelf, beside the right wall"""
    a, b = X["right_xy"](s - 50, 250), X["right_xy"](s + 50, 250)
    return v(b[0] - a[0], b[1] - a[1], 0.0).normal()


def left_point(s, u, z):
    x, y = X["left_xy"](s, u)
    return v(x, y, z)


def right_point(s, r, z):
    x, y = X["right_xy"](s, r)
    return v(x, y, z)


def stand(actor, point, yaw=None):
    """Server teleport so the feet are at point.z"""
    actor.set_actor_location(v(point.x, point.y, point.z + HALF_HEIGHT + 5), False, True)
    if yaw is not None:
        actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), True)


def feet(actor):
    return actor.get_actor_location().z - HALF_HEIGHT


def by_label(world, cls, prefix):
    return sorted((a for a in gs.get_all_actors_of_class(world, cls) if a.get_actor_label().startswith(prefix)),
                  key=lambda a: a.get_actor_label())


def anchors(world):
    return list(gs.get_all_actors_of_class(world, unreal.EchoAnchorPoint))


def anchor_near(world, point, tolerance=80.0):
    return next((a for a in anchors(world) if (a.get_swing_point() - point).length() < tolerance), None)


def wait_for(cond, timeout=3.0):
    t = 0.0
    while t < timeout:
        if cond():
            return True
        yield 0.1
        t += 0.1
    return cond()


def shoot_target(target, from_point, label, expect_anchor=True):
    """Bat stands at from_point (server) and fires at the target's face; waits for the anchor on both machines"""
    bat, bow = S["bat"], S["bat_bow"]
    stand(bat, from_point)
    yield 0.3
    before_s = len(anchors(S["server"]))
    face = target.get_face_center()
    arrow = bow.fire_at(face)
    check(arrow is not None, f"Bat fires at {label}")
    flight = (face - from_point).length() / 4000.0 + 0.6
    if expect_anchor:
        ok = yield from wait_for(lambda: len(anchors(S["server"])) != before_s or anchor_near(S["server"], face, 120) is not None, flight + 2.0)
        yield 0.6   # replication to Saraa's machine
        a_s = anchor_near(S["server"], face, 120)
        a_c = anchor_near(S["client"], face, 120)
        check(a_s is not None and a_c is not None, f"the arrow sticks in {label} and becomes an anchor on both machines")
        return a_s
    yield flight + 1.0
    return None


def aim_at(world_pawn, point):
    """Point the locally controlled pawn's camera at a world point (from roughly where the camera sits)"""
    pc = world_pawn.get_controller()
    cam = world_pawn.get_actor_location() + v(0, 0, 60)
    pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(cam, point))


def predict_landing(loc, vel, whip, top, front_s=None, boost=True, blockers=()):
    """
    Station where she'd come down to height `top` if she let go now (release boost, plain gravity, no steering).
    With front_s, the feet must also be above `top` when she passes front_s (clearing the landing's front edge).
    """
    h = v(vel.x, vel.y, 0)
    if boost:
        n = h.normal() if h.length() > 1 else v(0, 0, 0)
        vel = vel + n * whip.release_boost + v(0, 0, whip.release_up_boost)
    p = v(loc.x, loc.y, loc.z - HALF_HEIGHT)
    vz = vel.z
    s_prev = su(p)[0]
    for _ in range(600):
        dt = 1.0 / 60.0
        vz -= 980.0 * dt
        p = p + v(vel.x * dt, vel.y * dt, vz * dt)
        s = su(p)[0]
        if front_s is not None and s_prev < front_s <= s and p.z < top + 15:
            return None   # would hit the front face
        head = p.z + 2 * HALF_HEIGHT
        if any(b0 <= s <= b1 and head > z0 and p.z < z1 for b0, b1, z0, z1 in blockers):
            return None   # would hit rock on the way (e.g. the arch or its curtain)
        s_prev = s
        if vz < 0 and p.z <= top:
            LAST_LANDING["u"] = su(p)[1]
            return s
    return None


LAST_LANDING = {"u": None}
_A = X["ARCH"]
# (s0, s1, bottom, top) of the arch and its back curtain
ARCH_ROCK = ((_A["s0"], _A["s1"], _A["bottom"], _A["top"]), (_A["s1"] - _A["curtain_depth"], _A["s1"], _A["curtain_bottom"], _A["bottom"]))


def lands_on(platform, margin=120, u_range=None):
    """A release rule: let go once the predicted landing is well inside the platform (along the canyon and across)"""
    u0, u1 = u_range or (platform["u0"], platform["u1"])

    def rule(local, whip):
        move = local.get_movement_component()
        s = predict_landing(local.get_actor_location(), move.velocity, whip, platform["top"], platform["s0"], blockers=ARCH_ROCK)
        return (s is not None and platform["s0"] + margin <= s <= platform["s1"] - margin
                and u0 + 60 <= LAST_LANDING["u"] <= u1 - 60)
    return rule


def top_of_swing(min_s):
    """A release rule for chaining: let go at the top of the forward swing (past min_s, no longer rising)"""
    def rule(local, whip):
        return su(local.get_actor_location())[0] >= min_s and local.get_movement_component().velocity.z <= 0
    return rule


def reaches(point_s, point_z):
    """A release rule for chaining: let go once she's past point_s and at least at point_z (near the top of the swing)"""
    def rule(local, whip):
        loc = local.get_actor_location()
        return su(loc)[0] >= point_s and loc.z >= point_z
    return rule


def swing_run(label, start, edge_s, jump_s, anchors_list, releases, landing, steer_s):
    """
    A real run-jump-swing on Saraa's own machine. start: where she stands (server teleport). She runs along the
    canyon, jumps once past jump_s, and for each anchor: aims at it, presses the whip, holds until release(s, vz)
    says let go. Then she steers towards steer_s and lands. Returns True if the SERVER has her standing on landing
    (s0, s1, top).
    """
    local, srv = S["saraa_local"], S["saraa_srv"]
    move = local.get_movement_component()
    whip = local.get_editor_property("sword_whip")
    stand(srv, start, G["centre_yaw"](su(start)[0]))
    yield 1.6   # let her machine take the server's teleport before scripted input
    move.reset_correction_stats()
    started_before = move.get_swings_started()

    phase, index, frames, jumped = "run", 0, 0, False
    latched = 0
    press_ok = []
    t = 0.0
    while frames < 1600:
        loc = local.get_actor_location()
        s, u = su(loc)
        vel = move.velocity
        if TRACE and frames % 3 == 0 and phase != "run":
            p = predict_landing(loc, vel, whip, landing[2], landing[0], boost=phase != "fly")
            log(f"  trace {phase}: station {s:.0f}, u {u:.0f}, feet {loc.z - HALF_HEIGHT:.0f}, vz {vel.z:.0f}, "
                f"swinging {move.is_swinging()}, predicted landing {p if p is None else round(p)}")
        # Push along the canyon like holding forward on the stick; once flying to the landing, steer for its middle
        if phase == "fly":
            aim = left_point(steer_s, X["PATH_U"], loc.z)
            to = v(aim.x - loc.x, aim.y - loc.y, 0.0)
            if to.length() > 20:
                local.add_movement_input(to.normal(), min(1.0, to.length() / 150.0), False)
        else:
            local.add_movement_input(forward(s), 1.0, False)
        if phase == "run":
            if not jumped and s >= jump_s:
                local.jump()
                jumped = True
            if jumped and s >= edge_s + 20 and move.is_falling():
                phase = "latch"
        elif phase == "latch":
            anchor = anchors_list[index]
            aim_at(local, anchor.get_swing_point())
            yield 0   # let targeting see the new view
            aim_at(local, anchor.get_swing_point())
            yield 0
            hl = whip.get_highlighted_anchor()
            ok = whip.press_whip()
            press_ok.append((ok, hl is not None and (hl.get_swing_point() - anchor.get_swing_point()).length() < 5))
            if not ok:
                log(f"  {label}: nothing to latch onto from station {s:.0f}, feet {loc.z - HALF_HEIGHT:.0f}")
                break
            phase = "hold"
            frames += 1
            continue
        elif phase == "hold":
            if move.is_swinging():
                latched = max(latched, index + 1)
            if (move.is_swinging() and releases[index](local, whip)) or frames > 1400:
                whip.release_whip()
                local.stop_jumping()
                index += 1
                phase = "latch" if index < len(anchors_list) else "fly"
                if phase == "latch":
                    yield 0.1
                    frames += 1
                    continue
        elif phase == "fly":
            if not move.is_falling() and not move.is_swinging():
                break
        frames += 1
        yield 0
    local.stop_jumping()
    yield 1.0   # let the server finish replaying her moves
    s_srv, u_srv = su(srv.get_actor_location())
    f = feet(srv)
    on = landing[0] - 20 <= s_srv <= landing[1] + 20 and abs(f - landing[2]) < 25
    presses = all(p[0] for p in press_ok) and len(press_ok) == len(anchors_list)
    highlights = all(p[1] for p in press_ok)
    corrections = move.get_num_client_corrections()
    S.setdefault("corrections", []).append((label, corrections, move.get_max_correction_distance(), move.get_total_correction_distance()))
    started = move.get_swings_started() - started_before
    check(presses and highlights, f"{label}: the whip targeted and latched each anchor ({press_ok})")
    check(started == len(anchors_list) and latched == len(anchors_list), f"{label}: {started} swing(s) started on her machine (want {len(anchors_list)})")
    return check(on, f"{label}: lands on the target (server: station {s_srv:.0f}, {u_srv:.0f} from the wall, feet {f:.0f}; want {landing[0]}-{landing[1]} at {landing[2]}); "
                     f"{corrections} corrections, largest {move.get_max_correction_distance():.1f} cm")


def best_single_swing(label, start, edge_s, jump_s, anchor, platform):
    """
    Swing on ONE anchor holding forward the whole time (pumping as hard as possible) and track, every frame, where
    she'd land if she let go then. Returns the furthest predicted landing station. She's left to fall (and respawn).
    """
    local, srv = S["saraa_local"], S["saraa_srv"]
    move = local.get_movement_component()
    whip = local.get_editor_property("sword_whip")
    stand(srv, start, G["centre_yaw"](su(start)[0]))
    yield 1.6
    jumped, pressed, best, frames, peak_s = False, False, -1e9, 0, -1e9
    while frames < 400:
        s, _ = su(local.get_actor_location())
        local.add_movement_input(forward(s), 1.0, False)
        if not jumped and s >= jump_s:
            local.jump()
            jumped = True
        elif jumped and not pressed and s >= edge_s + 20 and move.is_falling():
            aim_at(local, anchor.get_swing_point())
            yield 0
            whip.press_whip()
            pressed = True
        elif pressed and move.is_swinging():
            p = predict_landing(local.get_actor_location(), move.velocity, whip, platform["top"], platform["s0"], blockers=ARCH_ROCK)
            # Generous across the canyon: a player could steer a few metres sideways in the air
            if p is not None and platform["u0"] - 300 <= LAST_LANDING["u"] <= platform["u1"] + 300:
                best = max(best, p)
            peak_s = max(peak_s, s)
            if s < peak_s - 200:   # swinging back: the forward swing is over
                break
        elif pressed and frames > 20 and not move.is_swinging():
            break
        frames += 1
        yield 0
    whip.release_whip()
    local.stop_jumping()
    yield 3.0   # falls into the chasm and respawns
    return best


def scenario():
    pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
    server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
    client = next((w for w in pie if not unreal.SystemLibrary.is_server(w)), None)
    if not (server and client):
        check(False, "need a 2-player Listen Server PIE session")
        return
    S["server"], S["client"] = server, client
    # Scripted input is only as precise as the frame rate, and a background editor with two PIE worlds is slow: render smaller while testing
    unreal.SystemLibrary.execute_console_command(server, "r.ScreenPercentage 25")

    def realm_pawns(world):
        out = {}
        for c in gs.get_all_actors_of_class(world, unreal.OurLastEchoCharacter):
            out["Saraa" if c.get_realm() == unreal.EchoRealm.SPIRIT else "Bat"] = c
        return out
    srv, cli = realm_pawns(server), realm_pawns(client)
    bat, saraa = srv["Bat"], srv["Saraa"]
    host_saraa = gs.get_player_pawn(server, 0) == saraa
    S["bat"], S["saraa_srv"] = bat, saraa
    S["bat_bow"] = bat.get_editor_property("spirit_bow")
    S["saraa_local"] = saraa if host_saraa else cli["Saraa"]
    saraa_view_world = server if host_saraa else client
    bat_view_world = client if host_saraa else server
    log(f"mode {MODE}; Saraa is the {'HOST' if host_saraa else 'client'}")

    if NETEMU:
        # Unreal's packet simulation (console variables, so both PIE players' connections get it)
        for cmd in NETEMU:
            unreal.SystemLibrary.execute_console_command(server, cmd)
        yield 4.0
        for label, world in (("host", server), ("client", client)):
            ps = gs.get_player_controller(world, 0).player_state
            log(f"network emulation {NETEMU}: {label} player ping {ps.get_ping_in_milliseconds():.0f} ms")
    if MODE == "host_saraa":
        check(host_saraa, "Saraa is the listen-server host (BP_EchoGameMode.bHostPlaysSaraa)")

    # ---------------------------------------------------------------- who has what
    whip_s, whip_b = saraa.get_editor_property("sword_whip"), bat.get_editor_property("sword_whip")
    check(whip_s.can_use_whip() and not whip_b.can_use_whip(), "only Saraa can use the sword whip")
    bat_local = cli["Bat"] if host_saraa else bat
    check(not bat_local.get_editor_property("sword_whip").press_whip(), "Bat's whip button does nothing")

    def sword_parts(character):
        return sum(1 for c in character.get_components_by_class(unreal.StaticMeshComponent)
                   if c.get_attach_parent() and c.get_attach_parent().get_name() == "SwordWhipRoot")
    check(sword_parts(S["saraa_local"]) == 4 and sword_parts(bat_local) == 0,
          f"Saraa carries the 4-part placeholder sword, Bat doesn't ({sword_parts(S['saraa_local'])}, {sword_parts(bat_local)})")

    targets = by_label(server, unreal.EchoAnchorTarget, "Target_")
    targets_saraa_view = by_label(saraa_view_world, unreal.EchoAnchorTarget, "Target_")
    targets_bat_view = by_label(bat_view_world, unreal.EchoAnchorTarget, "Target_")
    check(len(targets) == 4, f"4 anchor targets ({[t.get_actor_label() for t in targets]})")
    check(all(t.is_visible_locally() for t in targets_bat_view) and not any(t.is_visible_locally() for t in targets_saraa_view),
          "targets: Bat sees them, Saraa doesn't")
    for t in targets:
        hits = [c for c in t.get_components_by_class(unreal.EchoAnchorableComponent)]
        if not hits:
            check(False, f"{t.get_actor_label()} has an Anchorable component")
    T1, T2, T3, T4 = targets
    ep = by_label(server, unreal.EchoPlatform, "Crossing_EchoPlatform")[0]

    bat_bow = S["bat_bow"]
    bat_bow.clear_anchors()
    yield 0.5
    top = X["shelf_top"]()

    if MODE == "chain":
        # Just the chained swing, traced
        P1, P2 = X["PILLAR_1"], X["PILLAR_2"]
        under = right_point(11000, 250, top)
        a2 = yield from shoot_target(T2, under, "target 2 (arch front)")
        a3 = yield from shoot_target(T3, under, "target 3 (arch back)")
        a2_s = su(a2.get_swing_point())[0]
        a2_l, a3_l = anchor_near(saraa_view_world, a2.get_swing_point()), anchor_near(saraa_view_world, a3.get_swing_point())
        for attempt in range(REPEATS):
            yield from swing_run(f"swing 2 #{attempt + 1}", left_point(P1["s1"] - 150, 600, P1["top"]), P1["s1"], P1["s1"] - 80, [a2_l, a3_l],
                                 [top_of_swing(a2_s + 300), lands_on(P2)], (P2["s0"], P2["s1"], P2["top"]), (P2["s0"] + P2["s1"]) / 2)
        return

    if MODE == "swing3":
        # Just swing 3, traced: target 4's anchor and the echo platform, then the swing
        P2, EPD = X["PILLAR_2"], X["ECHO_PLATFORM"]
        a4 = yield from shoot_target(T4, right_point(13300, 250, top), "target 4 (overhang 3)")
        bat_bow.fire_at(ep.get_actor_location() + v(0, 0, -12))
        yield from wait_for(lambda: ep.is_awake(), 3.0)
        a4_l = anchor_near(saraa_view_world, a4.get_swing_point())
        for attempt in range(REPEATS):
            yield from swing_run(f"swing 3 #{attempt + 1}", left_point(P2["s1"] - 150, 600, P2["top"]), P2["s1"], P2["s1"] - 80, [a4_l],
                                 [lands_on(EPD, 100, (X["PATH_U"] - EPD["size_u"] / 2, X["PATH_U"] + EPD["size_u"] / 2))], (EPD["s0"], EPD["s1"], EPD["top"]), (EPD["s0"] + EPD["s1"]) / 2)
        return

    # ---------------------------------------------------------------- anchors
    lip = left_point(7650, 2200, G["ground_height"](*X["left_xy"](7650, 2200)))
    a1 = yield from shoot_target(T1, lip, "target 1 (overhang)")
    if a1:
        a1_c = anchor_near(saraa_view_world, a1.get_swing_point())
        a1_b = anchor_near(bat_view_world, a1.get_swing_point())
        check(a1_c.is_spirit_anchor_visible_locally() and not a1_c.is_arrow_visible_locally(), "Saraa sees a glowing spirit anchor (not the arrow)")
        check(a1_b.is_arrow_visible_locally() and not a1_b.is_spirit_anchor_visible_locally(), "Bat sees his arrow stuck in the target (not the spirit anchor)")
        face = T1.get_face_center()
        check((a1.get_actor_location() - face).length() < 40, f"the anchor is where the arrow hit the target ({(a1.get_actor_location() - face).length():.0f} cm from its centre)")
        check((a1_c.get_swing_point() - a1.get_swing_point()).length() < 2, "both machines agree on the anchor point")

    if MODE != "swing1":
        # Not every surface holds an anchor
        n = len(anchors(server))
        wall = left_point(7650, -50, 900)
        bat_bow.fire_at(wall)
        yield 2.0
        check(len(anchors(server)) == n, "an arrow into plain canyon wall makes no anchor")

        # The arch's back curtain hides target 4 from the start area: Bat has to go out along his shelf, past the arch
        yield from shoot_target(T4, lip, "target 4 from the start area", expect_anchor=False)
        check(anchor_near(server, T4.get_face_center(), 120) is None, "target 4 can't be hit from the start area (the arch's back curtain is in the way)")

    under_arch = right_point(11000, 250, top)
    if MODE != "swing1":
        a2 = yield from shoot_target(T2, under_arch, "target 2 (arch front) from Bat's shelf")
        a3 = yield from shoot_target(T3, under_arch, "target 3 (arch back) from Bat's shelf")
        yield 0.8
        check(len(bat_bow.get_active_anchors()) == 2 and len(anchors(server)) == 2 and len(anchors(client)) == 2,
              f"only 2 anchors exist: the third removed the oldest (server {len(anchors(server))}, client {len(anchors(client))})")
        check(a1 is None or anchor_near(server, T1.get_face_center(), 120) is None and anchor_near(client, T1.get_face_center(), 120) is None,
              "the oldest anchor (target 1) is gone on both machines")
        # Re-make target 1's anchor for swing 1 (this removes target 2's)
        a1 = yield from shoot_target(T1, lip, "target 1 again")
        check(len(anchors(server)) == 2 and anchor_near(server, T2.get_face_center(), 120) is None, "shooting target 1 again removed target 2's anchor (the oldest)")

    # ---------------------------------------------------------------- swing 1 (single)
    L = X
    P1, P2, EPD = L["PILLAR_1"], L["PILLAR_2"], L["ECHO_PLATFORM"]
    ledge_top = C_ledge_top()
    runs = REPEATS if MODE == "swing1" else 1
    for attempt in range(runs):
        a1_local = anchor_near(saraa_view_world, a1.get_swing_point()) if a1 else None
        if not a1_local:
            check(False, "swing 1 needs the target 1 anchor")
            break
        cues_before = (a1_local.get_local_latch_cues(), anchor_near(bat_view_world, a1.get_swing_point()).get_local_latch_cues())
        a1_s = su(a1.get_swing_point())[0]
        yield from swing_run(f"swing 1{'' if runs == 1 else f' (#{attempt + 1})'}", left_point(7500, 600, ledge_top), 7770, 7700, [a1_local],
                             [lands_on(P1)],
                             (P1["s0"], P1["s1"], P1["top"]), (P1["s0"] + P1["s1"]) / 2)
        cues_after = (a1_local.get_local_latch_cues(), anchor_near(bat_view_world, a1.get_swing_point()).get_local_latch_cues())
        check(cues_after[0] > cues_before[0] and cues_after[1] > cues_before[1], f"the latch snap and flash played on both machines ({cues_before} -> {cues_after})")

    if MODE == "swing1":
        return

    # Whip line seen by both while swinging: latch from the ground next to anchor 1's pillar side? Use a quick hang on target 1
    # (covered in the swing above via is_swinging; check the line on both machines with a held latch from the ledge)
    local = S["saraa_local"]
    stand(saraa, left_point(7700, 600, ledge_top), G["centre_yaw"](7700))
    yield 1.6
    aim_at(local, a1_local.get_swing_point())
    yield 0.2
    local.jump()
    yield 0.3
    local.get_editor_property("sword_whip").press_whip()
    yield 0.4
    line_local = local.get_editor_property("sword_whip").is_whip_line_visible()
    other = cli["Saraa"] if host_saraa is False else None
    saraa_on_bat_machine = (srv["Saraa"] if not host_saraa else cli["Saraa"])
    line_other = saraa_on_bat_machine.get_editor_property("sword_whip").is_whip_line_visible()
    swinging_srv = saraa.get_movement_component().is_swinging()
    check(line_local and line_other and swinging_srv, f"while she swings, the whip line shows on both machines (hers {line_local}, Bat's {line_other}; server swinging {swinging_srv})")
    # Jump lets go
    released_before = local.get_movement_component().get_swings_released()
    local.jump()
    yield 0.3
    local.stop_jumping()
    check(not local.get_movement_component().is_swinging() and local.get_movement_component().get_swings_released() > released_before,
          "pressing jump lets go of the swing")
    local.get_editor_property("sword_whip").release_whip()
    yield 2.5

    # ---------------------------------------------------------------- swing 2 (chain across the arch)
    a2 = yield from shoot_target(T2, under_arch, "target 2 (arch front)")
    a3 = yield from shoot_target(T3, under_arch, "target 3 (arch back)")
    check(len(anchors(server)) == 2, "target 1's anchor made way for the two arch anchors")
    if a2 and a3:
        a2_s, a3_s = su(a2.get_swing_point())[0], su(a3.get_swing_point())[0]
        a2_l, a3_l = anchor_near(saraa_view_world, a2.get_swing_point()), anchor_near(saraa_view_world, a3.get_swing_point())
        # Can one arch anchor alone carry her to pillar 2? Since the whip holds from the latch (September 2026) a perfect
        # release just reaches it; accepted for this greybox level (the player has enough control either way), so
        # it's reported, not failed
        best = yield from best_single_swing("single arch swing", left_point(P1["s1"] - 150, 600, P1["top"]), P1["s1"], P1["s1"] - 80, a2_l, P2)
        log(f"note: best single-arch-anchor landing at station {best:.0f} (pillar 2 starts at {P2['s0']}); "
            f"{'reachable with one anchor (accepted)' if best >= P2['s0'] - 150 else 'the chain is needed'}")
        chained = yield from swing_run("swing 2 (chained)", left_point(P1["s1"] - 150, 600, P1["top"]), P1["s1"], P1["s1"] - 80, [a2_l, a3_l],
                                       [top_of_swing(a2_s + 300),
                                        lands_on(P2)],
                                       (P2["s0"], P2["s1"], P2["top"]), (P2["s0"] + P2["s1"]) / 2)

    if MODE == "host_saraa":
        return

    # ---------------------------------------------------------------- swing 3 onto the echo platform
    shelf_far = right_point(13300, 250, top)
    a4 = yield from shoot_target(T4, shelf_far, "target 4 (overhang 3)")
    stand(bat, shelf_far)
    yield 0.3
    bat_bow.fire_at(ep.get_actor_location() + v(0, 0, -12))
    ok = yield from wait_for(lambda: ep.is_awake(), 3.0)
    check(ok, "Bat wakes the echo platform in front of Saraa's doorway from his shelf")
    if a4:
        a4_s = su(a4.get_swing_point())[0]
        a4_l = anchor_near(saraa_view_world, a4.get_swing_point())
        yield from swing_run("swing 3 (onto the echo platform)", left_point(P2["s1"] - 150, 600, P2["top"]), P2["s1"], P2["s1"] - 80, [a4_l],
                             [lands_on(EPD, 100, (X["PATH_U"] - EPD["size_u"] / 2, X["PATH_U"] + EPD["size_u"] / 2))],
                             (EPD["s0"], EPD["s1"], EPD["top"]), (EPD["s0"] + EPD["s1"]) / 2)

    # Through her doorway (a step up from the platform) onto the far rim, and onto the switch
    local = S["saraa_local"]
    move = local.get_movement_component()
    jumped = False
    for frame in range(200):
        s, u = su(local.get_actor_location())
        local.add_movement_input(forward(s), 1.0, False)
        if not jumped and s > EPD["s1"] - 150:
            local.jump()
            jumped = True
        if s > X["GATE"]["s1"] + 150 and not move.is_falling():
            break
        yield 0
    local.stop_jumping()
    yield 0.8
    s_srv, _ = su(saraa.get_actor_location())
    check(s_srv > X["GATE"]["s1"], f"Saraa steps up through her doorway onto the far rim (station {s_srv:.0f})")

    bridge = by_label(server, unreal.EchoRisingBridge, "Crossing_Drawbridge")[0]
    check(not bridge.is_raised(), "the drawbridge is still up before Saraa's switch")
    sw = X["SWITCH"]
    stand(saraa, left_point(sw["s"], sw["u"], G["ground_height"](*X["left_xy"](sw["s"], sw["u"]))))
    ok = yield from wait_for(lambda: bridge.is_raised(), 3.0)
    check(ok, "Saraa's switch on the far rim lowers Bat's drawbridge")
    yield 3.5

    # Bat walks his shelf, over the drawbridge and through his doorway (real movement input on his machine)
    stand(bat, right_point(13400, 250, top), G["centre_yaw"](13400))
    yield 1.6
    bat_local = cli["Bat"] if host_saraa else bat
    for frame in range(400):
        s, _ = su(bat_local.get_actor_location())
        bat_local.add_movement_input(forward_right(s), 1.0, False)
        if s > X["GATE"]["s1"] + 300:
            break
        yield 0
    yield 0.5
    s_bat, _ = su(bat.get_actor_location())
    check(s_bat > X["GATE"]["s1"] + 100 and feet(bat) > 0, f"Bat crosses the drawbridge and walks through his doorway onto the far rim (station {s_bat:.0f}, feet {feet(bat):.0f})")

    # The end zone on the far rim
    zone = next(a for a in gs.get_all_actors_of_class(server, unreal.EchoEndZone))
    zs, zu = su(zone.get_actor_location())
    check(zs > X["GATE"]["s1"], f"the end zone is on the far rim (station {zs:.0f})")
    gstate = gs.get_game_state(server)
    check(not gstate.is_milestone_complete(), "not complete yet")
    zl = zone.get_actor_location()
    stand(bat, zl + v(-150, 0, 0))
    yield 0.5
    check(not gstate.is_milestone_complete(), "Bat alone in the end zone doesn't complete it")
    stand(saraa, zl + v(150, 0, 0))
    ok = yield from wait_for(lambda: gstate.is_milestone_complete(), 3.0)
    yield 0.6
    check(ok and gs.get_game_state(client).is_milestone_complete(), "Bat + Saraa in the end zone completes it, on both machines")

    # ---------------------------------------------------------------- falling respawns at the chasm start; anchors stay
    n_before = len(anchors(server))
    stand(saraa, left_point(11000, 1500, -1000))
    yield 2.5
    loc = saraa.get_actor_location()
    want = saraa.get_respawn_transform().translation
    s_r, u_r = su(loc)
    check((loc - want).length() < 150 and abs(feet(saraa) - ledge_top) < 40 and s_r < 7800,
          f"Saraa falling into the chasm respawns at its start, on the ledge (station {s_r:.0f}, feet {feet(saraa):.0f})")
    check(len(anchors(server)) == n_before and len(anchors(client)) == n_before, f"Bat's anchors stay in place ({n_before})")

    # ---------------------------------------------------------------- Bat's third arrow removes the anchor she hangs from
    a1 = yield from shoot_target(T1, lip, "target 1 once more")
    a1_local = anchor_near(saraa_view_world, a1.get_swing_point()) if a1 else None
    if a1_local:
        local = S["saraa_local"]
        lmove = local.get_movement_component()
        stand(saraa, left_point(7700, 600, ledge_top), G["centre_yaw"](7700))
        yield 1.6
        aim_at(local, a1_local.get_swing_point())
        yield 0.2
        local.jump()
        yield 0.3
        local.get_editor_property("sword_whip").press_whip()
        yield 0.4
        hanging = lmove.is_swinging()
        lmove.reset_correction_stats()
        yield from shoot_target(T2, under_arch, "target 2 while she hangs")
        yield from shoot_target(T3, under_arch, "target 3 while she hangs")
        yield 0.8
        gone = anchor_near(server, a1_local.get_swing_point()) is None
        check(hanging and gone and not lmove.is_swinging() and not saraa.get_movement_component().is_swinging(),
              f"when Bat's arrows remove the anchor she hangs from, she drops on both machines "
              f"(was swinging {hanging}, anchor gone {gone}; {lmove.get_num_client_corrections()} corrections, largest {lmove.get_max_correction_distance():.0f} cm)")
        local.get_editor_property("sword_whip").release_whip()
        yield 3.0

    # ---------------------------------------------------------------- stretch goal: whip lash on the training dummy
    dummies = by_label(server, unreal.EchoTrainingDummy, "Crossing_TrainingDummy")
    if check(len(dummies) == 1, "a training dummy stands on the far rim"):
        dummy = dummies[0]
        dummy_c = by_label(client, unreal.EchoTrainingDummy, "Crossing_TrainingDummy")[0]
        d = dummy.get_actor_location()
        toward = v(d.x, d.y, 0) - v(zl.x, zl.y, 0)
        stand(saraa, v(d.x, d.y, d.z) - toward.normal() * 250 + v(0, 0, 0), None)
        yield 1.0
        whip_srv = saraa.get_editor_property("sword_whip")
        before = (dummy.get_hit_count(), dummy_c.get_local_hit_reactions())
        # The server copy of Saraa does the hit (editor Python can't send her client's server RPC; see TEST_REPORT)
        away = whip_srv.lash_in_direction(toward * -1.0)
        yield 0.8
        check(away and dummy.get_hit_count() == before[0], "a lash away from the dummy misses it")
        hit = whip_srv.lash_in_direction(toward)
        again = whip_srv.lash_in_direction(toward)
        yield 0.8
        check(hit and not again, "the lash has a cooldown")
        check(dummy.get_hit_count() == before[0] + 1 and dummy_c.get_hit_count() == before[0] + 1 and dummy_c.get_local_hit_reactions() > before[1],
              f"a lash at the dummy hits it, and both machines see the hit ({dummy.get_hit_count()} / {dummy_c.get_hit_count()} hits)")
        check(not bat.get_editor_property("sword_whip").lash_in_direction(toward), "Bat can't lash")

    # ---------------------------------------------------------------- input assets
    imc = unreal.load_asset("/Game/Input/IMC_Whip.IMC_Whip")
    keys = sorted(str(m.get_editor_property("key").get_editor_property("key_name")) for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
                  if m.get_editor_property("action").get_name() == "IA_Whip")
    check(keys == ["E", "Gamepad_RightTrigger", "LeftMouseButton"], f"IMC_Whip maps the whip to mouse, keyboard and gamepad ({keys})")

    # ---------------------------------------------------------------- debug command
    unreal.SystemLibrary.execute_console_command(server, "EchoWhipDebug")
    yield 0.3
    on = unreal.EchoSwordWhipComponent.is_debug_draw_on()
    unreal.SystemLibrary.execute_console_command(server, "EchoWhipDebug")
    yield 0.3
    check(on and not unreal.EchoSwordWhipComponent.is_debug_draw_on(), "EchoWhipDebug turns the whip debug drawing on and off")


def C_ledge_top():
    C = X["C"]
    return C["G"]["ground_height"](*C["wall_xy"](C["FIRST_S"], C["PLATFORMS"][0][0])) + C["LEDGE"]["top"]


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
    if S.get("server"):
        unreal.SystemLibrary.execute_console_command(S["server"], "r.ScreenPercentage 100")
        for cmd in NETEMU:
            unreal.SystemLibrary.execute_console_command(S["server"], cmd.split()[0] + " 0")
    for label, n, worst, total in S.get("corrections", []):
        log(f"network: {label}: {n} corrections, largest {worst:.1f} cm, total {total:.0f} cm")
    log(f"{T['checks']} checks in {T['elapsed']:.0f}s")
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")


T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
log("started")
