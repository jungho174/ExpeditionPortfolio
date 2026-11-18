"""Bind editable menu/turn audio and Space confirmation, preserving existing tuning on reruns."""
import unreal
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/Presentation'
path=root+'/DA_BattleUIAudio'
data=lib.load_asset(path) if lib.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_BattleUIAudio',root,unreal.BattleUIAudioData,unreal.DataAssetFactory())
assert data
if lib.get_metadata_tag(data,'BattleUIAudioVersion')!='1':
 for field,name in [('item_menu','UI_Battle_Item_Menu_Open'),('skill_menu','UI_Skill_NavigatePage'),('no_uses','UI_Battle_NoAP'),('ally_target_start','UI_Battle_Item_Target_Ally_Start'),('ally_target_move','UI_Battle_Select_Ally_01'),('ally_turn','UI_Battle_Turn_Start_Ally'),('enemy_turn','UI_Battle_Turn_Start_Enemy')]:
  sound=lib.load_asset('/Game/ExpeditionPortfolio/Audio/SFX/UI/Battle/'+name)
  assert sound,name
  setting=data.get_editor_property(field);setting.set_editor_property('sound',sound);data.set_editor_property(field,setting)
 lib.set_metadata_tag(data,'BattleUIAudioVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
pc=lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
cdo=unreal.get_default_object(pc.generated_class())
key=unreal.Key();key.set_editor_property('key_name','SpaceBar');cdo.set_editor_property('target_confirm_key',key)
component=cdo.get_component_by_class(unreal.BattlePresentationComponent)
assert component
component.set_editor_property('ui_audio_data',data)
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
assert lib.save_loaded_asset(pc,only_if_is_dirty=False)
unreal.log('BATTLE_UI_AUDIO_SETUP_OK confirm='+str(cdo.get_editor_property('target_confirm_key')))
