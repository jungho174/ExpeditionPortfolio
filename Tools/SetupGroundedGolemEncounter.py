"""Place WhiteCastle's two gate monsters fully on the side pavements and ground their capsules.
Preserve the encounter ID, original presets, species, stats, retreat point and environment.
"""
import unreal,json
from pathlib import Path
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not e.get_game_world()
assert e.get_editor_world().get_path_name().startswith('/Game/ExpeditionPortfolio/Maps/L_WhiteCastle.')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
group=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='WC_GateEncounter_Group')
lib=unreal.EditorAssetLibrary;before_id=unreal.GuidLibrary.conv_guid_to_string(group.get_editor_property('encounter_id'))
original=group.get_editor_property('composition');retreat=group.get_editor_property('retreat_point')
path='/Game/ExpeditionPortfolio/Exploration/Encounters/DA_WhiteCastleGateEncounter'
data=lib.load_asset(path) if lib.does_asset_exist(path) else lib.duplicate_asset(original.get_path_name(),path)
assert data
if lib.get_metadata_tag(data,'GroundedGateFormation')!='1':
    members=list(data.get_editor_property('members'));instances=list(group.get_encounter_enemies());assert len(members)==len(instances)==2
    result=[]
    for i,(old,actor) in enumerate(zip(members,instances)):
        old_tf=old.get_editor_property('relative_transform');pos=old_tf.translation
        candidate=unreal.Vector(pos.x,-450. if i==0 else 450.,pos.z)
        world=unreal.MathLibrary.transform_location(group.get_actor_transform(),candidate)
        found=unreal.BattleGroundingLibrary.query_actor_ground(actor,world,unreal.BattleGroundingSettings())
        assert found.get_editor_property('supported'),'No safe pavement support '+str(world)
        supported=found.get_editor_property('location')
        local=unreal.MathLibrary.inverse_transform_location(group.get_actor_transform(),supported)
        tf=unreal.Transform(location=local,rotation=old_tf.rotation.rotator(),scale=unreal.Vector(old_tf.scale3d.x,old_tf.scale3d.y,old_tf.scale3d.z))
        member=unreal.EncounterGroupMember();member.set_editor_property('enemy_class',old.get_editor_property('enemy_class'));member.set_editor_property('relative_transform',tf)
        result.append(member);unreal.log('GROUNDED_GATE_MEMBER '+str(i)+' '+str(supported))
    data.set_editor_property('members',result);lib.set_metadata_tag(data,'GroundedGateFormation','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
group.set_editor_property('composition',data);group.rebuild_preview()
assert unreal.GuidLibrary.conv_guid_to_string(group.get_editor_property('encounter_id'))==before_id
assert group.get_editor_property('retreat_point')==retreat
assert len(group.get_encounter_enemies())==2
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log('GROUNDED_GATE_SETUP_OK id='+before_id+' composition='+data.get_path_name())
