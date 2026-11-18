"""Initialize the target-selection UI sound without changing camera tuning."""
import unreal
lib=unreal.EditorAssetLibrary
data=lib.load_asset('/Game/ExpeditionPortfolio/Presentation/DA_BattlePresentation')
sound_path='/Game/ExpeditionPortfolio/Audio/SFX/UI/Battle/UI_Battle_Select_Enemy_01'
assert data and lib.does_asset_exist(sound_path)
sound=lib.load_asset(sound_path)
assert isinstance(sound,unreal.SoundBase)
if lib.get_metadata_tag(data,'TargetSelectionSoundSetupVersion')!='1':
 data.set_editor_property('target_selection_sound',sound)
 lib.set_metadata_tag(data,'TargetSelectionSoundSetupVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
unreal.log('TARGET_SELECTION_SOUND_SETUP_OK sound='+str(data.get_editor_property('target_selection_sound')))
