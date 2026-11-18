"""Create a short project-owned magic counter; preserve existing defense tuning."""
import unreal

lib = unreal.EditorAssetLibrary
root = '/Game/ExpeditionPortfolio/Animation/Defense'
path = root + '/DA_ParryCounter'
if not lib.does_asset_exist(path):
    assert lib.duplicate_asset('/Game/ExpeditionPortfolio/Animation/Magic/DA_BasicMagic', path)
    data = lib.load_asset(path)
    montage_path = root + '/AM_ParryCounter'
    if not lib.does_asset_exist(montage_path):
        assert lib.duplicate_asset('/Game/ExpeditionPortfolio/Animation/Magic/AM_BasicMagic', montage_path)
    montage = lib.load_asset(montage_path)
    for prop in ['blend_in', 'blend_out']:
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', 0.06)
        montage.set_editor_property(prop, blend)
    data.set_editor_property('attack_montage', montage)
    data.set_editor_property('charge_montage', None)
    data.set_editor_property('base_damage', 15.0)
    data.set_editor_property('attack_start_time', 0.65)
    data.set_editor_property('attack_play_rate', 2.0)
    data.set_editor_property('enable_timing_bonus', False)
    data.set_editor_property('projectile_speed', 1800.0)
    data.set_editor_property('max_flight_duration', 0.6)
    data.set_editor_property('attack_effect_tail_duration', 0.15)
    data.set_editor_property('counter_hit_reaction', lib.load_asset(root + '/AM_UnarmedHitReaction'))
    transform = data.get_editor_property('attack_effect_transform')
    transform.set_editor_property('scale3d', unreal.Vector(0.3, 0.3, 0.3))
    data.set_editor_property('attack_effect_transform', transform)
    impact = data.get_editor_property('impact_effect_transform')
    impact.set_editor_property('scale3d', unreal.Vector(0.3, 0.3, 0.3))
    data.set_editor_property('impact_effect_transform', impact)
    for asset in [montage, data]:
        assert lib.save_loaded_asset(asset, only_if_is_dirty=False)
else:
    data = lib.load_asset(path)
bp = lib.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
cdo = unreal.get_default_object(bp.generated_class())
cdo.get_editor_property('magic_presentation').set_editor_property('counter_magic', data)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert lib.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log('PARRY_COUNTER_SETUP_OK')

import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('SetupParryImpact.py')), run_name='__main__')
