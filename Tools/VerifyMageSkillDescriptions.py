import unreal,time,json
from pathlib import Path
started=time.monotonic(); stage=0; handle=None

def tick(dt):
 global stage,handle
 try:
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if time.monotonic()-started>240: raise RuntimeError('PIE verification timed out')
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0)
  if not pc or not unreal.GameplayStatics.get_player_pawn(w,0):return
  c=next((o for o in unreal.ObjectIterator(unreal.BattleCoordinator) if o.get_outer()==w),None)
  if not c:return
  g=next((a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.BattleEncounterGroup) if a.get_actor_label()=='WC_GateEncounter_Group'),None)
  if not g or len(g.get_encounter_enemies())!=2:return
  if stage==0:
   a=unreal.GameplayStatics.get_player_pawn(w,0);s=unreal.BattleGroundingLibrary.query_actor_ground(a,unreal.Vector(0,1700,a.get_actor_location().z),unreal.BattleGroundingSettings())
   if s.get_editor_property('supported'):a.set_actor_location(s.get_editor_property('location'),False,True)
   if c.get_current_state()==unreal.BattleState.EXPLORATION:assert c.request_group_battle_start(g,a,list(g.get_encounter_enemies()),[])
   stage=1
  if stage==1 and c.get_current_state()==unreal.BattleState.PLAYER_COMMAND:
   hud=next((h for h in unreal.ObjectIterator(unreal.CombatHUDWidget) if h.get_owning_player()==pc),None)
   if not hud:return
   hud.select_menu_entry(2)
   if hud.get_menu_page()!=2:return
   profile=c.get_character_actions();skills=list(profile.get_editor_property('skills'));names=[str(a.get_editor_property('display_name')) for a in skills]
   assert names==['단일공격','광역공격'],names
   assert [str(a.get_editor_property('description')) for a in skills]==['파이어볼','블리자드']
   assert [a.get_editor_property('uses_per_encounter') for a in skills]==[0,1]
   hud.back_to_main_menu();hud.select_menu_entry(2);assert hud.get_menu_page()==2
   labels=[str(t.get_text()) for t in unreal.ObjectIterator(unreal.TextBlock) if t.get_path_name().startswith(hud.get_path_name()) and ('파이어볼' in str(t.get_text()) or '블리자드' in str(t.get_text()))]
   assert len(labels)==2,labels
   assert all('무제한' not in t and '남은' not in t and '모든 적' not in t for t in labels),labels
   expected=['[%d]  %s\n%s · 피해 %.0f' % (i+1, a.get_editor_property('display_name'), a.get_editor_property('description'), a.get_editor_property('base_damage')) for i,a in enumerate(skills)]
   assert sorted(labels)==sorted(expected),labels
   unreal.log('MAGE_SKILL_TEXT_PIE_OK '+str(labels))
   positions=[a.get_actor_location() for a in g.get_encounter_enemies()]
   assert abs(abs(positions[0].x-positions[1].x)-500)<1
   Path(unreal.Paths.project_saved_dir()+'GateLabelsPIE.json').write_text(json.dumps({'names':names,'spacing':abs(positions[0].x-positions[1].x),'menu_page':hud.get_menu_page()},ensure_ascii=False),encoding='utf-8')
   unreal.log('GATE_LABELS_PIE_OK');unreal.unregister_slate_post_tick_callback(handle);stage=2
 except Exception as ex:
  unreal.log_error('GATE_LABELS_PIE_FAILED '+str(ex));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)



