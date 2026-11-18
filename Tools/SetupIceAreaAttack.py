"""Enable the mage's area spell without replacing its damage, QTE or audiovisual tuning."""
import unreal,json
from pathlib import Path

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/Characters/PlayerMage/Combat/'
ice=unreal.load_asset(root+'DA_PlayerMageIceSkill')
before={name:ice.get_editor_property(name) for name in ['base_damage','uses_per_encounter','qte_pattern','charge_duration','post_timing_hold_duration','attack_effect','charge_effect','attack_sound','impact_sound']}
ice.set_editor_property('target_scope',unreal.CharacterActionTargetScope.ALL_ENEMIES)
camera=ice.get_editor_property('camera_data')
assert camera
if lib.get_metadata_tag(camera,'AreaTargetSelectionVersion')!='1':
    common=unreal.load_asset('/Game/ExpeditionPortfolio/Presentation/DA_BattlePresentation').get_editor_property('target_selection_shot')
    shot=unreal.BattleCameraShot()
    for field in ['offset','origin_at_enemy','fit_target_bounds','target_framing_padding','look_at_enemy_weight','look_height','field_of_view','avoid_world_obstacles','collision_radius']:
        shot.set_editor_property(field,common.get_editor_property(field))
    shot.set_editor_property('origin_at_enemy',True)
    shot.set_editor_property('fit_target_bounds',True)
    shot.set_editor_property('offset',unreal.Vector(-650,0,0))
    shot.set_editor_property('look_height',0)
    camera.set_editor_property('override_target_selection',True)
    camera.set_editor_property('target_selection_shot',shot)
    lib.set_metadata_tag(camera,'AreaTargetSelectionVersion','1')
if lib.get_metadata_tag(camera,'AreaExecutionVersion')!='1':
    old=camera.get_editor_property('execution_shot')
    execution=unreal.BattleCameraShot()
    for field in ['offset','origin_at_enemy','fit_target_bounds','target_framing_padding','look_at_enemy_weight','look_height','field_of_view','avoid_world_obstacles','collision_radius']:
        execution.set_editor_property(field,old.get_editor_property(field))
    execution.set_editor_property('origin_at_enemy',True)
    execution.set_editor_property('offset',unreal.Vector(-650,old.get_editor_property('offset').y,70))
    camera.set_editor_property('execution_shot',execution)
    camera.set_editor_property('override_execution',True)
    lib.set_metadata_tag(camera,'AreaExecutionVersion','1')
assert lib.save_loaded_asset(camera,only_if_is_dirty=False)
assert lib.save_loaded_asset(ice,only_if_is_dirty=False)
for field,value in before.items():assert ice.get_editor_property(field)==value,field
for name in ['DA_PlayerMageFireSkill','DA_PlayerMageBasicAttack','DA_PlayerMageCounter']:
    assert unreal.load_asset(root+name).get_editor_property('target_scope')==unreal.CharacterActionTargetScope.SINGLE_ENEMY,name
report={'asset':ice.get_path_name(),'damage_per_enemy':before['base_damage'],'uses':before['uses_per_encounter'],'qte':str(before['qte_pattern']),'camera':camera.get_path_name()}
Path(unreal.Paths.project_saved_dir()+'IceAreaSetup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ICE_AREA_SETUP_OK '+json.dumps(report))
