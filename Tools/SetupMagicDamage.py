"""Initialize existing spell damage once without overwriting later user tuning."""
import unreal

lib = unreal.EditorAssetLibrary
for name, damage in [('DA_BasicMagic', 25.0), ('DA_StrongMagic', 40.0)]:
    asset = lib.load_asset('/Game/ExpeditionPortfolio/Animation/Magic/' + name)
    assert asset, name
    if lib.get_metadata_tag(asset, 'MagicDamageInitialized') != '1':
        asset.set_editor_property('base_damage', damage)
        lib.set_metadata_tag(asset, 'MagicDamageInitialized', '1')
        assert lib.save_loaded_asset(asset, only_if_is_dirty=False), name
    unreal.log('MAGIC_DAMAGE_READY ' + name + '=' + str(asset.get_editor_property('base_damage')))
