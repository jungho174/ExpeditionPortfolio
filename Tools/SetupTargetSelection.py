"""Save new target camera defaults without resetting existing camera/skill tuning. Run outside PIE."""
import unreal

lib = unreal.EditorAssetLibrary
data = lib.load_asset('/Game/ExpeditionPortfolio/Presentation/DA_BattlePresentation')
assert data
# Initialize only this feature's new shot once; preserve later user tuning and all existing shots.
if lib.get_metadata_tag(data, 'TargetSelectionSetupVersion') != '1':
    defaults = unreal.get_default_object(unreal.BattlePresentationData.static_class())
    data.set_editor_property('target_selection_shot', defaults.get_editor_property('target_selection_shot'))
    lib.set_metadata_tag(data, 'TargetSelectionSetupVersion', '1')
assert lib.save_loaded_asset(data, only_if_is_dirty=False)
for path in ['/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController',
             '/Game/ExpeditionPortfolio/UI/WBP_CombatHUD']:
    asset = lib.load_asset(path)
    assert asset
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    assert lib.save_loaded_asset(asset, only_if_is_dirty=False)
shot = data.get_editor_property('target_selection_shot')
unreal.log('TARGET_SELECTION_SETUP_OK origin_at_enemy=' + str(shot.get_editor_property('origin_at_enemy')))
