"""Update mage skill descriptions while preserving combat tuning and category labels."""
import unreal
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not ed.get_game_world()
lib=unreal.EditorAssetLibrary
for name,description in [('DA_PlayerMageFireSkill','파이어볼'),('DA_PlayerMageIceSkill','블리자드')]:
 a=lib.load_asset('/Game/ExpeditionPortfolio/Characters/PlayerMage/Combat/'+name)
 assert a
 before=(str(a.get_editor_property('display_name')),a.get_editor_property('base_damage'),a.get_editor_property('uses_per_encounter'))
 a.set_editor_property('description',description)
 assert lib.save_loaded_asset(a,only_if_is_dirty=False)
 assert str(a.get_editor_property('description'))==description
 assert before==(str(a.get_editor_property('display_name')),a.get_editor_property('base_damage'),a.get_editor_property('uses_per_encounter'))
 unreal.log('SKILL_DESCRIPTION_SAVED '+a.get_name()+' '+description+' '+str(before))
unreal.log('SKILL_DESCRIPTIONS_UPDATE_OK')
