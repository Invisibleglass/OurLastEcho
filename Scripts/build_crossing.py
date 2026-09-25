"""
Builds Milestone 4's level section, "The Crossing", into /Game/Echo/Maps/Lvl_SpiritPath, straight after The Climb:

  - A CHASM right across the canyon (cut into the landscape by build_canyon.py, CHASM_S), from the end of The Climb's
    ledge to a GATE WALL. Saraa can only cross it by swinging from anchors Bat makes by shooting ANCHOR TARGETS:
      1. Swing 1 (single, teaches the basics): from The Climb's ledge, under a target on OVERHANG 1, to PILLAR 1.
      2. Swing 2 (chained): from pillar 1 under two targets on the underside of a big ROCK ARCH, to PILLAR 2. Both
         anchors must exist before she jumps, and Bat can only have two, so this teaches the limit.
      3. Swing 3: from pillar 2 under a target on OVERHANG 3, landing on an ECHO PLATFORM Bat has to wake. From it she
         steps up through a DOORWAY in the gate wall onto the far rim.
    A rock "curtain" hangs from the arch's back edge, so target 4 (and the far end) can't be shot from the start
    area: Bat has to go out along his shelf, past the arch.
  - BAT'S ROUTE: a rock SHELF along the right wall, all the way to his own doorway in the gate wall, broken by a gap
    near the end. Saraa's SWITCH on the far rim lowers a DRAWBRIDGE over it.
  - CHECKPOINTS at the start of each route (realm-specific), a KILL VOLUME in the chasm, and the END ZONE moved to
    the far rim.

Only actors tagged CrossingBuilder are deleted and rebuilt, plus the EndZone is moved (the brief allows that). Run
build_canyon.py first (it cuts the chasm), then build_climb.py, then this, then build_canyon_rocks.py.
Run it in the open editor:
  py exec(open(r'<project>/Scripts/build_crossing.py').read())
Output lines are tagged ECHO_CROSSING in Saved/Logs/OurLastEcho.log.

Layout is in wall coordinates like build_climb.py: s = canyon station, u = cm out from the LEFT wall face; Bat's
shelf uses r = cm out from the RIGHT wall face. Heights are absolute (cm).
"""
import math

import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
MARK = "ECHO_CROSSING"
TAG = "CrossingBuilder"

# Canyon and Climb layout functions, without building either
G = {"ECHO_CANYON_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_canyon.py").read(), G)
C = {"ECHO_CLIMB_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_climb.py").read(), C)

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

CHASM_S0, CHASM_S1 = G["CHASM_S"]
DEEP = -1500.0                    # rock pieces reach below the chasm floor
PATH_U = 600.0                    # Saraa's swing line, out from the left wall
TARGET_DIAMETER = 120.0
TARGET_THICKNESS = 12.0

# Saraa's pieces (s0, s1, top): pillars and the echo platform
PILLAR_1 = dict(s0=9100, s1=9800, u0=150, u1=1100, top=650)
PILLAR_2 = dict(s0=12950, s1=13550, u0=150, u1=1100, top=600)
ECHO_PLATFORM = dict(s0=14600, s1=15000, size_u=400, top=550)

# Overhangs from the left wall (underside z is where the target goes) and the arch across the whole canyon
OVERHANG_1 = dict(s0=8250, s1=8650, u0=-1500, u1=900, bottom=1740, top=2400)
OVERHANG_3 = dict(s0=14000, s1=14400, u0=-1500, u1=900, bottom=1850, top=2500)
ARCH = dict(s0=10250, s1=12450, bottom=1750, top=2600, curtain_bottom=1150, curtain_depth=150)

# Anchor targets (label, station, height of the surface they're on); all on undersides at PATH_U, facing down
TARGETS = [
    ("Target_1_Overhang", 8450, OVERHANG_1["bottom"]),
    ("Target_2_ArchFront", 10600, ARCH["bottom"]),
    ("Target_3_ArchBack", 12200, ARCH["bottom"]),
    ("Target_4_Overhang", 14200, OVERHANG_3["bottom"]),
]

# Gate wall across the canyon at the chasm's far edge, with a doorway for each of them
GATE = dict(s0=CHASM_S1, s1=CHASM_S1 + 400, top=1500)
SARAA_DOOR = dict(u0=450, u1=750, sill=ECHO_PLATFORM["top"] + 90, height=260)
BAT_DOOR = dict(r0=50, r1=450, height=350)

# Bat's shelf along the right wall (r = out from the right wall face), and the gap the drawbridge spans
SHELF = dict(r0=-300, r1=450, ramp=700, s0=CHASM_S0 - 50, s1=CHASM_S1 + 10, gap_s0=13600, gap_s1=14600, piece=600)
BRIDGE = dict(r=250, width=400, thickness=40)

SWITCH = dict(s=15900, u=1400)    # a few metres aside from where she drops out of her doorway
END_ZONE = dict(s=16400)
KILL_VOLUME = dict(lo=(6000, -2600, -1500), hi=(17300, 8000, -300))
CHECKPOINT_SARAA = dict(s0=7050, s1=7770, u0=-300, u1=1000, respawn_s=7500, respawn_u=450)
CHECKPOINT_BAT = dict(s0=6900, s1=8200, r0=0, r1=1200, respawn_s=7000, respawn_r=400)


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def left_xy(s, u):
    return G["face_point"](-1, s, -u)


def right_xy(s, r):
    return G["face_point"](1, s, -r)


def canyon_width(s):
    lx, ly = G["face_point"](-1, s)
    rx, ry = G["face_point"](1, s)
    return math.hypot(rx - lx, ry - ly)


def wall_yaw(s):
    x0, y0 = G["face_point"](-1, s - 50)
    x1, y1 = G["face_point"](-1, s + 50)
    return math.degrees(math.atan2(y1 - y0, x1 - x0))


def ground_at(x, y):
    return G["ground_height"](x, y)


def shelf_top():
    """
    Bat's shelf (and his doorway's sill) sits just above the highest ground inside his doorway: the talus on the far
    rim is higher than the ground where he steps on at the near end, which gets a ramp (build_rock)
    """
    heights = [ground_at(*right_xy(s, r)) for s in range(int(GATE["s0"]), int(GATE["s1"]) + 1, 50)
               for r in range(BAT_DOOR["r0"], BAT_DOOR["r1"] + 1, 50)]
    return round(max(heights) + 10)


def slope_box(label, p_low, p_high, width, thickness, material, folder="Crossing/Rock"):
    """A walkable slab whose top surface runs from p_low up to p_high (world xyz)"""
    dx, dy, dz = p_high[0] - p_low[0], p_high[1] - p_low[1], p_high[2] - p_low[2]
    run = math.hypot(dx, dy)
    length = math.sqrt(run * run + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, run))
    # Drop the centre by half the thickness along the slab's up vector, so the TOP face runs through the points
    up = (-math.sin(math.radians(pitch)) * math.cos(math.radians(yaw)), -math.sin(math.radians(pitch)) * math.sin(math.radians(yaw)), math.cos(math.radians(pitch)))
    centre = [(p_low[i] + p_high[i]) / 2 - up[i] * thickness / 2 for i in range(3)]
    actor = spawn(unreal.StaticMeshActor, tuple(centre), (pitch, yaw, 0.0), label, folder)
    smc = actor.static_mesh_component
    smc.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Cube"))
    smc.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(length / 100.0, width / 100.0, thickness / 100.0))
    return actor


def spawn(cls, loc, rot=(0.0, 0.0, 0.0), label=None, folder="Crossing"):
    """rot = (pitch, yaw, roll)"""
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    if label:
        actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def box_between(label, pa, pb, depth, z0, z1, material, folder="Crossing/Rock"):
    """A rock block whose centre line runs from world XY pa to pb, `depth` wide, heights z0..z1"""
    length = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
    yaw = math.degrees(math.atan2(pb[1] - pa[1], pb[0] - pa[0]))
    actor = spawn(unreal.StaticMeshActor, ((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2, (z0 + z1) / 2), (0.0, yaw, 0.0), label, folder)
    smc = actor.static_mesh_component
    smc.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Cube"))
    smc.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(length / 100.0, depth / 100.0, (z1 - z0) / 100.0))
    return actor


def left_box(label, s0, s1, u0, u1, z0, z1, material, folder="Crossing/Rock"):
    """Wall-aligned block from the left wall's point of view (like build_climb.rock_box)"""
    um = (u0 + u1) / 2
    return box_between(label, left_xy(s0, um), left_xy(s1, um), u1 - u0, z0, z1, material, folder)


def right_box(label, s0, s1, r0, r1, z0, z1, material, folder="Crossing/Rock"):
    rm = (r0 + r1) / 2
    return box_between(label, right_xy(s0, rm), right_xy(s1, rm), r1 - r0, z0, z1, material, folder)


def across_box(label, s0, s1, z0, z1, material, u0=-1500.0, u1=None, folder="Crossing/Rock"):
    """A block across the canyon between stations s0..s1, from u0 (left wall) to u1 (default: past the right wall)"""
    sm = (s0 + s1) / 2
    if u1 is None:
        u1 = canyon_width(sm) + 1500.0   # well into both walls (upper strata are set back)
    return box_between(label, left_xy(sm, u0), left_xy(sm, u1), s1 - s0, z0, z1, material, folder)


def build_rock(rock):
    count = 0
    # Saraa's pillars
    for name, p in (("Pillar_1", PILLAR_1), ("Pillar_2", PILLAR_2)):
        left_box(f"Crossing_{name}", p["s0"], p["s1"], p["u0"], p["u1"], DEEP, p["top"], rock)
        count += 1

    # Overhangs from the left wall
    for name, o in (("Overhang_1", OVERHANG_1), ("Overhang_3", OVERHANG_3)):
        left_box(f"Crossing_{name}", o["s0"], o["s1"], o["u0"], o["u1"], o["bottom"], o["top"], rock)
        count += 1

    # The arch, in pieces along its depth so it follows the curving canyon, plus a rock curtain hanging from its back
    # edge. (A front curtain was tried and blocked Saraa's own view of the first arch anchor from pillar 1.)
    a = ARCH
    pieces = 4
    step = (a["s1"] - a["s0"]) / pieces
    for i in range(pieces):
        across_box(f"Crossing_Arch_{i}", a["s0"] + i * step - 20, a["s0"] + (i + 1) * step + 20, a["bottom"], a["top"], rock)
    across_box("Crossing_Arch_BackCurtain", a["s1"] - a["curtain_depth"], a["s1"], a["curtain_bottom"], a["bottom"] + 10, rock)
    count += pieces + 1

    # Gate wall with two doorways. Split along u into: left part, Saraa's doorway column, middle, Bat's doorway column, right part
    g = GATE
    gm = (g["s0"] + g["s1"]) / 2
    width = canyon_width(gm)
    top = shelf_top()
    bat_u0, bat_u1 = width - BAT_DOOR["r1"], width - BAT_DOOR["r0"]
    sd = SARAA_DOOR
    across_box("Crossing_Gate_Left", g["s0"], g["s1"], DEEP, g["top"], rock, -800, sd["u0"])
    across_box("Crossing_Gate_SaraaDoorBelow", g["s0"], g["s1"], DEEP, sd["sill"], rock, sd["u0"], sd["u1"])
    across_box("Crossing_Gate_SaraaDoorAbove", g["s0"], g["s1"], sd["sill"] + sd["height"], g["top"], rock, sd["u0"], sd["u1"])
    across_box("Crossing_Gate_Middle", g["s0"], g["s1"], DEEP, g["top"], rock, sd["u1"], bat_u0)
    across_box("Crossing_Gate_BatDoorBelow", g["s0"], g["s1"], DEEP, top, rock, bat_u0, bat_u1)
    across_box("Crossing_Gate_BatDoorAbove", g["s0"], g["s1"], top + BAT_DOOR["height"], g["top"], rock, bat_u0, bat_u1)
    across_box("Crossing_Gate_Right", g["s0"], g["s1"], DEEP, g["top"], rock, bat_u1, width + 800)
    count += 7

    # Bat's shelf in short pieces so it hugs the curving wall, with the drawbridge gap
    sh = SHELF
    s = sh["s0"]
    index = 0
    while s < sh["s1"]:
        s_next = min(sh["s1"], s + sh["piece"])
        if s_next > sh["gap_s0"] and s < sh["gap_s1"]:
            # Stop at the gap, resume after it
            if s < sh["gap_s0"]:
                s_next = sh["gap_s0"]
            else:
                s = sh["gap_s1"]
                continue
        right_box(f"Crossing_Shelf_{index:02d}", s - 30, s_next + 30 if s_next < sh["s1"] else s_next, sh["r0"], sh["r1"], DEEP, top, rock)
        index += 1
        count += 1
        s = s_next
    # A ramp up onto the shelf from the canyon floor at its near end
    rs0 = sh["s0"] - sh["ramp"]
    lx, ly = right_xy(rs0, 250)
    hx, hy = right_xy(sh["s0"] + 40, 250)
    slope_box("Crossing_ShelfRamp", (lx, ly, ground_at(lx, ly) - 5), (hx, hy, top), 700, 120, rock)
    count += 1
    log(f"rock: {count} blocks, shelf top {top} (ground at the ramp foot {ground_at(lx, ly):.0f})")
    return top


def build_targets():
    for label, s, surface_z in TARGETS:
        x, y = left_xy(s, PATH_U)
        # Facing straight down out of the underside (+X is the direction the target faces)
        t = spawn(unreal.EchoAnchorTarget, (x, y, surface_z), (-90.0, wall_yaw(s), 0.0), label, "Crossing/Targets")
        t.set_editor_property("diameter", TARGET_DIAMETER)
        t.set_editor_property("thickness", TARGET_THICKNESS)
    log(f"targets: {len(TARGETS)}")


def build_gameplay(top, rock):
    # Echo platform in front of Saraa's doorway
    ep = ECHO_PLATFORM
    sm = (ep["s0"] + ep["s1"]) / 2
    x, y = left_xy(sm, (SARAA_DOOR["u0"] + SARAA_DOOR["u1"]) / 2)
    p = spawn(unreal.EchoPlatform, (x, y, ep["top"]), (0.0, wall_yaw(sm), 0.0), "Crossing_EchoPlatform", "Crossing/Gameplay")
    p.set_editor_property("platform_size", unreal.Vector(ep["s1"] - ep["s0"], ep["size_u"], 25.0))
    p.set_editor_property("timed", False)

    # Drawbridge over the gap in Bat's shelf: hinged on the far side, stowed upright, swings down towards Bat
    sh, br = SHELF, BRIDGE
    hx, hy = right_xy(sh["gap_s1"], br["r"])
    fx, fy = right_xy(sh["gap_s0"], br["r"])
    length = math.hypot(fx - hx, fy - hy) + 80.0
    yaw = math.degrees(math.atan2(fy - hy, fx - hx))
    bridge = spawn(unreal.EchoRisingBridge, (hx, hy, top), (0.0, yaw, 0.0), "Crossing_Drawbridge", "Crossing/Gameplay")
    bridge.set_editor_property("bridge_size", unreal.Vector(length, br["width"], br["thickness"]))
    bridge.set_editor_property("lowered_offset", unreal.Vector(0, 0, 0))
    bridge.set_editor_property("hinge_at_start", True)
    bridge.set_editor_property("start_rotation_offset", unreal.Rotator(roll=0.0, pitch=88.0, yaw=0.0))
    log(f"drawbridge: {length:.0f} cm at shelf height {top}")

    # Saraa's switch on the far rim, just past her doorway
    sx, sy = left_xy(SWITCH["s"], SWITCH["u"])
    switch = spawn(unreal.EchoSpiritSwitch, (sx, sy, ground_at(sx, sy)), (0.0, wall_yaw(SWITCH["s"]), 0.0), "Crossing_Switch", "Crossing/Gameplay")
    switch.set_editor_property("target_bridge", bridge)

    # A low step inside Saraa's doorway down to the far rim is not needed: she simply drops ~5 m

    # Checkpoints: Saraa's at the end of The Climb's ledge, Bat's at the start of his shelf
    cs = CHECKPOINT_SARAA
    ledge_top = C["G"]["ground_height"](*C["wall_xy"](C["FIRST_S"], C["PLATFORMS"][0][0])) + C["LEDGE"]["top"]
    cx, cy = left_xy((cs["s0"] + cs["s1"]) / 2, (cs["u0"] + cs["u1"]) / 2)
    cp = spawn(unreal.EchoCheckpoint, (cx, cy, ledge_top - 100), (0.0, wall_yaw((cs["s0"] + cs["s1"]) / 2), 0.0), "Crossing_Checkpoint_Saraa", "Crossing/Gameplay")
    cp.set_editor_property("volume_size", unreal.Vector(cs["s1"] - cs["s0"], cs["u1"] - cs["u0"], 600))
    cp.set_editor_property("only_one_realm", True)
    cp.set_editor_property("only_realm", unreal.EchoRealm.SPIRIT)
    rx, ry = left_xy(cs["respawn_s"], cs["respawn_u"])
    cp.set_editor_property("respawn_point", unreal.MathLibrary.inverse_transform_location(cp.get_actor_transform(), unreal.Vector(rx, ry, ledge_top + 100)))

    cb = CHECKPOINT_BAT
    bm = (cb["s0"] + cb["s1"]) / 2
    bx, by = right_xy(bm, (cb["r0"] + cb["r1"]) / 2)
    cpb = spawn(unreal.EchoCheckpoint, (bx, by, top - 150), (0.0, wall_yaw(bm), 0.0), "Crossing_Checkpoint_Bat", "Crossing/Gameplay")
    cpb.set_editor_property("volume_size", unreal.Vector(cb["s1"] - cb["s0"], cb["r1"] - cb["r0"], 700))
    cpb.set_editor_property("only_one_realm", True)
    cpb.set_editor_property("only_realm", unreal.EchoRealm.LIVING)
    rx, ry = right_xy(cb["respawn_s"], cb["respawn_r"])
    cpb.set_editor_property("respawn_point", unreal.MathLibrary.inverse_transform_location(cpb.get_actor_transform(), unreal.Vector(rx, ry, ground_at(rx, ry) + 100)))

    # Kill volume filling the chasm below its rims
    lo, hi = KILL_VOLUME["lo"], KILL_VOLUME["hi"]
    kill = spawn(unreal.EchoRespawnVolume, ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2), label="Crossing_KillVolume", folder="Crossing/Gameplay")
    kill.set_editor_property("volume_size", unreal.Vector(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]))

    # Move the end zone to the far rim
    zone = next((a for a in eas.get_all_level_actors() if a.get_actor_label() == "EndZone"), None)
    if zone:
        zs = END_ZONE["s"]
        zx, zy = left_xy(zs, canyon_width(zs) / 2)
        zz = ground_at(zx, zy)
        zone.set_actor_location(unreal.Vector(zx, zy, zz), False, False)
        zone.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=wall_yaw(zs)), False)
        log(f"moved EndZone to ({zx:.0f}, {zy:.0f}, {zz:.0f})")
    else:
        log("WARNING: EndZone not found")


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(G["MAP_PATH"]):
        raise RuntimeError(f"open {G['MAP_PATH']} first")

    removed = 0
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            eas.destroy_actor(actor)
            removed += 1
    log(f"removed {removed} old crossing actors")

    rock = eal.load_asset("/Game/Echo/Materials/MI_CanyonRock")
    top = build_rock(rock)
    build_targets()
    build_gameplay(top, rock)

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    total = sum(1 for a in eas.get_all_level_actors() if a.actor_has_tag(TAG))
    log(f"done: {total} crossing actors")


# Scripts/test_crossing_live.py loads this file just for the layout, with ECHO_CROSSING_NO_BUILD set
if not globals().get("ECHO_CROSSING_NO_BUILD"):
    try:
        main()
    except Exception as e:
        import traceback
        log(f"FAILED: {e!r}\n{traceback.format_exc()}")
