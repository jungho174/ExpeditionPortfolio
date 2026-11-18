"""Initialize requested combat sounds once; keep existing motion and audio tuning."""
import unreal
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/'
assignments=[
 ('Animation/Defense/DA_PlayerDefensePresentation',{
  'hit_sound':'Audio/SFX/Hits/Hits_L2/SFX_Hit_Magic_Generic_Medium_01',
  'dodge_sound':'Audio/SFX/Swordman/Foley/F_ENM_Swordman_Run_02'}),
 ('Animation/Commands/DA_PlayerUtilityPresentation',{
  'approach_sound':'Audio/SFX/Swordman/Foley/F_ENM_Swordman_Run_08',
  'return_sound':'Audio/SFX/Swordman/Foley/F_ENM_Swordman_Run_09'}),
 ('Animation/Magic/DA_StrongMagic',{'impact_sound':'Audio/SFX/FeedBack/Buff_Debuff/SFX_FB_Effect_Frozen_Deactivate_01'})]
for path,fields in assignments:
 data=lib.load_asset(root+path);assert data
 if lib.get_metadata_tag(data,'DefenseTravelSoundsVersion')!='1':
  for field,sound_path in fields.items():
   assert lib.does_asset_exist(root+sound_path),sound_path
   sound=lib.load_asset(root+sound_path);assert isinstance(sound,unreal.SoundBase)
   if path.endswith('DA_StrongMagic'):data.set_editor_property(field,sound)
   else:
    settings=data.get_editor_property(field);settings.set_editor_property('sound',sound);data.set_editor_property(field,settings)
  lib.set_metadata_tag(data,'DefenseTravelSoundsVersion','1')
 assert lib.save_loaded_asset(data,only_if_is_dirty=False)
 for field in fields:unreal.log('DEFENSE_TRAVEL_SOUND_BOUND '+data.get_name()+' '+field+'='+str(data.get_editor_property(field)))
unreal.log('DEFENSE_TRAVEL_SOUNDS_SETUP_OK')
