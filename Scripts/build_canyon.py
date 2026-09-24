"""
Builds Milestone 2, the canyon around the Spirit Path, into /Game/Echo/Maps/Lvl_SpiritPath:
  - Materials  /Game/Echo/Materials: M_CanyonRock (world-space strata, no textures) + MI_CanyonRock,
               MI_CanyonBoulder, MI_CanyonGround
  - Landscape  canyon floor (via unreal.EchoEditorLibrary.create_landscape_from_heights) with a
               ravine cut right across the canyon under the Spirit Path gap
  - Walls      layered, terraced stacks of rotated engine cubes along both sides, closed at both ends
  - Overlook   raised rock shelf with a ramp, on the right wall just past the end zone
  - Bounds     EchoBoundaryVolumes along the wall faces and across both ends, plus a canyon-wide
               EchoRespawnVolume (kill volume) below the floor
  - Lighting   warm, low late-afternoon sun, hazy atmosphere and fog, a desaturating post-process
Rocks are scattered separately by PCG (/Game/Echo/PCG/PCG_CanyonRocks), not by this script.

Milestone 1 gameplay actors (tag EchoBuilder) are never moved or resized. This script only
deletes and respawns actors tagged CanyonBuilder, and retunes the existing Sun/Sky/Fog lights.
It needs a rendering editor (landscape edit layers are merged on the GPU), so run it from the
open editor's console, not headless:
  py exec(open(r'<project>/Scripts/build_canyon.py').read())
Output lines are tagged ECHO_CANYON in Saved/Logs/OurLastEcho.log.

Coordinates are cm. The canyon runs along +X; the Spirit Path sits at Y=0 from X=-1500 to 3600,
with the ravine (Bat's pit) between X=0 and X=1600. Walking surfaces there are at Z=0.
"""
import math
import random

import unreal

MARK = "ECHO_CANYON"
TAG = "CanyonBuilder"
MAP_PATH = "/Game/Echo/Maps/Lvl_SpiritPath"
MAT_DIR = "/Game/Echo/Materials"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

CUBE = "/Engine/BasicShapes/Cube"

# ------------------------------------------------------------------ layout

X_START_END = -16500        # 150 m before the start of the Spirit Path
X_FAR_END = 18600           # 150 m past the end zone side
PLAY_X = (-1500, 3600)      # Milestone 1 floor slabs
RAVINE_X = (0, 1600)        # Bat's pit
SEGMENT = 1400              # wall segment spacing along the canyon

# Control points along X: half-widths (centreline to wall face) and wall heights, per side.
# L = the -Y wall, R = the +Y wall. Floor width = WL + WR = 37..52 m.
CTRL_X = [-17000, -12000, -7000, -3000, 0, 3000, 6000, 10000, 14000, 19000]
CTRL_WL = [1700, 2400, 1600, 2000, 1800, 1900, 2600, 1700, 2300, 1800]
CTRL_WR = [2000, 1600, 2500, 1700, 2000, 2600, 2400, 2800, 1800, 2200]
CTRL_HL = [6000, 7500, 5000, 4500, 5500, 6500, 7800, 5200, 6800, 7000]
CTRL_HR = [5500, 4800, 7200, 6000, 4200, 5000, 6200, 7600, 5000, 6500]

# Overlook: rock shelf against the R wall, reached by a ramp from the far side
LEDGE_X = (3000, 4400)
LEDGE_TOP = 900
LEDGE_REACH = 900           # how far the shelf sticks out from the wall face
RAMP_FOOT_X = 7000

rng = random.Random(20260924)


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def smoothstep(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


def interp(xs, ys, x):
    """Cosine interpolation through control points (clamped at the ends)"""
    if x <= xs[0]:
        return ys[0]
    for i in range(len(xs) - 1):
        if x <= xs[i + 1]:
            t = (x - xs[i]) / (xs[i + 1] - xs[i])
            t = (1.0 - math.cos(t * math.pi)) * 0.5
            return ys[i] + (ys[i + 1] - ys[i]) * t
    return ys[-1]


def centre_y(x):
    """Canyon centreline: straight around the play area, bending away at both ends"""
    if x > 5600:
        return 6500.0 * smoothstep((x - 5600) / 13000.0)
    if x < -3500:
        return -5500.0 * smoothstep((-3500 - x) / 12500.0)
    return 0.0


def centre_yaw(x):
    return math.degrees(math.atan2(centre_y(x + 50) - centre_y(x - 50), 100.0))


def half_width(side, x):
    return interp(CTRL_X, CTRL_WL if side < 0 else CTRL_WR, x)


def wall_height(side, x):
    return interp(CTRL_X, CTRL_HL if side < 0 else CTRL_HR, x)


def outward(side, x):
    """Unit vector (x, y) pointing from the centreline into the given wall"""
    a = math.radians(centre_yaw(x))
    return (-math.sin(a) * side, math.cos(a) * side)


def face_point(side, x, extra=0.0):
    """World XY of the wall face (plus `extra` cm into the rock) at canyon station x"""
    nx, ny = outward(side, x)
    d = half_width(side, x) + extra
    return (x + nx * d, centre_y(x) + ny * d)


def in_play_zone(x, y, margin=0.0):
    return PLAY_X[0] - 500 - margin <= x <= PLAY_X[1] + 500 + margin and abs(y) <= 1000 + margin


# ------------------------------------------------------------------ materials

def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def build_rock_material():
    """
    Lit, texture-free rock: horizontal sedimentary bands from world Z (wobbled by X/Y so they
    aren't ruler-straight), occasional thin dark bands, and low-frequency noise for variation.
    Because it's world-space, it works on any scaled cube and on the landscape without UVs.
    """
    path = f"{MAT_DIR}/M_CanyonRock"
    if eal.does_asset_exist(path):
        # Rebuild the graph in place so the material instances keep their parent
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset("M_CanyonRock", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    wp =expr(mat, unreal.MaterialExpressionWorldPosition, -1800, 0)
    mask_x = expr(mat, unreal.MaterialExpressionComponentMask, -1600, -200, r=True, g=False, b=False, a=False)
    mask_y = expr(mat, unreal.MaterialExpressionComponentMask, -1600, 0, r=False, g=True, b=False, a=False)
    mask_z = expr(mat, unreal.MaterialExpressionComponentMask, -1600, 200, r=False, g=False, b=True, a=False)
    for m in (mask_x, mask_y, mask_z):
        mel.connect_material_expressions(wp, "", m, "")

    # wobble = (sin(X * 0.0013) + sin(Y * 0.0011)) * 120
    sx = expr(mat, unreal.MaterialExpressionMultiply, -1400, -200, const_b=0.0013)
    mel.connect_material_expressions(mask_x, "", sx, "A")
    sin_x = expr(mat, unreal.MaterialExpressionSine, -1250, -200, period=6.2831853)
    mel.connect_material_expressions(sx, "", sin_x, "")
    sy = expr(mat, unreal.MaterialExpressionMultiply, -1400, 0, const_b=0.0011)
    mel.connect_material_expressions(mask_y, "", sy, "A")
    sin_y = expr(mat, unreal.MaterialExpressionSine, -1250, 0, period=6.2831853)
    mel.connect_material_expressions(sy, "", sin_y, "")
    wob = expr(mat, unreal.MaterialExpressionAdd, -1100, -100)
    mel.connect_material_expressions(sin_x, "", wob, "A")
    mel.connect_material_expressions(sin_y, "", wob, "B")
    wob_amp = expr(mat, unreal.MaterialExpressionMultiply, -950, -100)
    wobble = expr(mat, unreal.MaterialExpressionScalarParameter, -1100, 60, parameter_name="Wobble", default_value=120.0)
    mel.connect_material_expressions(wob, "", wob_amp, "A")
    mel.connect_material_expressions(wobble, "", wob_amp, "B")

    # bands = sin((Z + wobble) / BandScale) * 0.5 + 0.5
    z_wob = expr(mat, unreal.MaterialExpressionAdd, -800, 150)
    mel.connect_material_expressions(mask_z, "", z_wob, "A")
    mel.connect_material_expressions(wob_amp, "", z_wob, "B")
    band_scale = expr(mat, unreal.MaterialExpressionScalarParameter, -950, 300, parameter_name="BandScale", default_value=90.0)
    z_div = expr(mat, unreal.MaterialExpressionDivide, -650, 150)
    mel.connect_material_expressions(z_wob, "", z_div, "A")
    mel.connect_material_expressions(band_scale, "", z_div, "B")
    band = expr(mat, unreal.MaterialExpressionSine, -500, 150, period=6.2831853)
    mel.connect_material_expressions(z_div, "", band, "")
    band01 = expr(mat, unreal.MaterialExpressionMultiply, -350, 150, const_b=0.5)
    mel.connect_material_expressions(band, "", band01, "A")
    band01b = expr(mat, unreal.MaterialExpressionAdd, -220, 150, const_b=0.5)
    mel.connect_material_expressions(band01, "", band01b, "A")

    color_a = expr(mat, unreal.MaterialExpressionVectorParameter, -350, -350, parameter_name="ColorA",
                   default_value=unreal.LinearColor(0.42, 0.22, 0.09, 1.0))
    color_b = expr(mat, unreal.MaterialExpressionVectorParameter, -350, -180, parameter_name="ColorB",
                   default_value=unreal.LinearColor(0.56, 0.41, 0.25, 1.0))
    lerp_ab = expr(mat, unreal.MaterialExpressionLinearInterpolate, -50, -150)
    mel.connect_material_expressions(color_a, "", lerp_ab, "A")
    mel.connect_material_expressions(color_b, "", lerp_ab, "B")
    mel.connect_material_expressions(band01b, "", lerp_ab, "Alpha")

    # thin dark bands: clamp(sin(Z / (BandScale * 0.37) + X * 0.0007) * 4 - 3, 0, 1)
    band_scale2 = expr(mat, unreal.MaterialExpressionMultiply, -650, 400, const_b=0.37)
    mel.connect_material_expressions(band_scale, "", band_scale2, "A")
    z_div2 = expr(mat, unreal.MaterialExpressionDivide, -500, 330)
    mel.connect_material_expressions(z_wob, "", z_div2, "A")
    mel.connect_material_expressions(band_scale2, "", z_div2, "B")
    x_drift = expr(mat, unreal.MaterialExpressionMultiply, -500, 480, const_b=0.0007)
    mel.connect_material_expressions(mask_x, "", x_drift, "A")
    thin_in = expr(mat, unreal.MaterialExpressionAdd, -350, 400)
    mel.connect_material_expressions(z_div2, "", thin_in, "A")
    mel.connect_material_expressions(x_drift, "", thin_in, "B")
    thin = expr(mat, unreal.MaterialExpressionSine, -220, 400, period=6.2831853)
    mel.connect_material_expressions(thin_in, "", thin, "")
    thin4 = expr(mat, unreal.MaterialExpressionMultiply, -100, 400, const_b=4.0)
    mel.connect_material_expressions(thin, "", thin4, "A")
    thin_off = expr(mat, unreal.MaterialExpressionSubtract, 20, 400, const_b=3.0)
    mel.connect_material_expressions(thin4, "", thin_off, "A")
    thin_sat = expr(mat, unreal.MaterialExpressionSaturate, 140, 400)
    mel.connect_material_expressions(thin_off, "", thin_sat, "")
    color_c = expr(mat, unreal.MaterialExpressionVectorParameter, 20, 230, parameter_name="ColorC",
                   default_value=unreal.LinearColor(0.26, 0.13, 0.06, 1.0))
    lerp_c = expr(mat, unreal.MaterialExpressionLinearInterpolate, 280, 0)
    mel.connect_material_expressions(lerp_ab, "", lerp_c, "A")
    mel.connect_material_expressions(color_c, "", lerp_c, "B")
    mel.connect_material_expressions(thin_sat, "", lerp_c, "Alpha")

    # brightness variation from world-space noise (0.8 .. 1.15)
    noise = expr(mat, unreal.MaterialExpressionNoise, 20, -400, scale=0.003, levels=4, output_min=0.8, output_max=1.15,
                 noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
    mel.connect_material_expressions(wp, "", noise, "Position")
    final = expr(mat, unreal.MaterialExpressionMultiply, 450, -100)
    mel.connect_material_expressions(lerp_c, "", final, "A")
    mel.connect_material_expressions(noise, "", final, "B")

    rough = expr(mat, unreal.MaterialExpressionScalarParameter, 300, 250, parameter_name="Roughness", default_value=0.93)
    mel.connect_material_property(final, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    log(f"built {path}")
    return mat


def build_instance(name, parent, vectors, scalars):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


# ------------------------------------------------------------------ actors

def spawn(cls, loc, rot=(0.0, 0.0, 0.0), label=None, folder="Canyon"):
    """rot = (pitch, yaw, roll) in degrees"""
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    if label:
        actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def rock_block(label, center, size, rot, material, folder):
    """Engine cube scaled to `size` (x = length, y = depth, z = height, cm) around `center`"""
    actor = spawn(unreal.StaticMeshActor, center, rot, label, folder)
    smc = actor.static_mesh_component
    smc.set_static_mesh(eal.load_asset(CUBE))
    smc.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


def slab_between(label, p_top, p_bottom, width, thickness, material, folder):
    """A walkable slab whose top surface runs from p_bottom up to p_top (3D points)"""
    dx, dy, dz = p_top[0] - p_bottom[0], p_top[1] - p_bottom[1], p_top[2] - p_bottom[2]
    run = math.hypot(dx, dy)
    length = math.sqrt(run * run + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, run))
    # Drop the centre by half the thickness along the slab's up vector so the TOP face hits the points
    up_z = math.cos(math.radians(pitch))
    up_h = -math.sin(math.radians(pitch))
    mid = ((p_top[0] + p_bottom[0]) / 2, (p_top[1] + p_bottom[1]) / 2, (p_top[2] + p_bottom[2]) / 2)
    cx = mid[0] - (thickness / 2) * up_h * math.cos(math.radians(yaw))
    cy = mid[1] - (thickness / 2) * up_h * math.sin(math.radians(yaw))
    cz = mid[2] - (thickness / 2) * up_z
    return rock_block(label, (cx, cy, cz), (length, width, thickness), (pitch, yaw, 0.0), material, folder)


def boundary(label, center, size, yaw):
    b = spawn(unreal.EchoBoundaryVolume, center, (0.0, yaw, 0.0), label, "Canyon/Bounds")
    b.set_editor_property("box_size", unreal.Vector(*size))
    return b


# ------------------------------------------------------------------ landscape

LS_QUADS = 63
LS_COMPONENTS = (6, 4)
LS_ORIGIN = (-17550.0, -12550.0, 0.0)   # vertices land on ..., -50, 50, ... so the ravine is fully open over X 0..1600
LS_SPACING = 100.0


def ground_height(x, y):
    """Landscape height (cm) at a world XY position"""
    c = centre_y(x)
    d = y - c
    side = 1 if d >= 0 else -1
    w = half_width(side, x)
    ad = abs(d)

    # Gentle rolling floor, flattened to just under the Milestone 1 slabs around the play area
    rolling = 70.0 * math.sin(x / 2300.0) * math.sin(y / 1700.0) + 45.0 * math.sin(x / 900.0 + y / 1300.0) + 20.0
    play_fade = 0.0
    if PLAY_X[0] - 2500 <= x <= PLAY_X[1] + 2500:
        dx = max(0.0, PLAY_X[0] - 500 - x, x - (PLAY_X[1] + 500))
        dy = max(0.0, abs(y) - 1000)
        play_fade = 1.0 - smoothstep(math.hypot(dx, dy) / 2000.0)
    h = rolling * (1.0 - play_fade) + (-3.0) * play_fade

    # Talus apron rising into the walls, and high ground hidden under/behind them
    t = (ad - (w - 700.0)) / 900.0
    if t > 0:
        h += 320.0 * smoothstep(t) ** 1.3
    if ad > w + 200:
        h += min(900.0, (ad - w - 200) * 0.6)

    # Rockfall ramps up into the end caps
    h += 700.0 * smoothstep((x - (X_FAR_END - 1600)) / 1600.0)
    h += 700.0 * smoothstep(((X_START_END + 1600) - x) / 1600.0)

    # Ravine right across the canyon under the Spirit Path gap (inside the Milestone 1 respawn volume)
    if RAVINE_X[0] < x < RAVINE_X[1] and ad < w + 2500:
        h = -1400.0
    return h


def build_landscape(material):
    verts_x = LS_COMPONENTS[0] * LS_QUADS + 1
    verts_y = LS_COMPONENTS[1] * LS_QUADS + 1
    heights = []
    for j in range(verts_y):
        y = LS_ORIGIN[1] + j * LS_SPACING
        for i in range(verts_x):
            heights.append(ground_height(LS_ORIGIN[0] + i * LS_SPACING, y))
    log(f"landscape heights {verts_x}x{verts_y}, min {min(heights):.0f} max {max(heights):.0f}")
    landscape = unreal.EchoEditorLibrary.create_landscape_from_heights(
        unreal.Vector(*LS_ORIGIN), unreal.Vector(LS_SPACING, LS_SPACING, 100.0), LS_COMPONENTS[0], LS_COMPONENTS[1],
        LS_QUADS, heights, material, "CanyonFloor", unreal.Name(TAG))
    if not landscape:
        raise RuntimeError("landscape creation failed (see LogOurLastEcho)")
    landscape.set_folder_path("Canyon")
    return landscape


# ------------------------------------------------------------------ walls

def near_ravine(x):
    return RAVINE_X[0] - 1600 < x < RAVINE_X[1] + 1600


def build_wall(side, rock):
    name = "L" if side < 0 else "R"
    count = 0
    x = X_START_END - 1000
    k = 0
    while x <= X_FAR_END + 1000:
        yaw = centre_yaw(x)
        nx, ny = outward(side, x)
        w = half_width(side, x)
        total = wall_height(side, x) + rng.uniform(-500, 500)
        base = -1700.0 if near_ravine(x) else -500.0

        # Split the height into 4-6 strata, thicker at the bottom
        layers = max(4, min(6, int(round(total / 1400.0))))
        weights = [rng.uniform(0.8, 1.3) * (1.35 if i == 0 else 1.0) for i in range(layers)]
        scale = total / sum(weights)
        z_bot, inset = base, 0.0
        for i in range(layers):
            z_top = sum(weights[:i + 1]) * scale
            if i == 0:
                # Players can walk up to this one: keep it flush and nearly upright so the
                # boundary volume just in front of it is never more than ~1-2 m from the rock
                jitter, yaw_j, tilt = rng.uniform(0, 100), rng.uniform(-3, 3), 1.0
            else:
                inset += rng.uniform(120, 600)
                jitter, yaw_j, tilt = rng.uniform(0, 250), rng.uniform(-7, 7), 3.0
            depth = 3200.0
            length = SEGMENT * rng.uniform(1.25, 1.6)
            dist = w + 60.0 + inset + jitter + depth / 2
            center = (x + nx * dist, centre_y(x) + ny * dist, (z_bot + z_top) / 2)
            rot = (rng.uniform(-tilt, tilt), yaw + yaw_j, rng.uniform(-tilt, tilt))
            rock_block(f"Wall_{name}_{k:02d}_{i}", center, (length, depth, z_top - z_bot), rot, rock, f"Canyon/Walls_{name}")
            count += 1
            z_bot = z_top - rng.uniform(80, 250)   # small overlap so strata read as layers, not gaps

        # Invisible wall just in front of the rock face, from below the ravine to far above the rim
        bx, by = face_point(side, x, 30.0 + 250.0)   # box is 500 deep, inner face 30 cm in front of the rock
        boundary(f"Bound_{name}_{k:02d}", (bx, by, 6000.0), (SEGMENT * 1.3, 500.0, 16000.0), yaw)
        x += SEGMENT
        k += 1
    return count


def build_end_cap(label, x_end, direction, rock):
    """Closes the canyon: stacked blocks across its full width, plus a boundary in front"""
    yaw = centre_yaw(x_end)
    c = centre_y(x_end)
    width = half_width(-1, x_end) + half_width(1, x_end) + 3000.0
    mid_offset = (half_width(1, x_end) - half_width(-1, x_end)) / 2
    fx, fy = math.cos(math.radians(yaw)) * direction, math.sin(math.radians(yaw)) * direction
    px, py = -math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    total = rng.uniform(5000, 7000)
    z_bot, inset, count = -300.0, 0.0, 0
    for i in range(5):
        z_top = total * (i + 1) / 5
        inset += 0 if i == 0 else rng.uniform(200, 700)
        depth = 3500.0
        dist = inset + depth / 2
        center = (x_end + fx * dist + px * mid_offset, c + fy * dist + py * mid_offset, (z_bot + z_top) / 2)
        rot = (rng.uniform(-2, 2), yaw + rng.uniform(-4, 4), rng.uniform(-2, 2))
        rock_block(f"{label}_{i}", center, (depth, width, z_top - z_bot), rot, rock, "Canyon/EndCaps")
        z_bot = z_top - 150
        count += 1
    bx, by = x_end - fx * 280 + px * mid_offset, c - fy * 280 + py * mid_offset
    boundary(f"Bound_{label}", (bx, by, 6000.0), (500.0, width, 16000.0), yaw)
    return count


def build_overlook(rock):
    """Rock shelf on the R wall past the end zone, and a ramp up to it from the far side"""
    x0, x1 = LEDGE_X
    xm = (x0 + x1) / 2
    w = half_width(1, xm)
    inner, outer = w - LEDGE_REACH, w + 900
    rock_block("Overlook_Shelf", (xm, (inner + outer) / 2, (LEDGE_TOP - 400) / 2),
               (x1 - x0, outer - inner, LEDGE_TOP + 400), (0.0, 0.0, 0.0), rock, "Canyon/Overlook")
    # A lip of broken rock along the shelf's +X end so the ramp joins it cleanly
    top = face_point(1, x1, -330.0)
    foot = face_point(1, RAMP_FOOT_X, -330.0)
    slab_between("Overlook_Ramp", (x1 - 20, top[1], LEDGE_TOP), (foot[0], foot[1], 0.0), 450.0, 80.0, rock, "Canyon/Overlook")


# ------------------------------------------------------------------ lighting

def set_props(obj, props):
    for k, v in props.items():
        try:
            obj.set_editor_property(k, v)
        except Exception as e:
            log(f"  could not set {obj.get_class().get_name()}.{k}: {e}")


def tune_lighting():
    by_label = {a.get_actor_label(): a for a in eas.get_all_level_actors()}

    sun = by_label.get("Sun")
    if sun:
        # Low, warm late-afternoon sun from down-canyon, raking across the +Y wall
        sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-19.0, yaw=140.0), False)
        lc = sun.light_component
        set_props(lc, {"intensity": 7.5, "light_color": unreal.Color(r=255, g=205, b=160, a=255),
                       "atmosphere_sun_light": True})

    atmo = by_label.get("SkyAtmosphere")
    if atmo:
        # More Mie (dust) and less Rayleigh (blue) for a hazy, desaturated sky
        set_props(atmo.get_component_by_class(unreal.SkyAtmosphereComponent), {
            "mie_scattering_scale": 0.008,
            "mie_absorption_scale": 0.002,
            "rayleigh_scattering_scale": 0.024,
            "aerial_pespective_view_distance_scale": 2.5,   # sic: the engine property is misspelt
        })

    fog = by_label.get("HeightFog")
    if fog:
        fog.set_actor_location(unreal.Vector(0, 0, -200), False, False)
        set_props(fog.component, {
            "fog_density": 0.012,
            "fog_height_falloff": 0.2,
            "fog_max_opacity": 0.7,
            "start_distance": 2500.0,
            "fog_inscattering_luminance": unreal.LinearColor(0.2, 0.14, 0.09, 1.0),
            "directional_inscattering_exponent": 8.0,
            "directional_inscattering_luminance": unreal.LinearColor(0.5, 0.3, 0.15, 1.0),
        })

    sky = by_label.get("SkyLight")
    if sky:
        set_props(sky.light_component, {"intensity": 1.2})

    # Unbound post-process: slightly desaturated, warm and dusty
    ppv = spawn(unreal.PostProcessVolume, (0, 0, 500), label="CanyonPostProcess", folder="Canyon/Lighting")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    for k, v in {
        "override_color_saturation": True, "color_saturation": unreal.Vector4(0.8, 0.8, 0.8, 1.0),
        "override_color_contrast": True, "color_contrast": unreal.Vector4(1.05, 1.05, 1.05, 1.0),
        "override_color_gain": True, "color_gain": unreal.Vector4(1.05, 1.0, 0.92, 1.0),
        "override_vignette_intensity": True, "vignette_intensity": 0.35,
    }.items():
        try:
            s.set_editor_property(k, v)
        except Exception as e:
            log(f"  could not set PostProcessSettings.{k}: {e}")
    ppv.set_editor_property("settings", s)


# ------------------------------------------------------------------ build

def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(MAP_PATH):
        raise RuntimeError(f"open {MAP_PATH} first (current: {world.get_path_name()})")

    removed = 0
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            eas.destroy_actor(actor)
            removed += 1
    log(f"removed {removed} old canyon actors")

    rock_master = build_rock_material()
    rock = build_instance("MI_CanyonRock", rock_master, {}, {})
    boulder = build_instance("MI_CanyonBoulder", rock_master, {
        "ColorA": unreal.LinearColor(0.36, 0.2, 0.1, 1.0),
        "ColorB": unreal.LinearColor(0.5, 0.36, 0.22, 1.0),
    }, {"BandScale": 35.0})
    ground = build_instance("MI_CanyonGround", rock_master, {
        "ColorA": unreal.LinearColor(0.5, 0.34, 0.18, 1.0),
        "ColorB": unreal.LinearColor(0.62, 0.48, 0.32, 1.0),
        "ColorC": unreal.LinearColor(0.44, 0.28, 0.15, 1.0),
    }, {"BandScale": 60.0, "Wobble": 90.0})

    build_landscape(ground)
    blocks = build_wall(-1, rock) + build_wall(1, rock)
    blocks += build_end_cap("EndCap_Start", X_START_END, -1, rock)
    blocks += build_end_cap("EndCap_Far", X_FAR_END, 1, rock)
    build_overlook(rock)
    log(f"walls and caps: {blocks} blocks")

    # Canyon-wide kill volume under the floor (the Milestone 1 one only covers the ravine)
    kill = spawn(unreal.EchoRespawnVolume, (1000, 0, -2300), label="CanyonKillVolume", folder="Canyon/Bounds")
    kill.set_editor_property("volume_size", unreal.Vector(40000, 26000, 1400))

    tune_lighting()

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    total = sum(1 for a in eas.get_all_level_actors() if a.actor_has_tag(TAG))
    log(f"done: {total} canyon actors")


try:
    main()
except Exception as e:
    import traceback
    log(f"FAILED: {e!r}\n{traceback.format_exc()}")
