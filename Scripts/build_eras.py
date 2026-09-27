"""
Milestone 6: two eras, one skeleton, two skins.

Splits Lvl_SpiritPath into
  - the SHARED layer (Lvl_SpiritPath itself, loaded on every machine): all gameplay actors and ALL collision. The
    canyon's rock and floor meshes and the PCG rocks stay here, still blocking, but hidden in game.
  - the PRESENT layer (/Game/Echo/Maps/Lvl_SpiritPath_Present, Bat's era) and the PAST layer
    (/Game/Echo/Maps/Lvl_SpiritPath_Past, Saraa's era): visible copies of those shapes (instanced, same transforms,
    NO collision) in each era's materials, plus each era's lighting, sky, fog, colour grading, props and ambience.
AEchoEraDirector (placed here in the shared layer) loads the local player's era on each machine at runtime.

What it builds:
  - /Game/Echo/Eras: era versions of the canyon materials (M_CanyonRock's FixedEra), prop colours, water, and the
    era looks of the other player (MI_GhostFromPresent), Bat's arrow seen from the past (MI_ArrowPast,
    MI_ArrowTrailPast) and Saraa's whip line seen from the present (MI_WhipLinePresent).
  - Present: low warm sunset sun, hazy dusty fog, desaturated ochre grading, a dry cracked riverbed along the floor,
    dead trees and stumps, ruins (a fallen cairn and broken walls), a wind ambience hook.
  - Past: bright mid-morning sun, clear blue sky, light mist, saturated green/blue grading, a shallow river where
    the riverbed is, grass on the floor, living trees where the dead ones stand, an intact ovoo cairn where the
    fallen one lies, intact walls, water / birds / distant drums ambience hooks.

Run it in the open editor, AFTER the other level builders (build_canyon, build_climb, build_crossing and
build_canyon_rocks respawn visible shared geometry and build_canyon a shared post-process volume):
  py exec(open(r'<project>/Scripts/build_eras.py').read())
It rebuilds both era levels from scratch and is safe to re-run. Output lines are tagged ECHO_ERAS.
"""
import math
import random

import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
MARK = "ECHO_ERAS"
TAG = "ErasBuilder"
SHARED_MAP = "/Game/Echo/Maps/Lvl_SpiritPath"
ERA_MAPS = {"Present": "/Game/Echo/Maps/Lvl_SpiritPath_Present", "Past": "/Game/Echo/Maps/Lvl_SpiritPath_Past"}
ERA_DIR = "/Game/Echo/Eras"
MAT_DIR = "/Game/Echo/Materials"
AUDIO_DIR = "/Game/Echo/Audio"
SOURCE_TAGS = ("CanyonBuilder", "ClimbBuilder", "CrossingBuilder", "EchoBuilder")
SHARED_LIGHTING = ("Sun", "SkyLight", "SkyAtmosphere", "HeightFog", "CanyonPostProcess")

CUBE = "/Engine/BasicShapes/Cube.Cube"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
CONE = "/Engine/BasicShapes/Cone.Cone"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Canyon layout functions (centreline, widths, ground height), without building anything
G = {"ECHO_CANYON_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_canyon.py").read(), G)


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


# ------------------------------------------------------------------ materials

def material_instance(name, parent, vectors=None, scalars=None):
    path = f"{ERA_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, ERA_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


def build_materials():
    """Returns {era: {source material path: era material}} for the canyon copies, and {era: {prop name: material}}"""
    glow = eal.load_asset(f"{MAT_DIR}/M_EchoGlow")
    outline = eal.load_asset(f"{MAT_DIR}/M_EchoOutline")
    ghost = eal.load_asset(f"{MAT_DIR}/M_Ghost")

    canyon = {}
    for era, fixed in (("Present", 0.0), ("Past", 1.0)):
        canyon[era] = {}
        for source in ("MI_CanyonRock", "MI_CanyonBoulder", "MI_CanyonGround"):
            parent = eal.load_asset(f"{MAT_DIR}/{source}")
            canyon[era][parent.get_path_name()] = material_instance(f"{source}_{era}", parent, scalars={"UseEraCollection": 0.0, "FixedEra": fixed})
        grid = eal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray")
        surface = (0.34, 0.26, 0.17, 1.0) if era == "Present" else (0.16, 0.24, 0.12, 1.0)
        canyon[era][grid.get_path_name()] = material_instance(f"MI_Floor_{era}", grid, vectors={"SurfaceColor": surface})

    def colour(name, rgb, glow_amount=0.0):
        return material_instance(name, glow, vectors={"Color": (*rgb, 1.0)}, scalars={"Glow": glow_amount})

    props = {
        "Present": {
            "wood": colour("MI_DeadWood", (0.13, 0.1, 0.075)),
            "stump": colour("MI_Stump", (0.2, 0.15, 0.1)),
            "stone": colour("MI_RuinStone", (0.33, 0.29, 0.24)),
            "bed": colour("MI_Riverbed", (0.26, 0.19, 0.12)),
            "crack": colour("MI_RiverbedCrack", (0.05, 0.035, 0.02)),
            "brush": colour("MI_DryBrush", (0.28, 0.22, 0.11)),
            "cloth": colour("MI_FadedCloth", (0.35, 0.4, 0.45)),
        },
        "Past": {
            "wood": colour("MI_Bark", (0.1, 0.06, 0.035)),
            "leaves": colour("MI_Leaves", (0.06, 0.3, 0.05)),
            "leaves2": colour("MI_LeavesDark", (0.035, 0.2, 0.04)),
            "grass": colour("MI_Grass", (0.09, 0.38, 0.05)),
            "grass2": colour("MI_GrassLight", (0.2, 0.45, 0.08)),
            "water": colour("MI_River", (0.02, 0.14, 0.4), 0.06),
            "stone": colour("MI_ShrineStone", (0.42, 0.42, 0.44)),
            "khadag": colour("MI_Khadag", (0.03, 0.2, 0.85), 0.25),
            "pole": colour("MI_ShrinePole", (0.3, 0.2, 0.1)),
        },
    }

    # The other side's things, in each era's style
    material_instance("MI_GhostFromPresent", ghost, vectors={"GhostColor": (1.0, 0.55, 0.2, 0.35)})
    material_instance("MI_ArrowPast", glow, vectors={"Color": (0.3, 0.7, 1.0, 1.0)}, scalars={"Glow": 5.0})
    material_instance("MI_ArrowTrailPast", outline, vectors={"Color": (0.35, 0.7, 1.0, 1.0)}, scalars={"Glow": 5.0, "Flicker": 0.0, "Opacity": 0.7})
    material_instance("MI_WhipLinePresent", outline, vectors={"Color": (0.7, 0.75, 0.8, 1.0)}, scalars={"Glow": 0.25, "Flicker": 0.0, "Opacity": 0.35})
    log("materials built")
    return canyon, props


# ------------------------------------------------------------------ the shared layer's visible shapes

def is_canyon_mesh(actor):
    return isinstance(actor, unreal.StaticMeshActor) and any(actor.actor_has_tag(t) for t in SOURCE_TAGS) \
        and actor.static_mesh_component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION \
        and actor.static_mesh_component.get_editor_property("static_mesh") is not None


def pcg_instance_components():
    comps = []
    for actor in eas.get_all_level_actors():
        if isinstance(actor, unreal.PCGVolume):
            comps += list(actor.get_components_by_class(unreal.InstancedStaticMeshComponent))
    return comps


def collect_shapes():
    """{(mesh path, material path): [world transforms]} for every canyon mesh and PCG rock in the shared layer"""
    groups = {}
    for actor in eas.get_all_level_actors():
        if is_canyon_mesh(actor):
            smc = actor.static_mesh_component
            key = (smc.get_editor_property("static_mesh").get_path_name(), smc.get_material(0).get_path_name())
            groups.setdefault(key, []).append(actor.get_actor_transform())
    for comp in pcg_instance_components():
        key = (comp.get_editor_property("static_mesh").get_path_name(), comp.get_material(0).get_path_name())
        for i in range(comp.get_instance_count()):
            groups.setdefault(key, []).append(comp.get_instance_transform(i, True))
    log(f"shared shapes: {sum(len(v) for v in groups.values())} in {len(groups)} mesh/material groups")
    return groups


def hide_shared_shapes():
    """Hidden in game (still solid, still visible in the editor): their era copies are what players see"""
    hidden = 0
    for actor in eas.get_all_level_actors():
        if is_canyon_mesh(actor):
            actor.set_actor_hidden_in_game(True)
            hidden += 1
    for comp in pcg_instance_components():
        comp.set_hidden_in_game(True)
        hidden += comp.get_instance_count()
    removed = 0
    for actor in eas.get_all_level_actors():
        if actor.get_actor_label() in SHARED_LIGHTING:
            eas.destroy_actor(actor)
            removed += 1
    log(f"shared layer: {hidden} shapes hidden in game (collision kept), {removed} lighting actors moved to the eras")


# ------------------------------------------------------------------ era levels

def open_map(path):
    if eal.does_asset_exist(path):
        les.load_level(path)
    else:
        # A blank map saved under the era's name (duplicating a map asset and then opening it leaks the copy's world,
        # which the editor treats as a fatal error)
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        if not world or not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
            raise RuntimeError(f"couldn't create {path}")
        log(f"created {path}")
    world = editor_world()
    if not world.get_path_name().startswith(path + "."):
        raise RuntimeError(f"couldn't open {path} (the editor is on {world.get_path_name()}); nothing was changed")
    return world


def clear_map():
    for actor in eas.get_all_level_actors():
        if isinstance(actor, unreal.Brush):
            # Every level has the editor's builder brush; an era layer mustn't collide, so not even that
            brush = actor.get_component_by_class(unreal.BrushComponent)
            if brush:
                brush.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        elif not isinstance(actor, unreal.WorldSettings):
            eas.destroy_actor(actor)


def spawn(cls, loc=(0.0, 0.0, 0.0), rot=(0.0, 0.0, 0.0), label=None, folder=""):
    """rot = (pitch, yaw, roll)"""
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    if label:
        actor.set_actor_label(label)
    if folder:
        actor.set_folder_path(folder)
    return actor


def props(label, mesh, material, transforms, folder, shadow=True):
    if not transforms:
        return None
    actor = spawn(unreal.EchoInstancedProps, label=label, folder=folder)
    actor.set_editor_property("mesh", eal.load_asset(mesh) if isinstance(mesh, str) else mesh)
    actor.set_editor_property("material", material)
    actor.set_editor_property("cast_shadow", shadow)
    actor.set_editor_property("instances", transforms)
    return actor


def xf(loc, rot=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    return unreal.Transform(unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]), unreal.Vector(*scale))


class Batch:
    """Collects transforms per (mesh, material name) and spawns one instanced-props actor per pair"""
    def __init__(self):
        self.groups = {}

    def add(self, mesh, mat, transform):
        self.groups.setdefault((mesh, mat), []).append(transform)

    def spawn(self, materials, folder, prefix):
        for (mesh, mat), transforms in self.groups.items():
            props(f"{prefix}_{mat}_{mesh.split('.')[-1]}", mesh, materials[mat], transforms, folder)


# ------------------------------------------------------------------ layout helpers

def floor_point(s, lateral):
    """World XY and ground height at canyon station s, `lateral` cm towards the R wall (+) or L wall (-)"""
    a = math.radians(G["centre_yaw"](s))
    x = s - math.sin(a) * lateral
    y = G["centre_y"](s) + math.cos(a) * lateral
    return x, y, G["ground_height"](x, y)


def usable_floor(s, lateral, margin=200.0):
    """On the canyon floor proper: not in the ravine or the chasm, not under the Milestone 1 slabs"""
    x, y, z = floor_point(s, lateral)
    side = 1 if lateral >= 0 else -1
    if abs(lateral) > G["half_width"](side, s) - margin:
        return False
    if z < -300 or G["in_play_zone"](x, y, 300):
        return False
    return True


S_RANGE = (G["X_START_END"] + 2200, G["X_FAR_END"] - 2200)

# Where the two eras differ on purpose: tree spots (station, side) and ruin sites (station, lateral, kind)
TREE_SPOTS = [(-13500, 1), (-11800, -1), (-10200, 1), (-8600, -1), (-7200, 1), (-5600, -1), (-4600, 1), (4700, -1),
              (5600, -1), (6700, 1), (16200, -1), (17200, 1)]
SHRINE_SITE = (-4200, -650)
WALL_SITES = [(-9300, 900), (16700, -700)]


def river_path():
    """The river (past) / dry riverbed (present) line along the floor: (x, y, z, heading) points"""
    points = []
    s = S_RANGE[0]
    while s <= S_RANGE[1]:
        lateral = 420.0 * math.sin(s / 2600.0) + 160.0 * math.sin(s / 900.0)
        x, y, z = floor_point(s, lateral)
        points.append((s, x, y, z))
        s += 260.0
    return points


def river_segments(width, lift):
    """Flat boxes along the river line, skipping where the floor drops into the ravine or chasm"""
    out = []
    pts = river_path()
    for (s0, x0, y0, z0), (s1, x1, y1, z1) in zip(pts, pts[1:]):
        if z0 < -300 or z1 < -300 or G["in_play_zone"](x0, y0, 200):
            continue
        length = math.hypot(x1 - x0, y1 - y0) * 1.15
        yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
        out.append(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2 + lift, yaw, length, s0))
    return out


# ------------------------------------------------------------------ era contents

def build_lighting(era):
    folder = "Lighting"
    present = era == "Present"
    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (-9.0, 140.0, 0.0) if present else (-38.0, 110.0, 0.0), "Sun", folder)
    lc = sun.light_component
    lc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("intensity", 6.5 if present else 10.0)
    lc.set_editor_property("light_color", unreal.Color(r=255, g=150, b=85, a=255) if present else unreal.Color(r=255, g=248, b=235, a=255))
    lc.set_editor_property("atmosphere_sun_light", True)

    atmo = spawn(unreal.SkyAtmosphere, label="SkyAtmosphere", folder=folder)
    ac = atmo.get_component_by_class(unreal.SkyAtmosphereComponent)
    if present:
        # Dust: more Mie, less Rayleigh blue
        ac.set_editor_property("mie_scattering_scale", 0.012)
        ac.set_editor_property("mie_absorption_scale", 0.003)
        ac.set_editor_property("rayleigh_scattering_scale", 0.02)
        ac.set_editor_property("aerial_pespective_view_distance_scale", 2.5)   # sic: the engine property is misspelt
    else:
        # Clean air: the engine's clear blue defaults, a touch more Rayleigh
        ac.set_editor_property("rayleigh_scattering_scale", 0.036)
        ac.set_editor_property("mie_scattering_scale", 0.003)

    sky = spawn(unreal.SkyLight, (0, 0, 800), label="SkyLight", folder=folder)
    sc = sky.light_component
    sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 1.0 if present else 1.25)

    fog = spawn(unreal.ExponentialHeightFog, (0, 0, -200), label="HeightFog", folder=folder)
    fc = fog.component
    for k, v in ({"fog_density": 0.022, "fog_height_falloff": 0.18, "fog_max_opacity": 0.85, "start_distance": 1500.0,
                  "fog_inscattering_luminance": unreal.LinearColor(0.36, 0.23, 0.12, 1.0),
                  "directional_inscattering_exponent": 6.0,
                  "directional_inscattering_luminance": unreal.LinearColor(0.65, 0.36, 0.15, 1.0)} if present else
                 {"fog_density": 0.008, "fog_height_falloff": 0.35, "fog_max_opacity": 0.55, "start_distance": 800.0,
                  "fog_inscattering_luminance": unreal.LinearColor(0.5, 0.6, 0.75, 1.0),
                  "directional_inscattering_exponent": 10.0,
                  "directional_inscattering_luminance": unreal.LinearColor(0.6, 0.6, 0.55, 1.0)}).items():
        fc.set_editor_property(k, v)

    ppv = spawn(unreal.PostProcessVolume, (0, 0, 500), label="EraGrading", folder=folder)
    ppv.set_editor_property("unbound", True)
    brush = ppv.get_component_by_class(unreal.BrushComponent)
    if brush:
        brush.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    s = ppv.get_editor_property("settings")
    grading = {
        "override_color_saturation": True, "color_saturation": unreal.Vector4(0.62, 0.62, 0.62, 1.0),
        "override_color_contrast": True, "color_contrast": unreal.Vector4(1.08, 1.08, 1.08, 1.0),
        "override_color_gain": True, "color_gain": unreal.Vector4(1.08, 0.98, 0.84, 1.0),
        "override_vignette_intensity": True, "vignette_intensity": 0.4,
    } if present else {
        "override_color_saturation": True, "color_saturation": unreal.Vector4(1.3, 1.3, 1.3, 1.0),
        "override_color_contrast": True, "color_contrast": unreal.Vector4(1.05, 1.05, 1.05, 1.0),
        "override_color_gain": True, "color_gain": unreal.Vector4(0.95, 1.02, 1.07, 1.0),
        "override_vignette_intensity": True, "vignette_intensity": 0.2,
    }
    for k, v in grading.items():
        s.set_editor_property(k, v)
    ppv.set_editor_property("settings", s)


def build_ambience(era):
    sfx = eal.load_asset(f"{AUDIO_DIR}/SC_SFX")
    music = eal.load_asset(f"{AUDIO_DIR}/SC_Music")
    hooks = [("Ambience_Wind", sfx)] if era == "Present" else [("Ambience_River", sfx), ("Ambience_Birds", sfx), ("Ambience_DistantDrums", music)]
    for label, sound_class in hooks:
        a = spawn(unreal.AmbientSound, (0, 0, 300), label=label, folder="Ambience")
        audio = a.get_component_by_class(unreal.AudioComponent)
        audio.set_editor_property("sound_class_override", sound_class)
        audio.set_editor_property("auto_activate", True)


def build_river(era, batch):
    if era == "Present":
        # A dry, cracked riverbed: a darker bed with cracks across it
        rng = random.Random(61)
        for x, y, z, yaw, length, s in river_segments(0, 1.5):
            batch.add(CUBE, "bed", xf((x, y, z), (0, yaw, 0), (length / 100.0, 4.6, 0.03)))
            for _ in range(7):
                along = rng.uniform(-length / 2, length / 2)
                across = rng.uniform(-200, 200)
                a = math.radians(yaw)
                cx = x + math.cos(a) * along - math.sin(a) * across
                cy = y + math.sin(a) * along + math.cos(a) * across
                batch.add(CUBE, "crack", xf((cx, cy, z + 1.5), (0, yaw + rng.uniform(40, 140), 0), (rng.uniform(0.6, 1.8), 0.05, 0.02)))
    else:
        # A shallow river where the bed is, visual only
        for x, y, z, yaw, length, s in river_segments(0, 4.0):
            batch.add(CUBE, "water", xf((x, y, z), (0, yaw, 0), (length / 100.0, 3.8, 0.04)))


def build_trees(era, batch):
    rng = random.Random(62)
    for s, side in TREE_SPOTS:
        lateral = side * (G["half_width"](side, s) - 900.0)
        if not usable_floor(s, lateral, 400):
            continue
        x, y, z = floor_point(s, lateral)
        height = rng.uniform(520, 760)
        kind = rng.random()
        if era == "Past":
            # A living tree: trunk and a crown of leaves
            batch.add(CYLINDER, "wood", xf((x, y, z + height / 2), (0, 0, 0), (0.55, 0.55, height / 100.0)))
            for i in range(5):
                a = i * 72 + rng.uniform(-20, 20)
                r = rng.uniform(80, 170)
                cx, cy = x + math.cos(math.radians(a)) * r, y + math.sin(math.radians(a)) * r
                size = rng.uniform(2.4, 3.4)
                batch.add(SPHERE, "leaves" if i % 2 else "leaves2", xf((cx, cy, z + height + rng.uniform(-80, 60)), (0, a, 0), (size, size, size * 0.8)))
            batch.add(SPHERE, "leaves", xf((x, y, z + height + 120), (0, 0, 0), (3.2, 3.2, 2.4)))
        elif kind < 0.45:
            # A stump where the tree stood, and the trunk lying next to it
            batch.add(CYLINDER, "stump", xf((x, y, z + 25), (0, 0, 0), (0.65, 0.65, 0.5)))
            ang = rng.uniform(0, 360)
            fx, fy = x + math.cos(math.radians(ang)) * height * 0.45, y + math.sin(math.radians(ang)) * height * 0.45
            batch.add(CYLINDER, "wood", xf((fx, fy, z + 22), (90, ang, 0), (0.45, 0.45, height * 0.8 / 100.0)))
        else:
            # A dead tree: grey trunk and a few bare branches
            batch.add(CYLINDER, "wood", xf((x, y, z + height / 2), (0, 0, 0), (0.5, 0.5, height / 100.0)))
            for i in range(4):
                a = i * 90 + rng.uniform(-30, 30)
                length = rng.uniform(150, 260)
                h = z + height * rng.uniform(0.6, 0.95)
                pitch = rng.uniform(35, 60)
                bx = x + math.cos(math.radians(a)) * length * 0.5 * math.cos(math.radians(pitch))
                by = y + math.sin(math.radians(a)) * length * 0.5 * math.cos(math.radians(pitch))
                bz = h + length * 0.5 * math.sin(math.radians(pitch))
                batch.add(CYLINDER, "wood", xf((bx, by, bz), (90 - pitch, a, 0), (0.14, 0.14, length / 100.0)))


def build_shrine(era, batch):
    """The ovoo: a cairn of stones round a pole hung with blue khadag scarves (past), fallen apart (present)"""
    s, lateral = SHRINE_SITE
    x, y, z = floor_point(s, lateral)
    rng = random.Random(63)
    if era == "Past":
        for ring, (radius, count, size) in enumerate([(170, 12, 1.1), (120, 9, 1.0), (75, 7, 0.9), (35, 4, 0.8)]):
            for i in range(count):
                a = math.radians(i * 360 / count + ring * 17)
                batch.add(SPHERE, "stone", xf((x + math.cos(a) * radius, y + math.sin(a) * radius, z + 30 + ring * 55),
                                              (0, rng.uniform(0, 360), 0), (size, size * 0.9, size * 0.7)))
        batch.add(CYLINDER, "pole", xf((x, y, z + 260), (0, 0, 0), (0.12, 0.12, 4.4)))
        for i in range(6):
            a = i * 60
            ax, ay = x + math.cos(math.radians(a)) * 90, y + math.sin(math.radians(a)) * 90
            batch.add(CUBE, "khadag", xf((ax, ay, z + 300), (-35, a, 0), (1.9, 0.25, 0.02)))
    else:
        for i in range(26):
            a = math.radians(rng.uniform(0, 360))
            r = rng.uniform(40, 420)
            size = rng.uniform(0.6, 1.1)
            batch.add(SPHERE, "stone", xf((x + math.cos(a) * r, y + math.sin(a) * r, z + 18), (rng.uniform(-10, 10), rng.uniform(0, 360), 0), (size, size * 0.9, size * 0.55)))
        batch.add(CYLINDER, "stump", xf((x + 30, y - 10, z + 45), (8, 20, 0), (0.12, 0.12, 0.9)))
        batch.add(CUBE, "cloth", xf((x + 140, y + 60, z + 3), (0, 35, 0), (0.9, 0.22, 0.01)))


def build_walls(era, batch):
    """Small stone walls (animal pens) standing in the past, broken down to their footings in the present"""
    rng = random.Random(64)
    for s, lateral in WALL_SITES:
        x, y, z = floor_point(s, lateral)
        yaw = G["centre_yaw"](s)
        for side in range(3):
            a = yaw + side * 90
            length = 600 if side != 1 else 450
            ox = x + math.cos(math.radians(a + 90)) * 220 * (side - 1)
            oy = y + math.sin(math.radians(a + 90)) * 220 * (side - 1)
            if era == "Past":
                batch.add(CUBE, "stone", xf((ox, oy, z + 60), (0, a, 0), (length / 100.0, 0.5, 1.2)))
            else:
                for j in range(4):
                    if rng.random() < 0.35:
                        continue
                    off = (j - 1.5) * length / 4
                    px, py = ox + math.cos(math.radians(a)) * off, oy + math.sin(math.radians(a)) * off
                    batch.add(CUBE, "stone", xf((px, py, z + rng.uniform(12, 35)), (rng.uniform(-6, 6), a + rng.uniform(-8, 8), rng.uniform(-6, 6)),
                                                (length / 400.0 * rng.uniform(0.6, 0.95), 0.5, rng.uniform(0.25, 0.6))))


def build_ground_cover(era, batch):
    """Past: grass tufts across the floor (thicker along the river). Present: a little dry brush"""
    rng = random.Random(65)
    count = 0
    target = 3200 if era == "Past" else 500
    tries = 0
    while count < target and tries < target * 6:
        tries += 1
        s = rng.uniform(*S_RANGE)
        side = 1 if rng.random() < 0.5 else -1
        lateral = side * rng.uniform(0, G["half_width"](side, s) - 600)
        if not usable_floor(s, lateral, 600):
            continue
        x, y, z = floor_point(s, lateral)
        if era == "Past":
            h = rng.uniform(0.28, 0.5)
            batch.add(CONE, "grass" if rng.random() < 0.6 else "grass2",
                      xf((x, y, z + h * 50 - 5), (rng.uniform(-12, 12), rng.uniform(0, 360), rng.uniform(-12, 12)), (rng.uniform(0.25, 0.45), rng.uniform(0.25, 0.45), h)))
        else:
            size = rng.uniform(0.25, 0.5)
            batch.add(SPHERE, "brush", xf((x, y, z + 8), (0, rng.uniform(0, 360), 0), (size, size, size * 0.45)))
        count += 1
    log(f"{era}: {count} {'grass tufts' if era == 'Past' else 'dry brush clumps'}")


def build_era(era, shapes, canyon_mats, prop_mats):
    open_map(ERA_MAPS[era])
    clear_map()

    # The canyon itself: every shared shape, same transform, in this era's material, no collision
    for (mesh, mat), transforms in shapes.items():
        era_mat = canyon_mats[era].get(mat)
        if era_mat is None:
            era_mat = eal.load_asset(mat.split(".")[0])
        props(f"Canyon_{mesh.split('.')[-1]}_{mat.split('.')[-1]}", mesh, era_mat, transforms, "Canyon")

    build_lighting(era)
    build_ambience(era)
    batch = Batch()
    build_river(era, batch)
    build_trees(era, batch)
    build_shrine(era, batch)
    build_walls(era, batch)
    build_ground_cover(era, batch)
    batch.spawn(prop_mats[era], "Props", era)

    les.save_current_level()
    actors = eas.get_all_level_actors()
    log(f"built {ERA_MAPS[era]}: {len(actors)} actors")


def main():
    world = editor_world()
    if not world.get_path_name().startswith(SHARED_MAP + "."):
        les.load_level(SHARED_MAP)
        world = editor_world()
        if not world.get_path_name().startswith(SHARED_MAP + "."):
            raise RuntimeError(f"couldn't open {SHARED_MAP}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    canyon_mats, prop_mats = build_materials()
    shapes = collect_shapes()
    for era in ("Present", "Past"):
        build_era(era, shapes, canyon_mats, prop_mats)

    # Back to the shared layer: hide the shapes the eras now draw, move the lighting out, place the director
    les.load_level(SHARED_MAP)
    hide_shared_shapes()
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            eas.destroy_actor(actor)
    director = spawn(unreal.EchoEraDirector, (0, 0, 200), label="EraDirector", folder="Eras")
    les.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f"placed {director.get_actor_label()}; done")


if not globals().get("ECHO_ERAS_NO_BUILD"):
    try:
        main()
    except Exception as e:
        import traceback
        log(f"FAILED: {e!r}\n{traceback.format_exc()}")
