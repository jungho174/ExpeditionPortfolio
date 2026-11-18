"""Configure only the weak magic's separate projectile/impact; preserve original vendor assets."""
import unreal

lib = unreal.EditorAssetLibrary
root = '/Game/ExpeditionPortfolio/Animation/Magic/VFX'
def copy(name):
    path = root + '/' + name
    if not lib.does_asset_exist(path):
        assert lib.duplicate_asset('/Game/Mixed_Magic_VFX_Pack/VFX/Sperate_VFX/' + name, path)
    return lib.load_asset(path)

new_projectile = not lib.does_asset_exist(root + '/NS_Magma_Shot_Projectile')
projectile = copy('NS_Magma_Shot_Projectile')
impact = copy('NS_Magma_Shot_Impact')
reference = lib.load_asset('/Game/Mixed_Magic_VFX_Pack/VFX/Sperate_VFX/NS_Magma_Shot_Projectile')
assert unreal.MagicPresentationEditorLibrary.prepare_projectile_system(projectile, reference)
if new_projectile:
    projectile.set_editor_property('warmup_tick_delta', 1.0 / 30.0)
    projectile.set_editor_property('warmup_time', 0.2)
basic = lib.load_asset('/Game/ExpeditionPortfolio/Animation/Magic/DA_BasicMagic')
first_setup = not basic.get_editor_property('use_projectile')
basic.set_editor_property('use_projectile', True)
basic.set_editor_property('attack_effect', projectile)
basic.set_editor_property('impact_effect', impact)
basic.set_editor_property('attack_effect_on_hit', True)
if first_setup:
    basic.set_editor_property('projectile_speed', 500.0)
    basic.set_editor_property('impact_effect_transform', unreal.Transform(scale=unreal.Vector(0.5,0.5,0.5)))
for asset in [projectile, impact, basic]:
    assert lib.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log('MAGIC_PROJECTILE_SETUP_OK')
