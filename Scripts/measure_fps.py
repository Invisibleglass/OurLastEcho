"""
Measures the editor's frame rate for a few seconds (works with or without PIE running).
  py exec(open(r'<project>/Scripts/measure_fps.py').read())
Prints ECHO_FPS lines: average fps, and the worst frame, over SECONDS of wall-clock time.
Note: the editor throttles itself when it isn't the foreground window (Editor Preferences >
Performance > "Use Less CPU when in Background"), and Windows may throttle background apps too,
so measure with the editor focused for real numbers.
"""
import time
import unreal

SECONDS = 5.0
_fps = {"start": None, "frames": 0, "worst": 0.0, "last": None, "handle": None}


def _fps_tick(delta):
    now = time.perf_counter()
    if _fps["start"] is None:
        _fps["start"] = _fps["last"] = now
        return
    _fps["frames"] += 1
    _fps["worst"] = max(_fps["worst"], now - _fps["last"])
    _fps["last"] = now
    if now - _fps["start"] >= SECONDS:
        unreal.unregister_slate_post_tick_callback(_fps["handle"])
        elapsed = now - _fps["start"]
        pie = len(unreal.EditorLevelLibrary.get_pie_worlds(False))
        unreal.log(f"ECHO_FPS: {_fps['frames'] / elapsed:.1f} fps average over {elapsed:.1f}s, worst frame {_fps['worst'] * 1000:.0f} ms, PIE worlds: {pie}")


_fps["handle"] = unreal.register_slate_post_tick_callback(_fps_tick)
