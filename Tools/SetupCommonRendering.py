"""Build common lighting profiles/templates and project-local exposure-aware VFX.

Run in one editor outside PIE, after the Editor target has been built. Existing
profile/instance tuning is preserved on reruns; vendor assets are never edited.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = '/Game/ExpeditionPortfolio/Rendering'
PRIVATE = ROOT + '/PrivateVFX'
MF_PATH = ROOT + '/Materials/MF_VFXExposure'
lib = unreal.EditorAssetLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
native = unreal.VFXExposureEditorLibrary


def load(path):
    assert lib.does_asset_exist(path), path
    result = unreal.load_asset(path)
    assert result, path
    return result


def save(asset):
    assert lib.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def create(name, folder, cls, factory):
    path = folder + '/' + name
    if lib.does_asset_exist(path):
        return load(path), False
    result = assets.create_asset(name, folder, cls, factory)
    assert result, path
    return result, True


def blueprint(name, parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/ExpeditionPortfolio.' + parent))
    bp, new = create(name, ROOT + '/Lighting', unreal.Blueprint, factory)
    if new:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
    return lib.load_blueprint_class(bp.get_path_name().split('.')[0])


def discover_bindings():
    bindings = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    def class_assets(name):
        entries = registry.get_assets_by_class(unreal.TopLevelAssetPath('/Script/ExpeditionPortfolio', name), True)
        return [entry.get_asset() for entry in entries if str(entry.package_name).startswith('/Game/ExpeditionPortfolio/')]
    for profile in class_assets('CharacterCombatData'):
        actions = [profile.get_editor_property('basic_attack'), profile.get_editor_property('parry_counter')] + list(profile.get_editor_property('skills'))
        for action in actions:
            if not isinstance(action, unreal.MagicActionData):
                continue
            for field in ['charge_effect', 'attack_effect', 'impact_effect']:
                effect = action.get_editor_property(field)
                if effect:
                    bindings.append((action, field, None, effect))
    for feedback in class_assets('ParryFeedbackData'):
        for field in ['intermediate', 'complete']:
            effect = feedback.get_editor_property(field).get_editor_property('effect')
            if effect:
                bindings.append((feedback, field, 'effect', effect))
    for item in class_assets('CombatItemData'):
        effect = item.get_editor_property('effect')
        if effect:
            bindings.append((item, 'effect', None, effect))
    # A shared action may appear in several profiles.
    return list({(owner.get_path_name(), field, nested): (owner, field, nested, fx)
                 for owner, field, nested, fx in bindings}.values())


def setup_vfx():
    function, new = create('MF_VFXExposure', ROOT + '/Materials', unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    assert native.initialize_exposure_function(function)
    save(function)
    material_cache = {}
    changes = []
    def adjusted_material(source, strength):
        original = lib.get_metadata_tag(source, 'RenderingSource') if source.get_path_name().startswith(PRIVATE + '/') else ''
        if original:
            source = load(original)
        key = (source.get_path_name(), strength)
        if key in material_cache:
            return material_cache[key]
        status = native.get_material_exposure_status(source)
        if status in ['common', 'existing-inverse', 'no-direct-emissive']:
            material_cache[key] = source
            return source
        assert status == 'uncompensated', 'Review material before conversion: ' + source.get_path_name() + ' ' + status
        suffix = hashlib.sha1((source.get_path_name() + str(strength)).encode()).hexdigest()[:8]
        name = source.get_name() + '_Exposure_' + suffix
        path = PRIVATE + '/Materials/' + name
        if lib.does_asset_exist(path):
            result = load(path)
        elif isinstance(source, unreal.MaterialInstanceConstant):
            parent = adjusted_material(source.get_editor_property('parent'), strength)
            result = assets.duplicate_asset(name, PRIVATE + '/Materials', source)
            assert result
            unreal.MaterialEditingLibrary.set_material_instance_parent(result, parent)
            unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(result, 'VFXExposureStrength', strength)
            unreal.MaterialEditingLibrary.update_material_instance(result)
        elif isinstance(source, unreal.Material):
            result = assets.duplicate_asset(name, PRIVATE + '/Materials', source)
            assert result
            assert native.add_material_exposure(result, function) in ['added', 'common']
        else:
            raise RuntimeError('Unsupported material: ' + source.get_path_name())
        lib.set_metadata_tag(result, 'RenderingSource', source.get_path_name())
        save(result)
        material_cache[key] = result
        return result
    systems = {}
    bindings = discover_bindings()
    for owner, field, nested, current in bindings:
        source_path = lib.get_metadata_tag(current, 'RenderingSource') if current.get_path_name().startswith(PRIVATE + '/') else ''
        source = load(source_path) if source_path else current
        key = source.get_path_name()
        if key not in systems:
            suffix = hashlib.sha1(key.encode()).hexdigest()[:8]
            name = source.get_name() + '_Exposure_' + suffix
            path = PRIVATE + '/Systems/' + name
            existing = lib.does_asset_exist(path)
            effect = load(path) if existing else assets.duplicate_asset(name, PRIVATE + '/Systems', source)
            assert effect
            if not existing or lib.get_metadata_tag(effect, 'ExposureVersion') != '3':
                slots = native.get_vfx_material_slots(effect)
                assert slots, 'No material slots: ' + key
                for slot in slots:
                    # Smoke keeps part of the environment response. Only emissive is wrapped;
                    # opacity/base color/normal and particle behavior are preserved.
                    label = (slot.emitter_name + ' ' + slot.material.get_name()).lower()
                    # Colored ice/charge/healing mist is magical emission. Neutral
                    # fire smoke keeps environmental response instead of glowing at night.
                    strength = .35 if 'magma' in key.lower() and any(word in label for word in ['smoke', 'dust']) else 1.0
                    material = adjusted_material(slot.material, strength)
                    if material != slot.material:
                        assert native.set_vfx_material_slot(effect, slot.emitter_index, slot.renderer_index, slot.material_slot, material)
                    changes.append({'system':key, 'emitter':slot.emitter_name, 'material':slot.material.get_path_name(), 'adjusted':material.get_path_name(), 'strength':strength})
                assert native.finalize_vfx_materials(effect)
                lib.set_metadata_tag(effect, 'RenderingSource', key)
                lib.set_metadata_tag(effect, 'ExposureVersion', '3')
                save(effect)
            systems[key] = effect
        effect = systems[key]
        if nested:
            value = owner.get_editor_property(field)
            value.set_editor_property(nested, effect)
            owner.set_editor_property(field, value)
        else:
            owner.set_editor_property(field, effect)
        save(owner)
    return bindings, systems, changes


def setup_profiles():
    profiles = []
    definitions = [
        ('DA_Lighting_Outdoor', {}),
        ('DA_Lighting_Dim', {'min_ev100':0., 'max_ev100':0., 'sun_intensity':.6, 'fill_intensity':.2, 'sky_intensity':.5}),
        ('DA_Lighting_BrightHDR', {'min_ev100':11.5, 'max_ev100':11.5, 'sun_intensity':18000., 'fill_intensity':2500.}),
    ]
    for name, values in definitions:
        profile, new = create(name, ROOT + '/Lighting/Profiles', unreal.EnvironmentLightingProfile, unreal.DataAssetFactory())
        if new:
            for key, value in values.items():
                profile.set_editor_property(key, value)
            save(profile)
        assert profile.is_valid_profile(), profile.get_path_name()
        profiles.append(profile)
    return profiles


def spawn(label, cls, location, folder='Lighting'):
    existing = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
    if existing:
        return existing
    obj = actors.spawn_actor_from_class(cls, unreal.Vector(*location))
    assert obj, label
    obj.set_actor_label(label)
    obj.set_folder_path(folder)
    return obj


def connect_rig(rig_class, profile, sun, fill, sky, post):
    rig = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.EnvironmentLightingRig)), None)
    if rig is None:
        rig = spawn('EP_LightingRig', rig_class, (0,0,0))
    elif rig.get_editor_property('profile'):
        profile = rig.get_editor_property('profile')
    for field, value in [('sun_light',sun), ('fill_light',fill), ('sky_light',sky), ('exposure_volume',post), ('profile',profile)]:
        rig.set_editor_property(field, value)
    assert rig.apply_profile()
    issues = list(rig.get_validation_issues())
    assert not issues, issues
    return rig


def make_reference_material(name, color):
    mat, new = create(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if new:
        node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -200,0)
        node.set_editor_property('constant', unreal.LinearColor(color,color,color,1))
        unreal.MaterialEditingLibrary.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        save(mat)
    return mat


def setup_template(path, rig_class, preview_class, profiles, systems, validation=False):
    is_new = not lib.does_asset_exist(path)
    assert levels.new_level(path) if is_new else levels.load_level(path)
    if is_new:
        floor = spawn('ReferenceFloor', unreal.StaticMeshActor, (0,0,-20), 'Environment')
        floor.static_mesh_component.set_static_mesh(load('/Engine/BasicShapes/Cube'))
        floor.set_actor_scale3d(unreal.Vector(50,50,.4))
        floor.static_mesh_component.set_material(0,make_reference_material('M_ReferenceGrey',.18))
        floor.static_mesh_component.set_collision_profile_name('BlockAll')
        start = spawn('PlayerStart', unreal.PlayerStart, (-650,0,100), 'Gameplay')
        start.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
        spawn('SkyAtmosphere',unreal.SkyAtmosphere,(0,0,0))
        fog=spawn('HeightFog',unreal.ExponentialHeightFog,(0,0,-100))
        fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property('fog_density',.002)
    sun=spawn('SunLight',unreal.DirectionalLight,(0,0,500))
    fill=spawn('FillLight',unreal.DirectionalLight,(0,0,500))
    sky=spawn('SkyLight',unreal.SkyLight,(0,0,500))
    post=spawn('Exposure',unreal.PostProcessVolume,(0,0,0))
    if is_new:
        sun.set_actor_rotation(unreal.Rotator(pitch=-35,yaw=-55,roll=0),False)
        fill.set_actor_rotation(unreal.Rotator(pitch=-60,yaw=125,roll=0),False)
        for a in [sun,fill]:a.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
        sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('atmosphere_sun_light',True)
        sun.get_component_by_class(unreal.DirectionalLightComponent).set_forward_shading_priority(1)
        fill.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('cast_shadows',False)
        fill.get_component_by_class(unreal.DirectionalLightComponent).set_light_color(unreal.LinearColor(.65,.77,1,1))
        sky.get_component_by_class(unreal.SkyLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property('real_time_capture',True)
    rig=connect_rig(rig_class,profiles[0],sun,fill,sky,post)
    if validation:
        preview=spawn('VFX_Validation',preview_class,(0,0,0),'Validation')
        preview.set_editor_property('lighting_rig',rig)
        preview.set_editor_property('lighting_profiles',profiles)
        entries=[]
        for name, effect in sorted(systems.items()):
            entry=unreal.VFXValidationEntry()
            entry.set_editor_property('name',unreal.Text(effect.get_name().split('_Exposure_')[0]))
            entry.set_editor_property('effect',effect)
            transform=unreal.Transform()
            transform.set_editor_property('translation',unreal.Vector(0,0,130))
            transform.set_editor_property('scale3d',unreal.Vector(.5,.5,.5))
            entry.set_editor_property('transform',transform)
            entries.append(entry)
        preview.set_editor_property('effects',entries)
        if is_new:
            for i,(label,color) in enumerate([('Black',.01),('Grey',.18),('White',.8)]):
                panel=spawn('Reference'+label,unreal.StaticMeshActor,(600,(i-1)*500,250),'Validation')
                panel.static_mesh_component.set_static_mesh(load('/Engine/BasicShapes/Cube'))
                panel.set_actor_scale3d(unreal.Vector(.2,5,5))
                panel.static_mesh_component.set_material(0,make_reference_material('M_Reference'+label,color))
    editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode',lib.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode'))
    assert levels.save_current_level()


def setup():
    assert not editor.get_game_world(),'End PIE before authoring'
    profiles=setup_profiles()
    rig_class=blueprint('BP_LevelLightingRig','EnvironmentLightingRig')
    preview_class=blueprint('BP_VFXValidation','VFXValidationActor')
    bindings,systems,changes=setup_vfx()
    setup_template('/Game/ExpeditionPortfolio/Maps/Templates/L_Template_Exploration',rig_class,preview_class,profiles,systems)
    setup_template('/Game/ExpeditionPortfolio/Maps/L_VFXValidation',rig_class,preview_class,profiles,systems,True)
    assert levels.load_level('/Game/ExpeditionPortfolio/Maps/L_WhiteCastle')
    current={a.get_actor_label():a for a in actors.get_all_level_actors()}
    connect_rig(rig_class,profiles[0],current['DirectionalLight'],current['WC_CourtyardSkyFill'],next(a for a in current.values() if isinstance(a,unreal.SkyLight)),current['WC_Exposure'])
    assert levels.save_current_level()
    report={'bindings':[{'owner':o.get_path_name(),'field':f,'nested':n,'source':s.get_path_name()} for o,f,n,s in bindings],
            'systems':{k:v.get_path_name() for k,v in systems.items()},'materials':changes}
    Path(unreal.Paths.project_saved_dir()+'CommonRenderingSetup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    original_dirty=[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if not p.get_name().startswith('/Game/ExpeditionPortfolio/')]
    assert not original_dirty,original_dirty
    unreal.log('COMMON_RENDERING_SETUP_OK '+json.dumps({'systems':len(systems),'bindings':len(bindings),'materials':len(changes)}))


if __name__=='__main__':setup()
