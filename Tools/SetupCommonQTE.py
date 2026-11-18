"""Bind shared QTE audio without changing spell damage, timing, hold or camera tuning."""
import unreal
lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Presentation'
path=root+'/DA_QTEPresentation'
data=lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset('DA_QTEPresentation',root,unreal.QTEPresentationData,unreal.DataAssetFactory())
assert data
if lib.get_metadata_tag(data,'CommonQTESetupVersion')!='1':
 for prop,name in [('start_sound','Start'),('regular_sound','Success_Regular'),('perfect_sound','Success_Perfect'),('miss_sound','Miss')]:
  sound_path='/Game/ExpeditionPortfolio/Audio/SFX/UI/Battle/UI_Battle_QTE/UI_Battle_QTE_'+name
  assert lib.does_asset_exist(sound_path),sound_path
  sound=lib.load_asset(sound_path)
  assert isinstance(sound,unreal.SoundBase)
  settings=data.get_editor_property(prop)
  settings.set_editor_property('sound',sound)
  data.set_editor_property(prop,settings)
 lib.set_metadata_tag(data,'CommonQTESetupVersion','1')
pc=lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
cdo=unreal.get_default_object(pc.generated_class())
component=cdo.get_component_by_class(unreal.QTEPresentationComponent)
if not component:
 component=next((o for o in unreal.ObjectIterator(unreal.QTEPresentationComponent) if o.get_outer()==cdo),None)
assert component,'QTE presentation native component not found'
component.set_editor_property('presentation_data',data)
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
gauge=lib.load_asset(root+'/WBP_AttackTiming')
unreal.BlueprintEditorLibrary.compile_blueprint(gauge)
for asset in [data,pc,gauge]:assert lib.save_loaded_asset(asset,only_if_is_dirty=False)
for prop in ['start_sound','regular_sound','perfect_sound','miss_sound']:
 unreal.log('COMMON_QTE_SOUND '+prop+'='+str(data.get_editor_property(prop).get_editor_property('sound')))
unreal.log('COMMON_QTE_SETUP_OK')
