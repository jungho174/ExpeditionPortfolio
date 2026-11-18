"""Create reusable QTE patterns; preserve existing strong spell tuning and subsequent user edits."""
import unreal
lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Combat/QTE'
basic=lib.load_asset('/Game/ExpeditionPortfolio/Animation/Magic/DA_BasicMagic')
strong=lib.load_asset('/Game/ExpeditionPortfolio/Animation/Magic/DA_StrongMagic')
assert basic and strong
def pattern(name,mode,values,multiplier):
 path=root+'/'+name
 if lib.does_asset_exist(path):return lib.load_asset(path)
 data=tools.create_asset(name,root,unreal.ActionQTEPatternData,unreal.DataAssetFactory())
 assert data
 steps=[]
 for duration,perfect,gap in values:
  step=unreal.ActionQTEStep()
  step.set_editor_property('duration',duration);step.set_editor_property('perfect_duration',perfect);step.set_editor_property('delay_after',gap)
  steps.append(step)
 data.set_editor_property('steps',steps);data.set_editor_property('mode',mode);data.set_editor_property('perfect_multiplier',multiplier)
 assert lib.save_loaded_asset(data,only_if_is_dirty=False)
 return data
single=pattern('DA_QTE_Single_Hard',unreal.QTEPatternMode.SEQUENTIAL,[(.75,.18,0)],basic.get_editor_property('timing_damage_multiplier'))
strong_single=pattern('DA_QTE_Single_Easy',unreal.QTEPatternMode.SEQUENTIAL,[(strong.get_editor_property('charge_duration'),strong.get_editor_property('timing_success_duration'),0)],strong.get_editor_property('timing_damage_multiplier'))
rhythm=pattern('DA_QTE_Rhythm_Double',unreal.QTEPatternMode.RHYTHM,[(.4,.12,0),(.4,.12,0)],1.5)
sequential=pattern('DA_QTE_Sequential_Double',unreal.QTEPatternMode.SEQUENTIAL,[(1,.25,.15),(1,.25,0)],1.5)
for data,ref in [(basic,single),(strong,strong_single)]:
 if lib.get_metadata_tag(data,'QTEPatternsSetupVersion')!='1':
  data.set_editor_property('qte_pattern',ref)
  if data==basic:
   data.set_editor_property('enable_timing_bonus',True)
   data.set_editor_property('post_timing_hold_duration',.1)
   data.set_editor_property('cast_qte_hold_time',.8)
  lib.set_metadata_tag(data,'QTEPatternsSetupVersion','1')
 assert lib.save_loaded_asset(data,only_if_is_dirty=False)
 unreal.log('QTE_PATTERN_BOUND '+data.get_name()+' pattern='+str(data.get_editor_property('qte_pattern'))+' hold='+str(data.get_editor_property('post_timing_hold_duration')))
gauge=lib.load_asset('/Game/ExpeditionPortfolio/Presentation/WBP_AttackTiming')
unreal.BlueprintEditorLibrary.compile_blueprint(gauge)
assert lib.save_loaded_asset(gauge,only_if_is_dirty=False)
unreal.log('QTE_PATTERNS_SETUP_OK')
