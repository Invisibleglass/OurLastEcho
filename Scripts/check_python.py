import unreal
maps = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True)
maps = [m for m in maps if unreal.EditorAssetLibrary.find_asset_data(m).asset_class_path.asset_name == "World"]
unreal.log("CLAUDE_PY_OK engine={} maps={}".format(unreal.SystemLibrary.get_engine_version(), maps))
