"""Initialize a separate strong-magic post-gauge preparation hold once."""
import unreal
lib=unreal.EditorAssetLibrary
data=lib.load_asset('/Game/ExpeditionPortfolio/Animation/Magic/DA_StrongMagic')
assert data
if lib.get_metadata_tag(data,'PostTimingHoldSetupVersion')!='1':
    data.set_editor_property('post_timing_hold_duration',0.5)
    lib.set_metadata_tag(data,'PostTimingHoldSetupVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
unreal.log('POST_TIMING_HOLD_SETUP_OK hold='+str(data.get_editor_property('post_timing_hold_duration')))
