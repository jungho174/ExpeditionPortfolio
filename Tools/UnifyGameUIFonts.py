"""Match HUD/QTE fonts to Roboto Bold; keep floating combat numbers independent."""
import unreal

lib = unreal.EditorAssetLibrary
font_asset = lib.load_asset('/Engine/EngineFonts/Roboto')
assert font_asset is not None


def assign_font(owner, property_name, fallback_size=None):
    info = owner.get_editor_property(property_name)
    # An unset font previously used the C++ fallback's effective point size.
    if not info.get_editor_property('font_object') and fallback_size is not None:
        info.set_editor_property('size', fallback_size)
    info.set_editor_property('font_object', font_asset)
    info.set_editor_property('typeface_font_name', 'Bold')
    owner.set_editor_property(property_name, info)
    actual = owner.get_editor_property(property_name)
    assert actual.get_editor_property('font_object') == font_asset
    assert str(actual.get_editor_property('typeface_font_name')) == 'Bold'


health = lib.load_asset('/Game/ExpeditionPortfolio/UI/DA_CombatHealthHUD')
assert health is not None
assign_font(health, 'name_font', 15)
assign_font(health, 'health_font', 20)
assert lib.save_loaded_asset(health, only_if_is_dirty=False)

# Floating damage/healing numbers retain their separate Libre Caslon Display style.

timing = lib.load_asset('/Game/ExpeditionPortfolio/Presentation/WBP_AttackTiming')
assert timing is not None
unreal.BlueprintEditorLibrary.compile_blueprint(timing)
assign_font(unreal.get_default_object(timing.generated_class()), 'input_font', 16)
unreal.BlueprintEditorLibrary.compile_blueprint(timing)
assert lib.save_loaded_asset(timing, only_if_is_dirty=False)
unreal.log('GAME_UI_FONTS_SAVED Roboto.Bold health/name/QTE; floating numbers preserved')
