"""Migrate the existing Rhythm sample to independent, staggered gauges without changing spell references."""
import unreal
lib=unreal.EditorAssetLibrary
data=lib.load_asset('/Game/ExpeditionPortfolio/Combat/QTE/DA_QTE_Rhythm_Double')
assert data
if lib.get_metadata_tag(data,'OverlappingRhythmVersion')!='1':
 data.set_editor_property('spawn_interval',.18)
 lib.set_metadata_tag(data,'OverlappingRhythmVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
gauge=lib.load_asset('/Game/ExpeditionPortfolio/Presentation/WBP_AttackTiming')
unreal.BlueprintEditorLibrary.compile_blueprint(gauge)
assert lib.save_loaded_asset(gauge,only_if_is_dirty=False)
unreal.log('OVERLAPPING_QTE_ASSETS_OK interval='+str(data.get_editor_property('spawn_interval')))
