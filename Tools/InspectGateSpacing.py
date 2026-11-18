import unreal,json
from pathlib import Path
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not ed.get_game_world()
g=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='WC_GateEncounter_Group')
d=g.get_editor_property('composition'); enemies=list(g.get_encounter_enemies()); rows=[]
for i,(m,a) in enumerate(zip(d.get_editor_property('members'),enemies)):
 t=m.get_editor_property('relative_transform'); p=t.translation; cap=a.get_component_by_class(unreal.CapsuleComponent)
 row={'i':i,'local':str(p),'world':str(a.get_actor_location()),'capsule':str(cap.get_scaled_capsule_radius()),'samples':[]}
 for y in range(-450,451,25):
  q=unreal.MathLibrary.transform_location(g.get_actor_transform(),unreal.Vector(p.x,y,p.z)); s=unreal.BattleGroundingLibrary.query_actor_ground(a,q,unreal.BattleGroundingSettings()); row['samples'].append({'y':y,'ok':s.get_editor_property('supported'),'pos':str(s.get_editor_property('location'))})
 rows.append(row)
Path(unreal.Paths.project_saved_dir()+'GateSpacingInspect.json').write_text(json.dumps({'group':str(g.get_actor_transform()),'rows':rows},indent=2),encoding='utf-8')
unreal.log('GATE_SPACING_INSPECT_OK')
