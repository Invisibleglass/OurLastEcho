"""
Live Play-In-Editor test for the settings / pause menu. Run INSIDE the open editor while a fresh 2-player Listen
Server PIE session is running:
  py exec(open(r'<project>/Scripts/test_menu_live.py').read())
Output lines are tagged ECHO_MENU_TEST; the last line is PASS or FAIL.

Covers: opening the menu pauses BOTH machines and freezes gameplay (movement, a timed platform's countdown);
the other player is told who paused; with both players in the menu the game stays paused until BOTH close it;
closing resumes both; the volume setting is saved and read back; the input assets are set up.
Not covered here (needs a person): pressing the real keys (Esc/P/gamepad Start), dragging the slider with the
mouse, and hearing the volume change. A client opening the menu goes through a server RPC, which editor Python
can't exercise (every call runs locally during Python), so Saraa's side is driven on the server's copy of her
controller instead.
"""
import unreal

gs = unreal.GameplayStatics
T = {"failures": 0, "checks": 0, "wait": 0.0, "elapsed": 0.0, "handle": None, "gen": None}


def log(msg):
    unreal.log(f"ECHO_MENU_TEST: {msg}")


def check(ok, text):
    T["checks"] += 1
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def scenario():
    pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
    server = next((w for w in pie if unreal.SystemLibrary.is_server(w)), None)
    client = next((w for w in pie if not unreal.SystemLibrary.is_server(w)), None)
    if not (server and client):
        check(False, "need a 2-player Listen Server PIE session")
        return

    bat_pc = gs.get_player_controller(server, 0)   # Bat is the listen-server host
    game_mode = gs.get_game_mode(server)
    saraa_pc_server = next(pc for pc in gs.get_all_actors_of_class(server, unreal.PlayerController) if pc != bat_pc)
    saraa_c = gs.get_player_pawn(client, 0)
    state_s, state_c = gs.get_game_state(server), gs.get_game_state(client)

    # ---------------------------------------------------------------- assets
    action = unreal.load_asset("/Game/Input/Actions/IA_Menu.IA_Menu")
    imc = unreal.load_asset("/Game/Input/IMC_Menu.IMC_Menu")
    keys = sorted(str(m.get_editor_property("key").get_editor_property("key_name"))
                  for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"))
    check(keys == ["Escape", "Gamepad_Special_Right", "P"], f"IMC_Menu maps Esc, P and gamepad Start {keys}")
    check(action.get_editor_property("trigger_when_paused"), "IA_Menu still triggers while paused (so the other player can open theirs)")
    check(unreal.load_class(None, "/Game/Echo/UI/WBP_SettingsMenu.WBP_SettingsMenu_C") is not None, "WBP_SettingsMenu exists")

    # ---------------------------------------------------------------- a timed platform to watch freeze
    timed = next((p for p in gs.get_all_actors_of_class(server, unreal.EchoPlatform) if p.get_editor_property("timed")), None)
    if timed:
        timed.awaken()
    yield 0.5

    # ---------------------------------------------------------------- Bat opens the menu
    check(not gs.is_game_paused(server) and not gs.is_game_paused(client), "not paused to start with")
    bat_pc.open_settings_menu()
    yield 1.0
    check(bat_pc.is_settings_menu_open(), "the menu is open on Bat's screen")
    check(gs.is_game_paused(server) and gs.is_game_paused(client), "opening it pauses BOTH machines")
    check(len(state_c.get_players_in_settings_menu()) == 1, "Saraa's machine knows who is in the menu (for its Paused banner)")

    before = saraa_c.get_actor_location()
    remaining_before = timed.get_remaining_awake_time() if timed else 0
    saraa_c.add_movement_input(unreal.Vector(1, 0, 0), 1.0, False)
    yield 2.0
    saraa_c.add_movement_input(unreal.Vector(1, 0, 0), 1.0, False)
    moved = (saraa_c.get_actor_location() - before).length()
    check(moved < 1.0, f"Saraa can't move while paused ({moved:.1f} cm)")
    if timed:
        remaining_after = timed.get_remaining_awake_time()
        check(abs(remaining_after - remaining_before) < 0.1, f"a timed platform's countdown is frozen while paused ({remaining_before:.1f}s -> {remaining_after:.1f}s)")

    # ---------------------------------------------------------------- both in the menu: needs both to close
    game_mode.set_player_in_settings_menu(saraa_pc_server, True)
    yield 0.5
    check(len(state_c.get_players_in_settings_menu()) == 2, "both players in the menu")
    bat_pc.close_settings_menu()
    yield 1.0
    check(not bat_pc.is_settings_menu_open(), "Bat closed his menu")
    check(gs.is_game_paused(server) and gs.is_game_paused(client), "still paused while Saraa has hers open")
    game_mode.set_player_in_settings_menu(saraa_pc_server, False)
    yield 1.0
    check(not gs.is_game_paused(server) and not gs.is_game_paused(client), "resumes on BOTH machines once nobody is in the menu")
    check(len(state_c.get_players_in_settings_menu()) == 0, "nobody listed in the menu")

    # ---------------------------------------------------------------- the game runs again
    before = saraa_c.get_actor_location()
    for _ in range(10):
        saraa_c.add_movement_input(unreal.Vector(1, 0, 0), 1.0, False)
        yield 0
    moved = (saraa_c.get_actor_location() - before).length()
    check(moved > 20, f"Saraa can move again after resuming ({moved:.0f} cm)")

    # ---------------------------------------------------------------- volume
    old = unreal.EchoAudioSettings.get_master_volume()
    unreal.EchoAudioSettings.set_master_volume(server, 0.35)
    check(abs(unreal.EchoAudioSettings.get_master_volume() - 0.35) < 0.001, "the volume setting is saved and read back (35%)")
    unreal.EchoAudioSettings.set_master_volume(server, 1.7)
    check(unreal.EchoAudioSettings.get_master_volume() == 1.0, "volume is clamped to 0..100%")
    unreal.EchoAudioSettings.set_master_volume(server, old)


def on_tick(delta):
    T["elapsed"] += delta
    if T["wait"] > 0:
        T["wait"] -= delta
        return
    try:
        T["wait"] = next(T["gen"]) or 0.0
    except StopIteration:
        finish()
    except Exception as e:
        import traceback
        check(False, f"crashed: {e!r} {traceback.format_exc()}")
        finish()


def finish():
    unreal.unregister_slate_post_tick_callback(T["handle"])
    log(f"{T['checks']} checks in {T['elapsed']:.0f}s")
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")


T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
log("started")
