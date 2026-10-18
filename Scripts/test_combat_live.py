"""
Live Play-In-Editor test for Milestone 8 (first combat). Run INSIDE the open editor while a fresh 2-player Listen Server
PIE session on CombatTest is running (Scripts/tools/run_combat_test.ps1 starts one):
  py exec(open(r'<project>/Scripts/test_combat_live.py').read())
Options (Python globals set before exec):
  ECHO_COMBAT_MODE = "normal" | "host_saraa" (roles swapped: Echo.HostPlaysSaraa 1 before PIE) | "travel" (the doors)
  ECHO_COMBAT_NETEMU = ["NetEmulation.PktLag 60", ...]  network emulation for the run (reset to 0 at the end)
Output lines are tagged ECHO_COMBAT_TEST; the last line is PASS or FAIL.

Each PIE world is one machine. Server-side rules are checked on the server world's actors; what each player sees
on their own world. Real attacks and dodges go through the input path (the EchoShoot / EchoLash / EchoDodge console
commands, queued on each player's own controller so the guest's go over the network as RPCs).
"""
import math

import unreal

gs = unreal.GameplayStatics
ui = unreal.EchoUITestLibrary
MODE = globals().get("ECHO_COMBAT_MODE", "normal")
NETEMU = globals().get("ECHO_COMBAT_NETEMU", [])
T = {"failures": 0, "checks": 0, "wait": 0.0, "handle": None, "gen": None, "start": 0.0}
LIVING, SPIRIT = unreal.EchoRealm.LIVING, unreal.EchoRealm.SPIRIT
R = unreal.EchoDamageResult
K = unreal.EchoAttackKind
TYPES = {"wraith": unreal.EchoEnemyType.DUST_WRAITH, "shade": unreal.EchoEnemyType.SHADE,
         "guardian": unreal.EchoEnemyType.STONE_GUARDIAN, "talon": unreal.EchoEnemyType.SKY_TALON}


def log(msg):
    unreal.log(f"ECHO_COMBAT_TEST: {msg}")


def check(ok, text):
    T["checks"] += 1
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


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


# ------------------------------------------------------------------ world access

def worlds():
    pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
    server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
    client = next((w for w in pie if not unreal.SystemLibrary.is_server(w)), None)
    return server, client


def character(world, realm):
    for c in gs.get_all_actors_of_class(world, unreal.OurLastEchoCharacter):
        if c.get_realm() == realm:
            return c
    return None


def same_actor(world, actor):
    """The copy of a server actor in another world (matched by name)"""
    for a in gs.get_all_actors_of_class(world, actor.get_class()):
        if a.get_name() == actor.get_name():
            return a
    return None


def arena(world):
    found = gs.get_all_actors_of_class(world, unreal.EchoArenaManager)
    return found[0] if found else None


def enemies(world):
    return [e for e in gs.get_all_actors_of_class(world, unreal.EchoEnemy) if not e.is_defeated()]


def director(world):
    found = gs.get_all_actors_of_class(world, unreal.EchoEraDirector)
    return found[0] if found else None


def damage(target, amount, kind, instigator):
    info = unreal.EchoDamageInfo()
    info.amount = amount
    info.kind = kind
    info.instigator = instigator
    info.hit_location = target.get_actor_location()
    return target.get_health().apply_damage(info)


def console(pc, cmd):
    """Runs a console command on that player's own controller on the next tick (outside Python, so a guest's
    server RPCs really go over the network)"""
    ui.queue_console_command(pc, cmd)


def face(pc, char, target_location):
    d = target_location - char.get_actor_location()
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), yaw=math.degrees(math.atan2(d.y, d.x))))


def face_from_camera(pc, target_location):
    """Puts the reticle on the target, like a player would (the aiming camera sits over the shoulder)"""
    for _ in range(3):
        d = target_location - pc.player_camera_manager.get_camera_location()
        pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), yaw=math.degrees(math.atan2(d.y, d.x))))


def clear_enemies(server):
    for e in gs.get_all_actors_of_class(server, unreal.EchoEnemy):
        e.remove_quietly()


def freeze(enemy):
    """A target dummy: it stays put and never attacks"""
    enemy.set_editor_property("move_speed", 0.0)
    enemy.get_editor_property("character_movement").set_editor_property("max_walk_speed", 0.0)
    enemy.get_editor_property("character_movement").set_editor_property("max_fly_speed", 0.0)
    enemy.set_editor_property("attack_range", 0.0)


def god(pc, char, on):
    if char.get_health().is_god_mode() != on:
        console(pc, "EchoGod")


# ------------------------------------------------------------------ scenario

def scenario():
    server, client = worlds()
    if not (server and client):
        check(False, "need a 2-player Listen Server PIE session on CombatTest")
        return
    host_realm, guest_realm = (SPIRIT, LIVING) if MODE == "host_saraa" else (LIVING, SPIRIT)
    host_pc, guest_pc = gs.get_player_controller(server, 0), gs.get_player_controller(client, 0)
    for cmd in NETEMU:
        unreal.SystemLibrary.execute_console_command(server, cmd)
    log(f"mode {MODE}, map {server.get_name()}, network emulation {NETEMU or 'off'}")

    if MODE == "travel":
        yield from travel_scenario()
        return

    ok = yield from wait_for(lambda: character(server, LIVING) and character(server, SPIRIT) and character(client, LIVING) and character(client, SPIRIT)
                             and director(server).is_ready() and director(client).is_ready(), 20.0)
    check(ok and server.get_name() == "CombatTest", f"both players are in CombatTest with their eras loaded ({server.get_name()})")
    bat_s, saraa_s = character(server, LIVING), character(server, SPIRIT)
    bat_c, saraa_c = character(client, LIVING), character(client, SPIRIT)
    host_s = bat_s if host_realm == LIVING else saraa_s
    guest_s = saraa_s if host_realm == LIVING else bat_s
    guest_c = same_actor(client, guest_s)
    host_era = "present" if host_realm == LIVING else "past"
    d_s, d_c = director(server), director(client)
    check(d_s.is_era_loaded(unreal.EchoEra.PRESENT if host_realm == LIVING else unreal.EchoEra.PAST)
          and d_c.is_era_loaded(unreal.EchoEra.PAST if host_realm == LIVING else unreal.EchoEra.PRESENT),
          f"the arena's era layers load per player (host {host_era})")
    a_s = arena(server)
    check(a_s is not None and a_s.get_state() == unreal.EchoArenaState.IDLE, "the arena waits for a player to walk in")
    a_s.set_editor_property("intermission_seconds", 0.6)
    a_s.set_editor_property("restart_delay", 0.6)
    # Players get moved around (and roll) inside the arena during the checks: only the wave checks start it
    a_s.get_editor_property("start_zone").set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    yield 1.5

    # ---- health and the debug commands
    check(abs(bat_s.get_health().get_health() - 100) < 0.1 and abs(saraa_s.get_health().get_health() - 100) < 0.1, "both players start at full health")
    console(host_pc, "EchoHurt 30")
    console(guest_pc, "EchoHurt 20")
    ok = yield from wait_for(lambda: abs(host_s.get_health().get_health() - 70) < 0.1 and abs(guest_s.get_health().get_health() - 80) < 0.1, 3.0)
    check(ok, f"EchoHurt reaches the server from both players (host {host_s.get_health().get_health():.0f}, guest {guest_s.get_health().get_health():.0f})")
    ok = yield from wait_for(lambda: abs(guest_c.get_health().get_health() - 80) < 0.1 and abs(same_actor(client, host_s).get_health().get_health() - 70) < 0.1, 3.0)
    check(ok, "health replicates to the other machine")
    god(host_pc, host_s, True)
    god(guest_pc, guest_s, True)
    ok = yield from wait_for(lambda: host_s.get_health().is_god_mode() and guest_s.get_health().is_god_mode(), 3.0)
    check(ok and damage(bat_s, 10, K.ENEMY, None) == R.IMMUNE, "EchoGod: god mode makes a player immune")

    # ---- presence: who sees, touches and can hurt each enemy
    spots = {"wraith": unreal.Vector(300, -900, 100), "shade": unreal.Vector(300, 900, 100),
             "guardian": unreal.Vector(900, 0, 150), "talon": unreal.Vector(500, 0, 700)}
    spawned = {name: a_s.spawn_enemy(TYPES[name], spots[name], False) for name in spots}
    for e in spawned.values():
        freeze(e)
    yield 2.0
    copies = {name: same_actor(client, e) for name, e in spawned.items()}
    check(all(copies.values()), "every enemy replicates to the guest's machine")
    host_world, guest_world = (server, client)
    seen_host = {n: e.is_shown_locally() for n, e in spawned.items()}
    seen_guest = {n: e.is_shown_locally() for n, e in copies.items()}
    bat_sees = seen_host if host_realm == LIVING else seen_guest
    saraa_sees = seen_guest if host_realm == LIVING else seen_host
    check(bat_sees["wraith"] and not saraa_sees["wraith"], f"Dust Wraith: only Bat sees it (Bat {bat_sees['wraith']}, Saraa {saraa_sees['wraith']})")
    check(saraa_sees["shade"] and not bat_sees["shade"], f"Shade: only Saraa sees it (Saraa {saraa_sees['shade']}, Bat {bat_sees['shade']})")
    check(all(bat_sees[n] and saraa_sees[n] for n in ("guardian", "talon")), "Stone Guardian and Sky Talon: both players see them")
    wcap, scap, gcap = (spawned[n].get_component_by_class(unreal.CapsuleComponent) for n in ("wraith", "shade", "guardian"))
    check(wcap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_SPIRIT_PAWN) == unreal.CollisionResponseType.ECR_IGNORE
          and wcap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_BLOCK,
          "Dust Wraith: Bat bumps into it, Saraa walks through it")
    check(scap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_IGNORE
          and scap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_SPIRIT_PAWN) == unreal.CollisionResponseType.ECR_BLOCK,
          "Shade: Saraa bumps into it, Bat walks through it")
    check(gcap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_BLOCK
          and gcap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_SPIRIT_PAWN) == unreal.CollisionResponseType.ECR_BLOCK,
          "Stone Guardian: solid for both")
    check(damage(spawned["wraith"], 10, K.WHIP_LIGHT, saraa_s) == R.IGNORED and damage(spawned["shade"], 10, K.AIMED_SHOT, bat_s) == R.IGNORED,
          "Saraa's whip can't touch a Dust Wraith, Bat's arrows can't touch a Shade")
    check(damage(spawned["wraith"], 10, K.AIMED_SHOT, bat_s) == R.APPLIED and damage(spawned["shade"], 10, K.WHIP_LIGHT, saraa_s) == R.APPLIED,
          "... and each player hurts their own")

    # ---- Stone Guardian: Saraa first
    g, gc = spawned["guardian"], copies["guardian"]
    g.set_editor_property("open_duration", 1.5)
    hp = g.get_health().get_health()
    check(damage(g, 20, K.AIMED_SHOT, bat_s) == R.BLOCKED and g.get_health().get_health() == hp, "Stone Guardian: arrows bounce off the shell (blocked, no damage)")
    check(damage(g, 12, K.WHIP_LIGHT, saraa_s) == R.OPENED and g.get_health().get_health() == hp, "... Saraa's whip cracks it open (no damage from the whip)")
    ok = yield from wait_for(lambda: gc.is_open() and gc.get_open_fraction() > 0.5, 2.0)
    check(ok, f"... the guest's machine sees it open, with its timer ({gc.get_open_fraction():.2f})")
    check(damage(g, 20, K.AIMED_SHOT, bat_s) == R.APPLIED and g.get_health().get_health() < hp, "... while open, Bat's arrows hurt it")
    ok = yield from wait_for(lambda: not g.is_open(), 3.0)
    check(ok and damage(g, 20, K.AIMED_SHOT, bat_s) == R.BLOCKED, "... and the shell reforms after its open time: arrows bounce again")

    # ---- Sky Talon: Bat first
    t, tc = spawned["talon"], copies["talon"]
    t.set_editor_property("stun_duration", 2.0)
    z_air = t.get_actor_location().z
    hp = t.get_health().get_health()
    check(damage(t, 12, K.WHIP_LIGHT, saraa_s) == R.BLOCKED, "Sky Talon: out of the whip's reach (blocked)")
    check(damage(t, 20, K.AIMED_SHOT, bat_s) == R.OPENED and t.get_health().get_health() == hp, "... Bat's arrow knocks it down (no damage from the arrow)")
    ok = yield from wait_for(lambda: tc.is_stunned() and t.get_actor_location().z < z_air - 300, 3.0)
    check(ok, f"... it falls to the ground stunned, seen on both machines (z {z_air:.0f} -> {t.get_actor_location().z:.0f})")
    check(damage(t, 12, K.WHIP_LIGHT, saraa_s) == R.APPLIED and t.get_health().get_health() < hp, "... now Saraa's whip hurts it")
    check(damage(t, 20, K.AIMED_SHOT, bat_s) == R.BLOCKED, "... and Bat's arrows do nothing while it's down")
    # Frozen as a dummy so far; let it fly again (it climbs back to its hover height after the stun)
    t.get_editor_property("character_movement").set_editor_property("max_fly_speed", 650.0)
    z_down = t.get_actor_location().z
    ok = yield from wait_for(lambda: not t.is_stunned() and t.get_actor_location().z > z_down + 150, 5.0)
    check(ok, f"... after the stun it flies back up (z {z_down:.0f} -> {t.get_actor_location().z:.0f})")
    clear_enemies(server)
    yield 1.0

    # ---- real attacks through the input path
    yield from bow_checks(server, host_pc if host_realm == LIVING else guest_pc, bat_s, a_s)
    yield from whip_checks(server, guest_pc if host_realm == LIVING else host_pc, saraa_s, a_s)
    yield from dodge_checks(guest_pc, guest_s, host_pc, host_s)

    # ---- the brains: StateTree + perception, targets only in their world, telegraphed attacks
    yield from ai_checks(server, client, bat_s, saraa_s, a_s)

    # ---- downed and respawn
    yield from downed_checks(server, client, host_pc, guest_pc, host_s, guest_s, a_s)

    # ---- waves, the restart pad, and the remaining debug commands
    yield from wave_checks(server, client, host_pc, guest_pc, host_s, guest_s, a_s)


def bow_checks(server, bat_pc, bat_s, a_s):
    target = a_s.spawn_enemy(TYPES["wraith"], bat_s.get_actor_location() + bat_s.get_actor_forward_vector() * 700 + unreal.Vector(0, 0, 0), False)
    freeze(target)
    target.get_health().set_editor_property("max_health", 500.0)
    target.get_health().revive()
    # Saraa stands in the line of fire: the aim and the arrows go through players
    saraa = character(server, SPIRIT)
    saraa_home = saraa.get_actor_location()
    saraa.set_actor_location(bat_s.get_actor_location() + bat_s.get_actor_forward_vector() * 250, False, True)
    yield 1.5
    bow = bat_s.get_spirit_bow()
    for kind, cmd, amount, wait in ((K.QUICK_SHOT, "quick", 10, 1.0), (K.AIMED_SHOT, "aimed", 20, 1.5), (K.CHARGED_SHOT, "charged", 45, 2.5)):
        face(bat_pc, bat_s, target.get_actor_location())
        before_shots, before_hp = bow.get_shot_count(kind), target.get_health().get_health()
        if kind != K.QUICK_SHOT:
            face_from_camera(bat_pc, target.get_actor_location())
        console(bat_pc, f"EchoShoot {cmd}")
        if kind == K.CHARGED_SHOT:
            # The camera moves over the shoulder while drawing: keep the reticle on the target until release
            for _ in range(4):
                yield 0.2
                face_from_camera(bat_pc, target.get_actor_location())
        ok = yield from wait_for(lambda: bow.get_shot_count(kind) > before_shots and target.get_health().get_health() < before_hp - 0.5, wait + 1.0)
        check(ok and abs((before_hp - target.get_health().get_health()) - amount) < 0.5,
              f"Bat's {cmd} shot fires through Saraa and hits for {amount} (dealt {before_hp - target.get_health().get_health():.0f})")
        yield 0.8
    check(target.get_health().get_last_hit_cue().heavy, "... the charged shot lands as a heavy hit (hit-stop)")
    bow.set_aiming(False)
    saraa.set_actor_location(saraa_home, False, True)
    target.remove_quietly()
    yield 0.5


def whip_checks(server, saraa_pc, saraa_s, a_s):
    target = a_s.spawn_enemy(TYPES["shade"], saraa_s.get_actor_location() + saraa_s.get_actor_forward_vector() * 280, False)
    freeze(target)
    target.get_health().set_editor_property("max_health", 500.0)
    target.get_health().revive()
    yield 1.5
    whip = saraa_s.get_sword_whip()
    counts = lambda: (whip.get_lash_kind_count(K.WHIP_LIGHT), whip.get_lash_kind_count(K.WHIP_FINISHER), whip.get_lash_kind_count(K.WHIP_CHARGED))
    before, hp = counts(), target.get_health().get_health()
    for _ in range(3):
        face(saraa_pc, saraa_s, target.get_actor_location())
        yield 0.1
        console(saraa_pc, "EchoLash combo")
        yield 0.35
    ok = yield from wait_for(lambda: counts()[1] > before[1], 2.0)
    after = counts()
    dealt = hp - target.get_health().get_health()
    check(ok and after[0] - before[0] == 2 and after[1] - before[1] == 1, f"Saraa's light combo: two lashes and a finisher reach the server ({after[0] - before[0]} + {after[1] - before[1]})")
    check(abs(dealt - (12 + 12 + 20)) < 0.5, f"... and hit for 12 + 12 + 20 (dealt {dealt:.0f})")
    yield 0.8
    hp = target.get_health().get_health()
    face(saraa_pc, saraa_s, target.get_actor_location())
    console(saraa_pc, "EchoLash charged")
    ok = yield from wait_for(lambda: counts()[2] > after[2] and target.get_health().get_health() < hp - 0.5, 2.0)
    check(ok and abs((hp - target.get_health().get_health()) - 40) < 0.5, f"Saraa's charged lash hits for 40 (dealt {hp - target.get_health().get_health():.0f})")
    target.remove_quietly()
    yield 0.5


def dodge_checks(guest_pc, guest_s, host_pc, host_s):
    for pc, char, who in ((guest_pc, guest_s, "the guest"), (host_pc, host_s, "the host")):
        god(pc, char, False)
        yield 0.6
        combat = char.get_player_combat()
        before, start = combat.get_dodge_count(), char.get_actor_location()
        console(pc, "EchoDodge")
        ok = yield from wait_for(lambda: combat.get_dodge_count() > before, 2.0)
        immune = damage(char, 10, K.ENEMY, None) == R.IMMUNE
        yield 0.8
        moved = (char.get_actor_location() - start).length()
        check(ok and immune and moved > 200, f"{who}'s dodge roll: granted by the server, invulnerable during it, {moved:.0f} cm")
        god(pc, char, True)
        yield 0.4


def ai_checks(server, client, bat_s, saraa_s, a_s):
    # A Dust Wraith closer to Saraa than to Bat: it must still only go for Bat
    near_saraa = saraa_s.get_actor_location() + unreal.Vector(250, 0, 0)
    w = a_s.spawn_enemy(TYPES["wraith"], near_saraa, False)
    s = a_s.spawn_enemy(TYPES["shade"], bat_s.get_actor_location() + unreal.Vector(250, 0, 0), False)
    phases, wc = set(), None
    targets_w, targets_s = set(), set()
    immune_before = bat_s.get_health().get_hit_count(R.IMMUNE)
    t = 0.0
    while t < 9.0:
        wc = wc or same_actor(client, w)
        if wc:
            phases.add(str(wc.get_attack_phase()))
        targets_w.add(w.get_target().get_name() if w.get_target() else "none")
        targets_s.add(s.get_target().get_name() if s.get_target() else "none")
        yield 0.1
        t += 0.1
    brain = w.get_controller()
    check(brain is not None and brain.is_brain_running(), f"the enemy brain (StateTree) runs (state {w.get_brain_state()})")
    check(saraa_s.get_name() not in targets_w and bat_s.get_name() in targets_w, f"the Dust Wraith only ever targets Bat ({sorted(targets_w)})")
    check(bat_s.get_name() not in targets_s and saraa_s.get_name() in targets_s, f"the Shade only ever targets Saraa ({sorted(targets_s)})")
    check(any("WIND_UP" in p for p in phases) and any("STRIKE" in p for p in phases), f"attacks are telegraphed: wind-up, then strike, seen on the other machine ({sorted(phases)})")
    check(bat_s.get_health().get_hit_count(R.IMMUNE) > immune_before, "the wraith's blows land on Bat (god mode makes them do nothing)")
    w.remove_quietly()
    s.remove_quietly()
    yield 0.5


def downed_checks(server, client, host_pc, guest_pc, host_s, guest_s, a_s):
    god(host_pc, host_s, False)
    god(guest_pc, guest_s, False)
    yield 0.8
    guest_c = same_actor(client, guest_s)
    console(guest_pc, "EchoHurt 500")
    ok = yield from wait_for(lambda: guest_s.is_downed() and guest_c.is_downed(), 3.0)
    check(ok, "a player at zero health goes down (seen on both machines)")
    delay = guest_s.get_player_combat().get_editor_property("respawn_delay")
    ok = yield from wait_for(lambda: not guest_s.is_downed(), delay + 3.0)
    dist = (guest_s.get_actor_location() - host_s.get_actor_location()).length()
    check(ok and dist < 450 and abs(guest_s.get_health().get_health() - 100) < 0.1, f"... and is back after {delay:.0f} s beside the other player ({dist:.0f} cm), at full health")

    # A revived player is invulnerable for a moment
    yield 2.0
    restarts = a_s.get_restart_count()
    console(host_pc, "EchoHurt 500")
    console(guest_pc, "EchoHurt 500")
    ok = yield from wait_for(lambda: host_s.is_downed() and guest_s.is_downed(), 3.0)
    check(ok, "both players down")
    ok = yield from wait_for(lambda: a_s.get_restart_count() > restarts and not host_s.is_downed() and not guest_s.is_downed(), 8.0)
    entrance = unreal.Vector(-1950, 0, 100)
    check(ok and (host_s.get_actor_location() - entrance).length() < 700 and (guest_s.get_actor_location() - entrance).length() < 700,
          f"... the arena restarts from its checkpoint, both back on their feet at the entrance")
    yield 1.0
    a_s.restart_arena()
    clear_enemies(server)
    god(host_pc, host_s, True)
    god(guest_pc, guest_s, True)
    yield 1.0


def wave_checks(server, client, host_pc, guest_pc, host_s, guest_s, a_s):
    expected = [{"EchoDustWraith": 2, "EchoShade": 2}, {"EchoStoneGuardian": 2}, {"EchoSkyTalon": 2},
                {"EchoDustWraith": 1, "EchoShade": 1, "EchoStoneGuardian": 1, "EchoSkyTalon": 1}]
    a_s.start_arena()
    a_c = arena(client)
    for number, want in enumerate(expected, 1):
        ok = yield from wait_for(lambda: a_s.get_state() == unreal.EchoArenaState.FIGHTING and a_s.get_wave_number() == number, 6.0)
        live = a_s.get_live_enemies()
        got = {}
        for e in live:
            parent = e.get_class()
            name = next((n for n in ("EchoDustWraith", "EchoShade", "EchoStoneGuardian", "EchoSkyTalon") if isinstance(e, getattr(unreal, n))), parent.get_name())
            got[name] = got.get(name, 0) + 1
        check(ok and got == want, f"wave {number}: {got}")
        yield 0.5
        if number == 1:
            check(a_c.get_wave_number() == 1 and "Wave 1" in str(a_c.get_status_text()), f"the guest's HUD shows the wave ({a_c.get_status_text()})")
        for e in live:
            damage(e, 9999, K.DEBUG, None)
        yield 0.5
    ok = yield from wait_for(lambda: a_s.get_state() == unreal.EchoArenaState.COMPLETE, 4.0)
    check(ok, "all four waves cleared: the arena is complete")

    # The restart pad by the return door
    pads = gs.get_all_actors_of_class(server, unreal.EchoArenaRestartPad)
    restarts = a_s.get_restart_count()
    if check(len(pads) == 1, "there's a restart pad"):
        host_s.set_actor_location(pads[0].get_actor_location() + unreal.Vector(0, 0, 100), False, True)
        ok = yield from wait_for(lambda: a_s.get_restart_count() > restarts, 3.0)
        check(ok, "stepping on the restart pad restarts the arena")
        ok = yield from wait_for(lambda: a_s.get_state() == unreal.EchoArenaState.FIGHTING and a_s.get_wave_number() == 1, 4.0)
        check(ok, "... and wave 1 starts again")

    # Debug: skip to a wave, show all enemies to both
    console(guest_pc, "EchoSkipToWave 3")
    ok = yield from wait_for(lambda: a_s.get_wave_number() == 3 and a_s.get_state() == unreal.EchoArenaState.FIGHTING, 3.0)
    check(ok, "EchoSkipToWave 3 (from the guest) jumps to the Sky Talons")
    w = a_s.spawn_enemy(TYPES["wraith"], unreal.Vector(0, 0, 100), False)
    s = a_s.spawn_enemy(TYPES["shade"], unreal.Vector(0, 300, 100), False)
    yield 1.5
    wc, sc = same_actor(client, w), same_actor(client, s)
    hidden_before = (not w.is_shown_locally() or not wc.is_shown_locally()) and (not s.is_shown_locally() or not sc.is_shown_locally())
    console(guest_pc, "EchoShowAllEnemies")
    ok = yield from wait_for(lambda: w.is_shown_locally() and wc.is_shown_locally() and s.is_shown_locally() and sc.is_shown_locally(), 3.0)
    check(hidden_before and ok, "EchoShowAllEnemies shows every enemy to both players")
    console(guest_pc, "EchoShowAllEnemies")
    yield 1.0
    a_s.restart_arena()
    clear_enemies(server)
    yield 1.0


def travel_scenario():
    """The doors: the arena's return door takes both players back to the first level, beside its combat door; that
    door brings them back. Eras load right on both machines after each trip"""
    def roles(world):
        b, s = character(world, LIVING), character(world, SPIRIT)
        return b, s

    for leg, (door_map, arrive_map, tag) in enumerate((("CombatTest", "Lvl_SpiritPath", "CombatDoor"), ("Lvl_SpiritPath", "CombatTest", "ArenaEntrance"))):
        ok = yield from wait_for(lambda: worlds()[0] and worlds()[1] and worlds()[0].get_name() == door_map and worlds()[1].get_name() == door_map
                                 and character(worlds()[0], LIVING) and character(worlds()[1], SPIRIT) and director(worlds()[0]).is_ready(), 30.0)
        server, client = worlds()
        check(ok, f"both players in {door_map}")
        yield 2.5   # the door arms a moment after the level starts
        doors = gs.get_all_actors_of_class(server, unreal.EchoTravelDoor)
        if not check(len(doors) >= 1, f"{door_map} has a door"):
            return
        door = doors[0]
        bat = character(server, LIVING)
        log(f"Bat walks into {door.get_actor_label()} ({door.get_editor_property('target_map')})")
        # Walk in for real: a door opened from inside Python (a teleport into its trigger) would run the guest's
        # ClientTravel RPC on the host (editor Python runs RPCs locally), and the host would leave without the guest
        fwd = door.get_actor_forward_vector()
        bat.set_actor_location(door.get_actor_location() + fwd * 400 + unreal.Vector(0, 0, 100), False, True)
        yield 0.5

        def walk_and_arrived():
            b = character(worlds()[0], LIVING) if worlds()[0] else None
            if b and worlds()[0].get_name() == door_map:
                b.add_movement_input(fwd * -1.0, 1.0, False)
            return worlds()[0] and worlds()[1] and worlds()[0].get_name() == arrive_map and worlds()[1].get_name() == arrive_map \
                and character(worlds()[0], LIVING) and character(worlds()[0], SPIRIT) and character(worlds()[1], SPIRIT)
        ok = yield from wait_for(walk_and_arrived, 40.0, step=0.02)
        check(ok, f"the door takes BOTH players to {arrive_map}")
        if not ok:
            return
        server, client = worlds()
        ok = yield from wait_for(lambda: director(server).is_ready() and director(client).is_ready(), 20.0)
        check(ok and director(server).is_era_loaded(unreal.EchoEra.PRESENT) and director(client).is_era_loaded(unreal.EchoEra.PAST)
              and not director(server).is_era_loaded(unreal.EchoEra.PAST) and not director(client).is_era_loaded(unreal.EchoEra.PRESENT),
              f"... with each player's own era loaded in {arrive_map}")
        starts = {str(s.get_editor_property("player_start_tag")): s for s in gs.get_all_actors_of_class(server, unreal.PlayerStart)}
        bat, saraa = character(server, LIVING), character(server, SPIRIT)
        sb, ss = starts.get(f"{tag}_Bat"), starts.get(f"{tag}_Saraa")
        db = (bat.get_actor_location() - sb.get_actor_location()).length() if sb else -1
        ds = (saraa.get_actor_location() - ss.get_actor_location()).length() if ss else -1
        check(0 <= db < 300 and 0 <= ds < 300, f"... arriving at the {tag} starts (Bat {db:.0f} cm, Saraa {ds:.0f} cm away; starts {sorted(str(k) for k in starts)})")
        yield 1.0


# ------------------------------------------------------------------ driver

def on_tick(delta):
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
    server, _ = worlds()
    if server and NETEMU:
        for cmd in ("NetEmulation.PktLag 0", "NetEmulation.PktLagVariance 0", "NetEmulation.PktLoss 0"):
            unreal.SystemLibrary.execute_console_command(server, cmd)
    log(f"{T['checks']} checks in {unreal.SystemLibrary.get_game_time_in_seconds(server) - T['start'] if server else 0:.0f}s")
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")


_server, _ = worlds()
T["start"] = unreal.SystemLibrary.get_game_time_in_seconds(_server) if _server else 0.0
T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
