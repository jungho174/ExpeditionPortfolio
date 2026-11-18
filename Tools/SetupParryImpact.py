"""Bind short parry impact assets once; retain later user tuning."""
import unreal
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/Animation/Defense'
data=lib.load_asset(root+'/DA_ParryCounter')
assert data
if lib.get_metadata_tag(data,'ParryImpactInitialized')!='1':
    effect_path=root+'/NS_ParryImpact'
    if not lib.does_asset_exist(effect_path):
        assert lib.duplicate_asset('/Game/SlashTrail_SoftTofu/Niagara/Scifi/NS_Hit_Scifi_Once',effect_path)
    data.set_editor_property('parry_impact_effect',lib.load_asset(effect_path))
    data.set_editor_property('parry_impact_sound',lib.load_asset('/Game/SlashTrail_SoftTofu/Resource/Audio/Hit/SW_Hit_01'))
    shake_path=root+'/BP_ParryImpactShake'
    if not lib.does_asset_exist(shake_path):
        assert lib.duplicate_asset('/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player',shake_path)
    shake=lib.load_asset(shake_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(shake)
    data.set_editor_property('parry_camera_shake',shake.generated_class())
    data.set_editor_property('parry_slow_duration',0.15)
    data.set_editor_property('parry_slow_factor',0.15)
    lib.set_metadata_tag(data,'ParryImpactInitialized','1')
    for asset in [lib.load_asset(effect_path),shake,data]:
        assert lib.save_loaded_asset(asset,only_if_is_dirty=False)
unreal.log('PARRY_IMPACT_SETUP_OK')

import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('SetupParrySound.py')), run_name='__main__')
