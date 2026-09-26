"""
Builds Milestone 5's front end:
  - Audio   /Game/Echo/Audio: sound classes SC_Master (parent) with SC_Music, SC_SFX, SC_Dialogue, SC_Voice, and the
            SMix_Settings sound mix the settings drive (UEchoGameUserSettings::ApplyAudio).
  - UI      /Game/Echo/UI: DT_UIActions (CommonUI's Confirm and Back actions for keyboard and gamepad) and
            BP_EchoUIInputData, the CommonUI input data class set in DefaultGame.ini.
  - Input   player-mappable names on the input actions and the WASD mappings, so Settings > Controls can rebind
            them (Enhanced Input user settings): MoveForward/MoveBackward/MoveLeft/MoveRight, Jump, Aim, Fire, Whip, Menu.
  - FX      M_Leaf and NS_TitleLeaves: leaves drifting in the wind (Niagara's BlowingParticles template, placeholder
            quads with a leaf-coloured material).
  - Map     /Game/Echo/Maps/TitleScreen: a small greybox corner of the canyon at sunset, Bat (AEchoTitleBat) seated
            on a rock by a small tree, the drifting cinematic camera (AEchoTitleCamera), leaves, soft warm fog, and
            silent music and wind ambience hooks. Its game mode is AEchoFrontEndGameMode. It's the game's default
            map (DefaultEngine.ini) and, opened with ?listen, the lobby.

Safe to re-run: assets are updated in place and the map is rebuilt from scratch.
Run in the open editor (it opens TitleScreen to build it, then reopens Lvl_SpiritPath):
  py exec(open(r'<project>/Scripts/build_title_screen.py').read())
Output lines are tagged ECHO_TITLE in Saved/Logs/OurLastEcho.log.
"""
import json
import math

import unreal

MARK = "ECHO_TITLE"
AUDIO_DIR = "/Game/Echo/Audio"
UI_DIR = "/Game/Echo/UI"
FX_DIR = "/Game/Echo/FX"
MAT_DIR = "/Game/Echo/Materials"
TITLE_MAP = "/Game/Echo/Maps/TitleScreen"
GAME_MAP = "/Game/Echo/Maps/Lvl_SpiritPath"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def get_or_create(name, folder, cls, factory):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    log(f"created {path}")
    return asset_tools.create_asset(name, folder, cls, factory)


def save(asset):
    eal.save_asset(asset.get_path_name().split(".")[0], only_if_is_dirty=False)


# ------------------------------------------------------------------ audio

def build_audio():
    master = get_or_create("SC_Master", AUDIO_DIR, unreal.SoundClass, unreal.SoundClassFactory())
    children = []
    for name in ("SC_Music", "SC_SFX", "SC_Dialogue", "SC_Voice"):
        child = get_or_create(name, AUDIO_DIR, unreal.SoundClass, unreal.SoundClassFactory())
        child.set_editor_property("parent_class", master)
        children.append(child)
    master.set_editor_property("child_classes", children)
    for sound_class in [master] + children:
        save(sound_class)

    mix = get_or_create("SMix_Settings", AUDIO_DIR, unreal.SoundMix, unreal.SoundMixFactory())
    save(mix)
    log(f"audio: {master.get_name()} -> {[c.get_name() for c in master.get_editor_property('child_classes')]}, mix {mix.get_name()}")


# ------------------------------------------------------------------ CommonUI input data

def build_ui_input():
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.CommonInputActionDataBase.static_struct())
    table = get_or_create("DT_UIActions", UI_DIR, unreal.DataTable, factory)
    rows = [
        {
            "Name": "Confirm", "DisplayName": "Confirm",
            "KeyboardInputTypeInfo": {"Key": "Enter", "AdditionalKeys": ["SpaceBar"]},
            "DefaultGamepadInputTypeInfo": {"Key": "Gamepad_FaceButton_Bottom"},
            "TouchInputTypeInfo": {"Key": "TouchKeys.Touch1"},
        },
        {
            "Name": "Back", "DisplayName": "Back",
            "KeyboardInputTypeInfo": {"Key": "Escape", "AdditionalKeys": ["P"]},
            "DefaultGamepadInputTypeInfo": {"Key": "Gamepad_FaceButton_Right", "AdditionalKeys": ["Gamepad_Special_Right"]},
        },
    ]
    existing = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    if sorted(existing) != ["Back", "Confirm"]:
        # The JSON import lists every field these rows leave at its default in a modal message box: click OK once
        # (it only happens when the table is first made)
        ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
        save(table)
        log(f"DT_UIActions filled: {ok}")
    log(f"DT_UIActions rows {[str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]}")

    path = f"{UI_DIR}/BP_EchoUIInputData"
    if eal.does_asset_exist(path):
        bp = eal.load_asset(path)
    else:
        bp_factory = unreal.BlueprintFactory()
        bp_factory.set_editor_property("parent_class", unreal.CommonUIInputData)
        bp = asset_tools.create_asset("BP_EchoUIInputData", UI_DIR, unreal.Blueprint, bp_factory)
        log(f"created {path}")
    cdo = unreal.get_default_object(bp.generated_class())
    for prop, row in (("default_click_action", "Confirm"), ("default_back_action", "Back")):
        handle = unreal.DataTableRowHandle()
        handle.set_editor_property("data_table", table)
        handle.set_editor_property("row_name", row)
        cdo.set_editor_property(prop, handle)
    save(bp)
    log(f"BP_EchoUIInputData: click {cdo.get_editor_property('default_click_action').get_editor_property('row_name')}, "
        f"back {cdo.get_editor_property('default_back_action').get_editor_property('row_name')}")


# ------------------------------------------------------------------ rebindable input

def mappable(outer, name, display, category):
    settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=outer)
    settings.set_editor_property("name", name)
    settings.set_editor_property("display_name", display)
    settings.set_editor_property("display_category", category)
    return settings


def build_input_settings():
    actions = [
        ("IA_Jump", "Jump", "Jump", "Movement"),
        ("IA_Aim", "Aim", "Aim the bow (Bat)", "Bow"),
        ("IA_Fire", "Fire", "Fire the bow (Bat)", "Bow"),
        ("IA_Whip", "Whip", "Sword whip (Saraa)", "Whip"),
        ("IA_Menu", "Menu", "Menu", "General"),
    ]
    for asset, name, display, category in actions:
        action = eal.load_asset(f"/Game/Input/Actions/{asset}")
        action.set_editor_property("player_mappable_key_settings", mappable(action, name, display, category))
        save(action)

    # WASD all drive IA_Move, so each key gets its own mappable name (the stick stays fixed)
    imc = eal.load_asset("/Game/Input/IMC_Default")
    per_key = {"W": ("MoveForward", "Move forward"), "S": ("MoveBackward", "Move back"),
               "A": ("MoveLeft", "Move left"), "D": ("MoveRight", "Move right")}
    data = imc.get_editor_property("default_key_mappings")
    mappings = data.get_editor_property("mappings")
    named = []
    # Array elements come back as copies: edit each copy, then write the whole list back
    updated = []
    for i in range(len(mappings)):
        mapping = mappings[i]
        key = str(mapping.get_editor_property("key").get_editor_property("key_name"))
        if mapping.get_editor_property("action").get_name() == "IA_Move" and key in per_key:
            name, display = per_key[key]
            mapping.set_editor_property("setting_behavior", unreal.PlayerMappableKeySettingBehaviors.OVERRIDE_SETTINGS)
            mapping.set_editor_property("player_mappable_key_settings", mappable(imc, name, display, "Movement"))
            named.append(f"{key}={name}")
        updated.append(mapping)
    data.set_editor_property("mappings", updated)
    imc.set_editor_property("default_key_mappings", data)
    save(imc)
    log(f"mappable keys: actions {[a[1] for a in actions]}, IMC_Default {named}")


# ------------------------------------------------------------------ leaves

def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def build_leaves():
    path = f"{MAT_DIR}/M_Leaf"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset("M_Leaf", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_niagara_sprites", True)
    color = expr(mat, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color",
                 default_value=unreal.LinearColor(0.42, 0.16, 0.03, 1.0))
    particle = expr(mat, unreal.MaterialExpressionParticleColor, -500, 200)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(particle, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = expr(mat, unreal.MaterialExpressionMultiply, -250, 250, const_b=0.15)
    mel.connect_material_expressions(mul, "", glow, "A")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    save(mat)

    # Made once and then reused: replacing a live Niagara system under the same name crashed the editor when the
    # map assigned it. Delete NS_TitleLeaves by hand to make it again
    path = f"{FX_DIR}/NS_TitleLeaves"
    if eal.does_asset_exist(path):
        system = eal.load_asset(path)
    else:
        system = unreal.EchoEditorLibrary.create_niagara_system_from_emitter(
            path, "/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles.BlowingParticles", mat)
        if system:
            save(system)
    log(f"leaves: {system.get_path_name() if system else 'FAILED'}")
    return system


# ------------------------------------------------------------------ map

def spawn(cls, loc, rot=(0.0, 0.0, 0.0), label=None, scale=None):
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    if label:
        actor.set_actor_label(label)
    if scale:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def shape(mesh, label, loc, size, rot=(0.0, 0.0, 0.0), material=None, folder="Scene"):
    """size in cm (engine shapes are 100 cm)"""
    actor = spawn(unreal.StaticMeshActor, loc, rot, label, (size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    smc = actor.static_mesh_component
    smc.set_static_mesh(eal.load_asset(f"/Engine/BasicShapes/{mesh}"))
    if material:
        smc.set_material(0, material)
    actor.set_folder_path(folder)
    return actor


def build_map(leaves):
    # Open (or create) the title map and empty it. Never delete the map asset and make a new one in its place:
    # new_level then fails quietly and everything below would land in whatever level is open
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if eal.does_asset_exist(TITLE_MAP):
        les.load_level(TITLE_MAP)
    else:
        les.new_level(TITLE_MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(TITLE_MAP):
        raise RuntimeError(f"couldn't open {TITLE_MAP} (the editor is on {world.get_path_name()}); nothing was changed")
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
            eas.destroy_actor(actor)
    world.get_world_settings().set_editor_property("default_game_mode", unreal.EchoFrontEndGameMode)

    rock = eal.load_asset("/Game/Echo/Materials/MI_CanyonRock")
    ground = eal.load_asset("/Game/Echo/Materials/MI_CanyonGround")
    boulder = eal.load_asset("/Game/Echo/Materials/MI_CanyonBoulder")
    glow = eal.load_asset("/Game/Echo/Materials/M_EchoGlow")
    foliage = unreal.MaterialInstanceConstantFactoryNew()
    leaf_mi = get_or_create("MI_TitleFoliage", MAT_DIR, unreal.MaterialInstanceConstant, foliage)
    mel.set_material_instance_parent(leaf_mi, glow)
    mel.set_material_instance_vector_parameter_value(leaf_mi, "Color", unreal.LinearColor(0.32, 0.14, 0.03, 1.0))
    mel.set_material_instance_scalar_parameter_value(leaf_mi, "Glow", 0.0)
    save(leaf_mi)
    bark_mi = get_or_create("MI_TitleBark", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(bark_mi, glow)
    mel.set_material_instance_vector_parameter_value(bark_mi, "Color", unreal.LinearColor(0.08, 0.045, 0.025, 1.0))
    mel.set_material_instance_scalar_parameter_value(bark_mi, "Glow", 0.0)
    save(bark_mi)

    # A corner of the canyon: floor, a wall close on the left, a taller one further right, the canyon running
    # away ahead (+X) towards the setting sun
    shape("Cube", "Floor", (1500, 0, -50), (9000, 7000, 100), material=ground)
    for i, (x, y, h, d) in enumerate([(-200, -700, 1400, 700), (900, -900, 2600, 800), (2200, -1300, 3600, 900),
                                      (3800, -1500, 4200, 1000), (5600, -1300, 4600, 900)]):
        shape("Cube", f"WallLeft_{i}", (x, y - d / 2, h / 2 - 100), (1500, d, h), (0.0, 4.0 * (i % 3) - 3.0, 0.0), rock, "Scene/Walls")
    for i, (x, y, h, d) in enumerate([(300, 2600, 3000, 900), (2000, 2900, 4200, 1000), (4000, 2600, 5000, 1100), (6000, 2300, 5200, 1000)]):
        shape("Cube", f"WallRight_{i}", (x, y + d / 2, h / 2 - 100), (1700, d, h), (0.0, 3.0 - 3.0 * (i % 2), 0.0), rock, "Scene/Walls")
    shape("Cube", "FarCap", (8200, 700, 2000), (1000, 6000, 4200), (0.0, 8.0, 0.0), rock, "Scene/Walls")
    for i, (x, y, s) in enumerate([(700, -250, 120), (1600, 900, 220), (2600, -500, 160), (-400, 600, 140), (3400, 1300, 260)]):
        shape("Sphere", f"Boulder_{i}", (x, y, s * 0.3), (s * 1.4, s, s * 0.8), (0.0, i * 37.0, 0.0), boulder, "Scene/Rocks")

    # Bat's rock and the tree beside it
    shape("Sphere", "BatRock", (0, 0, 10), (130, 110, 90), material=boulder, folder="Scene/Bat")
    shape("Cylinder", "TreeTrunk", (-40, 170, 110), (22, 22, 240), (0.0, 0.0, 6.0), bark_mi, "Scene/Tree")
    shape("Cylinder", "TreeBranch", (-5, 200, 205), (10, 10, 110), (0.0, 20.0, 38.0), bark_mi, "Scene/Tree")
    for i, (x, y, z, s) in enumerate([(-40, 170, 250, 150), (20, 220, 225, 110), (-90, 140, 215, 100), (-30, 200, 300, 90)]):
        shape("Sphere", f"TreeCanopy_{i}", (x, y, z), (s, s * 0.9, s * 0.75), material=leaf_mi, folder="Scene/Tree")
    shape("Sphere", "Bush", (120, 260, 25), (110, 90, 60), material=leaf_mi, folder="Scene/Tree")

    # Bat, seated and facing down the canyon (+X), turned a little away from the camera
    bat = spawn(unreal.EchoTitleBat, (0, 0, 0), (0.0, 12.0, 0.0), "TitleBat")
    bat.set_folder_path("Scene/Bat")
    pelvis = bat.get_bone_location("pelvis")
    foot = min(bat.get_bone_location("foot_l").z, bat.get_bone_location("foot_r").z)
    rock_top = 52.0
    bat.set_actor_location(unreal.Vector(-8, 0, rock_top - (pelvis.z - 10.0)), False, False)
    log(f"Bat: pelvis {pelvis.z:.0f} above feet {foot:.0f} in the pose; seated with the pelvis on the rock top ({rock_top:.0f})")

    # Camera: behind Bat and to his left, looking down the canyon towards the sun, with Bat on the right third
    # (the menu sits on the left)
    cam_loc = unreal.Vector(-420, -130, 165)
    target = unreal.Vector(900, -420, 190)
    rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, target)
    camera = spawn(unreal.EchoTitleCamera, (cam_loc.x, cam_loc.y, cam_loc.z), (rot.pitch, rot.yaw, 0.0), "TitleCamera")
    camera.set_folder_path("Scene/Camera")
    cine = camera.get_cine_camera_component()
    cine.set_editor_property("current_focal_length", 22.0)
    cine.set_editor_property("current_aperture", 5.6)
    focus = cine.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
    focus.set_editor_property("manual_focus_distance", (unreal.Vector(0, 0, 100) - cam_loc).length())
    cine.set_editor_property("focus_settings", focus)

    # Sunset: a low warm sun ahead, a hazy sky and soft warm fog
    sun = spawn(unreal.DirectionalLight, (0, 0, 800), (-7.0, 200.0, 0.0), "Sun")
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_editor_property("intensity", 6.0)
    light.set_editor_property("light_color", unreal.Color(r=255, g=150, b=90, a=255))
    light.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, (0, 0, 400), label="SkyLight")
    sky_light = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_light.set_editor_property("real_time_capture", True)
    sky_light.set_editor_property("intensity", 0.8)
    fog = spawn(unreal.ExponentialHeightFog, (0, 0, -100), label="Fog")
    fog_c = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_c.set_editor_property("fog_density", 0.035)
    fog_c.set_editor_property("fog_height_falloff", 0.35)
    fog_c.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.55, 0.32, 0.18, 1.0))
    post = spawn(unreal.PostProcessVolume, (0, 0, 0), label="TitlePostProcess")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for k, v in (("override_vignette_intensity", True), ("vignette_intensity", 0.45),
                 ("override_color_saturation", True), ("color_saturation", unreal.Vector4(0.9, 0.9, 0.9, 1.0)),
                 ("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.3)):
        settings.set_editor_property(k, v)
    post.set_editor_property("settings", settings)

    # Leaves blowing through the shot
    if leaves:
        fx = spawn(unreal.NiagaraActor, (-150, -300, 150), (0.0, 10.0, 0.0), "Leaves")
        fx.get_component_by_class(unreal.NiagaraComponent).set_asset(leaves)
        fx.set_folder_path("Scene/FX")

    # Hooks for title music and wind: drop sound assets onto these (they're silent for now)
    music_class = eal.load_asset(f"{AUDIO_DIR}/SC_Music")
    sfx_class = eal.load_asset(f"{AUDIO_DIR}/SC_SFX")
    for label, sound_class in (("TitleMusic", music_class), ("WindAmbience", sfx_class)):
        ambient = spawn(unreal.AmbientSound, (0, 0, 200), label=label)
        audio = ambient.get_component_by_class(unreal.AudioComponent)
        audio.set_editor_property("sound_class_override", sound_class)
        audio.set_editor_property("auto_activate", True)
        ambient.set_folder_path("Scene/Audio")

    les.save_current_level()
    log(f"built {TITLE_MAP}: {len(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())} actors")


def main():
    build_audio()
    build_ui_input()
    build_input_settings()
    leaves = build_leaves()
    build_map(leaves)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(GAME_MAP)
    log("done")


if not globals().get("ECHO_TITLE_NO_BUILD"):
    try:
        main()
    except Exception as e:
        import traceback
        log(f"FAILED: {e!r}\n{traceback.format_exc()}")
