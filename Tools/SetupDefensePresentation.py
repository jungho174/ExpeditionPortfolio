"""Create project-owned defense montages and bind player presentation data; preserve vendor assets."""
import unreal

lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Animation/Defense'
def save(asset):
    if not lib.save_loaded_asset(asset,only_if_is_dirty=False): raise RuntimeError('Cannot save '+asset.get_path_name())
def montage(source,name,lock_root=False):
    sequence_path=root+'/'+source.rsplit('/',1)[-1]
    if not lib.does_asset_exist(sequence_path):
        assert lib.duplicate_asset(source,sequence_path)
    sequence=lib.load_asset(sequence_path)
    if lock_root:
        sequence.set_editor_property('enable_root_motion',False)
        sequence.set_editor_property('force_root_lock',True)
    # Full guard/dodge poses and additive hit poses share the player's compatible bone hierarchy.
    skeleton.add_compatible_skeleton(sequence.get_editor_property('skeleton'))
    path=root+'/'+name
    if lib.does_asset_exist(path): result=lib.load_asset(path)
    else:
        factory=unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton',sequence.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation',sequence)
        result=tools.create_asset(name,root,unreal.AnimMontage,factory)
    assert result
    save(sequence)
    save(result)
    unreal.log('DEFENSE_ASSET '+result.get_path_name()+' length='+str(result.get_play_length()))
    return result

bp=lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
cdo=unreal.get_default_object(bp.generated_class())
skeleton=cdo.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
parry=montage('/Game/CombatMagicAnims/Animations/AS_MagicDefend','AM_ParryGuard',True)
dodge=montage('/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash','AM_DefenseDodge',True)
hit=montage('/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01','AM_UnarmedHitReaction')
path=root+'/DA_PlayerDefensePresentation'
if lib.does_asset_exist(path): data=lib.load_asset(path)
else:
    data=tools.create_asset('DA_PlayerDefensePresentation',root,unreal.DefensePresentationData,unreal.DataAssetFactory())
    data.set_editor_property('dodge_pose_time',0.05)
assert data
data.set_editor_property('parry_montage',parry)
data.set_editor_property('dodge_montage',dodge)
data.set_editor_property('hit_reaction_montage',hit)
cdo.get_editor_property('defense_presentation').set_editor_property('presentation_data',data)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for asset in [skeleton,data,bp]:save(asset)
unreal.log('DEFENSE_PRESENTATION_SETUP_OK')
