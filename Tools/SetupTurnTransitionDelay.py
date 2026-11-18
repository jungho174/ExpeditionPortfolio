import unreal
lib=unreal.EditorAssetLibrary
data=lib.load_asset('/Game/ExpeditionPortfolio/Combat/Commands/DA_PlayerCommands')
assert data
if lib.get_metadata_tag(data,'TurnTransitionDelayVersion')!='1':
 data.set_editor_property('turn_transition_delay',1)
 lib.set_metadata_tag(data,'TurnTransitionDelayVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
unreal.log('TURN_TRANSITION_DELAY_SAVED '+str(data.get_editor_property('turn_transition_delay')))
