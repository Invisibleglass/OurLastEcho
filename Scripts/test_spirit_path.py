"""
End-to-end test for Milestone 1, run inside a real (headless) GAME world. The editor world has no
collision in a commandlet, and Saraa's SpiritPawn collision is only applied when gameplay starts.

Don't run this directly; use Scripts/run_spirit_path_test.ps1, which launches the game with Bat as the
local player, summons a BP_Saraa, runs this script via the `py` console command, and waits for it to quit.

Characters are moved with swept moves using their own capsule collision, so this checks the real
collision setup, overlap triggers and server-side logic - but not rendering or player input.
Output lines are tagged ECHO_TEST; the last line is PASS or FAIL.
"""
import unreal

world = unreal.find_object(None, "/Game/Echo/Maps/Lvl_SpiritPath.Lvl_SpiritPath")
gs = unreal.GameplayStatics

failures = 0


def check(ok, text):
    global failures
    failures += 0 if ok else 1
    unreal.log(f"ECHO_TEST: {'ok  ' if ok else 'FAIL'} {text}")


def drop(actor, x, y, top):
    """Places the actor above (x, y) and sweeps it down. Returns True if something stopped it above `top`"""
    actor.set_actor_location(unreal.Vector(x, y, top + 400), False, True)
    actor.set_actor_location(unreal.Vector(x, y, top - 600), True, False)
    return actor.get_actor_location().z > top


def first_of(cls):
    actors = gs.get_all_actors_of_class(world, cls)
    return actors[0] if actors else None


def finish():
    unreal.log(f"ECHO_TEST: {'PASS' if failures == 0 else f'FAIL ({failures} failures)'}")
    unreal.SystemLibrary.execute_console_command(world, "quit")


characters = {}
for actor in gs.get_all_actors_of_class(world, unreal.OurLastEchoCharacter):
    characters.setdefault("Saraa" if actor.get_realm() == unreal.EchoRealm.SPIRIT else "Bat", actor)
bat, saraa = characters.get("Bat"), characters.get("Saraa")
bridge = first_of(unreal.EchoRisingBridge)
switch = first_of(unreal.EchoSpiritSwitch)

def run_immediate_steps():
    # --- Step 1: spirit platform collision
    for surface, x, y, top, bat_lands, saraa_lands in [
        ("start floor", -750, 0, 0, True, True),
        ("spirit platform 1", 250, -300, 0, False, True),
        ("spirit platform 3", 1050, -300, 60, False, True),
        ("far side floor", 2600, 0, 0, True, True),
    ]:
        check(drop(bat, x, y, top) == bat_lands, f"Bat   on {surface:18} -> {'lands' if bat_lands else 'falls through'}")
        check(drop(saraa, x, y, top) == saraa_lands, f"Saraa on {surface:18} -> {'lands' if saraa_lands else 'falls through'}")

    # --- Step 2: switch only responds to Saraa
    check(not drop(bat, 800, 300, 0), "Bat falls where the bridge will be (bridge still lowered)")
    drop(bat, 1900, -300, 0)
    check(not switch.get_editor_property("activated"), "Bat stepping on the switch does nothing")
    drop(saraa, 1900, -300, 0)
    check(switch.get_editor_property("activated"), "Saraa stepping on the switch activates it")
    check(bridge.get_editor_property("raised"), "switch tells the bridge to raise")

    # Drop Bat into the middle of the pit so the respawn volume should send him home while we wait
    bat.set_actor_location(unreal.Vector(500, 0, -700), False, True)


def run_delayed_steps():
    # --- Step 3: after the bridge has had time to rise (RaiseDuration is 3s)
    check(bat.get_actor_location().x < -1000, f"Bat fell into the pit and respawned at the start (x={bat.get_actor_location().x:.0f})")
    check(drop(bat, 800, 300, 0), "Bat lands on the risen bridge")

    game_state = gs.get_game_state(world)
    drop(bat, 3100, -150, 0)
    check(not game_state.is_milestone_complete(), "Bat alone in the end zone does not complete the milestone")
    drop(saraa, 3100, 150, 0)
    check(game_state.is_milestone_complete(), "Bat + Saraa in the end zone completes the milestone")


state = {"elapsed": 0.0, "handle": None}


def on_tick(delta):
    state["elapsed"] += delta
    if state["elapsed"] < 4.0:
        return
    unreal.unregister_slate_post_tick_callback(state["handle"])
    try:
        run_delayed_steps()
    except Exception as e:
        check(False, f"delayed steps crashed: {e!r}")
    finish()


if not (bat and saraa and bridge and switch):
    check(False, f"setup: found characters={sorted(characters)} bridge={bridge is not None} switch={switch is not None}")
    finish()
else:
    try:
        run_immediate_steps()
        state["handle"] = unreal.register_slate_post_tick_callback(on_tick)
    except Exception as e:
        check(False, f"immediate steps crashed: {e!r}")
        finish()
