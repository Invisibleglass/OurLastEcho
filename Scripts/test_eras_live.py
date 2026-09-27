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
