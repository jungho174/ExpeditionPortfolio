"""Create project-owned magic animations, montages and presentation data. No vendor assets are edited.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>, with
PythonScriptPlugin and EditorScriptingUtilities enabled for that invocation.
"""
import unreal

LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = '/Game/ExpeditionPortfolio/Animation/Magic'


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def load_or_create(name, cls, factory):
    asset_path = ROOT + '/' + name
    if LIB.does_asset_exist(asset_path):
        return LIB.load_asset(asset_path)
    asset = TOOLS.create_asset(name, ROOT, cls, factory)
    if not asset:
        raise RuntimeError('Could not create ' + asset_path)
    return asset


def montage(source_name, name, hit_time=None):
    sequence_path = ROOT + '/' + source_name
    if not LIB.does_asset_exist(sequence_path):
        if not LIB.duplicate_asset('/Game/CombatMagicAnims/Animations/' + source_name, sequence_path):
            raise RuntimeError('Could not duplicate ' + source_name)
    sequence = LIB.load_asset(sequence_path)
    # The imported and player meshes were audited: same 89 bones and parent hierarchy.
    # Skeleton is read-only on animation assets. Register compatibility on the player skeleton.
    source_skeleton = sequence.get_editor_property('skeleton')
    skeleton.add_compatible_skeleton(source_skeleton)
    save(skeleton)
    save(sequence)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', source_skeleton)
    factory.set_editor_property('source_animation', sequence)
    result = load_or_create(name, unreal.AnimMontage, factory)
    if hit_time is not None:
        if 'MagicHit' in [str(n) for n in unreal.AnimationLibrary.get_animation_notify_track_names(result)]:
            unreal.AnimationLibrary.remove_animation_notify_track(result, 'MagicHit')
        unreal.AnimationLibrary.add_animation_notify_track(result, 'MagicHit')
        unreal.AnimationLibrary.add_animation_notify_event(result, 'MagicHit', hit_time, unreal.AnimNotify_MagicHit)
    save(result)
    unreal.log('MAGIC_MONTAGE ' + name + ' length=' + str(result.get_play_length()))
    return result


bp = LIB.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
cdo = unreal.get_default_object(bp.generated_class())
mesh = cdo.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset')
skeleton = mesh.get_editor_property('skeleton')
basic_montage = montage('AS_SpellAttack', 'AM_BasicMagic', 1.0)
charge_montage = montage('AS_LevitatingFireballCharge', 'AM_StrongMagicCharge')
attack_montage = montage('AS_LevitatingMagicAttack', 'AM_StrongMagicAttack', 0.8)

basic = load_or_create('DA_BasicMagic', unreal.MagicActionData, unreal.DataAssetFactory())
basic.set_editor_property('attack_montage', basic_montage)
basic.set_editor_property('attack_effect', LIB.load_asset('/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magma_Shot'))
basic.set_editor_property('cast_socket', 'hand_l')
basic.set_editor_property('attack_effect_on_hit', True)
basic.set_editor_property('attack_effect_tail_duration', 1.0)
basic.set_editor_property('attack_effect_transform', unreal.Transform(
    scale=unreal.Vector(0.5, 0.5, 0.5)))
save(basic)

strong = load_or_create('DA_StrongMagic', unreal.MagicActionData, unreal.DataAssetFactory())
strong.set_editor_property('charge_montage', charge_montage)
strong.set_editor_property('charge_duration', 2.5)
strong.set_editor_property('attack_montage', attack_montage)
strong.set_editor_property('charge_effect', LIB.load_asset('/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magic_Bubbles_Owner_Cast_Spell'))
strong.set_editor_property('attack_effect', LIB.load_asset('/Game/Mixed_Magic_VFX_Pack/VFX/NS_Ice_Mist'))
strong.set_editor_property('cast_socket', 'spine_03')
strong.set_editor_property('charge_effect_between_hands', True)
strong.set_editor_property('attack_effect_on_hit', False)
strong.set_editor_property('charge_effect_transform', unreal.Transform(
    scale=unreal.Vector(0.5, 0.5, 0.5)))
strong.set_editor_property('attack_effect_at_target', False)
strong.set_editor_property('attack_effect_aim_at_target', True)
strong.set_editor_property('attack_effect_transform', unreal.Transform(
    location=unreal.Vector(40, 0, 0), scale=unreal.Vector(0.6, 0.6, 0.6)))
save(strong)

presentation = cdo.get_editor_property('magic_presentation')
presentation.set_editor_property('basic_magic', basic)
presentation.set_editor_property('strong_magic', strong)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
save(bp)
unreal.log('MAGIC_SETUP_OK')

import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('SetupMagicProjectile.py')), run_name='__main__')
runpy.run_path(str(Path(__file__).with_name('SetupMagicDamage.py')), run_name='__main__')
