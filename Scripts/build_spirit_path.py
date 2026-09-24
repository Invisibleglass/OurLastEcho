"""
Builds Milestone 1, "The Spirit Path" greybox:
  - Materials   /Game/Echo/Materials  (M_EchoGlow + instances, M_Ghost)
  - Blueprints  /Game/Echo/Blueprints (BP_Saraa, BP_EchoGameMode)
  - Level       /Game/Echo/Maps/Lvl_SpiritPath

Safe to re-run. Assets are updated in place. In the level, only actors tagged "EchoBuilder"
are deleted and respawned, so actors you add by hand survive - but any hand-tweaks to the
builder's own actors are reset. Once the layout is being polished by hand, stop re-running this.

Run headless (editor closed):
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script=<this file> -unattended -nop4 -nosplash
Output lines are tagged ECHO_BUILD in Saved/Logs/OurLastEcho.log.
"""
import unreal

MARK = "ECHO_BUILD"
BUILDER_TAG = "EchoBuilder"

MAT_DIR = "/Game/Echo/Materials"
BP_DIR = "/Game/Echo/Blueprints"
MAP_PATH = "/Game/Echo/Maps/Lvl_SpiritPath"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"{MARK}: {msg}")


# ---------------------------------------------------------------- materials

def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def build_glow_material():
    """Lit material: BaseColor = Color, Emissive = Color * Glow"""
    path = f"{MAT_DIR}/M_EchoGlow"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)

    mat = asset_tools.create_asset("M_EchoGlow", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = expr(mat, unreal.MaterialExpressionVectorParameter, -600, 0,
                 parameter_name="Color", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    glow = expr(mat, unreal.MaterialExpressionScalarParameter, -600, 250,
                parameter_name="Glow", default_value=0.0)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -300, 150)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    log(f"created {path}")
    return mat


def build_ghost_material():
    """Unlit translucent pale-blue with a brighter, more opaque fresnel rim"""
    path = f"{MAT_DIR}/M_Ghost"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)

    mat = asset_tools.create_asset("M_Ghost", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("used_with_skeletal_mesh", True)

    color = expr(mat, unreal.MaterialExpressionVectorParameter, -900, 0,
                 parameter_name="GhostColor", default_value=unreal.LinearColor(0.7, 0.88, 1.0, 1.0))
    fresnel = expr(mat, unreal.MaterialExpressionFresnel, -900, 250, exponent=3.0)

    # Emissive = GhostColor * (0.4 + Fresnel * 1.6)
    rim = expr(mat, unreal.MaterialExpressionMultiply, -600, 200, const_b=1.6)
    mel.connect_material_expressions(fresnel, "", rim, "A")
    bright = expr(mat, unreal.MaterialExpressionAdd, -400, 200, const_b=0.4)
    mel.connect_material_expressions(rim, "", bright, "A")
    emissive = expr(mat, unreal.MaterialExpressionMultiply, -200, 0)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(bright, "", emissive, "B")

    # Opacity = 0.2 + Fresnel * 0.6
    op_rim = expr(mat, unreal.MaterialExpressionMultiply, -600, 400, const_b=0.6)
    mel.connect_material_expressions(fresnel, "", op_rim, "A")
    opacity = expr(mat, unreal.MaterialExpressionAdd, -400, 400, const_b=0.2)
    mel.connect_material_expressions(op_rim, "", opacity, "A")

    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    log(f"created {path}")
    return mat


def build_instance(name, parent, vectors, scalars):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


# ---------------------------------------------------------------- blueprints

def get_or_create_bp(name, parent_class):
    path = f"{BP_DIR}/{name}"
    if eal.does_asset_exist(path):
        bp = eal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        bp = asset_tools.create_asset(name, BP_DIR, unreal.Blueprint, factory)
        log(f"created {path}")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp


def bp_class(bp):
    return unreal.BlueprintEditorLibrary.generated_class(bp)


def save_bp(bp):
    eal.save_asset(bp.get_path_name().split(".")[0], only_if_is_dirty=False)


# ---------------------------------------------------------------- level helpers

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def spawn(cls, loc, yaw=0.0, pitch=0.0, label=None, folder="SpiritPath"):
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    actor.set_editor_property("tags", [unreal.Name(BUILDER_TAG)])
    if label:
        actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def block(label, cx, cy, top_z, size, material):
    """Greybox cube. size = (x, y, z) in cm, positioned by its top surface"""
    actor = spawn(unreal.StaticMeshActor, (cx, cy, top_z - size[2] / 2.0), label=label, folder="SpiritPath/Geometry")
    smc = actor.static_mesh_component
    smc.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Cube"))
    smc.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


# ---------------------------------------------------------------- build

def main():
    # Materials
    glow = build_glow_material()
    ghost = build_ghost_material()
    build_instance("MI_SpiritPlatform", glow, {"Color": unreal.LinearColor(0.02, 0.18, 1.0, 1.0)}, {"Glow": 0.8})   # brighter/paler washed out to white in the Milestone 2 canyon light
    build_instance("MI_Switch", glow, {"Color": unreal.LinearColor(1.0, 0.7, 0.1, 1.0)}, {"Glow": 2.0})
    build_instance("MI_EndZone", glow, {"Color": unreal.LinearColor(0.2, 1.0, 0.35, 1.0)}, {"Glow": 1.5})
    grid = eal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray")

    # Blueprints. Bat is the template character as-is (realm defaults to Living)
    bat_class = eal.load_blueprint_class("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter")

    saraa_bp = get_or_create_bp("BP_Saraa", bat_class)
    saraa_cdo = unreal.get_default_object(bp_class(saraa_bp))
    saraa_cdo.set_editor_property("realm", unreal.EchoRealm.SPIRIT)
    saraa_cdo.set_editor_property("ghost_material", ghost)
    save_bp(saraa_bp)

    template_gm_cdo = unreal.get_default_object(
        eal.load_blueprint_class("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode"))
    gm_bp = get_or_create_bp("BP_EchoGameMode", unreal.EchoGameMode)
    gm_cdo = unreal.get_default_object(bp_class(gm_bp))
    gm_cdo.set_editor_property("player_controller_class", template_gm_cdo.get_editor_property("player_controller_class"))
    gm_cdo.set_editor_property("default_pawn_class", bat_class)
    gm_cdo.set_editor_property("bat_pawn_class", bat_class)
    gm_cdo.set_editor_property("saraa_pawn_class", bp_class(saraa_bp))
    save_bp(gm_bp)

    # Level
    if eal.does_asset_exist(MAP_PATH):
        les.load_level(MAP_PATH)
        removed = 0
        for actor in eas.get_all_level_actors():
            if actor.actor_has_tag(BUILDER_TAG):
                eas.destroy_actor(actor)
                removed += 1
        log(f"loaded existing level, removed {removed} builder actors")
    else:
        les.new_level(MAP_PATH)
        log(f"created level {MAP_PATH}")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", bp_class(gm_bp))

    # Lighting
    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), yaw=-30.0, pitch=-40.0, label="Sun", folder="SpiritPath/Lighting")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sky = spawn(unreal.SkyLight, (0, 0, 800), label="SkyLight", folder="SpiritPath/Lighting")
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere", folder="SpiritPath/Lighting")
    spawn(unreal.ExponentialHeightFog, (0, 0, -500), label="HeightFog", folder="SpiritPath/Lighting")

    # Layout (cm). Walking surfaces are at Z=0. The gap runs X 0..1600.
    block("StartArea", -750, 0, 0, (1500, 1200, 100), grid)   # X -1500..0
    block("FarSide", 2600, 0, 0, (2000, 1200, 100), grid)     # X 1600..3600

    spawn(unreal.PlayerStart, (-1200, -150, 100), label="PlayerStart_A", folder="SpiritPath/Gameplay")
    spawn(unreal.PlayerStart, (-1200, 150, 100), label="PlayerStart_B", folder="SpiritPath/Gameplay")

    # Saraa's lane (Y=-300): stepping stones with small hops
    for i, (x, top) in enumerate([(250, 0), (650, 30), (1050, 60), (1400, 30)]):
        p = spawn(unreal.EchoSpiritPlatform, (x, -300, top), label=f"SpiritPlatform_{i + 1}", folder="SpiritPath/Gameplay")
        p.set_editor_property("platform_size", unreal.Vector(250, 250, 25))

    # Bat's lane (Y=+300): bridge that rises from the pit
    bridge = spawn(unreal.EchoRisingBridge, (800, 300, 0), label="RisingBridge", folder="SpiritPath/Gameplay")
    bridge.set_editor_property("bridge_size", unreal.Vector(1600, 300, 30))

    switch = spawn(unreal.EchoSpiritSwitch, (1900, -300, 0), label="SpiritSwitch", folder="SpiritPath/Gameplay")
    switch.set_editor_property("target_bridge", bridge)

    zone = spawn(unreal.EchoEndZone, (3100, 0, 0), label="EndZone", folder="SpiritPath/Gameplay")
    zone.set_editor_property("zone_size", unreal.Vector(600, 800, 300))

    spawn(unreal.EchoRespawnVolume, (1000, 0, -1300), label="RespawnVolume", folder="SpiritPath/Gameplay")

    les.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    count = sum(1 for a in eas.get_all_level_actors() if a.actor_has_tag(BUILDER_TAG))
    log(f"done: {count} builder actors in {MAP_PATH}")


main()
