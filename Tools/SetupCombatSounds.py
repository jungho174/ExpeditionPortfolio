"""Assign requested one-shot combat sounds once. Preserve subsequent DA tuning."""
import unreal

lib = unreal.EditorAssetLibrary
entries = [
    ('/Game/ExpeditionPortfolio/Animation/Commands/DA_PlayerUtilityPresentation', 'potion_sound',
     '/Game/ExpeditionPortfolio/Audio/SFX/FeedBack/Item/SFX_FB_Heal_Tint_Light_01'),
    ('/Game/ExpeditionPortfolio/Animation/Magic/DA_BasicMagic', 'attack_sound',
     '/Game/ExpeditionPortfolio/Audio/SFX/FeedBack/Buff_Debuff/SFX_FB_Effect_Burn_Activate_01'),
    ('/Game/ExpeditionPortfolio/Animation/Magic/DA_StrongMagic', 'attack_sound',
     '/Game/ExpeditionPortfolio/Audio/SFX/FeedBack/Buff_Debuff/SFX_FB_Effect_Burn_Deactivate_01'),
]
for path, field, sound_path in entries:
    assert lib.does_asset_exist(path) and lib.does_asset_exist(sound_path), (path, sound_path)
    data, sound = lib.load_asset(path), lib.load_asset(sound_path)
    assert isinstance(sound, unreal.SoundBase), sound_path
    if lib.get_metadata_tag(data, 'CombatSoundSetupVersion') != '1':
        data.set_editor_property(field, sound)
        lib.set_metadata_tag(data, 'CombatSoundSetupVersion', '1')
        assert lib.save_loaded_asset(data, only_if_is_dirty=False)
    assigned = data.get_editor_property(field)
    unreal.log('COMBAT_SOUND_ASSIGNED ' + data.get_name() + ' ' + field + '=' + str(assigned))
unreal.log('COMBAT_SOUNDS_SETUP_OK')
