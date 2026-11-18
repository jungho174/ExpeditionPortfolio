"""Match WhiteCastle lighting/exposure to the existing combat VFX brightness range.

Run in the owning editor, outside PIE. Preserve layout, light colors and shadows.
The EV delta sets the light multiplier, so rerunning at the target EV is harmless.
"""
import json
from pathlib import Path
import unreal

TARGET_EV = 1.5
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'End PIE before saving lighting changes'
world = editor.get_editor_world()
assert world.get_path_name().startswith('/Game/ExpeditionPortfolio/Maps/L_WhiteCastle.'), world.get_path_name()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rigs = [actor for actor in actors if isinstance(actor, unreal.EnvironmentLightingRig)]
if rigs:
    # The profile owns lighting after the shared setup; never recalibrate it twice.
    assert len(rigs) == 1, 'Resolve duplicate lighting rigs before applying a profile'
    assert rigs[0].apply_profile(), rigs[0].get_validation_issues()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.log('WHITE_CASTLE_COMMON_LIGHTING_PROFILE_APPLIED')
else:
    post = next(a for a in actors if a.get_actor_label() == 'WC_Exposure')
    settings = post.get_editor_property('settings')
    old_ev = settings.get_editor_property('auto_exposure_min_brightness')
    assert abs(old_ev - settings.get_editor_property('auto_exposure_max_brightness')) < .001
    factor = 2.0 ** (TARGET_EV - old_ev)
    changes = []
    with unreal.ScopedEditorTransaction('Match WhiteCastle lighting to combat VFX'):
        for actor in actors:
            if isinstance(actor, (unreal.DirectionalLight, unreal.PointLight, unreal.SpotLight, unreal.RectLight)):
                light = actor.get_component_by_class(unreal.LightComponent)
                old_intensity = light.get_editor_property('intensity')
                light.modify()
                light.set_intensity(old_intensity * factor)
                changes.append({'label':actor.get_actor_label(), 'before':old_intensity, 'after':light.get_editor_property('intensity')})
        post.modify()
        for name in ['auto_exposure_min_brightness', 'auto_exposure_max_brightness']:
            settings.set_editor_property('override_' + name, True)
            settings.set_editor_property(name, TARGET_EV)
        post.set_editor_property('settings', settings)
        # Skylight captures the scaled sun/sky; its capture multiplier stays unchanged.
        for actor in actors:
            if isinstance(actor, unreal.SkyLight):
                actor.get_component_by_class(unreal.SkyLightComponent).recapture_sky()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report = {'old_ev':old_ev, 'new_ev':TARGET_EV, 'light_factor':factor, 'lights':changes}
    Path(unreal.Paths.project_saved_dir() + 'WhiteCastleLightingChange.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('WHITE_CASTLE_LIGHTING_SAVED ' + json.dumps(report))
