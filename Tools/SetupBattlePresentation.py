"""Connect camera shots and strong-magic timing. Run outside PIE with other Unreal processes closed."""
import unreal

lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/ExpeditionPortfolio/Presentation'

def blueprint(name,parent,widget=False):
    path=root+'/'+name
    if lib.does_asset_exist(path):return lib.load_asset(path)
    factory=unreal.WidgetBlueprintFactory() if widget else unreal.BlueprintFactory()
    factory.set_editor_property('parent_class',parent)
    return tools.create_asset(name,root,unreal.WidgetBlueprint if widget else unreal.Blueprint,factory)

rig=blueprint('BP_BattleCameraRig',unreal.BattleCameraRig)
new_gauge=not lib.does_asset_exist(root+'/WBP_AttackTiming')
gauge=blueprint('WBP_AttackTiming',unreal.AttackTimingWidget,True)
assert rig and gauge
for bp in [rig,gauge]:unreal.BlueprintEditorLibrary.compile_blueprint(bp)
if new_gauge:
    gauge_defaults=unreal.get_default_object(gauge.generated_class())
    transform=gauge_defaults.get_editor_property('render_transform')
    transform.set_editor_property('translation',unreal.Vector2D(220,0))
    gauge_defaults.set_editor_property('render_transform',transform)
    unreal.BlueprintEditorLibrary.compile_blueprint(gauge)

path=root+'/DA_BattlePresentation'
first=not lib.does_asset_exist(path)
data=tools.create_asset('DA_BattlePresentation',root,unreal.BattlePresentationData,unreal.DataAssetFactory()) if first else lib.load_asset(path)
assert data
data.set_editor_property('camera_rig_class',rig.generated_class())
if first:
    def shot(offset,weight,height,fov):
        value=unreal.BattleCameraShot()
        value.set_editor_property('offset',unreal.Vector(*offset))
        value.set_editor_property('look_at_enemy_weight',weight)
        value.set_editor_property('look_height',height)
        value.set_editor_property('field_of_view',fov)
        return value
    data.set_editor_property('command_shot',shot((-260,180,120),.85,45,60))
    data.set_editor_property('preparation_shot',shot((340,200,190),0,70,70))
    data.set_editor_property('attack_shot',shot((-320,140,150),.75,40,65))
    data.set_editor_property('enemy_shot',shot((-340,200,180),.6,25,65))
    data.set_editor_property('intro_duration',.55)
    data.set_editor_property('blend_duration',.35)

pc=lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
cdo=unreal.get_default_object(pc.generated_class())
cdo.get_editor_property('battle_presentation').set_editor_property('presentation_data',data)
cdo.set_editor_property('attack_timing_widget_class',gauge.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(pc)

strong=lib.load_asset('/Game/ExpeditionPortfolio/Animation/Magic/DA_StrongMagic')
if first:
    strong.set_editor_property('enable_timing_bonus',True)
    strong.set_editor_property('timing_success_duration',.3)
    strong.set_editor_property('timing_damage_multiplier',1.5)

player=unreal.get_default_object(lib.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter'))
rotation=player.get_editor_property('mesh').get_editor_property('relative_rotation')
enemy=lib.load_asset('/Game/ExpeditionPortfolio/Combat/Enemies/BP_PrototypeEnemy')
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
found=False
for handle in sub.k2_gather_subobject_data_for_blueprint(enemy):
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj,unreal.SkeletalMeshComponent):
        obj.set_editor_property('relative_rotation',rotation)
        found=True
assert found
unreal.BlueprintEditorLibrary.compile_blueprint(enemy)
for asset in [rig,gauge,data,pc,strong,enemy]:
    assert lib.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()
unreal.log('BATTLE_PRESENTATION_SETUP_OK enemy_mesh_yaw='+str(rotation.yaw))
