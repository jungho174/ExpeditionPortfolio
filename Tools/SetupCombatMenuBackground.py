"""Give existing command buttons a screen-space antialiased UI material.

Run in the editor outside PIE. Preserve layout, text, all state tints and motion settings.
"""
import unreal


def setup():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before saving HUD assets'
    lib = unreal.EditorAssetLibrary
    edit = unreal.MaterialEditingLibrary
    path = '/Game/ExpeditionPortfolio/UI/Materials/M_CommandPanelAA'
    if lib.does_asset_exist(path):
        material = lib.load_asset(path)
        assert isinstance(material, unreal.Material), path
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_CommandPanelAA', '/Game/ExpeditionPortfolio/UI/Materials', unreal.Material, unreal.MaterialFactoryNew())
        assert material
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    expressions = edit.get_material_expressions(material)
    def node(cls, x, y):
        return next((e for e in expressions if isinstance(e, cls)), None) or edit.create_material_expression(material, cls, x, y)
    uv = node(unreal.MaterialExpressionTextureCoordinate, -600, 0)
    color = node(unreal.MaterialExpressionConstant3Vector, -600, -200)
    color.set_editor_property('constant', unreal.LinearColor(1, 1, 1, 1))
    feather = node(unreal.MaterialExpressionScalarParameter, -600, 200)
    if str(feather.get_editor_property('parameter_name')) != 'FeatherPixels':
        feather.set_editor_property('default_value', 1.0)
    feather.set_editor_property('parameter_name', 'FeatherPixels')
    edge = node(unreal.MaterialExpressionCustom, -250, 0)
    edge.set_editor_property('description', 'One screen-pixel edge coverage, with transparent geometry boundary')
    inputs = []
    for name in ['UV', 'FeatherPixels']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    edge.set_editor_property('inputs', inputs)
    edge.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    edge.set_editor_property('code', '''
float2 pixelWidth = max(fwidth(UV), float2(0.000001, 0.000001));
float2 edgeDistance = min(UV, 1.0 - UV) / pixelWidth;
// Keep the geometry boundary fully transparent; fade to the solid interior in screen pixels.
float2 coverage = saturate((edgeDistance - 0.5) / max(FeatherPixels, 0.25));
return min(coverage.x, coverage.y);
''')
    assert edit.connect_material_expressions(uv, '', edge, 'UV')
    assert edit.connect_material_expressions(feather, '', edge, 'FeatherPixels')
    # Slate's normal button draw multiplies the brush tint after evaluating the material.
    # Keep this material white so the existing tint/opacity is applied exactly once.
    assert edit.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert edit.connect_material_property(edge, '', unreal.MaterialProperty.MP_OPACITY)
    errors = edit.recompile_material(material)
    assert not errors, 'Material compile failed: ' + str(errors)
    assert lib.save_loaded_asset(material, only_if_is_dirty=False)

    hud = lib.load_asset('/Game/ExpeditionPortfolio/UI/WBP_CombatHUD')
    tree = unreal.find_object(hud, 'WidgetTree')
    names = {'BTN_Items', 'BTN_Skills', 'BTN_Melee', 'BTN_WeakMagic', 'BTN_StrongMagic', 'BTN_Potion', 'BTN_MenuBack'}
    found = set()
    for button in unreal.ObjectIterator(unreal.Button):
        if button.get_outer() != tree or button.get_name() not in names:
            continue
        style = button.get_editor_property('widget_style')
        for state in ['normal', 'hovered', 'pressed', 'disabled']:
            brush = style.get_editor_property(state)
            brush.set_editor_property('resource_object', material)
            brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
            brush.set_editor_property('image_type', unreal.SlateBrushImageType.FULL_COLOR)
            brush.set_editor_property('margin', unreal.Margin())
            style.set_editor_property(state, brush)
        button.set_style(style)
        found.add(button.get_name())
    assert found == names, 'Missing command buttons: ' + str(names - found)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud)
    assert lib.save_loaded_asset(hud, only_if_is_dirty=False)
    unreal.log('COMBAT_MENU_AA_SETUP_OK buttons=' + str(len(found)))


if __name__ == '__main__':
    setup()
