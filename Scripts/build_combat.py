"""
Milestone 8 builder: first combat. Run in the OPEN editor (it creates and opens maps):
    py exec(open(r'D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho/Scripts/build_combat.py').read())

It builds:
  - Materials (/Game/Echo/Combat): M_EnemyBody (Color, GlowColor, Glow), the door and restart-pad looks
  - Input: IA_Dodge + IMC_Combat (Left Shift / gamepad B), rebindable as "Dodge"
  - Enemy Blueprints BP_DustWraith, BP_Shade, BP_StoneGuardian, BP_SkyTalon (tune them there)
  - The enemy StateTree ST_EchoEnemy (UEchoEditorLibrary.build_enemy_state_tree)
  - CombatTest (shared layer: the arena's geometry and all collision, hidden in game; the arena manager, spawn
    points, entrance checkpoint and PlayerStarts, the return door and restart pad, a navmesh, the era director)
    and its era layers CombatTest_Present / CombatTest_Past (visible copies of the arena + era lighting and props)
  - The "Combat Test" door in Lvl_SpiritPath at the back of the start platform, with its arrival PlayerStarts
    (the only change to the first level)
Re-running rebuilds everything it made (actors tagged CombatBuilder; the CombatTest maps from scratch).
"""
import math
import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
MARK = "ECHO_COMBAT"
TAG = "CombatBuilder"
COMBAT_DIR = "/Game/Echo/Combat"
INPUT_DIR = "/Game/Input"
FIRST_LEVEL = "/Game/Echo/Maps/Lvl_SpiritPath"
MAPS = {"Shared": "/Game/Echo/Maps/CombatTest", "Present": "/Game/Echo/Maps/CombatTest_Present", "Past": "/Game/Echo/Maps/CombatTest_Past"}

CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CONE = "/Engine/BasicShapes/Cone.Cone"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# The era builder's helpers (spawning, era lighting and ambience, instanced copies), without running it
E = {"ECHO_ERAS_NO_BUILD": True}
exec(open(f"{PROJECT}/Scripts/build_eras.py").read(), E)
E["TAG"] = TAG


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def save(asset):
    eal.save_asset(asset.get_path_name().split(".")[0], only_if_is_dirty=False)


def v(x, y, z):
    return unreal.Vector(x, y, z)


# ------------------------------------------------------------------ assets

def get_or_create(name, folder, cls, factory):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    log(f"created {path}")
    return asset_tools.create_asset(name, folder, cls, factory)


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, val in props.items():
        e.set_editor_property(k, val)
    return e


def build_materials():
    """M_EnemyBody: BaseColor = Color, Emissive = GlowColor * Glow * 3 (telegraphs, hit flashes, open states)"""
    path = f"{COMBAT_DIR}/M_EnemyBody"
    if not eal.does_asset_exist(path):
        mat = asset_tools.create_asset("M_EnemyBody", COMBAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        color = expr(mat, unreal.MaterialExpressionVectorParameter, -800, -100, parameter_name="Color", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        glow_color = expr(mat, unreal.MaterialExpressionVectorParameter, -800, 150, parameter_name="GlowColor", default_value=unreal.LinearColor(1.0, 0.3, 0.05, 1.0))
        glow = expr(mat, unreal.MaterialExpressionScalarParameter, -800, 380, parameter_name="Glow", default_value=0.0)
        mul = expr(mat, unreal.MaterialExpressionMultiply, -500, 250)
        mel.connect_material_expressions(glow_color, "", mul, "A")
        mel.connect_material_expressions(glow, "", mul, "B")
        boost = expr(mat, unreal.MaterialExpressionMultiply, -300, 250, const_b=3.0)
        mel.connect_material_expressions(mul, "", boost, "A")
        rough = expr(mat, unreal.MaterialExpressionConstant, -300, 50, r=0.65)
        mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(mat)
        save(mat)
        log(f"created {path}")

    glow = eal.load_asset("/Game/Echo/Materials/M_EchoGlow")
    for name, rgb, amount in (("MI_DoorFrame", (0.2, 0.14, 0.1), 0.0), ("MI_DoorPortal", (1.0, 0.45, 0.1), 1.6),
                              ("MI_RestartButton", (1.0, 0.12, 0.06), 1.4), ("MI_SpawnMarker", (0.6, 0.1, 0.1), 0.3)):
        mi = get_or_create(name, COMBAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, glow)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*rgb, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Glow", amount)
        mel.update_material_instance(mi)
        save(mi)
    log("materials built")


def build_input():
    action = get_or_create("IA_Dodge", f"{INPUT_DIR}/Actions", unreal.InputAction, unreal.InputAction_Factory() if hasattr(unreal, "InputAction_Factory") else None)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=action)
    settings.set_editor_property("name", "Dodge")
    settings.set_editor_property("display_name", "Dodge roll")
    settings.set_editor_property("display_category", "Combat")
    action.set_editor_property("player_mappable_key_settings", settings)
    save(action)

    imc = get_or_create("IMC_Combat", INPUT_DIR, unreal.InputMappingContext,
                        unreal.InputMappingContext_Factory() if hasattr(unreal, "InputMappingContext_Factory") else None)
    imc.unmap_all()
    for key_name in ("LeftShift", "Gamepad_FaceButton_Right"):
        key = unreal.Key()
        key.import_text(key_name)
        imc.map_key(action, key)
    save(imc)
    log("input: IA_Dodge on Left Shift / gamepad B (IMC_Combat)")


ENEMY_BPS = {"BP_DustWraith": unreal.EchoDustWraith, "BP_Shade": unreal.EchoShade,
             "BP_StoneGuardian": unreal.EchoStoneGuardian, "BP_SkyTalon": unreal.EchoSkyTalon}


def build_blueprints():
    for name, parent in ENEMY_BPS.items():
        path = f"{COMBAT_DIR}/{name}"
        if eal.does_asset_exist(path):
            continue
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = asset_tools.create_asset(name, COMBAT_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        log(f"created {path}")


def build_state_tree():
    tree = unreal.EchoEditorLibrary.build_enemy_state_tree(f"{COMBAT_DIR}/ST_EchoEnemy")
    if not tree:
        raise RuntimeError("the enemy StateTree didn't compile (see the log)")
    save(tree)
    log("StateTree ST_EchoEnemy built")


# ------------------------------------------------------------------ the arena (shared layer)

ARENA_HALF = 2500.0          # the floor is 50 x 50 m, top at z = 0
WALL_H = 900.0
LEDGE = {"x": (-700.0, 900.0), "y": (1650.0, 2400.0), "top": 360.0}
ENTRANCE_X = -2100.0


def shape(label, mesh, material, loc, scale, rot=(0.0, 0.0, 0.0), folder="Arena"):
    """A solid greybox piece of the arena (shared layer). Hidden in game later: the era copies are what players see"""
    a = E["spawn"](unreal.StaticMeshActor, loc, rot, label, folder)
    smc = a.static_mesh_component
    smc.set_editor_property("static_mesh", eal.load_asset(mesh))
    smc.set_material(0, material)
    a.set_actor_scale3d(v(*scale))
    smc.set_collision_profile_name("BlockAll")
    a.tags = [unreal.Name(TAG), unreal.Name("CombatShape")]
    return a


def build_arena_geometry():
    floor_mat = eal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray")
    rock = eal.load_asset("/Game/Echo/Materials/MI_CanyonRock")
    boulder = eal.load_asset("/Game/Echo/Materials/MI_CanyonBoulder")
    n = 0
    shape("ArenaFloor", CUBE, floor_mat, (0, 0, -50), (ARENA_HALF / 50.0, ARENA_HALF / 50.0, 1.0)); n += 1

    # Canyon walls in two terraces around the arena (the entrance side has the doorway in the middle)
    for tier, (offset, height, z0) in enumerate(((0.0, WALL_H, 0.0), (300.0, 700.0, WALL_H))):
        d = ARENA_HALF + 100.0 + offset
        span = (ARENA_HALF + 400.0 + offset) / 50.0
        shape(f"Wall_N_{tier}", CUBE, rock, (0, d, z0 + height / 2), (span, 2.0, height / 100.0)); n += 1
        shape(f"Wall_S_{tier}", CUBE, rock, (0, -d, z0 + height / 2), (span, 2.0, height / 100.0)); n += 1
        shape(f"Wall_E_{tier}", CUBE, rock, (d, 0, z0 + height / 2), (2.0, span, height / 100.0)); n += 1
        shape(f"Wall_W_{tier}", CUBE, rock, (-d, 0, z0 + height / 2), (2.0, span, height / 100.0)); n += 1

    # Cover: four pillars and some big rocks
    for i, (x, y) in enumerate(((-700, -900), (700, -1000), (-500, 900), (900, 700))):
        shape(f"Pillar_{i}", CYLINDER, rock, (x, y, 350), (2.4, 2.4, 7.0)); n += 1
    for i, (x, y, s) in enumerate(((150, -1600, 3.4), (1500, -300, 3.0), (-1300, 250, 2.6), (1700, 1500, 3.2))):
        shape(f"Rock_{i}", SPHERE, boulder, (x, y, s * 25), (s * 1.3, s, s * 0.9), (0, i * 37.0, 0)); n += 1

    # The raised ledge for Bat to shoot from (against the north wall), and a ramp up to it from the west
    lx, ly, top = LEDGE["x"], LEDGE["y"], LEDGE["top"]
    shape("Ledge", CUBE, rock, ((lx[0] + lx[1]) / 2, (ly[0] + ly[1]) / 2, top / 2), ((lx[1] - lx[0]) / 100.0, (ly[1] - ly[0]) / 100.0, top / 100.0)); n += 1
    ramp_len = 1000.0
    angle = math.degrees(math.atan2(top, ramp_len))
    length = math.hypot(ramp_len, top)
    shape("LedgeRamp", CUBE, rock, (lx[0] - ramp_len / 2 + 10, 2100, top / 2 - 20), (length / 100.0, 4.0, 0.4), (angle, 0, 0)); n += 1
    log(f"arena geometry: {n} pieces")


def build_arena_actors():
    # Arrivals (through the door from the first level) and the entrance checkpoint
    for role, y in (("Bat", -160.0), ("Saraa", 160.0)):
        start = E["spawn"](unreal.PlayerStart, (ENTRANCE_X, y, 100.0), (0, 0, 0), f"ArenaEntrance_{role}", "Arena|Entrance")
        start.set_editor_property("player_start_tag", f"ArenaEntrance_{role}")
    cp = E["spawn"](unreal.EchoCheckpoint, (ENTRANCE_X + 150.0, 0.0, 0.0), (0, 0, 0), "Arena_Checkpoint", "Arena|Entrance")
    cp.set_editor_property("volume_size", v(500.0, 1200.0, 400.0))
    cp.set_editor_property("respawn_point", v(0.0, 0.0, 100.0))

    # The way back, and the restart button beside it
    door = E["spawn"](unreal.EchoTravelDoor, (-ARENA_HALF + 60.0, 0.0, 0.0), (0, 0, 0), "ReturnDoor", "Arena|Entrance")
    door.set_editor_property("target_map", FIRST_LEVEL)
    door.set_editor_property("arrival_tag", "CombatDoor")
    door.set_editor_property("label", "Back to the canyon")
    door.set_editor_property("glow_color", unreal.LinearColor(0.4, 0.75, 1.0, 1.0))
    E["spawn"](unreal.EchoArenaRestartPad, (ENTRANCE_X + 100.0, 650.0, 0.0), (0, 0, 0), "RestartArenaPad", "Arena|Entrance")

    # Spawn points and the arena manager (its start zone covers the arena floor past the entrance)
    ground = []
    for i, (x, y) in enumerate(((1900, -1300), (1900, 1100), (1100, -2000), (300, -2050), (2100, 0), (400, 1350))):
        ground.append(E["spawn"](unreal.TargetPoint, (x, y, 100.0), (0, 180, 0), f"Spawn_Ground_{i}", "Arena|Spawns"))
    air = []
    for i, (x, y) in enumerate(((1700, 0), (900, -1300), (900, 1300))):
        air.append(E["spawn"](unreal.TargetPoint, (x, y, 700.0), (0, 180, 0), f"Spawn_Air_{i}", "Arena|Spawns"))
    arena = E["spawn"](unreal.EchoArenaManager, (400.0, 0.0, 300.0), (0, 0, 0), "ArenaManager", "Arena")
    arena.set_editor_property("ground_spawn_points", ground)
    arena.set_editor_property("air_spawn_points", air)
    zone = arena.get_component_by_class(unreal.BoxComponent)
    zone.set_box_extent(v(2000.0, 2300.0, 600.0))

    # A navmesh over the arena for the ground enemies, and a kill floor far below just in case
    nav = E["spawn"](unreal.NavMeshBoundsVolume, (0.0, 0.0, 300.0), (0, 0, 0), "ArenaNavBounds", "Arena")
    nav.set_actor_scale3d(v(ARENA_HALF / 100.0 + 1, ARENA_HALF / 100.0 + 1, 8.0))
    kill = E["spawn"](unreal.EchoRespawnVolume, (0.0, 0.0, -1500.0), (0, 0, 0), "Arena_KillVolume", "Arena")
    kill.set_editor_property("volume_size", v(12000.0, 12000.0, 600.0))

    # Era layers for this level
    # (its era levels are set once they exist: set_director_levels)
    E["spawn"](unreal.EchoEraDirector, (0.0, 0.0, 200.0), (0, 0, 0), "EraDirector", "Eras")
    log("arena actors placed")


def set_director_levels():
    present, past = eal.load_asset(MAPS["Present"]), eal.load_asset(MAPS["Past"])
    for actor in eas.get_all_level_actors():
        if isinstance(actor, unreal.EchoEraDirector):
            actor.set_editor_property("present_level", present)
            actor.set_editor_property("past_level", past)
            log(f"era director: {present.get_path_name()} / {past.get_path_name()}")


def set_game_mode(world):
    gm = unreal.load_class(None, "/Game/Echo/Blueprints/BP_EchoGameMode.BP_EchoGameMode_C")
    ws = world.get_world_settings()
    ws.set_editor_property("default_game_mode", gm)


def build_shared():
    world = E["open_map"](MAPS["Shared"])
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)) and not isinstance(actor, unreal.RecastNavMesh):
            eas.destroy_actor(actor)
    set_game_mode(world)
    build_arena_geometry()
    build_arena_actors()
    shapes = collect_arena_shapes()
    # The players see the era copies; the shared pieces stay solid but hidden in game
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag("CombatShape"):
            actor.set_actor_hidden_in_game(True)
    les.save_current_level()
    log(f"built {MAPS['Shared']}: {len(eas.get_all_level_actors())} actors")
    return shapes


def collect_arena_shapes():
    groups = {}
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag("CombatShape"):
            smc = actor.static_mesh_component
            key = (smc.get_editor_property("static_mesh").get_path_name(), smc.get_material(0).get_path_name())
            groups.setdefault(key, []).append(actor.get_actor_transform())
    return groups


# ------------------------------------------------------------------ era layers

def era_material(era, source_path):
    """The arena piece's material as each era shows it (the era builder's era instances, made in Milestone 6)"""
    name = source_path.split(".")[-1]
    if name == "MI_PrototypeGrid_Gray":
        return eal.load_asset(f"/Game/Echo/Eras/MI_Floor_{era}")
    candidate = f"/Game/Echo/Eras/{name}_{era}"
    return eal.load_asset(candidate) if eal.does_asset_exist(candidate) else eal.load_asset(source_path.split(".")[0])


def build_era(era, shapes):
    E["open_map"](MAPS[era])
    E["clear_map"]()
    for (mesh, mat), transforms in shapes.items():
        E["props"](f"Arena_{mesh.split('.')[-1]}_{mat.split('.')[-1]}", mesh, era_material(era, mat), transforms, "Arena")
    E["build_lighting"](era)
    E["build_ambience"](era)

    # A little of each era's world around the edges
    xf = E["xf"]
    present = era == "Present"
    edge = [(x, y) for x in range(-2200, 2300, 550) for y in (-2250, 2250)] + [(x, y) for y in range(-1650, 1700, 550) for x in (2250,)]
    if present:
        wood = eal.load_asset("/Game/Echo/Eras/MI_DeadWood")
        brush = eal.load_asset("/Game/Echo/Eras/MI_DryBrush")
        trunks = [xf((x, y, 220), (0, 0, (x * 7 + y) % 9 - 4), (0.35, 0.35, 4.4)) for x, y in edge[::2]]
        branches = [xf((x + 40, y, 330), (0, (x + y) % 360, 50), (0.15, 0.15, 1.6)) for x, y in edge[::2]]
        tufts = [xf((x * 0.8 + 90, y * 0.8, 15), (0, (x * y) % 360, 0), (0.5, 0.5, 0.25)) for x, y in edge]
        E["props"]("DeadTrees", CYLINDER, wood, trunks + branches, "Props")
        E["props"]("DryBrush", SPHERE, brush, tufts, "Props")
    else:
        bark = eal.load_asset("/Game/Echo/Eras/MI_Bark")
        leaves = eal.load_asset("/Game/Echo/Eras/MI_Leaves")
        grass = eal.load_asset("/Game/Echo/Eras/MI_Grass")
        trunks = [xf((x, y, 220), (0, 0, 0), (0.4, 0.4, 4.4)) for x, y in edge[::2]]
        crowns = [xf((x, y, 520), (0, (x + y) % 360, 0), (2.6, 2.6, 2.2)) for x, y in edge[::2]]
        blades = []
        for gx in range(-2300, 2400, 230):
            for gy in range(-2300, 2400, 230):
                if abs(gx) > 1800 or abs(gy) > 1800 or (gx * 31 + gy * 17) % 5 == 0:
                    blades.append(xf((gx + (gy % 97), gy + (gx % 89), 12), (0, (gx * gy) % 360, 0), (0.18, 0.18, 0.35)))
        E["props"]("Trees_Trunks", CYLINDER, bark, trunks, "Props")
        E["props"]("Trees_Crowns", SPHERE, leaves, crowns, "Props")
        E["props"]("Grass", CONE, grass, blades, "Props", shadow=False)
    les.save_current_level()
    log(f"built {MAPS[era]}: {len(eas.get_all_level_actors())} actors")


# ------------------------------------------------------------------ the door in the first level

def build_first_level_door():
    les.load_level(FIRST_LEVEL)
    world = editor_world()
    if not world.get_path_name().startswith(FIRST_LEVEL + "."):
        raise RuntimeError(f"couldn't open {FIRST_LEVEL}")
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            eas.destroy_actor(actor)
    # At the back of the start platform (which runs x -1500..0), facing along the path; the players start in front of it
    door = E["spawn"](unreal.EchoTravelDoor, (-1440.0, 0.0, 0.0), (0, 0, 0), "CombatTestDoor", "Combat")
    door.set_editor_property("target_map", MAPS["Shared"])
    door.set_editor_property("arrival_tag", "ArenaEntrance")
    door.set_editor_property("label", "Combat Test")
    for role, y in (("Bat", -160.0), ("Saraa", 160.0)):
        start = E["spawn"](unreal.PlayerStart, (-1150.0, y, 100.0), (0, 0, 0), f"CombatDoor_{role}", "Combat")
        start.set_editor_property("player_start_tag", f"CombatDoor_{role}")
    les.save_current_level()
    log("first level: the Combat Test door and its arrival starts (tag CombatBuilder)")


def main():
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    build_materials()
    build_input()
    build_blueprints()
    build_state_tree()
    shapes = build_shared()
    for era in ("Present", "Past"):
        build_era(era, shapes)
    # Back to the arena: point the era director at its era levels, and let the navmesh build with everything in place
    E["open_map"](MAPS["Shared"])
    set_director_levels()
    les.save_current_level()
    build_first_level_door()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("done")


if not globals().get("ECHO_COMBAT_NO_BUILD"):
    try:
        main()
    except Exception as e:
        import traceback
        log(f"FAILED: {e!r}\n{traceback.format_exc()}")
