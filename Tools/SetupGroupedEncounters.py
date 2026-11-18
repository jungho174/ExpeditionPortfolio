"""Author editable group presets and migrate WhiteCastle's single gate encounter.

Run in one editor outside PIE, after a successful Editor build. The encounter ID,
retreat settings, existing environment and all user-tuned combat data are preserved.
"""
import json
from pathlib import Path
import unreal

lib=unreal.EditorAssetLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
root='/Game/ExpeditionPortfolio'
folder=root+'/Exploration/Encounters'

def save(asset):
    assert lib.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def create(name,folder,cls,factory):
    path=folder+'/'+name
    existing=lib.does_asset_exist(path)
    asset=unreal.load_asset(path) if existing else assets.create_asset(name,folder,cls,factory)
    assert asset,path
    return asset,not existing

golem=lib.load_blueprint_class(root+'/Combat/Enemies/BP_StoneGolem')
assert golem
presets=[]
for count in [2,3]:
    data,new=create('DA_Encounter_%dEnemies'%count,folder,unreal.EncounterGroupData,unreal.DataAssetFactory())
    if new:
        entries=[]
        for index in range(count):
            member=unreal.EncounterGroupMember()
            member.set_editor_property('enemy_class',golem)
            member.set_editor_property('relative_transform',unreal.Transform(location=unreal.Vector(750,(index-(count-1)/2)*490,-5),rotation=unreal.Rotator(pitch=0,yaw=180,roll=0)))
            entries.append(member)
        data.set_editor_property('members',entries)
    assert data.is_valid_composition()
    save(data)
    factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.BattleEncounterGroup)
    bp,new=create('BP_Encounter_%dEnemies'%count,folder,unreal.Blueprint,factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    defaults=unreal.get_default_object(bp.generated_class())
    if new or not defaults.get_editor_property('composition'):
        defaults.set_editor_property('composition',data)
        defaults.get_editor_property('encounter_volume').set_box_extent(unreal.Vector(100,820,400),False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    presets.append((data,bp))

appearance,new=create('DA_CombatHealthHUD',root+'/UI',unreal.CombatHealthHUDData,unreal.DataAssetFactory())
if new:
    font=unreal.SlateFontInfo()
    font.set_editor_property('font_object',unreal.load_asset('/Engine/EngineFonts/Roboto'))
    font.set_editor_property('typeface_font_name','Bold');font.set_editor_property('size',23)
    appearance.set_editor_property('health_font',font)
save(appearance)
hud=unreal.load_asset(root+'/UI/WBP_CombatHUD')
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
assert unreal.CombatMenuEditorLibrary.configure_party_hud(hud,appearance)
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
assert unreal.CombatMenuEditorLibrary.configure_party_hud(hud,appearance)
save(hud)

presentation=unreal.load_asset(root+'/Presentation/DA_BattlePresentation')
if lib.get_metadata_tag(presentation,'GroupTargetFraming')!='1':
    shot=presentation.get_editor_property('target_selection_shot')
    shot.set_editor_property('fit_target_bounds',True)
    presentation.set_editor_property('target_selection_shot',shot)
    lib.set_metadata_tag(presentation,'GroupTargetFraming','1')
    save(presentation)

assert levels.load_level(root+'/Maps/L_WhiteCastle')
current={a.get_actor_label():a for a in actors.get_all_level_actors()}
group=current.get('WC_GateEncounter_Group')
if not group:
    old=current['WC_GateEncounter'];old_enemy=current['WC_GatekeeperGolem']
    identity=old.get_editor_property('encounter_id')
    expected_id=unreal.GuidLibrary.conv_guid_to_string(identity)
    location=old.get_actor_location()
    group=actors.spawn_actor_from_class(presets[0][1].generated_class(),location,unreal.Rotator(pitch=0,yaw=-90,roll=0))
    group.set_actor_label('WC_GateEncounter_Group')
    group.set_folder_path('WhiteCastle/Gameplay')
    group.set_editor_property('encounter_id',identity)
    group.set_editor_property('retreat_point',old.get_editor_property('retreat_point'))
    group.set_editor_property('retreat_distance',old.get_editor_property('retreat_distance'))
    group.set_editor_property('composition',presets[0][0])
    group.get_editor_property('encounter_volume').set_box_extent(unreal.Vector(100,820,400),False)
    group.rebuild_preview()
    assert len(group.get_encounter_enemies())==2
    assert unreal.GuidLibrary.conv_guid_to_string(group.get_editor_property('encounter_id'))==expected_id
    assert actors.destroy_actor(old_enemy)
    assert actors.destroy_actor(old)
else:
    if not group.get_editor_property('composition'):
        group.set_editor_property('composition',presets[0][0])
    group.rebuild_preview()
assert len(group.get_encounter_enemies())==2
for label in ['WC_GateEncounter','WC_GatekeeperGolem']:
    old_actor=next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)
    if old_actor and old_actor!=group:
        assert actors.destroy_actor(old_actor)
assert levels.save_current_level()
report={'id':unreal.GuidLibrary.conv_guid_to_string(group.get_editor_property('encounter_id')),
        'group':group.get_path_name(),'members':[a.get_path_name() for a in group.get_encounter_enemies()],
        'presets':[bp.get_path_name() for data,bp in presets],'hud':appearance.get_path_name()}
Path(unreal.Paths.project_saved_dir()+'GroupedEncountersSetup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('GROUPED_ENCOUNTERS_SETUP_OK '+json.dumps(report))
