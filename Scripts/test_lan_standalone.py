"""
Two real game processes on one machine, outside Play-In-Editor: one hosts through Online Subsystem Null (LAN), the
other finds and joins the game; both ready up, the host starts, and both must arrive in Lvl_SpiritPath as Bat
(host) and Saraa (guest). Run by Scripts/run_lan_test.ps1 (editor closed), which sets ECHO_LAN_ROLE to "host" or
"guest" before running this file in each process.
The host hosts with the EchoHost console command; everything else goes through the menus (Play > Join Game, the
lobby's Ready and Start Game). Output lines are tagged ECHO_LAN (host) / ECHO_LAN_GUEST; each process ends with
PASS or FAIL.
"""
import unreal

ROLE = globals().get("ECHO_LAN_ROLE", "host")
TAG = "ECHO_LAN" if ROLE == "host" else "ECHO_LAN_GUEST"
gs = unreal.GameplayStatics
T = {"failures": 0, "wait": 0.0, "elapsed": 0.0, "handle": None, "gen": None}
MAPS = ("/Game/Echo/Maps/Lvl_SpiritPath.Lvl_SpiritPath", "/Game/Echo/Maps/TitleScreen.TitleScreen")


def log(msg):
    unreal.log(f"{TAG}: {msg}")


def check(ok, text):
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def world():
    """The world this process is playing in (it changes when the map changes)"""
    for path in MAPS:
        w = unreal.find_object(None, path)
        if w and gs.get_player_controller(w, 0):
            return w
    return None


def map_name():
    w = world()
    return w.get_name() if w else ""


def run(command):
    w = world()
    if w:
        unreal.SystemLibrary.execute_console_command(w, command)


def net_mode():
    w = world()
    if not w:
        return "none"
    if unreal.SystemLibrary.is_standalone(w):
        return "standalone"
    return "server" if unreal.SystemLibrary.is_server(w) else "client"


def players():
    w = world()
    state = gs.get_game_state(w) if w else None
    return list(state.get_editor_property("player_array")) if state else []


def all_ready():
    ps = players()
    return len(ps) == 2 and all(p.is_ready() for p in ps if hasattr(p, "is_ready"))


def local_realm():
    w = world()
    pawn = gs.get_player_pawn(w, 0) if w else None
    return pawn.get_realm() if pawn and hasattr(pawn, "get_realm") else None


def wait_for(cond, timeout):
    t = 0.0
    while t < timeout:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.25
        t += 0.25
    return False


def host():
    yield 3.0
    check(map_name() == "TitleScreen", f"the game starts on the title screen ({map_name()})")
    run("EchoHost")
    ok = yield from wait_for(lambda: net_mode() == "server" and map_name() == "TitleScreen", 20.0)
    check(ok, f"hosting: the lobby is open on a listen server ({net_mode()})")
    ok = yield from wait_for(lambda: len(players()) == 2, 60.0)
    check(ok, f"the guest joined the lobby ({len(players())} players)")
    if not ok:
        return
    ok = yield from wait_for(lambda: top_is("EchoLobbyScreen"), 10.0)
    log(f"lobby screen {ok}: Ready -> {queue_click('Ready') if ok else None}")
    ok = yield from wait_for(all_ready, 30.0)
    check(ok, f"both players are ready ({[(p.get_player_name(), p.is_ready()) for p in players()]})")
    yield 1.0
    log(f"Start Game -> {queue_click('Start Game') if top_is('EchoLobbyScreen') else None}")
    ok = yield from wait_for(lambda: map_name() == "Lvl_SpiritPath" and len(players()) == 2 and local_realm() is not None, 60.0)
    check(ok, f"Start takes the host into the game with the guest ({map_name()}, {len(players())} players)")
    check(local_realm() == unreal.EchoRealm.LIVING, f"the host plays Bat ({local_realm()})")
    yield 8.0  # let the guest finish its checks before the host closes the game


def guest():
    yield 8.0
    check(map_name() == "TitleScreen" and net_mode() == "standalone", f"the game starts on the title screen ({map_name()}, {net_mode()})")
    found = False
    for _ in range(5):
        found = yield from find_games()
        if found:
            break
    check(found, "the hosted game shows up in the game list")
    if not found:
        return
    top().join_first_game()
    ok = yield from wait_for(lambda: net_mode() == "client" and map_name() == "TitleScreen", 30.0)
    check(ok, f"joined the host's lobby ({net_mode()}, {map_name()})")
    yield 1.0
    ok = yield from wait_for(lambda: top_is("EchoLobbyScreen"), 10.0)
    log(f"lobby screen {ok}: Ready -> {queue_click('Ready') if ok else None}")
    ok = yield from wait_for(lambda: map_name() == "Lvl_SpiritPath" and local_realm() is not None, 60.0)
    check(ok, f"the host's Start brings the guest into the game ({map_name()})")
    check(local_realm() == unreal.EchoRealm.SPIRIT, f"the guest plays Saraa ({local_realm()})")
    yield 12.0  # stay in the game while the host checks (the guest quitting sends the host back to the title)


def top():
    w = world()
    pc = gs.get_player_controller(w, 0) if w else None
    root = pc.get_ui_root() if pc and hasattr(pc, "get_ui_root") else None
    return root.get_top_screen() if root else None


def queue_click(label):
    """A real mouse click on the next engine tick, outside Python: while Python runs, the engine runs RPCs locally
    (even in -game from UnrealEditor.exe), so a click that has to reach the host must not happen inside it"""
    t = top()
    button = t.find_button(label) if t else None
    if not button or not button.get_is_enabled():
        return False
    unreal.EchoUITestLibrary.queue_click_widget(button)
    return True


def top_is(name):
    t = top()
    return t is not None and t.get_class().get_name() == name


def find_games():
    """Through the menus: Play > Join Game (or Refresh); true when the host's game is listed"""
    if not top_is("EchoJoinMenuScreen"):
        if top_is("EchoMainMenuScreen"):
            log(f"on {top().get_class().get_name()}: Play -> {top().click_button('Play')}")
            yield from wait_for(lambda: top_is("EchoPlayMenuScreen"), 3.0)
            yield 0.5
        log(f"on {top().get_class().get_name() if top() else None}: Join Game -> {top().click_button('Join Game') if top() else None}")
        yield from wait_for(lambda: top_is("EchoJoinMenuScreen"), 3.0)
        yield 0.5
    else:
        top().click_button("Refresh")
    ok = yield from wait_for(lambda: top_is("EchoJoinMenuScreen") and top().get_num_games_listed() > 0, 8.0)
    return ok


def scenario():
    log(f"started ({ROLE})")
    yield from (host() if ROLE == "host" else guest())


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
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")
    w = world()
    if w:
        unreal.SystemLibrary.execute_console_command(w, "quit")


T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
