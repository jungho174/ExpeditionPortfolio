"""Author the command HUD in its existing Blueprint; preserve existing graphs and tuning."""
import unreal

lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def save(asset):
    assert lib.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()

def data_asset(root, name, cls):
    path = root + '/' + name
    if lib.does_asset_exist(path):
        asset = lib.load_asset(path)
        assert isinstance(asset, cls), path
        return asset
    return tools.create_asset(name, root, cls, unreal.DataAssetFactory())

root = '/Game/ExpeditionPortfolio/Animation/Commands'
sequence_path = root + '/MM_Attack_01'
if not lib.does_asset_exist(sequence_path):
    assert lib.duplicate_asset('/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01', sequence_path)
sequence = lib.load_asset(sequence_path)
sequence.set_editor_property('enable_root_motion', False)
sequence.set_editor_property('force_root_lock', True)
montage_path = root + '/AM_MeleeAttack'
if lib.does_asset_exist(montage_path):
    montage = lib.load_asset(montage_path)
else:
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', sequence.get_editor_property('skeleton'))
    factory.set_editor_property('source_animation', sequence)
    montage = tools.create_asset('AM_MeleeAttack', root, unreal.AnimMontage, factory)
assert montage
unreal.log('MELEE_MONTAGE length=' + str(montage.get_play_length()))
presentation = data_asset(root, 'DA_PlayerUtilityPresentation', unreal.PlayerUtilityPresentationData)
presentation.set_editor_property('melee_montage', montage)
for direction, prop in [('Fwd', 'approach_montage'), ('Bwd', 'return_montage')]:
    if lib.get_metadata_tag(presentation, 'MeleeDashSetupVersion') == '1':
        continue
    name = 'MF_Unarmed_Jog_' + direction
    dest = root + '/' + name
    if not lib.does_asset_exist(dest):
        assert lib.duplicate_asset('/Game/Characters/Mannequins/Anims/Unarmed/Jog/' + name, dest)
    travel = lib.load_asset(dest)
    travel.set_editor_property('enable_root_motion', False)
    travel.set_editor_property('force_root_lock', True)
    path = root + '/AM_CommandJog' + direction
    if lib.does_asset_exist(path):
        travel_montage = lib.load_asset(path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton', travel.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation', travel)
        travel_montage = tools.create_asset('AM_CommandJog' + direction, root, unreal.AnimMontage, factory)
    presentation.set_editor_property(prop, travel_montage)
    save(travel)
    save(travel_montage)
presentation.set_editor_property('potion_effect', lib.load_asset('/Game/Mixed_Magic_VFX_Pack/VFX/NS_Potion'))
rules = data_asset('/Game/ExpeditionPortfolio/Combat/Commands', 'DA_PlayerCommands', unreal.PlayerCommandData)
player = lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
cdo = unreal.get_default_object(player.generated_class())
cdo.get_editor_property('utility_presentation').set_editor_property('presentation_data', presentation)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
found = False
for handle in sub.k2_gather_subobject_data_for_blueprint(player):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj, unreal.CombatantComponent):
        obj.set_editor_property('player_command_data', rules)
        found = True
assert found, 'Player CombatantComponent missing'
unreal.BlueprintEditorLibrary.compile_blueprint(player)
hud = lib.load_asset('/Game/ExpeditionPortfolio/UI/WBP_CombatHUD')
tree = unreal.find_object(hud, 'WidgetTree')
for obj in unreal.ObjectIterator(unreal.Widget):
    if obj.get_outer() == tree:
        unreal.log('HUD_WIDGET ' + obj.get_name() + ' parent=' + str(obj.get_parent()))
assert unreal.CombatMenuEditorLibrary.build_command_layout(hud)
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
for asset in [sequence, montage, presentation, rules, player, hud]:
    save(asset)
unreal.log('COMBAT_COMMAND_MENU_SETUP_OK')

# Apply the same antialiased background when authoring a fresh command layout.
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('SetupCombatMenuBackground.py')), run_name='__main__')
