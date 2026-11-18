"""Assign and save stable IDs on placed encounters; preserve existing IDs and combat tuning."""
import unreal
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/ThirdPerson/Lvl_ThirdPerson')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
seen=set()
count=0
for actor in actors:
    if not isinstance(actor,unreal.BattleEncounterTrigger):
        continue
    actor.ensure_encounter_id()
    key=unreal.GuidLibrary.conv_guid_to_string(actor.get_editor_property('encounter_id'))
    assert key not in seen,'Duplicate encounter ID: '+key
    seen.add(key)
    count+=1
    unreal.log('ENCOUNTER_ID_READY '+actor.get_name()+' '+key)
assert count>0,'No encounter triggers found in combat map'
assert level.save_current_level()
unreal.log('ENCOUNTER_PERSISTENCE_SETUP_OK count='+str(count))
