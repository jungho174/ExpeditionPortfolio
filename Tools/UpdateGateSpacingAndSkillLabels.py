"""Rename the two mage skills and tighten the gate formation on its existing pavements."""
import unreal,json
from pathlib import Path
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not ed.get_game_world()
assert ed.get_editor_world().get_path_name().startswith('/Game/ExpeditionPortfolio/Maps/L_WhiteCastle.')
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/Characters/PlayerMage/Combat/'
report={'skills':[],'members':[]}
for name,label,scope in [('DA_PlayerMageFireSkill','단일공격','SINGLE_ENEMY'),('DA_PlayerMageIceSkill','광역공격','ALL_ENEMIES')]:
 a=lib.load_asset(root+name);assert a
 assert a.get_editor_property('target_scope')==getattr(unreal.CharacterActionTargetScope,scope)
 a.set_editor_property('display_name',label)
 assert lib.save_loaded_asset(a,only_if_is_dirty=False)
 assert str(a.get_editor_property('display_name'))==label
 report['skills'].append({'asset':a.get_path_name(),'name':label})
g=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='WC_GateEncounter_Group')
data=g.get_editor_property('composition');guid=unreal.GuidLibrary.conv_guid_to_string(g.get_editor_property('encounter_id'))
old=list(data.get_editor_property('members'));enemies=list(g.get_encounter_enemies());assert len(old)==len(enemies)==2
new=[]
for i,(m,a) in enumerate(zip(old,enemies)):
 t=m.get_editor_property('relative_transform');p=t.translation;y=-250. if i==0 else 250.
 world=unreal.MathLibrary.transform_location(g.get_actor_transform(),unreal.Vector(p.x,y,p.z))
 s=unreal.BattleGroundingLibrary.query_actor_ground(a,world,unreal.BattleGroundingSettings());assert s.get_editor_property('supported')
 target=s.get_editor_property('location');assert abs(target.z-a.get_actor_location().z)<1
 local=unreal.MathLibrary.inverse_transform_location(g.get_actor_transform(),target)
 n=unreal.EncounterGroupMember();n.set_editor_property('enemy_class',m.get_editor_property('enemy_class'));n.set_editor_property('relative_transform',unreal.Transform(location=local,rotation=t.rotation.rotator(),scale=t.scale3d));new.append(n)
 report['members'].append({'x':target.x,'y':target.y,'z':target.z})
data.set_editor_property('members',new);assert lib.save_loaded_asset(data,only_if_is_dirty=False)
g.rebuild_preview();assert len(g.get_encounter_enemies())==2
assert unreal.GuidLibrary.conv_guid_to_string(g.get_editor_property('encounter_id'))==guid
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
Path(unreal.Paths.project_saved_dir()+'GateLabelsUpdate.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('GATE_LABELS_UPDATE_OK '+json.dumps(report,ensure_ascii=False))

