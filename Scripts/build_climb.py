"""
Builds Milestone 3's level section, "The Climb", into /Game/Echo/Maps/Lvl_SpiritPath, on the left (-Y) canyon
wall just past the Spirit Path's far side:

  - 7 echo platforms (AEchoPlatform) zig-zagging up the wall, 90 cm higher each (the characters' jump apex is
    ~128 cm), 4.1-4.5 m apart centre to centre. P3 is TIMED.
  - A tall rock SCREEN stands in front of the wall around P6 and P7: from the open canyon floor Bat can't see or
    shoot them. He has to walk into the gap between the screen and the wall ("the specific spot").
  - A rock LEDGE 7.2 m up at the top of the climb. Saraa's switch on it swings down a hinged RAMP
    (AEchoRisingBridge with bHingeAtStart) so Bat can walk up. The END ZONE moves onto the ledge.
  - A CHECKPOINT at the climb base: anyone who falls into a kill volume after reaching it respawns there.

Only actors tagged ClimbBuilder are deleted and rebuilt, plus the Milestone 1 EndZone is moved (the brief allows
that). Run build_canyon_rocks.py afterwards so no rocks land in the climb area (it has an exclusion zone for it).
Run it in the open editor:
  py exec(open(r'<project>/Scripts/build_climb.py').read())
Output lines are tagged ECHO_CLIMB in Saved/Logs/OurLastEcho.log.

Layout is in "wall coordinates": s = canyon station (X along the centreline, as in build_canyon.py),
u = cm out from the left wall's face into the canyon, heights relative to the ground at the first platform.
"""
import math

import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
MARK = "ECHO_CLIMB"
TAG = "ClimbBuilder"

# Canyon layout functions (face_point, ground_height, ...), without building the canyon
G = {"ECHO_CANYON_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_canyon.py").read(), G)

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

RISE = 90.0                  # height step between platforms
PLATFORM = 250.0             # platform footprint (square)
HOP = 430.0                  # centre-to-centre distance between platforms (edge gap ~1.8 m)
FIRST_S = 4100.0             # station of P1

# (u, timed) for P1..P7; heights are RISE * index above the base ground. Stations follow from HOP
PLATFORMS = [
    (450, False),
    (600, False),
    (450, True),             # timed: Bat has to wake it just before Saraa needs it
    (350, False),
    (300, False),
    (280, False),            # behind the screen
    (320, False),            # behind the screen
]
TIMED_SECONDS = 7.0

SCREEN = dict(u0=600, u1=750, top=1500)           # rock screen in front of P6/P7, from just before P6 to the ledge
LEDGE = dict(length=1400, u0=-300, u1=1000, top=8 * RISE, gap=100)  # 7.2 m above the base; front edge ~1 m past P7
RAMP = dict(offset=700, width=400, thickness=40, run=1400)          # stations after the ledge front; hinged at its outer edge
SWITCH = dict(offset=280, u=450)
END_ZONE = dict(offset=1080, u=450)
CHECKPOINT = dict(s0=3700, s1=4700, u0=0, u1=2600, height=800, respawn_s=3950, respawn_u=1300)


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def wall_xy(s, u):
    """World XY at station s, u cm out from the left wall face"""
    return G["face_point"](-1, s, -u)


def wall_yaw(s):
    """Yaw pointing along the left wall in +s direction"""
    x0, y0 = wall_xy(s - 50, 0)
    x1, y1 = wall_xy(s + 50, 0)
    return math.degrees(math.atan2(y1 - y0, x1 - x0))


def advance(s, u_from, u_to, distance, direction=1):
    """Station where wall_xy(station, u_to) is `distance` cm (horizontally) from wall_xy(s, u_from)"""
    x0, y0 = wall_xy(s, u_from)
    step = 5.0 * direction
    t = s
    while abs(t - s) < 5000:
        t += step
        x, y = wall_xy(t, u_to)
        if math.hypot(x - x0, y - y0) >= distance:
            return t
    raise RuntimeError("advance: no station found")


def layout():
    """Stations of every piece, derived from HOP so hops stay the same physical length around the bend"""
    stations = [FIRST_S]
    for i in range(1, len(PLATFORMS)):
        stations.append(advance(stations[-1], PLATFORMS[i - 1][0], PLATFORMS[i][0], HOP))
    last_s, last_u = stations[-1], PLATFORMS[-1][0]
    ledge_s0 = advance(last_s, last_u, last_u, PLATFORM / 2 + LEDGE["gap"])
    # Along the wall only (same u), so the screen starts ~2 m before P6 whatever the sideways offsets are
    screen_s0 = advance(stations[5], PLATFORMS[5][0], PLATFORMS[5][0], PLATFORM / 2 + 80, -1)
    return dict(stations=stations, ledge_s0=ledge_s0, ledge_s1=ledge_s0 + LEDGE["length"], screen_s0=screen_s0,
                ramp_s=ledge_s0 + RAMP["offset"], switch_s=ledge_s0 + SWITCH["offset"], zone_s=ledge_s0 + END_ZONE["offset"])


def spawn(cls, loc, rot=(0.0, 0.0, 0.0), label=None, folder="Climb"):
    """rot = (pitch, yaw, roll)"""
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    if label:
        actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def rock_box(label, s0, s1, u0, u1, z0, z1, material):
    """A rock block covering a wall-aligned box (stations s0..s1, u0..u1, heights z0..z1)"""
    xa, ya = wall_xy(s0, (u0 + u1) / 2)
    xb, yb = wall_xy(s1, (u0 + u1) / 2)
    length = math.hypot(xb - xa, yb - ya)
    yaw = math.degrees(math.atan2(yb - ya, xb - xa))
    actor = spawn(unreal.StaticMeshActor, ((xa + xb) / 2, (ya + yb) / 2, (z0 + z1) / 2), (0.0, yaw, 0.0), label, "Climb/Rock")
    smc = actor.static_mesh_component
    smc.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Cube"))
    smc.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(length / 100.0, (u1 - u0) / 100.0, (z1 - z0) / 100.0))
    return actor


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(G["MAP_PATH"]):
        raise RuntimeError(f"open {G['MAP_PATH']} first")

    removed = 0
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            eas.destroy_actor(actor)
            removed += 1
    log(f"removed {removed} old climb actors")

    rock = eal.load_asset("/Game/Echo/Materials/MI_CanyonRock")
    L = layout()
    base_x, base_y = wall_xy(FIRST_S, PLATFORMS[0][0])
    g0 = G["ground_height"](base_x, base_y)
    log(f"base ground height {g0:.0f}")

    # Echo platforms
    previous = None
    for index, ((u, timed), s) in enumerate(zip(PLATFORMS, L["stations"]), start=1):
        x, y = wall_xy(s, u)
        z = g0 + RISE * index
        p = spawn(unreal.EchoPlatform, (x, y, z), (0.0, wall_yaw(s), 0.0), f"EchoPlatform_{index}{'_Timed' if timed else ''}", "Climb/Platforms")
        p.set_editor_property("platform_size", unreal.Vector(PLATFORM, PLATFORM, 25.0))
        p.set_editor_property("timed", timed)
        if timed:
            p.set_editor_property("awake_duration", TIMED_SECONDS)
        if previous:
            px, py, pz = previous
            d = math.hypot(x - px, y - py)
            log(f"  P{index - 1} -> P{index}: {d:.0f} cm apart (edge gap ~{d - PLATFORM:.0f}), rise {z - pz:.0f}")
        previous = (x, y, z)

    # Screen and ledge
    rock_box("Climb_Screen", L["screen_s0"], L["ledge_s0"] + 50, SCREEN["u0"], SCREEN["u1"], g0 - 300, g0 + SCREEN["top"], rock)
    ledge = rock_box("Climb_Ledge", L["ledge_s0"], L["ledge_s1"], LEDGE["u0"], LEDGE["u1"], g0 - 300, g0 + LEDGE["top"], rock)
    ledge_top = g0 + LEDGE["top"]
    lx, ly = wall_xy(L["ledge_s0"], PLATFORMS[-1][0])
    log(f"  P7 -> ledge: front edge {math.hypot(lx - previous[0], ly - previous[1]) - PLATFORM / 2:.0f} cm past P7's edge, rise {ledge_top - previous[2]:.0f}")
    log(f"  stations: platforms {[round(s) for s in L['stations']]}, screen from {L['screen_s0']:.0f}, ledge {L['ledge_s0']:.0f}-{L['ledge_s1']:.0f}")

    # Ramp: hinged exactly on the ledge block's outer face (the block is straight but the wall curves, so the
    # face isn't at u1 everywhere) and sloping straight out from it to meet the ground, so there's no step at the top
    lc = ledge.get_actor_location()
    lyaw = math.radians(ledge.get_actor_rotation().yaw)
    half_depth = (LEDGE["u1"] - LEDGE["u0"]) / 2
    ax, ay = math.cos(lyaw), math.sin(lyaw)               # along the ledge
    nx, ny = -ay, ax                                       # across it; flip to point into the canyon
    ox, oy = wall_xy(L["ramp_s"], LEDGE["u1"] + 500)
    if (ox - lc.x) * nx + (oy - lc.y) * ny < 0:
        nx, ny = -nx, -ny
    px, py = wall_xy(L["ramp_s"], LEDGE["u1"])
    along = (px - lc.x) * ax + (py - lc.y) * ay
    hx, hy = lc.x + ax * along + nx * half_depth, lc.y + ay * along + ny * half_depth
    fx, fy = hx + nx * RAMP["run"], hy + ny * RAMP["run"]
    foot_z = G["ground_height"](fx, fy)
    run = math.hypot(fx - hx, fy - hy)
    drop = ledge_top - foot_z
    length = math.hypot(run, drop) + 60.0   # a little extra so the foot tucks into the ground
    pitch = -math.degrees(math.atan2(drop, run))
    yaw = math.degrees(math.atan2(fy - hy, fx - hx))
    ramp = spawn(unreal.EchoRisingBridge, (hx, hy, ledge_top), (pitch, yaw, 0.0), "Climb_Ramp", "Climb/Gameplay")
    ramp.set_editor_property("bridge_size", unreal.Vector(length, RAMP["width"], RAMP["thickness"]))
    ramp.set_editor_property("lowered_offset", unreal.Vector(0, 0, 0))
    ramp.set_editor_property("hinge_at_start", True)
    # Stowed nearly upright at the ledge edge, like a raised drawbridge
    ramp.set_editor_property("start_rotation_offset", unreal.Rotator(roll=0.0, pitch=-pitch + 88.0, yaw=0.0))
    log(f"  ramp: {length:.0f} cm long at {-pitch:.1f} degrees, foot ground {foot_z:.0f}")

    # Saraa's switch on the ledge
    sx, sy = wall_xy(L["switch_s"], SWITCH["u"])
    switch = spawn(unreal.EchoSpiritSwitch, (sx, sy, ledge_top), (0.0, wall_yaw(L["switch_s"]), 0.0), "Climb_Switch", "Climb/Gameplay")
    switch.set_editor_property("target_bridge", ramp)

    # Checkpoint at the base
    cx, cy = wall_xy((CHECKPOINT["s0"] + CHECKPOINT["s1"]) / 2, (CHECKPOINT["u0"] + CHECKPOINT["u1"]) / 2)
    cyaw = wall_yaw((CHECKPOINT["s0"] + CHECKPOINT["s1"]) / 2)
    cp = spawn(unreal.EchoCheckpoint, (cx, cy, g0 - 100), (0.0, cyaw, 0.0), "Climb_Checkpoint", "Climb/Gameplay")
    cp.set_editor_property("volume_size", unreal.Vector(CHECKPOINT["s1"] - CHECKPOINT["s0"], CHECKPOINT["u1"] - CHECKPOINT["u0"], CHECKPOINT["height"]))
    rx, ry = wall_xy(CHECKPOINT["respawn_s"], CHECKPOINT["respawn_u"])
    rz = G["ground_height"](rx, ry) + 100.0
    # RespawnPoint is relative to the checkpoint actor
    local = unreal.MathLibrary.inverse_transform_location(cp.get_actor_transform(), unreal.Vector(rx, ry, rz))
    cp.set_editor_property("respawn_point", local)

    # Move the Milestone 1 end zone to the top of the climb
    zone = next((a for a in eas.get_all_level_actors() if a.get_actor_label() == "EndZone"), None)
    if zone:
        zx, zy = wall_xy(L["zone_s"], END_ZONE["u"])
        zone.set_actor_location(unreal.Vector(zx, zy, ledge_top), False, False)
        zone.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=wall_yaw(L["zone_s"])), False)
        log(f"moved EndZone to ({zx:.0f}, {zy:.0f}, {ledge_top:.0f})")
    else:
        log("WARNING: EndZone not found")

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    total = sum(1 for a in eas.get_all_level_actors() if a.actor_has_tag(TAG))
    log(f"done: {total} climb actors")


# Scripts/test_climb_live.py loads this file just for the layout, with ECHO_CLIMB_NO_BUILD set
if not globals().get("ECHO_CLIMB_NO_BUILD"):
    try:
        main()
    except Exception as e:
        import traceback
        log(f"FAILED: {e!r}\n{traceback.format_exc()}")
