"""
Live Play-In-Editor test for Milestone 6 (two eras). Run INSIDE the open editor while a fresh 2-player Listen Server
PIE session on Lvl_SpiritPath is running (Scripts/tools/run_gameplay_test.ps1 -Script test_eras_live.py
-Marker ECHO_ERAS_TEST starts one):
  py exec(open(r'<project>/Scripts/test_eras_live.py').read())
Set ECHO_ERAS_MODE = "host_saraa" first (with the console variable Echo.HostPlaysSaraa 1 set before PIE started) to
test the roles swapped: the host then plays Saraa and must see the past.
Output lines are tagged ECHO_ERAS_TEST; the last line is PASS or FAIL.

Each PIE world is one machine: the server world is the host's, the client world the other player's.
"""
import math

import unreal

gs = unreal.GameplayStatics
MODE = globals().get("ECHO_ERAS_MODE", "normal")
TRACE_CLIMB = globals().get("ECHO_ERAS_TRACE", False)
T = {"failures": 0, "checks": 0, "wait": 0.0, "elapsed": 0.0, "handle": None, "gen": None}
PRESENT, PAST = unreal.EchoEra.PRESENT, unreal.EchoEra.PAST
SOURCE_TAGS = ("CanyonBuilder", "ClimbBuilder", "CrossingBuilder", "EchoBuilder")


def log(msg):
    unreal.log(f"ECHO_ERAS_TEST: {msg}")


def check(ok, text):
    T["checks"] += 1
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def director(world):
    found = gs.get_all_actors_of_class(world, unreal.EchoEraDirector)
    return found[0] if found else None


def era_state(world):
    d = director(world)
    return "present" if d.is_era_visible(PRESENT) and not d.is_era_loaded(PAST) else \
        "past" if d.is_era_visible(PAST) and not d.is_era_loaded(PRESENT) else \
        "both" if d.is_era_visible(PAST) and d.is_era_visible(PRESENT) else "none"


def mpc_past(world):
    mpc = unreal.load_asset("/Game/Echo/Eras/MPC_Era.MPC_Era")
    return unreal.MaterialLibrary.get_scalar_parameter_value(world, mpc, "Past")


def wait_for(cond, timeout=10.0, step=0.1):
    t = 0.0
    while t < timeout:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield step
        t += step
    try:
        return bool(cond())
    except Exception:
        return False


def colliding_components(world, era):
    """Every primitive in this era's level that can collide with anything"""
    bad = []
    for actor in director(world).get_era_actors(era):
        for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
            if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                bad.append(f"{actor.get_actor_label()}.{comp.get_name()}")
    return bad


def rounded(t):
    l = t.translation
    return (round(l.x), round(l.y), round(l.z))


def shared_shapes(world):
    """World locations of the shared layer's canyon shapes (collision on, hidden in game) and whether all are hidden"""
    locs, all_hidden, all_solid = [], True, True
    for a in gs.get_all_actors_of_class(world, unreal.StaticMeshActor):
        if not any(a.actor_has_tag(t) for t in SOURCE_TAGS):
            continue
        smc = a.static_mesh_component
        if smc.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        locs.append(rounded(a.get_actor_transform()))
        all_hidden &= a.get_editor_property("hidden")
    for vol in gs.get_all_actors_of_class(world, unreal.PCGVolume):
        for comp in vol.get_components_by_class(unreal.InstancedStaticMeshComponent):
            all_hidden &= comp.get_editor_property("hidden_in_game")
            all_solid &= comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
            for i in range(comp.get_instance_count()):
                locs.append(rounded(comp.get_instance_transform(i, True)))
    return locs, all_hidden, all_solid


def era_canyon_locations(world, era):
    locs = []
    for actor in director(world).get_era_actors(era):
        if isinstance(actor, unreal.EchoInstancedProps) and actor.get_actor_label().startswith("Canyon_"):
            locs += [rounded(t) for t in actor.get_editor_property("instances")]
    return locs


def era_sun_pitch(world, era):
    for actor in director(world).get_era_actors(era):
        if isinstance(actor, unreal.DirectionalLight):
            return actor.get_actor_rotation().pitch
    return None


def character(world, realm):
    for c in gs.get_all_actors_of_class(world, unreal.OurLastEchoCharacter):
        if c.get_realm() == realm:
            return c
    return None


def mesh_material(char):
    m = char.mesh.get_material(0)
    return m.get_name() if m else ""


def scenario():
    pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
    server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
    client = next((w for w in pie if not unreal.SystemLibrary.is_server(w)), None)
    if not (server and client):
        check(False, "need a 2-player Listen Server PIE session")
        return
    host_era, guest_era = ("past", "present") if MODE == "host_saraa" else ("present", "past")
    log(f"mode {MODE}: the host should see the {host_era}, the guest the {guest_era}")

    ok = yield from wait_for(lambda: director(server).is_ready() and director(client).is_ready(), 20.0)
    check(ok, "both machines have loaded an era")
    check(era_state(server) == host_era, f"the host's machine loaded only the {host_era} ({era_state(server)})")
    check(era_state(client) == guest_era, f"the guest's machine loaded only the {guest_era} ({era_state(client)})")

    # No collision in either era layer (checked on the machine that has it loaded)
    for world, era_name in ((server, host_era), (client, guest_era)):
        era = PRESENT if era_name == "present" else PAST
        bad = colliding_components(world, era)
        n = len(director(world).get_era_actors(era))
        check(n > 0 and not bad, f"the {era_name} layer ({n} actors) has no collision {bad[:5]}")

    # The shared layer keeps all the collision, hidden; the eras draw exactly the same shapes
    shared, hidden, solid = shared_shapes(server)
    check(len(shared) > 500 and hidden and solid, f"the shared layer's {len(shared)} canyon shapes still collide and are hidden in game")
    for world, era_name in ((server, host_era), (client, guest_era)):
        era = PRESENT if era_name == "present" else PAST
        copies = era_canyon_locations(world, era)
        missing = sorted(set(shared) - set(copies))
        check(sorted(copies) == sorted(shared), f"the {era_name} draws every shared shape at exactly its place ({len(copies)} copies, {len(missing)} missing)")

    # The eras look different: sun, and the tint on what's left visible in the shared layer (the canyon floor)
    present_world = server if host_era == "present" else client
    past_world = client if host_era == "present" else server
    p_sun, a_sun = era_sun_pitch(present_world, PRESENT), era_sun_pitch(past_world, PAST)
    check(p_sun is not None and a_sun is not None and p_sun > -15 and a_sun < -30,
          f"a low sunset sun in the present ({p_sun:.0f} deg), a high morning sun in the past ({a_sun:.0f} deg)")
    check(abs(mpc_past(present_world)) < 0.01 and abs(mpc_past(past_world) - 1.0) < 0.01,
          f"the shared floor is tinted per machine (present {mpc_past(present_world):.1f}, past {mpc_past(past_world):.1f})")

    # Each player sees the other as a ghost
    bat_in_past = character(past_world, unreal.EchoRealm.LIVING)
    bat_in_present = character(present_world, unreal.EchoRealm.LIVING)
    saraa_in_present = character(present_world, unreal.EchoRealm.SPIRIT)
    yield 0.5
    check(bat_in_past and "GhostFromPresent" in mesh_material(bat_in_past), f"in the past, Bat is a ghost from the present ({mesh_material(bat_in_past) if bat_in_past else None})")
    check(bat_in_present and "Ghost" not in mesh_material(bat_in_present), f"in his own present, Bat isn't ({mesh_material(bat_in_present) if bat_in_present else None})")
    check(saraa_in_present and "Ghost" in mesh_material(saraa_in_present), f"in the present, Saraa is a ghost ({mesh_material(saraa_in_present) if saraa_in_present else None})")

    # Bat's arrow, seen from each era
    bat_server = character(server, unreal.EchoRealm.LIVING)
    bow = bat_server.get_editor_property("spirit_bow")
    loc = bat_server.get_actor_location()
    fired = bow.fire_at(unreal.Vector(loc.x + 3000, loc.y, loc.z + 300))
    ok = yield from wait_for(lambda: len(gs.get_all_actors_of_class(client, unreal.EchoArrow)) > 0 and len(gs.get_all_actors_of_class(server, unreal.EchoArrow)) > 0, 3.0)
    if check(fired is not None and ok, "Bat fires an arrow; both machines have it"):
        def arrow_mats(world):
            a = gs.get_all_actors_of_class(world, unreal.EchoArrow)[0]
            return {c.get_material(0).get_name() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_material(0)}
        check("MI_ArrowPast" in arrow_mats(past_world), f"in the past it's a spirit arrow {sorted(arrow_mats(past_world))}")
        check("MI_Arrow" in arrow_mats(present_world), f"in the present it's his own arrow {sorted(arrow_mats(present_world))}")

    # Debug: swap the local view (the real console command, on the guest's machine), and back
    guest_pc = gs.get_player_controller(client, 0)
    other = "past" if guest_era == "present" else "present"
    unreal.EchoUITestLibrary.queue_console_command(guest_pc, "EchoSwapEra")
    ok = yield from wait_for(lambda: era_state(client) == other and director(client).is_ready(), 10.0)
    check(ok, f"EchoSwapEra shows the guest the {other} instead ({era_state(client)})")
    check(era_state(server) == host_era, f"... on the guest's machine only: the host still sees the {host_era}")
    unreal.EchoUITestLibrary.queue_console_command(guest_pc, "EchoSwapEra")
    ok = yield from wait_for(lambda: era_state(client) == guest_era and director(client).is_ready(), 10.0)
    check(ok, f"EchoSwapEra again: back to the {guest_era} ({era_state(client)})")
    unreal.EchoUITestLibrary.queue_console_command(guest_pc, "EchoEra auto")

    # Debug: both eras at once, to check they line up
    unreal.EchoUITestLibrary.queue_console_command(guest_pc, "EchoShowBothEras")
    ok = yield from wait_for(lambda: era_state(client) == "both" and director(client).is_ready(), 10.0)
    check(ok, f"EchoShowBothEras loads both eras ({era_state(client)})")
    if ok:
        both_bad = colliding_components(client, PRESENT) + colliding_components(client, PAST)
        check(not both_bad, "still no collision with both loaded")
        check(sorted(era_canyon_locations(client, PRESENT)) == sorted(era_canyon_locations(client, PAST)),
              "the two eras' canyons line up shape for shape")
    unreal.EchoUITestLibrary.queue_console_command(guest_pc, "EchoShowBothEras")
    ok = yield from wait_for(lambda: era_state(client) == guest_era and director(client).is_ready(), 10.0)
    check(ok, f"EchoShowBothEras again: just the {guest_era} ({era_state(client)})")

    # Respawning keeps the era (the guest's character falls into the Spirit Path ravine and respawns)
    guest_realm = unreal.EchoRealm.SPIRIT if guest_era == "past" else unreal.EchoRealm.LIVING
    guest_char = character(server, guest_realm)
    start = guest_char.get_actor_location()
    guest_char.set_actor_location(unreal.Vector(800, 0, -900), False, True)
    ok = yield from wait_for(lambda: guest_char.get_actor_location().z > -200, 8.0)
    check(ok, f"the guest falls into the ravine and respawns ({guest_char.get_actor_location().z:.0f})")
    yield 1.0
    check(era_state(client) == guest_era and director(client).is_ready(), f"after respawning the guest still sees the {guest_era}")
    guest_char.set_actor_location(start, False, True)

    # Stretch goal: Saraa plants a sapling in the past; a climbable tree stands there in Bat's present
    spots = gs.get_all_actors_of_class(server, unreal.EchoSaplingSpot)
    if not check(len(spots) == 1, "there's a sapling spot"):
        return
    spot_s = spots[0]
    spot_c = gs.get_all_actors_of_class(client, unreal.EchoSaplingSpot)[0]
    spot_present = spot_s if host_era == "present" else spot_c
    spot_past = spot_c if host_era == "present" else spot_s
    yield 0.5
    check(not spot_s.is_planted() and not spot_s.is_tree_solid() and not spot_c.is_tree_solid(), "before planting: no tree, and nothing to bump into")
    check(not spot_present.is_tree_visible() and not spot_past.is_sapling_visible(), "... and nothing drawn in either era but the mark")
    saraa = character(server, unreal.EchoRealm.SPIRIT)
    bat = character(server, unreal.EchoRealm.LIVING)
    saraa_start, bat_start = saraa.get_actor_location(), bat.get_actor_location()
    here = spot_s.get_actor_location()
    # Bat walking onto the mark doesn't plant anything
    bat.set_actor_location(unreal.Vector(here.x, here.y, here.z + 100), False, True)
    yield 1.0
    check(not spot_s.is_planted(), "Bat stepping on the mark doesn't plant it")
    bat.set_actor_location(bat_start, False, True)
    saraa.set_actor_location(unreal.Vector(here.x, here.y, here.z + 100), False, True)
    ok = yield from wait_for(lambda: spot_s.is_planted() and spot_c.is_planted(), 5.0)
    check(ok, "Saraa steps on the mark: the sapling is planted, on both machines")
    yield 0.5
    check(spot_s.is_tree_solid() and spot_c.is_tree_solid(), "the tree is solid on both machines (it's in the shared layer)")
    check(spot_past.is_sapling_visible() and not spot_past.is_tree_visible(), "the past shows the sapling")
    check(spot_present.is_tree_visible() and not spot_present.is_sapling_visible(), "the present shows the grown tree")
    # Bat can stand on its top branch (level with the ledge); Saraa falls through it
    top = spot_s.get_top_step_location()
    bat.set_actor_location(unreal.Vector(top.x, top.y, top.z + 250), False, True)
    bat.set_actor_location(unreal.Vector(top.x, top.y, top.z - 400), True, False)
    bat_feet = bat.get_actor_location().z - 96
    check(abs(bat_feet - top.z) < 15, f"Bat can stand on the tree's top branch (feet {bat_feet:.0f}, branch {top.z:.0f})")
    saraa.set_actor_location(unreal.Vector(top.x, top.y, top.z + 250), False, True)
    saraa.set_actor_location(unreal.Vector(top.x, top.y, top.z - 400), True, False)
    check(saraa.get_actor_location().z - 96 < top.z - 100, "Saraa passes through it (it isn't in her era)")
    saraa.set_actor_location(saraa_start, False, True)

    # Bat climbs it with real jumps (his own machine's input), branch to branch, from the ground to the ledge
    if MODE != "host_saraa":
        n = spot_s.get_num_steps()
        climbed = 0
        for i in range(-1, n - 1):
            here_top = spot_s.get_actor_location() if i < 0 else spot_s.get_step_location(i)
            nxt = spot_s.get_step_location(i + 1)
            # Stand at the near edge of this branch (or on the ground by the trunk), facing the next one
            if i >= 0:
                dx, dy = nxt.x - here_top.x, nxt.y - here_top.y
                d = math.hypot(dx, dy) or 1.0
                ux, uy = dx / d, dy / d
                start_xy = (here_top.x - ux * 60, here_top.y - uy * 60)
            else:
                # From the ground beside the first branch (it points at the ledge, so not from beyond its tip)
                ox, oy = nxt.x - here_top.x, nxt.y - here_top.y
                d = math.hypot(ox, oy) or 1.0
                px, py = -oy / d, ox / d
                start_xy = (nxt.x + px * 170, nxt.y + py * 170)
                ux, uy = -px, -py
            bat.set_actor_location(unreal.Vector(start_xy[0], start_xy[1], here_top.z + 110), False, True)
            bat.set_actor_location(unreal.Vector(start_xy[0], start_xy[1], here_top.z - 200), True, False)
            yield 0.3
            bat.jump()
            for f in range(40):
                # Steer at the branch's middle and let go once over it, like a player landing a jump
                l = bat.get_actor_location()
                tx, ty = nxt.x - l.x, nxt.y - l.y
                if math.hypot(tx, ty) > 30:
                    bat.add_movement_input(unreal.Vector(tx, ty, 0).normal(), 1.0, False)
                if TRACE_CLIMB and f % 5 == 0:
                    l = bat.get_actor_location()
                    log(f"   trace {i}: frame {f} at ({l.x:.0f}, {l.y:.0f}, feet {l.z - 96:.0f})")
                yield 0.0
            # Judge the jump once he's standing on something again
            yield from wait_for(lambda: bat.get_movement_component().is_moving_on_ground(), 3.0)
            yield 0.2
            feet = bat.get_actor_location().z - 96
            on = abs(feet - nxt.z) < 20 and math.hypot(bat.get_actor_location().x - nxt.x, bat.get_actor_location().y - nxt.y) < 160
            if not on:
                log(f"   climb: from {'the ground' if i < 0 else f'branch {i + 1}'} to branch {i + 2} missed (feet {feet:.0f}, branch {nxt.z:.0f})")
                break
            climbed += 1
        check(climbed == n, f"Bat climbs the tree with real jumps, branch by branch, up to ledge height ({climbed}/{n} jumps)")
        if climbed == n:
            # ...and walks off the top branch onto the overlook ledge it points at
            base, top = spot_s.get_actor_location(), spot_s.get_top_step_location()
            ox, oy = top.x - base.x, top.y - base.y
            d = math.hypot(ox, oy) or 1.0
            for _ in range(60):
                bat.add_movement_input(unreal.Vector(ox / d, oy / d, 0), 1.0, False)
                yield 0.0
            yield from wait_for(lambda: bat.get_movement_component().is_moving_on_ground(), 3.0)
            l = bat.get_actor_location()
            out = math.hypot(l.x - base.x, l.y - base.y)
            check(abs(l.z - 96 - top.z) < 20 and out > 300, f"... and steps off the top branch onto the ledge ({out:.0f} cm from the trunk, feet {l.z - 96:.0f})")
    bat.set_actor_location(bat_start, False, True)


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
