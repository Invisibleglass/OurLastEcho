"""
Builds /Game/Echo/UI/WBP_SettingsMenu (parent class UEchoSettingsMenu) through the Unreal MCP UMG toolset.

This is NOT an editor Python script: it runs in the MCP server's ProgrammaticToolset sandbox, which only offers
execute_tool(...) and json. Run it with the editor open:
  & Scripts/tools/mcp.ps1 -Tool execute_tool_script -Toolset editor_toolset.toolsets.programmatic.ProgrammaticToolset `
      -ArgsJson (@{ script = [IO.File]::ReadAllText('Scripts/build_settings_menu_widget.mcp.py') } | ConvertTo-Json -Compress)
It deletes and rebuilds the widget blueprint, so hand-made layout changes are lost (restyle in the editor instead of
re-running it). The widget names MasterVolumeSlider, MasterVolumeText, ResumeButton and QuitButton are what
UEchoSettingsMenu binds to; keep them if you restyle.
"""
import json

U = 'UMGToolSet.UMGToolSet.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
PATH = '/Game/Echo/UI/WBP_SettingsMenu'

WARM = {'r': 1.0, 'g': 0.84, 'b': 0.6, 'a': 1.0}
SOFT = {'r': 0.8, 'g': 0.76, 'b': 0.7, 'a': 1.0}
WHITE = {'r': 1.0, 'g': 1.0, 'b': 1.0, 'a': 1.0}


def call(tool, args):
    return execute_tool(tool, json.dumps(args))['returnValue']


def setp(obj, values):
    ok = call(O + 'set_properties', {'instance': obj, 'values': json.dumps(values)})
    if not ok:
        raise RuntimeError('set_properties failed on %s: %s' % (obj, list(values)))


def add(wbp, cls, name, parent, index=0):
    return call(U + 'AddWidget', {'widgetBlueprint': wbp, 'widgetClass': {'refPath': '/Script/UMG.' + cls},
                                  'widgetDisplayName': name, 'parentWidget': parent, 'childIndex': index})


def margin(left=0, top=0, right=0, bottom=0):
    return {'left': left, 'top': top, 'right': right, 'bottom': bottom}


def text(wbp, name, parent, index, value, size, color, typeface='Regular', justify='Center'):
    t = add(wbp, 'TextBlock', name, parent, index)
    setp(t['widget'], {'text': value, 'justification': justify,
                       'colorAndOpacity': {'specifiedColor': color, 'colorUseRule': 'UseColor_Specified'},
                       'font': {'fontObject': {'refPath': '/Engine/EngineFonts/Roboto.Roboto'}, 'typefaceFontName': typeface, 'size': size}})
    return t


def brush(color, radius=0):
    b = {'tintColor': {'specifiedColor': color, 'colorUseRule': 'UseColor_Specified'}, 'drawAs': 'Image'}
    if radius:
        b['drawAs'] = 'RoundedBox'
        b['outlineSettings'] = {'cornerRadii': {'x': radius, 'y': radius, 'z': radius, 'w': radius}, 'roundingType': 'FixedRadius',
                                'color': {'specifiedColor': {'r': 0.55, 'g': 0.4, 'b': 0.25, 'a': 0.9}, 'colorUseRule': 'UseColor_Specified'}, 'width': 1.5}
    return b


def run():
    if call(A + 'exists', {'path': PATH}):
        call(A + 'delete', {'path': PATH})
    wbp = call(U + 'CreateWidgetBlueprint', {'folderPath': '/Game/Echo/UI', 'assetName': 'WBP_SettingsMenu',
                                             'parentClass': {'refPath': '/Script/OurLastEcho.EchoSettingsMenu'}})

    root = add(wbp, 'CanvasPanel', 'Root', None)

    # Full-screen dim behind the panel
    dim = add(wbp, 'Border', 'Dim', root['widget'], 0)
    setp(dim['slot'], {'layoutData': {'offsets': margin(), 'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}}, 'alignment': {'x': 0, 'y': 0}}})
    setp(dim['widget'], {'background': brush({'r': 0.0, 'g': 0.0, 'b': 0.0, 'a': 0.55})})

    # Centred panel
    panel = add(wbp, 'Border', 'Panel', root['widget'], 1)
    setp(panel['slot'], {'layoutData': {'offsets': margin(0, 0, 560, 380), 'anchors': {'minimum': {'x': 0.5, 'y': 0.5}, 'maximum': {'x': 0.5, 'y': 0.5}},
                                        'alignment': {'x': 0.5, 'y': 0.5}}})
    setp(panel['widget'], {'background': brush({'r': 0.07, 'g': 0.05, 'b': 0.035, 'a': 0.95}, radius=14), 'padding': margin(36, 28, 36, 28)})

    rows = add(wbp, 'VerticalBox', 'Rows', panel['widget'])

    title = text(wbp, 'TitleText', rows['widget'], 0, 'Paused', 40, WARM, 'Bold')
    setp(title['slot'], {'padding': margin(0, 0, 0, 4)})
    sub = text(wbp, 'SubtitleText', rows['widget'], 1, 'The game is paused for both players while this menu is open', 14, SOFT)
    setp(sub['slot'], {'padding': margin(0, 0, 0, 26)})

    label = text(wbp, 'VolumeLabel', rows['widget'], 2, 'Sound volume', 18, WHITE, 'Regular', 'Left')
    setp(label['slot'], {'padding': margin(0, 0, 0, 6)})

    row = add(wbp, 'HorizontalBox', 'VolumeRow', rows['widget'], 3)
    slider = add(wbp, 'Slider', 'MasterVolumeSlider', row['widget'], 0)
    setp(slider['slot'], {'size': {'sizeRule': 'Fill', 'value': 1.0}, 'verticalAlignment': 'VAlign_Center'})
    setp(slider['widget'], {'value': 1.0, 'stepSize': 0.05})
    pct = text(wbp, 'MasterVolumeText', row['widget'], 1, '100%', 18, WARM, 'Bold', 'Right')
    setp(pct['slot'], {'padding': margin(16, 0, 0, 0), 'verticalAlignment': 'VAlign_Center'})
    setp(pct['widget'], {'minDesiredWidth': 64})

    buttons = []
    for i, (name, caption, top) in enumerate((('ResumeButton', 'Resume', 30), ('QuitButton', 'Quit game', 10))):
        button = add(wbp, 'Button', name, rows['widget'], 4 + i)
        setp(button['slot'], {'padding': margin(0, top, 0, 0)})
        caption_text = text(wbp, name.replace('Button', 'Label'), button['widget'], 0, caption, 20, WHITE, 'Bold')
        buttons.append(button)

    for w in (slider, pct, buttons[0], buttons[1]):
        call(U + 'ToggleWidgetAsVariable', {'widgetBlueprint': wbp, 'widget': w['widget'], 'bIsVariable': True})

    compiled = call(U + 'CompileWidgetBlueprint', {'widgetBlueprint': wbp})
    saved = call(A + 'save_assets', {'asset_paths': [PATH]})
    return {'widget_blueprint': wbp, 'compiled': compiled, 'saved': saved}
