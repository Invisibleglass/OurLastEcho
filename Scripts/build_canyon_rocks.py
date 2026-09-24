"""
Sets up and runs the canyon rock scatter (PCG) in Lvl_SpiritPath. Run it after build_canyon.py,
from the open editor's console:
  py exec(open(r'<project>/Scripts/build_canyon_rocks.py').read())

What it does:
  - gives the Static Mesh Spawner in /Game/Echo/PCG/PCG_CanyonRocks its rock meshes (engine Sphere,
    LevelPrototyping ChamferCube and engine Cone, all in MI_CanyonBoulder)
  - (re)places the rock exclusion zones: hidden, collision-free boxes tagged EchoRockExclusion
    over the Spirit Path and the overlook ramp; the graph removes any rock inside them
  - sizes the CanyonRocks PCG volume to cover the whole canyon, sets it to generate on demand
    only (never at runtime: the result is saved in the level, identical for every player) and
    regenerates it

The graph itself was built through the Unreal MCP PCG toolset (see NOTES.md). The PCG volume is
spawned the same way (PCGToolset.SpawnGraphInstance, label "CanyonRocks"), because a volume
spawned from Python gets no brush.
Output lines are tagged ECHO_ROCKS in Saved/Logs/OurLastEcho.log.
"""
import unreal

MARK = "ECHO_ROCKS"
TAG = "CanyonRocksBuilder"
EXCLUSION_TAG = "EchoRockExclusion"
GRAPH_PATH = "/Game/Echo/PCG/PCG_CanyonRocks"

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Canyon landscape extents (must match build_canyon.py), with a little margin
CANYON_MIN = (-17600, -12600, -2000)
CANYON_MAX = (20300, 12700, 4000)

# (label, min corner, max corner) in cm
EXCLUSIONS = [
    ("RockExclusion_SpiritPath", (-2300, -1300, -2000), (4400, 1300, 2000)),
    ("RockExclusion_OverlookRamp", (2800, 1300, -500), (7600, 3000, 2000)),
]

# (mesh, weight)
ROCK_MESHES = [
    ("/Game/LevelPrototyping/Meshes/SM_ChamferCube", 6),
    ("/Engine/BasicShapes/Sphere", 2),
    ("/Engine/BasicShapes/Cone", 1),
]


def log(msg):
    unreal.log(f"{MARK}: {msg}")


def configure_spawner(graph):
    material = eal.load_asset("/Game/Echo/Materials/MI_CanyonBoulder")
    spawner = None
    for node in graph.get_editor_property("nodes"):
        settings = node.get_settings()
        if isinstance(settings, unreal.PCGStaticMeshSpawnerSettings):
            spawner = settings
    if not spawner:
        raise RuntimeError("no Static Mesh Spawner node in the graph")

    selector = spawner.get_editor_property("mesh_selector_parameters")
    if not isinstance(selector, unreal.PCGMeshSelectorWeighted):
        raise RuntimeError(f"unexpected mesh selector {selector}")

    entries = []
    for path, weight in ROCK_MESHES:
        entry = unreal.PCGMeshSelectorWeightedEntry()
        entry.set_editor_property("weight", weight)
        descriptor = entry.get_editor_property("descriptor")
        descriptor.set_editor_property("static_mesh", eal.load_asset(path))
        descriptor.set_editor_property("override_materials", [material])
        entry.set_editor_property("descriptor", descriptor)
        entries.append(entry)
    selector.set_editor_property("mesh_entries", entries)
    eal.save_asset(GRAPH_PATH, only_if_is_dirty=False)
    log(f"spawner meshes: {[p.split('/')[-1] for p, _ in ROCK_MESHES]}")


def place_exclusions():
    for actor in eas.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            eas.destroy_actor(actor)
    for label, lo, hi in EXCLUSIONS:
        center = unreal.Vector((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2)
        box = eas.spawn_actor_from_class(unreal.TriggerBox, center, unreal.Rotator(0, 0, 0))
        box.set_actor_label(label)
        box.set_folder_path("Canyon/Rocks")
        box.set_editor_property("tags", [unreal.Name(TAG), unreal.Name(EXCLUSION_TAG)])
        shape = box.get_editor_property("collision_component")
        shape.set_box_extent(unreal.Vector((hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2))
        # Only a marker for PCG: no collision, no overlap events, nothing at runtime
        shape.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        shape.set_editor_property("generate_overlap_events", False)
    log(f"placed {len(EXCLUSIONS)} exclusion zones")


def setup_volume(graph):
    volume = next((a for a in eas.get_all_level_actors()
                   if isinstance(a, unreal.PCGVolume) and a.get_actor_label().startswith("CanyonRocks")), None)
    if not volume:
        raise RuntimeError("no CanyonRocks PCG volume: spawn one with the MCP PCGToolset.SpawnGraphInstance tool first")
    volume.set_actor_label("CanyonRocks")
    volume.set_folder_path("Canyon/Rocks")

    # Scale the volume's brush to cover the canyon
    volume.set_actor_scale3d(unreal.Vector(1, 1, 1))
    volume.set_actor_location(unreal.Vector(0, 0, 0), False, False)
    origin, extent = volume.get_actor_bounds(False)
    size = [CANYON_MAX[i] - CANYON_MIN[i] for i in range(3)]
    center = [(CANYON_MAX[i] + CANYON_MIN[i]) / 2 for i in range(3)]
    volume.set_actor_scale3d(unreal.Vector(size[0] / (2 * extent.x), size[1] / (2 * extent.y), size[2] / (2 * extent.z)))
    volume.set_actor_location(unreal.Vector(*center), False, False)

    pcg = volume.get_component_by_class(unreal.PCGComponent)
    pcg.set_graph(graph)
    pcg.set_editor_property("generation_trigger", unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)
    pcg.generate_local(True)
    log(f"volume {volume.get_name()} covers {size}, generating")
    return volume


def main():
    graph = eal.load_asset(GRAPH_PATH)
    if not graph:
        raise RuntimeError(f"{GRAPH_PATH} not found")
    configure_spawner(graph)
    place_exclusions()
    setup_volume(graph)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("done (generation finishes asynchronously; save the level again once rocks appear)")


try:
    main()
except Exception as e:
    import traceback
    log(f"FAILED: {e!r}\n{traceback.format_exc()}")
