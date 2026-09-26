"""
Opens the map for a Play-In-Editor test setup (ECHO_PIE_MAP). Used by Scripts/tools/pie_mode.ps1, which also sets
the PIE net mode and player count (those settings aren't reachable from Python). Output is tagged ECHO_PIE_MODE.
"""
import unreal

level = globals().get("ECHO_PIE_MAP", "/Game/Echo/Maps/Lvl_SpiritPath")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(level):
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(level)
unreal.log(f"ECHO_PIE_MODE: map {unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name()}")
