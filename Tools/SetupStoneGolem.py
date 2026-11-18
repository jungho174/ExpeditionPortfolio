"""Author the approved stone golem and three indexed multi-hit patterns.

Run alone, outside PIE. Vendor meshes/animations are never overwritten.
Existing tuning on project-owned Data Assets is preserved on reruns.
"""
import unreal
import json

lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Animation/StoneGolem'

def load(path):
    assert lib.does_asset_exist(path), path
    return lib.load_asset(path)

def save(asset):
    assert lib.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def create(name,cls,factory,folder=root):
    path=folder+'/'+name
    if lib.does_asset_exist(path): return load(path),False
    asset=tools.create_asset(name,folder,cls,factory)
    assert asset,path
    return asset,True

source=load('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
target=load('/Game/Stone_Golem/mesh/SKM_Stone_Golem')
skeleton=target.get_editor_property('skeleton')
rigs=[]
for name,mesh in [('IK_GolemSource',source),('IK_StoneGolem',target)]:
    rig,new=create(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
    controller=unreal.IKRigController.get_controller(rig)
    if new:
        assert controller.set_skeletal_mesh(mesh)
        assert controller.apply_auto_generated_retarget_definition(),name
        controller.apply_auto_fbik()
        save(rig)
    rigs.append(rig)
retarget,new=create('RTG_Manny_StoneGolem',unreal.IKRetargeter,unreal.IKRetargetFactory())
if new:
    controller=unreal.IKRetargeterController.get_controller(retarget)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,rigs[0])
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,rigs[1])
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE,source)
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET,target)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
    save(retarget)

sources=[
    '/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle',
    '/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd',
    '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01',
    '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02',
    '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03',
    '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack',
    '/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01',
    '/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01']
missing=[p for p in sources if not lib.does_asset_exist(root+'/'+p.rsplit('/',1)[1]+'_Golem')]
if missing:
    inputs=unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property('assets_to_retarget',[lib.find_asset_data(p) for p in missing])
    inputs.set_editor_property('source_mesh',source)
    inputs.set_editor_property('target_mesh',target)
    inputs.set_editor_property('ik_retarget_asset',retarget)
    inputs.set_editor_property('target_path',root)
    inputs.set_editor_property('suffix','_Golem')
    inputs.set_editor_property('include_referenced_assets',False)
    inputs.set_editor_property('overwrite_existing_files',False)
    outputs=unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    assert len(outputs)==len(missing),(len(outputs),len(missing))

clips={p.rsplit('/',1)[1]:load(root+'/'+p.rsplit('/',1)[1]+'_Golem') for p in sources}
for anim in clips.values():
    anim.set_editor_property('enable_root_motion',False)
    anim.set_editor_property('force_root_lock',True)
    save(anim)
unreal.log('GOLEM_RETARGET_OK '+json.dumps({name:seq.get_play_length() for name,seq in clips.items()}))

def montage(name,sequence):
    factory=unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton',skeleton)
    factory.set_editor_property('source_animation',sequence)
    asset,new=create(name,unreal.AnimMontage,factory)
    if new: save(asset)
    return asset

walk=montage('AM_GolemWalk',clips['MF_Unarmed_Walk_Fwd'])
hit=montage('AM_GolemHit',clips['MM_HitReact_Front_Med_01'])
death=montage('AM_GolemDeath',clips['MM_Death_Back_01'])
factory=unreal.AnimBlueprintFactory()
factory.set_editor_property('target_skeleton',skeleton)
abp,new=create('ABP_StoneGolem',unreal.AnimBlueprint,factory)
assert unreal.EnemyAnimationEditorLibrary.build_idle_graph(abp,clips['MM_Idle'])
save(abp)

patterns=[]
definitions=[
    ('Double','묵직한 2연타',['MM_Attack_01','MM_Attack_02'],[.7,.7],[.3,.3],[10,10],1),
    ('Triple','빠른 3연타',['MM_Attack_01','MM_Attack_02','MM_Attack_01'],[1.1,1.1,1.1],[.3,.3,.3],[8,8,12],1),
    ('Delayed','지연 마무리',['MM_Attack_01','MM_Attack_02','MM_ChargedAttack'],[1.1,1.1,.65],[.3,.3,1.0],[7,7,18],1)]
audio_registry=unreal.AssetRegistryHelpers.get_asset_registry()
audio={str(a.asset_name):a for a in audio_registry.get_assets_by_path('/Game/ExpeditionPortfolio/Audio',recursive=True)}
rock=audio.get('SFX_Hit_Magic_Rock_01')
rock_sound=rock.get_asset() if rock else None
for key,label,names,rates,times,damage,weight in definitions:
    attack=montage('AM_Golem'+key,clips[names[0]])
    pattern,new=create('DA_Golem'+key,unreal.EnemyAttackPatternData,unreal.DataAssetFactory(),'/Game/ExpeditionPortfolio/Combat/Patterns/StoneGolem')
    if new:
        assert unreal.EnemyAnimationEditorLibrary.build_combo_montage(attack,[clips[n] for n in names],rates,times,clips['MM_Idle'])
        pattern.set_editor_property('pattern_id','Golem'+key)
        pattern.set_editor_property('display_name',label)
        pattern.set_editor_property('attack_montage',attack)
        pattern.set_editor_property('selection_weight',weight)
        pattern.set_editor_property('telegraph_duration',.8)
        hits=[]
        cursor=.5
        for i,name in enumerate(names):
            entry=unreal.EnemyAttackHit()
            entry.set_editor_property('damage',damage[i])
            entry.set_editor_property('defense_window_duration',.45)
            entry.set_editor_property('parry_success_duration',.2)
            entry.set_editor_property('fallback_hit_time',cursor+times[i]/rates[i])
            entry.set_editor_property('impact_sound',rock_sound)
            hits.append(entry)
            cursor+=clips[name].get_play_length()/rates[i]
        pattern.set_editor_property('hits',hits)
        save(attack); save(pattern)
    patterns.append(pattern)
    assert not pattern.get_validation_error(),pattern.get_path_name()+': '+pattern.get_validation_error()
    unreal.log('GOLEM_PATTERN_VALID '+key+' '+str([h.get_editor_property('hit_time') for h in pattern.get_resolved_hits()]))

presentation,new=create('DA_GolemPresentation',unreal.EnemyPresentationData,unreal.DataAssetFactory())
if new:
    presentation.set_editor_property('approach_target',True)
    presentation.set_editor_property('approach_duration',.65)
    presentation.set_editor_property('return_duration',.7)
    presentation.set_editor_property('target_distance',205.)
    presentation.set_editor_property('walk_montage',walk)
    presentation.set_editor_property('hit_reaction',hit)
    presentation.set_editor_property('death',death)
    sound=unreal.QTESound()
    sound.set_editor_property('sound',rock_sound)
    sound.set_editor_property('volume',.4)
    presentation.set_editor_property('hit_sound',sound)
    save(presentation)

split_content=lib.does_asset_exist('/Game/ExpeditionPortfolio/Combat/Enemies/DA_StoneGolemVisual')
bp=load('/Game/ExpeditionPortfolio/Combat/Enemies/'+('BP_StoneGolem' if split_content else 'BP_PrototypeEnemy'))
needs_resize=False
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles=sub.k2_gather_subobject_data_for_blueprint(bp)
for handle in handles:
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj,unreal.SkeletalMeshComponent):
        unreal.log('GOLEM_OLD_MESH '+obj.get_name()+' '+str(obj.get_relative_transform()))
        obj.set_skinned_asset_and_update(target)
        obj.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_BLUEPRINT)
        obj.set_editor_property('anim_class',abp.generated_class())
        obj.set_editor_property('relative_location',unreal.Vector(0,0,-165))
    if isinstance(obj,unreal.CapsuleComponent):
        needs_resize=obj.get_unscaled_capsule_half_height()<164
        obj.set_capsule_size(90,165)
    if isinstance(obj,unreal.CombatantComponent):
        obj.set_editor_property('enemy_attack_patterns',patterns)
        obj.set_editor_property('combat_display_name','돌 골렘')
        if new: obj.set_editor_property('max_health',200.)
    if isinstance(obj,unreal.EnemyAttackPresentationComponent):
        obj.set_editor_property('presentation_data',presentation)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
save(bp)
if needs_resize:
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if actor.get_class()==bp.generated_class():
            location=actor.get_actor_location()
            location.z+=75
            actor.set_actor_location(location,False,False)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
# Keep the placed actor and stable encounter identity; expose a clearly named reusable child asset.
if not split_content:
    factory=unreal.BlueprintFactory()
    factory.set_editor_property('parent_class',bp.generated_class())
    golem_bp,created=create('BP_StoneGolem',unreal.Blueprint,factory,'/Game/ExpeditionPortfolio/Combat/Enemies')
    if created: unreal.BlueprintEditorLibrary.compile_blueprint(golem_bp); save(golem_bp)

# Enemy setup must not overwrite independently tuned shared parry feedback or character actions.
# See SetupCharacterActions.py for the one-time player/common-data migration.
unreal.log('STONE_GOLEM_SETUP_OK patterns='+str(len(patterns)))
