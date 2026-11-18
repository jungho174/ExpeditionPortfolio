"""Create project-owned masked fade variants; vendor materials and live appearances stay unchanged."""
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
lib=unreal.EditorAssetLibrary;edit=unreal.MaterialEditingLibrary
root='/Game/ExpeditionPortfolio/Rendering/Enemies/StoneGolem'
source='/Game/Stone_Golem/materials/M_Stone_Golem'
path=root+'/M_StoneGolemDeathFade'
new=not lib.does_asset_exist(path)
mat=lib.duplicate_asset(source,path) if new else lib.load_asset(path)
assert mat and not mat.get_editor_property('use_material_attributes'),'Review material attribute layout before authoring fade'
if lib.get_metadata_tag(mat,'EnemyDeathFadeVersion')!='1':
    assert not edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_OPACITY_MASK),'Preserve existing opacity mask: manual review required'
    opacity=edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,800,650)
    opacity.set_editor_property('parameter_name','DeathOpacity');opacity.set_editor_property('default_value',1.)
    opacity.set_editor_property('group','Death Fade')
    dither=edit.create_material_expression(mat,unreal.MaterialExpressionMaterialFunctionCall,1050,650)
    assert dither.set_material_function(lib.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
    names=list(edit.get_material_expression_input_names(dither));unreal.log('DEATH_FADE_INPUTS '+str(names))
    alpha=next(n for n in names if 'Alpha' in n)
    assert edit.connect_material_expressions(opacity,'',dither,alpha)
    assert edit.connect_material_property(dither,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    assert not edit.recompile_material(mat)
    lib.set_metadata_tag(mat,'EnemyDeathFadeVersion','1')
assert lib.save_loaded_asset(mat,only_if_is_dirty=False)
instance_path=root+'/MI_StoneGolemDeathFade'
if not lib.does_asset_exist(instance_path):
    instance=lib.duplicate_asset('/Game/Stone_Golem/materials/MI_Stone_Golem_Inst',instance_path)
    assert instance;edit.set_material_instance_parent(instance,mat)
else:instance=lib.load_asset(instance_path)
overrides=instance.get_editor_property('base_property_overrides')
overrides.set_editor_property('override_blend_mode',True)
overrides.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
instance.set_editor_property('base_property_overrides',overrides)
edit.set_material_instance_scalar_parameter_value(instance,'DeathOpacity',1.)
assert edit.get_material_instance_scalar_parameter_value(instance,'DeathOpacity')==1.
edit.update_material_instance(instance);assert lib.save_loaded_asset(instance,only_if_is_dirty=False)
data=lib.load_asset('/Game/ExpeditionPortfolio/Animation/StoneGolem/DA_GolemPresentation')
if lib.get_metadata_tag(data,'EnemyDeathFadeVersion')!='1':
    data.set_editor_property('death_fade_materials',[instance])
    data.set_editor_property('death_fade_parameter','DeathOpacity')
    data.set_editor_property('death_fade_duration',.8)
    lib.set_metadata_tag(data,'EnemyDeathFadeVersion','1')
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
unreal.log('ENEMY_DEATH_FADE_SETUP_OK duration='+str(data.get_editor_property('death_fade_duration')))
