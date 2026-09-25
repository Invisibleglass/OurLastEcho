"""
Builds the Milestone 4 assets (no level changes):
  - Enhanced Input  /Game/Input/Actions/IA_Whip (bool) and /Game/Input/IMC_Whip:
        Whip = Left Mouse Button / E / Gamepad Right Trigger   (hold to swing, release to let go)
    UEchoSwordWhipComponent adds IMC_Whip for Saraa's local player at priority 1. Jump (the template's
    IA_Jump: Space / gamepad bottom face button) also lets go of a swing.
  - Materials /Game/Echo/Materials:
        MI_SpiritAnchor       Saraa's glowing blue anchor orb (M_EchoGlow)
        MI_SpiritAnchorHalo   faint translucent blue disc behind the orb (M_EchoOutline, no flicker)
        MI_WhipLine           the whip line while swinging (M_EchoOutline, no flicker)
        MI_SpiritSword        pale blue steel for the placeholder sword (M_EchoGlow)
        MI_AnchorTargetBoard  straw-coloured target board, and MI_AnchorTargetRing, its red ring (M_EchoGlow)
        MI_TrainingDummy      the stretch goal's training dummy (M_EchoGlow)

Safe to re-run: assets are updated in place (the mapping context is cleared and remapped).
Needs M_EchoGlow (build_spirit_path.py) and M_EchoOutline (build_spirit_bow.py).
Runs headless (editor closed) or in the open editor:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script=<this file> -unattended -nop4 -nosplash
  py exec(open(r'<project>/Scripts/build_sword_whip.py').read())
Output lines are tagged ECHO_WHIP in Saved/Logs/OurLastEcho.log.
"""
import unreal

MARK = "ECHO_WHIP"
MAT_DIR = "/Game/Echo/Materials"
INPUT_DIR = "/Game/Input"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def get_or_create(name, folder, cls, factory_names):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = next((getattr(unreal, n)() for n in factory_names if hasattr(unreal, n)), None)
    log(f"created {path}")
    return asset_tools.create_asset(name, folder, cls, factory)


def save(asset):
    eal.save_asset(asset.get_path_name().split(".")[0], only_if_is_dirty=False)


def build_input():
    action = get_or_create("IA_Whip", f"{INPUT_DIR}/Actions", unreal.InputAction, ("InputAction_Factory", "InputActionFactory"))
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(action)

    imc = get_or_create("IMC_Whip", INPUT_DIR, unreal.InputMappingContext, ("InputMappingContext_Factory", "InputMappingContextFactory"))
    imc.unmap_all()
    for key_name in ("LeftMouseButton", "E", "Gamepad_RightTrigger"):
        key = unreal.Key()
        key.import_text(key_name)   # FKey takes a plain key name as text
        imc.map_key(action, key)
    save(imc)

    mapped = [(m.get_editor_property("action").get_name(), str(m.get_editor_property("key").get_editor_property("key_name")))
              for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")]
    log(f"IMC_Whip mappings: {mapped}")


def build_instance(name, parent, vectors, scalars):
    mi = get_or_create(name, MAT_DIR, unreal.MaterialInstanceConstant, ("MaterialInstanceConstantFactoryNew",))
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

    glow = eal.load_asset(f"{MAT_DIR}/M_EchoGlow")
    outline = eal.load_asset(f"{MAT_DIR}/M_EchoOutline")
    if not glow or not outline:
        raise RuntimeError("M_EchoGlow / M_EchoOutline missing: run build_spirit_path.py and build_spirit_bow.py first")

    # Colours are linear and the canyon's auto-exposure clips glow above ~1.5 to white, so: saturated, modest glow
    build_instance("MI_SpiritAnchor", glow, {"Color": unreal.LinearColor(0.05, 0.32, 1.0, 1.0)}, {"Glow": 1.2})
    build_instance("MI_SpiritAnchorHalo", outline, {"Color": unreal.LinearColor(0.08, 0.35, 1.0, 1.0)}, {"Glow": 1.0, "Flicker": 0.0, "Opacity": 0.3})
    build_instance("MI_WhipLine", outline, {"Color": unreal.LinearColor(0.12, 0.45, 1.0, 1.0)}, {"Glow": 1.4, "Flicker": 0.0, "Opacity": 0.85})
    build_instance("MI_SpiritSword", glow, {"Color": unreal.LinearColor(0.25, 0.45, 0.85, 1.0)}, {"Glow": 0.35})
    build_instance("MI_AnchorTargetBoard", glow, {"Color": unreal.LinearColor(0.45, 0.3, 0.1, 1.0)}, {"Glow": 0.15})
    build_instance("MI_AnchorTargetRing", glow, {"Color": unreal.LinearColor(0.55, 0.03, 0.02, 1.0)}, {"Glow": 0.35})
    build_instance("MI_TrainingDummy", glow, {"Color": unreal.LinearColor(0.35, 0.22, 0.1, 1.0)}, {"Glow": 0.0})
    log("done")


try:
    main()
except Exception as e:
    import traceback
    log(f"FAILED: {e!r}\n{traceback.format_exc()}")
