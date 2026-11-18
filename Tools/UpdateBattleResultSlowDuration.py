"""Apply the requested three-second victory/defeat slowdown without replacing other tuning."""
import unreal
lib=unreal.EditorAssetLibrary
data=lib.load_asset('/Game/ExpeditionPortfolio/Presentation/DA_BattleResults')
assert data
before=data.get_editor_property('slow_duration')
data.set_editor_property('slow_duration',3.0)
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
assert data.get_editor_property('slow_duration')==3.0
unreal.log('BATTLE_RESULT_SLOW_UPDATED before='+str(before)+' after='+str(data.get_editor_property('slow_duration'))+' factor='+str(data.get_editor_property('slow_factor')))
