"""Create editable victory/defeat layouts and bind session HP persistence. Preserve subsequent user tuning."""
import unreal

lib = unreal.EditorAssetLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
root = '/Game/ExpeditionPortfolio'

def widget(name, parent, member=False):
    path = root + '/UI/' + name
    if lib.does_asset_exist(path):
        result = lib.load_asset(path)
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        result = assets.create_asset(name, root + '/UI', unreal.WidgetBlueprint, factory)
    assert result
    assert unreal.BattleResultEditorLibrary.build_result_layout(result, member)
    unreal.BlueprintEditorLibrary.compile_blueprint(result)
    assert lib.save_loaded_asset(result, only_if_is_dirty=False)
    return result

member = widget('WBP_BattleResultMember', unreal.BattleResultMemberWidget, True)
result = widget('WBP_BattleResult', unreal.BattleResultWidget)
cdo = unreal.get_default_object(result.generated_class())
if not cdo.get_editor_property('member_widget_class'):
    cdo.set_editor_property('member_widget_class', member.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(result)
assert lib.save_loaded_asset(result, only_if_is_dirty=False)
path = root + '/Presentation/DA_BattleResults'
data = lib.load_asset(path) if lib.does_asset_exist(path) else assets.create_asset('DA_BattleResults', root+'/Presentation', unreal.BattleResultPresentationData, unreal.DataAssetFactory())
assert data
if not data.get_editor_property('widget_class'):
    data.set_editor_property('widget_class', result.generated_class())
assert lib.save_loaded_asset(data, only_if_is_dirty=False)
pc = lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
pc_cdo = unreal.get_default_object(pc.generated_class())
presentation = pc_cdo.get_component_by_class(unreal.BattleResultPresentationComponent)
assert presentation
if not presentation.get_editor_property('presentation_data'):
    presentation.set_editor_property('presentation_data', data)
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
assert lib.save_loaded_asset(pc, only_if_is_dirty=False)
player = lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
unreal.BlueprintEditorLibrary.compile_blueprint(player)
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
health = None
for handle in subobjects.k2_gather_subobject_data_for_blueprint(player):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj, unreal.CombatantComponent):
        health = obj
        break
assert health
if str(health.get_editor_property('persistent_character_id')) == 'None':
    health.set_editor_property('persistent_character_id', 'Player')
if str(health.get_editor_property('combat_display_name')) in ['', '적']:
    health.set_editor_property('combat_display_name', '플레이어')
unreal.BlueprintEditorLibrary.compile_blueprint(player)
assert lib.save_loaded_asset(player, only_if_is_dirty=False)
unreal.log('BATTLE_RESULTS_SETUP_OK duration='+str(data.get_editor_property('slow_duration'))+' player_id='+str(health.get_editor_property('persistent_character_id')))
