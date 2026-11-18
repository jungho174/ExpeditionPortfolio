"""Create project-owned root-locked forward/back dashes from AM_Dash's sequence."""
import unreal
lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Animation/Commands'
source=lib.load_asset('/Game/Variant_Platforming/Anims/AM_Dash')
assert source
tracks=source.get_editor_property('slot_anim_tracks')
assert len(tracks)==1
segments=tracks[0].get_editor_property('anim_track').get_editor_property('anim_segments')
assert len(segments)==1
original=segments[0].get_editor_property('anim_reference')
path=root+'/AS_MeleeDash'
if not lib.does_asset_exist(path):assert lib.duplicate_asset(original.get_path_name(),path)
sequence=lib.load_asset(path)
sequence.set_editor_property('enable_root_motion',False)
sequence.set_editor_property('force_root_lock',True)
assert lib.save_loaded_asset(sequence,only_if_is_dirty=False)
montages=[]
for name,reverse in [('AM_MeleeDashForward',False),('AM_MeleeDashBack',True)]:
 path=root+'/'+name
 new=not lib.does_asset_exist(path)
 if new:
  factory=unreal.AnimMontageFactory()
  factory.set_editor_property('target_skeleton',sequence.get_editor_property('skeleton'))
  factory.set_editor_property('source_animation',sequence)
  montage=tools.create_asset(name,root,unreal.AnimMontage,factory)
  blend=montage.get_editor_property('blend_in');blend.set_editor_property('blend_time',.04);montage.set_editor_property('blend_in',blend)
  blend=montage.get_editor_property('blend_out');blend.set_editor_property('blend_time',.04);montage.set_editor_property('blend_out',blend)
  if reverse:
   slots=montage.get_editor_property('slot_anim_tracks')
   track=slots[0].get_editor_property('anim_track')
   segs=track.get_editor_property('anim_segments')
   segs[0].set_editor_property('anim_play_rate',-1.0)
   track.set_editor_property('anim_segments',segs)
   slots[0].set_editor_property('anim_track',track)
   montage.set_editor_property('slot_anim_tracks',slots)
 else:montage=lib.load_asset(path)
 assert montage and lib.save_loaded_asset(montage,only_if_is_dirty=False)
 montages.append(montage)
data=lib.load_asset(root+'/DA_PlayerUtilityPresentation')
assert data
if lib.get_metadata_tag(data,'MeleeDashSetupVersion')!='1':
 data.set_editor_property('approach_montage',montages[0]);data.set_editor_property('return_montage',montages[1])
 data.set_editor_property('approach_duration',.32);data.set_editor_property('return_duration',.3)
 data.set_editor_property('approach_leap_height',30.0);data.set_editor_property('return_leap_height',18.0)
 lib.set_metadata_tag(data,'MeleeDashSetupVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
unreal.log('MELEE_DASH_SETUP_OK approach='+str(data.get_editor_property('approach_montage'))+' return='+str(data.get_editor_property('return_montage')))
