"""
Read-only dump of every actor in the level currently open in the editor: label, class, folder,
location, bounds and tags, plus the key settings of the Echo gameplay actors.
Safe to run in the open editor (it never loads or saves anything):
  py exec(open(r'<project>/Scripts/dump_level.py').read())
Output lines are tagged ECHO_DUMP in Saved/Logs/OurLastEcho.log.
"""
import unreal


def _dump():
    def log(msg):
        unreal.log(f"ECHO_DUMP: {msg}")

    def v(vec):
        return f"({vec.x:.0f}, {vec.y:.0f}, {vec.z:.0f})"

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    all_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    log(f"level {world.get_path_name()} - {len(all_actors)} actors")
    for actor in sorted(all_actors, key=lambda a: (str(a.get_folder_path()), a.get_actor_label())):
        cls = actor.get_class().get_name()
        origin, extent = actor.get_actor_bounds(False)
        tags = ",".join(str(t) for t in actor.tags)
        line = (f"{actor.get_actor_label()} [{cls}] folder={actor.get_folder_path()} loc={v(actor.get_actor_location())} "
                f"yaw={actor.get_actor_rotation().yaw:.0f} bounds_center={v(origin)} extent={v(extent)} tags={tags}")
        if cls == "EchoSpiritPlatform":
            line += f" size={v(actor.get_editor_property('platform_size'))}"
        elif cls == "EchoRisingBridge":
            line += f" size={v(actor.get_editor_property('bridge_size'))}"
        elif cls == "EchoSpiritSwitch":
            target = actor.get_editor_property("target_bridge")
            line += f" target={target.get_actor_label() if target else None} realm={actor.get_editor_property('required_realm')}"
        elif cls == "EchoEndZone":
            line += f" size={v(actor.get_editor_property('zone_size'))}"
        elif cls == "EchoRespawnVolume":
            line += f" size={v(actor.get_editor_property('volume_size'))}"
        log(line)
    log("end")


_dump()
