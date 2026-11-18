"""Separate shared rules from actor/action content without replacing placed actors.

Run alone after building, outside PIE. Existing destination tuning is preserved.
"""
import json
import unreal

lib = unreal.EditorAssetLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
mage_folder = '/Game/ExpeditionPortfolio/Characters/PlayerMage/Combat'
enemy_folder = '/Game/ExpeditionPortfolio/Combat/Enemies'

def load(path):
    assert lib.does_asset_exist(path), path
    return lib.load_asset(path)

def save(obj):
    assert lib.save_loaded_asset(obj, only_if_is_dirty=False), obj.get_path_name()

def data(name, cls, folder):
    path = folder + '/' + name
    if lib.does_asset_exist(path):
        return load(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    obj = assets.create_asset(name, folder, cls, factory)
    assert obj, path
    return obj, True

def components(bp, cls):
    result = []
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
        if isinstance(obj, cls) and obj not in result:
            result.append(obj)
    return result

def one(bp, cls):
    found = components(bp, cls)
    assert len(found) == 1, (bp.get_name(), cls, [o.get_path_name() for o in found])
    return found[0]

def compile_bp(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

def anchor(socket='None', offset=(0, 0, 0), top=False, top_offset=0):
    value = unreal.CombatantAnchor()
    value.set_editor_property('socket', socket)
    value.set_editor_property('world_offset', unreal.Vector(*offset))
    value.set_editor_property('bounds_top_fallback', top)
    value.set_editor_property('bounds_top_offset', top_offset)
    return value

player_bp = load('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
stats = one(player_bp, unreal.CombatantComponent)
profile = stats.get_editor_property('character_actions')
assert profile and str(profile.get_editor_property('character_id')) != 'None'
# Different party definitions must not claim the same stable identity. Reusing a
# definition on transient enemy actors does not create extra character identities.
definition_ids = {}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for entry in registry.get_assets_by_path('/Game/ExpeditionPortfolio/Characters', recursive=True):
    if str(entry.asset_class_path.asset_name) != 'CharacterCombatData':
        continue
    definition = entry.get_asset()
    identifier = str(definition.get_editor_property('character_id'))
    assert identifier != 'None', definition.get_path_name()
    assert identifier not in definition_ids, (identifier, definition_ids.get(identifier), definition.get_path_name())
    definition_ids[identifier] = definition.get_path_name()
actions = [profile.get_editor_property('basic_attack')] + list(profile.get_editor_property('skills')) + [profile.get_editor_property('parry_counter')]
damage_before = [a.get_editor_property('base_damage') for a in actions]
utility_source = load('/Game/ExpeditionPortfolio/Animation/Commands/DA_PlayerUtilityPresentation')
command_source = load('/Game/ExpeditionPortfolio/Combat/Commands/DA_PlayerCommands')
controller_bp = load('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
controller = unreal.get_default_object(controller_bp.generated_class())
presentation_component = controller.get_editor_property('battle_presentation')
camera_source = presentation_component.get_editor_property('presentation_data')
feedback = presentation_component.get_editor_property('parry_feedback')
numbers = controller.get_editor_property('combat_number_data')
hud = unreal.get_default_object(load('/Game/ExpeditionPortfolio/UI/WBP_CombatHUD').generated_class())

for cls, name in [(unreal.MagicPresentationComponent, 'MagicPresentation'),
                  (unreal.PlayerUtilityPresentationComponent, 'UtilityPresentation')]:
    assert unreal.CombatCompositionEditorLibrary.ensure_action_executor(player_bp, cls, name), name
compile_bp(player_bp)
stats = one(player_bp, unreal.CombatantComponent)
utility = one(player_bp, unreal.PlayerUtilityPresentationComponent)
utility.set_editor_property('presentation_data', None)

def setup_visual(name, folder, is_mage):
    visual, new = data(name, unreal.CombatantVisualData, folder)
    if new:
        number_offset = numbers.get_editor_property('world_offset')
        visual.set_editor_property('damage_number', anchor(numbers.get_editor_property('anchor_bone'), (number_offset.x, number_offset.y, number_offset.z)))
        visual.set_editor_property('target_marker', anchor('head', (0, 0, 30), True, 28))
        visual.set_editor_property('aim', anchor('spine_03'))
        if is_mage:
            visual.set_editor_property('parry_intermediate', anchor(feedback.get_editor_property('intermediate').get_editor_property('socket')))
            visual.set_editor_property('parry_complete', anchor(feedback.get_editor_property('complete').get_editor_property('socket')))
            visual.set_editor_property('menu_motion_bone', hud.get_editor_property('menu_motion_bone'))
            visual.set_editor_property('menu_reference_bone', 'pelvis')
            visual.set_editor_property('menu_world_offset', unreal.Vector(0, 0, 30))
            visual.set_editor_property('menu_screen_offset', hud.get_editor_property('menu_screen_offset'))
        save(visual)
    return visual

mage_visual = setup_visual('DA_PlayerMageVisual', mage_folder, True)
stats.set_editor_property('visual_data', mage_visual)
stats.set_editor_property('persistence', unreal.CombatantPersistence.UNIQUE_PARTY_MEMBER)
stats.set_editor_property('persistent_character_id', 'None')
stats.set_editor_property('player_command_data', None)

item, new = data('DA_HealingPotion', unreal.CombatItemData, '/Game/ExpeditionPortfolio/Combat/Items')
if new:
    item.set_editor_property('item_id', 'HealingPotion')
    item.set_editor_property('display_name', '회복 물약')
    item.set_editor_property('description', '아군 한 명의 체력을 회복합니다.')
    item.set_editor_property('healing_amount', command_source.get_editor_property('potion_healing'))
    item.set_editor_property('effect', utility_source.get_editor_property('potion_effect'))
    sound = unreal.QTESound()
    for dest, src in [('sound', 'potion_sound'), ('volume', 'potion_sound_volume'),
                      ('delay', 'potion_sound_delay'), ('start_time', 'potion_sound_start_time')]:
        sound.set_editor_property(dest, utility_source.get_editor_property(src))
    item.set_editor_property('use_sound', sound)
    save(item)

rules, new = data('DA_BattleRules', unreal.BattleRulesData, '/Game/ExpeditionPortfolio/Combat/Rules')
if new:
    rules.set_editor_property('turn_transition_delay', command_source.get_editor_property('turn_transition_delay'))
    stock = unreal.CombatItemStock()
    stock.set_editor_property('item', item)
    stock.set_editor_property('quantity', command_source.get_editor_property('potions_per_encounter'))
    rules.set_editor_property('initial_items', [stock])
    save(rules)
item_use, new = data('DA_PlayerMageItemUse', unreal.CharacterItemUseData, mage_folder)
if new:
    item_use.set_editor_property('apply_time', utility_source.get_editor_property('potion_apply_time'))
    item_use.set_editor_property('duration', utility_source.get_editor_property('potion_duration'))
    save(item_use)
if not profile.get_editor_property('item_use_presentation'):
    profile.set_editor_property('item_use_presentation', item_use)
save(profile)
mode = load('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode')
unreal.get_default_object(mode.generated_class()).set_editor_property('battle_rules', rules)
compile_bp(mode)
save(mode)

for index, action in enumerate(actions):
    camera, new = data('DA_' + action.get_name().removeprefix('DA_') + 'Camera', unreal.ActionCameraData, mage_folder + '/Cameras')
    if new:
        camera.set_editor_property('preparation_shot', camera_source.get_editor_property('preparation_shot'))
        camera.set_editor_property('execution_shot', camera_source.get_editor_property('enemy_shot' if index == len(actions)-1 else 'attack_shot'))
        save(camera)
    if not action.get_editor_property('camera_data'):
        action.set_editor_property('camera_data', camera)
    if isinstance(action, unreal.MagicActionData):
        action.set_editor_property('counter_hit_reaction', None)
    save(action)
compile_bp(player_bp)
save(player_bp)
assert damage_before == [a.get_editor_property('base_damage') for a in actions]

legacy = load(enemy_folder + '/BP_PrototypeEnemy')
species = load(enemy_folder + '/BP_StoneGolem')
old_stats = one(legacy, unreal.CombatantComponent)
old_enemy_presentation = one(legacy, unreal.EnemyAttackPresentationComponent)
enemy_snapshot = {p: old_stats.get_editor_property(p) for p in ['max_health', 'combat_display_name', 'enemy_attack_patterns']}
enemy_snapshot['enemy_attack_patterns'] = list(enemy_snapshot['enemy_attack_patterns'])
enemy_presentation = old_enemy_presentation.get_editor_property('presentation_data')
mesh_before = one(legacy, unreal.SkeletalMeshComponent).get_skinned_asset().get_path_name()
golem_visual = setup_visual('DA_StoneGolemVisual', enemy_folder, False)
assert unreal.CombatCompositionEditorLibrary.split_enemy_content(legacy, species)
species_cdo = unreal.get_default_object(species.generated_class())
species_stats = species_cdo.get_editor_property('combatant')
for prop, value in enemy_snapshot.items():
    species_stats.set_editor_property(prop, value)
species_stats.set_editor_property('visual_data', golem_visual)
species_stats.set_editor_property('persistence', unreal.CombatantPersistence.TRANSIENT)
species_cdo.get_editor_property('enemy_presentation').set_editor_property('presentation_data', enemy_presentation)
compile_bp(species)
compile_bp(legacy)
assert one(legacy, unreal.SkeletalMeshComponent).get_skinned_asset().get_path_name() == mesh_before
assert one(legacy, unreal.CombatantComponent).get_editor_property('max_health') == enemy_snapshot['max_health']
save(species)
save(legacy)

# The original map actor and trigger remain untouched. Blueprint inheritance supplies the new defaults.
unreal.log('SHARED_OWNERSHIP_MIGRATION_OK ' + json.dumps({
    'character_id': str(profile.get_editor_property('character_id')),
    'damage': damage_before, 'golem_health': enemy_snapshot['max_health'],
    'patterns': [p.get_path_name() for p in enemy_snapshot['enemy_attack_patterns']],
    'item_healing': item.get_editor_property('healing_amount'),
    'item_count': [(str(s.get_editor_property('item').get_editor_property('item_id')), s.get_editor_property('quantity')) for s in rules.get_editor_property('initial_items')],
    'turn_delay': rules.get_editor_property('turn_transition_delay'),
    'mage_executors': [o.get_name() for o in components(player_bp, unreal.ActorComponent) if isinstance(o, (unreal.MagicPresentationComponent, unreal.PlayerUtilityPresentationComponent, unreal.ItemPresentationComponent))]
}, ensure_ascii=False))
