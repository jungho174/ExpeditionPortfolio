"""Bind the original floating-number font and editable styling; preserve user tuning."""
import unreal

lib=unreal.EditorAssetLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio'
font_path=root+'/UI/Fonts/LibreCaslonDisplay-Regular_Font'
font=lib.load_asset(font_path) if lib.does_asset_exist(font_path) else None
assert font,'Expected composite font: '+font_path
path=root+'/Presentation/DA_CombatNumbers'
exists=lib.does_asset_exist(path)
data=lib.load_asset(path) if exists else assets.create_asset('DA_CombatNumbers',root+'/Presentation',unreal.CombatNumberData,unreal.DataAssetFactory())
assert data
if not exists or not data.get_editor_property('font').get_editor_property('font_object'):
    info=data.get_editor_property('font')
    info.set_editor_property('font_object',font)
    info.set_editor_property('typeface_font_name','Default')
    info.set_editor_property('size',54)
    outline=info.get_editor_property('outline_settings')
    outline.set_editor_property('outline_size',1)
    outline.set_editor_property('outline_color',unreal.LinearColor(0.08,0.06,0.035,0.65))
    info.set_editor_property('outline_settings',outline)
    data.set_editor_property('font',info)
assert lib.save_loaded_asset(data,only_if_is_dirty=False)
pc=lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
cdo=unreal.get_default_object(pc.generated_class())
if not cdo.get_editor_property('combat_number_data'):
    cdo.set_editor_property('combat_number_data',data)
unreal.BlueprintEditorLibrary.compile_blueprint(pc)
assert lib.save_loaded_asset(pc,only_if_is_dirty=False)
unreal.log('COMBAT_NUMBERS_SETUP_OK font='+font.get_path_name()+' data='+data.get_path_name())
