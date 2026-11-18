"""Connect the approved single-hit enemy animation and validate the committed HUD layout.

Preserves existing combat tuning on reruns. TXT_DefenseResult is authored in WBP_CombatHUD.
"""
import unreal

lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Animation/Enemy'
sequence_path=root+'/AS_Spell_Cast_Attack'
if not lib.does_asset_exist(sequence_path):
    assert lib.duplicate_asset('/Game/CombatMagicAnims/Animations/AS_Spell_Cast_Attack',sequence_path)
sequence=lib.load_asset(sequence_path)
path=root+'/AM_EnemySpellAttack'
created=not lib.does_asset_exist(path)
if created:
    factory=unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton',sequence.get_editor_property('skeleton'))
    factory.set_editor_property('source_animation',sequence)
    montage=tools.create_asset('AM_EnemySpellAttack',root,unreal.AnimMontage,factory)
    unreal.AnimationLibrary.add_animation_notify_track(montage,'EnemyHit')
    unreal.AnimationLibrary.add_animation_notify_event(montage,'EnemyHit',4.7,unreal.AnimNotify_EnemyHit)
else:
    montage=lib.load_asset(path)
assert montage
pattern=lib.load_asset('/Game/ExpeditionPortfolio/Combat/Patterns/DA_EnemyBasicAttack')
pattern.set_editor_property('attack_montage',montage)
if created:
    pattern.set_editor_property('fallback_hit_time',4.7)
    pattern.set_editor_property('hit_time_offset',0.0)

bp=lib.load_asset('/Game/ExpeditionPortfolio/Combat/Enemies/BP_PrototypeEnemy')
player=unreal.get_default_object(lib.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter'))
player_mesh=player.get_editor_property('mesh')
skeleton=player_mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
skeleton.add_compatible_skeleton(sequence.get_editor_property('skeleton'))
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles=sub.k2_gather_subobject_data_for_blueprint(bp)
has_presentation=False
for handle in handles:
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj,unreal.EnemyAttackPresentationComponent): has_presentation=True
    if isinstance(obj,unreal.SkeletalMeshComponent):
        obj.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_BLUEPRINT)
        obj.set_editor_property('anim_class',player_mesh.get_editor_property('anim_class'))
if not has_presentation:
    params=unreal.AddNewSubobjectParams(parent_handle=handles[0],new_class=unreal.EnemyAttackPresentationComponent,blueprint_context=bp)
    handle,reason=sub.add_new_subobject(params)
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if not isinstance(obj,unreal.EnemyAttackPresentationComponent): raise RuntimeError('Cannot add enemy presentation: '+str(reason))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)

hud=lib.load_asset('/Game/ExpeditionPortfolio/UI/WBP_CombatHUD')
tree=unreal.find_object(hud,'WidgetTree')
if not tree: raise RuntimeError('HUD source WidgetTree not found')
canvas=next((w for w in unreal.ObjectIterator(unreal.CanvasPanel) if w.get_outer()==tree and w.get_parent() is None),None)
if not isinstance(canvas,unreal.CanvasPanel): raise RuntimeError('HUD root must be the existing CanvasPanel')
existing=next((w for w in unreal.ObjectIterator(unreal.TextBlock) if w.get_outer()==tree and w.get_name()=='TXT_DefenseResult'),None)
if not existing:
    raise RuntimeError('The committed HUD layout must contain TXT_DefenseResult; author it in the UMG Designer before compiling')
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
for asset in [sequence,montage,pattern,skeleton,bp,hud]:
    if not lib.save_loaded_asset(asset,only_if_is_dirty=False): raise RuntimeError('Cannot save '+asset.get_path_name())
unreal.log('ENEMY_PRESENTATION_SETUP_OK '+path)
