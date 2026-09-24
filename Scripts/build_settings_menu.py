"""
Builds the settings/pause menu's input assets:
  /Game/Input/Actions/IA_Menu (bool, triggers while the game is paused, so either player can open the menu
  while the other already has it open) and /Game/Input/IMC_Menu: Escape, P and Gamepad Start.
AOurLastEchoPlayerController adds IMC_Menu (priority 10) and toggles the menu on IA_Menu.
In PIE, Escape stops the session, so use P there.

The menu LAYOUT, /Game/Echo/UI/WBP_SettingsMenu (parent class UEchoSettingsMenu), is built through the Unreal MCP
UMG toolset by Scripts/build_settings_menu_widget.mcp.py.

Safe to re-run. Runs headless (editor closed) or in the open editor:
  py exec(open(r'<project>/Scripts/build_settings_menu.py').read())
Output lines are tagged ECHO_MENU in Saved/Logs/OurLastEcho.log.
"""
import unreal

MARK = "ECHO_MENU"
eal = unreal.EditorAssetLibrary
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


def main():
    action = get_or_create("IA_Menu", "/Game/Input/Actions", unreal.InputAction, ("InputAction_Factory", "InputActionFactory"))
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property("trigger_when_paused", True)
    eal.save_asset("/Game/Input/Actions/IA_Menu", only_if_is_dirty=False)

    imc = get_or_create("IMC_Menu", "/Game/Input", unreal.InputMappingContext, ("InputMappingContext_Factory", "InputMappingContextFactory"))
    imc.unmap_all()
    for key_name in ("Escape", "P", "Gamepad_Special_Right"):
        key = unreal.Key()
        key.import_text(key_name)
        imc.map_key(action, key)
    eal.save_asset("/Game/Input/IMC_Menu", only_if_is_dirty=False)

    mapped = [str(m.get_editor_property("key").get_editor_property("key_name"))
              for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")]
    log(f"IMC_Menu keys {mapped}, IA_Menu triggers when paused: {action.get_editor_property('trigger_when_paused')}")
    log("done")


try:
    main()
except Exception as e:
    import traceback
    log(f"FAILED: {e!r}\n{traceback.format_exc()}")
