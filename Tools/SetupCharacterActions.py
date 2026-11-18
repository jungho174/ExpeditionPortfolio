"""One-way, non-destructive migration to character-owned actions and shared parry feedback.

Run alone after the Editor build, outside PIE. Existing destination tuning is preserved.
"""
import json
import unreal

# Historical first migration reads native executor properties that no longer exist.
# Current projects use SetupSharedCombatOwnership.py; never reconnect old defaults.
if unreal.EditorAssetLibrary.does_asset_exist('/Game/ExpeditionPortfolio/Combat/Rules/DA_BattleRules'):
    raise RuntimeError('Character actions are already migrated. Run Tools/SetupSharedCombatOwnership.py instead.')

lib=unreal.EditorAssetLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/ExpeditionPortfolio/Characters/PlayerMage/Combat'
shared='/Game/ExpeditionPortfolio/Presentation'

def get(obj,name):
    # Non-editable serialized migration fields retain their native reflection names.
    try: return obj.get_editor_property(name)
    except Exception: return obj.get_editor_property(''.join(part[:1].upper()+part[1:] for part in name.split('_')))

def put(obj,name,value):
    try: obj.set_editor_property(name,value)
    except Exception: obj.set_editor_property(''.join(part[:1].upper()+part[1:] for part in name.split('_')),value)


def load(path):
    assert lib.does_asset_exist(path),path
    return lib.load_asset(path)

def save(obj):
    assert lib.save_loaded_asset(obj,only_if_is_dirty=False),obj.get_path_name()

def data(name,cls,dest=folder):
    path=dest+'/'+name
    if lib.does_asset_exist(path): return load(path),False
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
    obj=assets.create_asset(name,dest,cls,factory)
    assert obj,path
    return obj,True

def duplicate(source,name):
    path=folder+'/'+name
    if lib.does_asset_exist(path): return load(path),False
    assert source,name
    obj=assets.duplicate_asset(name,folder,source)
    assert obj,path
    return obj,True

bp=load('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
player=unreal.get_default_object(bp.generated_class())
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
combatant=None
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj,unreal.CombatantComponent): combatant=obj; break
assert combatant,'Player CombatantComponent missing'
magic=player.get_editor_property('magic_presentation')
utility=player.get_editor_property('utility_presentation')
old_item=utility.get_editor_property('presentation_data')
old_counter=get(magic,'counter_magic')
old_fire=get(magic,'basic_magic')
old_ice=get(magic,'strong_magic')
old_defense=player.get_editor_property('defense_presentation').get_editor_property('presentation_data')

basic,new=data('DA_PlayerMageBasicAttack',unreal.MeleeActionData)
if new:
    basic.set_editor_property('action_id','PlayerMage.BasicAttack')
    basic.set_editor_property('display_name','일반 공격')
    basic.set_editor_property('description','도약 후 주먹 공격')
    basic.set_editor_property('base_damage',get(combatant.get_editor_property('player_command_data'),'melee_damage'))
    basic.set_editor_property('executor_component_name',utility.get_name())
    for prop in ['approach_montage','return_montage','hit_time','approach_duration','return_duration',
                 'target_distance','approach_leap_height','return_leap_height','approach_sound','return_sound']:
        basic.set_editor_property(prop,get(old_item,prop))
    basic.set_editor_property('attack_montage',get(old_item,'melee_montage'))
    sound=unreal.QTESound()
    for dest,src in [('sound','melee_hit_sound'),('volume','melee_hit_sound_volume'),('delay','melee_hit_sound_delay'),('start_time','melee_hit_sound_start_time')]:
        sound.set_editor_property(dest,get(old_item,src))
    basic.set_editor_property('hit_sound',sound)
    save(basic)

actions=[]
for source,name,identifier,label,description,uses in [
    (old_fire,'DA_PlayerMageFireSkill','PlayerMage.Fire','약한 마법','불꽃 발사',0),
    (old_ice,'DA_PlayerMageIceSkill','PlayerMage.Ice','강한 마법','얼음 안개',1),
    (old_counter,'DA_PlayerMageCounter','PlayerMage.Counter','마법사 반격','불꽃 반격',0)]:
    obj,new=duplicate(source,name)
    if new:
        for prop,value in [('action_id',identifier),('display_name',label),('description',description),('uses_per_encounter',uses),('executor_component_name',magic.get_name())]:
            obj.set_editor_property(prop,value)
        # Old impact fields remain only for reading old packages. New character attacks contain no shared feedback references.
        for prop in ['parry_impact_effect','parry_impact_sound','parry_camera_shake']:
            put(obj,prop,None)
        put(obj,'parry_slow_duration',0)
        save(obj)
    actions.append(obj)

feedback,new=data('DA_ParryFeedback',unreal.ParryFeedbackData,shared)
if new:
    mid=unreal.ParryFeedbackStyle()
    mid.set_editor_property('sound',get(old_defense,'success_sound'))
    mid.set_editor_property('effect',get(old_defense,'success_effect'))
    mid.set_editor_property('effect_scale',unreal.Vector(*([get(old_defense,'success_effect_scale')]*3)))
    mid.set_editor_property('slow_duration',get(old_defense,'success_slow_duration'))
    mid.set_editor_property('slow_factor',get(old_defense,'success_slow_factor'))
    complete=unreal.ParryFeedbackStyle()
    sound=unreal.QTESound()
    for dest,src in [('sound','parry_impact_sound'),('volume','parry_sound_volume'),('delay','parry_sound_delay'),('start_time','parry_sound_start_time')]:
        sound.set_editor_property(dest,get(old_counter,src))
    complete.set_editor_property('sound',sound)
    for dest,src in [('effect','parry_impact_effect'),('effect_scale','parry_impact_scale'),('slow_duration','parry_slow_duration'),
                     ('slow_factor','parry_slow_factor'),('socket','cast_socket'),('camera_shake','parry_camera_shake'),('camera_shake_scale','parry_camera_shake_scale')]:
        complete.set_editor_property(dest,get(old_counter,src))
    feedback.set_editor_property('intermediate',mid)
    feedback.set_editor_property('complete',complete)
    save(feedback)

profile,new=data('DA_PlayerMageCombat',unreal.CharacterCombatData)
if new:
    profile.set_editor_property('character_id','PlayerMage')
    profile.set_editor_property('basic_attack',basic)
    profile.set_editor_property('skills',actions[:2])
    profile.set_editor_property('parry_counter',actions[2])
    save(profile)
combatant.set_editor_property('character_actions',profile)
# Legacy references are deliberately disconnected on the active character.
for prop in ['basic_magic','strong_magic','counter_magic']: put(magic,prop,None)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
save(bp)

pc_bp=load('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
pc=unreal.get_default_object(pc_bp.generated_class())
pc.get_editor_property('battle_presentation').set_editor_property('parry_feedback',feedback)
unreal.BlueprintEditorLibrary.compile_blueprint(pc_bp); save(pc_bp)
connected=[pc_bp.get_path_name()]

assert combatant.get_editor_property('character_actions')==profile
for action in [basic]+actions:
    assert action.get_editor_property('base_damage')>=0
    assert str(action.get_editor_property('action_id'))!='None'
unreal.log('CHARACTER_ACTION_MIGRATION_OK '+json.dumps({'profile':profile.get_path_name(),'actions':[a.get_path_name() for a in [basic]+actions],
    'damage':[a.get_editor_property('base_damage') for a in [basic]+actions],'controller':connected,
    'complete_slow':feedback.get_editor_property('complete').get_editor_property('slow_duration')},ensure_ascii=False))
