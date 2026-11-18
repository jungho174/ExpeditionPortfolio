"""Initialize projectile impact, counter launch/impact, and melee sounds once."""
import unreal
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/'
assignments=[
 ('Animation/Magic/DA_BasicMagic',{'impact_sound':'Audio/SFX/FeedBack/Buff_Debuff/SFX_FB_Effect_Burn_Feedback_01'}),
 ('Animation/Defense/DA_ParryCounter',{
  'attack_sound':'Audio/SFX/FeedBack/Buff_Debuff/SFX_FB_Effect_Burn_Activate_01',
  'impact_sound':'Audio/SFX/FeedBack/Buff_Debuff/SFX_FB_Effect_Burn_Feedback_01'}),
 ('Animation/Commands/DA_PlayerUtilityPresentation',{'melee_hit_sound':'Audio/SFX/Hits/Hits_L2/SFX_Hit_GestralPunch_Medium_01'})]
for path,fields in assignments:
 data=lib.load_asset(root+path)
 assert data
 if lib.get_metadata_tag(data,'ImpactSoundSetupVersion')!='1':
  for field,sound_path in fields.items():
   assert lib.does_asset_exist(root+sound_path),sound_path
   sound=lib.load_asset(root+sound_path)
   assert isinstance(sound,unreal.SoundBase)
   data.set_editor_property(field,sound)
  lib.set_metadata_tag(data,'ImpactSoundSetupVersion','1')
 assert lib.save_loaded_asset(data,only_if_is_dirty=False)
 for field in fields:unreal.log('IMPACT_SOUND_ASSIGNED '+data.get_name()+' '+field+'='+str(data.get_editor_property(field)))
unreal.log('IMPACT_SOUNDS_SETUP_OK')
