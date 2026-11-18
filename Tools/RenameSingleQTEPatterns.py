"""User-requested names; preserve data and keep redirects for older references."""
import unreal
lib=unreal.EditorAssetLibrary
root='/Game/ExpeditionPortfolio/Combat/QTE/'
for old,new in [('DA_QTE_BasicMagic','DA_QTE_Single_Hard'),('DA_QTE_StrongMagic','DA_QTE_Single_Easy')]:
 if not lib.does_asset_exist(root+new):
  assert lib.does_asset_exist(root+old)
  assert lib.rename_asset(root+old,root+new)
 asset=lib.load_asset(root+new)
 assert asset and lib.save_loaded_asset(asset,only_if_is_dirty=False)
 for path in lib.find_package_referencers_for_asset(root+new,load_assets_to_confirm=True):
  ref=lib.load_asset(path)
  if ref:assert lib.save_loaded_asset(ref,only_if_is_dirty=True)
 unreal.log('SINGLE_QTE_RENAMED '+asset.get_path_name())
unreal.log('SINGLE_QTE_RENAME_OK')
