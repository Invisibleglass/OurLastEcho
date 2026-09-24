"""
Read-only check of what build_spirit_path.py produced, loaded fresh from disk.
Also prints each spirit platform's collision response to Pawn (Bat) vs SpiritPawn (Saraa).
Output lines are tagged ECHO_VERIFY in Saved/Logs/OurLastEcho.log.
"""
import unreal

eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"ECHO_VERIFY: {msg}")


saraa = unreal.get_default_object(eal.load_blueprint_class("/Game/Echo/Blueprints/BP_Saraa"))
log(f"BP_Saraa realm={saraa.get_editor_property('realm')} ghost={saraa.get_editor_property('ghost_material')}")
log(f"BP_Saraa mesh={saraa.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset')}")

gm = unreal.get_default_object(eal.load_blueprint_class("/Game/Echo/Blueprints/BP_EchoGameMode"))
for prop in ["player_controller_class", "bat_pawn_class", "saraa_pawn_class", "game_state_class", "hud_class"]:
    log(f"BP_EchoGameMode {prop}={gm.get_editor_property(prop)}")

ghost = eal.load_asset("/Game/Echo/Materials/M_Ghost")
log(f"M_Ghost blend={ghost.get_editor_property('blend_mode')} skel={ghost.get_editor_property('used_with_skeletal_mesh')}")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Echo/Maps/Lvl_SpiritPath")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log(f"world game mode={world.get_world_settings().get_editor_property('default_game_mode')}")

for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    cls = actor.get_class().get_name()
    if not cls.startswith("Echo"):
        continue
    origin, extent = actor.get_actor_bounds(False)
    line = f"{actor.get_actor_label()} ({cls}) loc={actor.get_actor_location()} bounds_extent={extent}"
    if cls == "EchoSpiritSwitch":
        target = actor.get_editor_property("target_bridge")
        line += f" target={target.get_actor_label() if target else None}"
    log(line)

    if cls == "EchoSpiritPlatform":
        mesh = actor.get_editor_property("mesh")
        vs_pawn = mesh.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
        vs_spirit = mesh.get_collision_response_to_channel(unreal.CollisionChannel.ECC_SPIRIT_PAWN)
        log(f"  collision vs Pawn(Bat)={vs_pawn} vs SpiritPawn(Saraa)={vs_spirit} enabled={mesh.get_collision_enabled()}")
