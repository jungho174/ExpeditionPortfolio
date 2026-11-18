"""Apply the requested result labels to the existing Blueprint without rebuilding its layout."""
import unreal

lib = unreal.EditorAssetLibrary
bp = lib.load_asset('/Game/ExpeditionPortfolio/UI/WBP_BattleResult')
assert bp
tree = unreal.find_object(bp, 'WidgetTree')
assert tree
labels = {obj.get_name(): obj for obj in unreal.ObjectIterator(unreal.TextBlock) if obj.get_outer() == tree}
for name, value in [('TXT_Continue', '계속'), ('TXT_DefeatHint', '')]:
    assert name in labels, name
    labels[name].set_editor_property('text', unreal.Text(value))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert lib.save_loaded_asset(bp, only_if_is_dirty=False)
assert str(labels['TXT_Continue'].get_text()) == '계속'
assert str(labels['TXT_DefeatHint'].get_text()) == ''
unreal.log('BATTLE_RESULT_LABELS_UPDATED_OK continue=계속 defeat_hint=empty')
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([bp])
