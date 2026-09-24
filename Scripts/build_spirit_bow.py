"""
Builds the Milestone 3 assets (no level changes):
  - Enhanced Input  /Game/Input/Actions/IA_Aim, IA_Fire (bool) and /Game/Input/IMC_Bow:
        Aim  = Right Mouse Button / Gamepad Left Trigger   (hold)
        Fire = Left Mouse Button  / Gamepad Right Trigger
    UEchoSpiritBowComponent adds IMC_Bow for Bat's local player at priority 1, on top of the template's IMC_Default.
  - Materials /Game/Echo/Materials:
        M_EchoOutline   unlit translucent glow with an optional flicker (params Color, Glow, Flicker 0..1, Opacity)
        MI_EchoOutline  warm gold, flickering: the outline Bat sees on echo platforms
        MI_ArrowTrail   gold streak behind arrows (no flicker)
        MI_Arrow        bright gold glow for the arrow (from M_EchoGlow)
        MI_Bow          warm dark wood/gold for the placeholder bow (from M_EchoGlow)

Safe to re-run: assets are updated in place (the mapping context is cleared and remapped).
Runs headless (editor closed) or in the open editor:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script=<this file> -unattended -nop4 -nosplash
  py exec(open(r'<project>/Scripts/build_spirit_bow.py').read())
Output lines are tagged ECHO_BOW in Saved/Logs/OurLastEcho.log.
"""
import unreal

MARK = "ECHO_BOW"
MAT_DIR = "/Game/Echo/Materials"
INPUT_DIR = "/Game/Input"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def get_or_create(name, folder, cls, factory):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    asset = asset_tools.create_asset(name, folder, cls, factory)
    log(f"created {path}")
    return asset


def save(asset):
    eal.save_asset(asset.get_path_name().split(".")[0], only_if_is_dirty=False)


# ------------------------------------------------------------------ input

def factory(*names):
    for name in names:
        cls = getattr(unreal, name, None)
        if cls:
            return cls()
    return None


def build_input():
    actions = {}
    for name in ("IA_Aim", "IA_Fire"):
        action = get_or_create(name, f"{INPUT_DIR}/Actions", unreal.InputAction, factory("InputAction_Factory", "InputActionFactory"))
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
        actions[name] = action

    imc = get_or_create("IMC_Bow", INPUT_DIR, unreal.InputMappingContext, factory("InputMappingContext_Factory", "InputMappingContextFactory"))
    imc.unmap_all()
    for action_name, keys in (("IA_Aim", ("RightMouseButton", "Gamepad_LeftTrigger")),
                              ("IA_Fire", ("LeftMouseButton", "Gamepad_RightTrigger"))):
        for key in keys:
            k = unreal.Key()
            k.import_text(key)   # FKey takes a plain key name as text
            imc.map_key(actions[action_name], k)
    save(imc)

    mapped = [(m.get_editor_property("action").get_name(), str(m.get_editor_property("key").get_editor_property("key_name")))
              for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")]
    log(f"IMC_Bow mappings: {mapped}")


# ------------------------------------------------------------------ materials

def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def build_outline_material():
    """Unlit translucent: Emissive = Color * Glow * F, Opacity = Opacity * F, F = lerp(1, flicker wave, Flicker)"""
    path = f"{MAT_DIR}/M_EchoOutline"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset("M_EchoOutline", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    color = expr(mat, unreal.MaterialExpressionVectorParameter, -900, -200, parameter_name="Color",
                 default_value=unreal.LinearColor(1.0, 0.72, 0.28, 1.0))
    glow = expr(mat, unreal.MaterialExpressionScalarParameter, -900, 0, parameter_name="Glow", default_value=2.5)
    flicker = expr(mat, unreal.MaterialExpressionScalarParameter, -900, 350, parameter_name="Flicker", default_value=1.0)
    opacity = expr(mat, unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name="Opacity", default_value=0.55)

    # wave = 0.55 + 0.45 * sin(t * 9) * sin(t * 3.7 + 1.3): an uneven flicker between ~0.1 and 1
    time = expr(mat, unreal.MaterialExpressionTime, -1300, 200)
    t9 = expr(mat, unreal.MaterialExpressionMultiply, -1150, 150, const_b=9.0)
    mel.connect_material_expressions(time, "", t9, "A")
    s9 = expr(mat, unreal.MaterialExpressionSine, -1000, 150, period=6.2831853)
    mel.connect_material_expressions(t9, "", s9, "")
    t37 = expr(mat, unreal.MaterialExpressionMultiply, -1150, 280, const_b=3.7)
    mel.connect_material_expressions(time, "", t37, "A")
    t37b = expr(mat, unreal.MaterialExpressionAdd, -1050, 280, const_b=1.3)
    mel.connect_material_expressions(t37, "", t37b, "A")
    s37 = expr(mat, unreal.MaterialExpressionSine, -950, 280, period=6.2831853)
    mel.connect_material_expressions(t37b, "", s37, "")
    prod = expr(mat, unreal.MaterialExpressionMultiply, -800, 200)
    mel.connect_material_expressions(s9, "", prod, "A")
    mel.connect_material_expressions(s37, "", prod, "B")
    scaled = expr(mat, unreal.MaterialExpressionMultiply, -680, 200, const_b=0.45)
    mel.connect_material_expressions(prod, "", scaled, "A")
    wave = expr(mat, unreal.MaterialExpressionAdd, -560, 200, const_b=0.55)
    mel.connect_material_expressions(scaled, "", wave, "A")

    factor = expr(mat, unreal.MaterialExpressionLinearInterpolate, -420, 250, const_a=1.0)
    mel.connect_material_expressions(wave, "", factor, "B")
    mel.connect_material_expressions(flicker, "", factor, "Alpha")

    cg = expr(mat, unreal.MaterialExpressionMultiply, -600, -100)
    mel.connect_material_expressions(color, "", cg, "A")
    mel.connect_material_expressions(glow, "", cg, "B")
    emissive = expr(mat, unreal.MaterialExpressionMultiply, -250, -50)
    mel.connect_material_expressions(cg, "", emissive, "A")
    mel.connect_material_expressions(factor, "", emissive, "B")
    op = expr(mat, unreal.MaterialExpressionMultiply, -250, 400)
    mel.connect_material_expressions(opacity, "", op, "A")
    mel.connect_material_expressions(factor, "", op, "B")

    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    save(mat)
    log(f"built {path}")
    return mat


def build_instance(name, parent, vectors, scalars):
    mi = get_or_create(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    save(mi)
    return mi


def main():
    build_input()

    outline = build_outline_material()
    glow = eal.load_asset(f"{MAT_DIR}/M_EchoGlow")   # from build_spirit_path.py
    # Saturated amber at modest glow: paler or brighter values tonemap to cream-white in the canyon's exposure
    build_instance("MI_EchoOutline", outline, {"Color": unreal.LinearColor(1.0, 0.33, 0.03, 1.0)}, {"Glow": 1.4, "Flicker": 1.0, "Opacity": 0.6})
    build_instance("MI_ArrowTrail", outline, {"Color": unreal.LinearColor(1.0, 0.55, 0.15, 1.0)}, {"Glow": 5.0, "Flicker": 0.0, "Opacity": 0.7})
    build_instance("MI_Arrow", glow, {"Color": unreal.LinearColor(1.0, 0.6, 0.2, 1.0)}, {"Glow": 5.0})
    # Linear colour, so dark wood needs small values: (0.32, 0.18, 0.08) looked pale tan-white in the canyon sun
    build_instance("MI_Bow", glow, {"Color": unreal.LinearColor(0.05, 0.02, 0.008, 1.0)}, {"Glow": 0.3})
    # Saraa's spirit blue, shared with the Milestone 1 spirit platforms (build_spirit_path.py uses the same value):
    # Glow 4 (and even a pale blue at 1.5) blew out to white under the canyon's auto-exposure; a deep blue at 0.8 stays blue
    build_instance("MI_SpiritPlatform", glow, {"Color": unreal.LinearColor(0.02, 0.18, 1.0, 1.0)}, {"Glow": 0.8})
    log("done")


try:
    main()
except Exception as e:
    import traceback
    log(f"FAILED: {e!r}\n{traceback.format_exc()}")
