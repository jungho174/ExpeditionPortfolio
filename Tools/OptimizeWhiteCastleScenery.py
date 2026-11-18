"""Create a local Nanite derivative; never modify the purchased source mesh."""
import unreal
lib=unreal.EditorAssetLibrary
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
folder='/Game/ExpeditionPortfolio/Environment/WhiteCastle/PrivateGeometry'
path=folder+'/SM_WhiteCastleScenery_Nanite'
if lib.does_asset_exist(path):
    mesh=lib.load_asset(path)
else:
    mesh=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SM_WhiteCastleScenery_Nanite',folder,lib.load_asset('/Game/WhiteCastle/Meshes/SM_Scene'))
    assert mesh
mesh.set_editor_property('support_ray_tracing',False)
sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
settings=sub.get_nanite_settings(mesh)
# Nanite may automatically change material usage flags. Keep those flags on local
# material copies rather than saving changes into the purchased pack.
materials={}
def local_material(source):
    if not source or not source.get_path_name().startswith('/Game/WhiteCastle/'):
        return source
    key=source.get_path_name()
    if key in materials:return materials[key]
    dest=folder+'/Materials/'+source.get_name()+'_Nanite'
    material=lib.load_asset(dest) if lib.does_asset_exist(dest) else unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(source.get_name()+'_Nanite',folder+'/Materials',source)
    assert material
    if isinstance(material,unreal.Material):material.set_editor_property('used_with_nanite',True)
    assert lib.save_loaded_asset(material,only_if_is_dirty=False)
    materials[key]=material
    return material
slots=list(mesh.get_editor_property('static_materials'))
for slot in slots:slot.set_editor_property('material_interface',local_material(slot.get_editor_property('material_interface')))
mesh.set_editor_property('static_materials',slots)
if not settings.get_editor_property('enabled'):
    settings.set_editor_property('enabled',True)
    sub.set_nanite_settings(mesh,settings,True)
assert lib.save_loaded_asset(mesh,only_if_is_dirty=False)
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label()=='WC_CastleExterior':
        a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_editor_property('visible_in_ray_tracing',False)
        for i in range(a.static_mesh_component.get_num_materials()):
            a.static_mesh_component.set_material(i,local_material(a.static_mesh_component.get_material(i)))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
original_materials=[p for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name().startswith('/Game/WhiteCastle/Materials/')]
if original_materials:
    reloaded,error=unreal.EditorLoadingAndSavingUtils.reload_packages(original_materials,unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    assert reloaded and not str(error),str(error)
unreal.log('WHITE_CASTLE_NANITE_SAVED source_unchanged=1 ray_tracing_geometry=0')
