"""Repair WhiteCastle traversal without resetting manually edited actor placements.

Run in the owning editor, outside PIE. The project-local scenery derivative already
has complex-as-simple collision; use its cooked surface instead of broad blockers.
"""
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'End PIE before fixing map collision'
world = editor.get_editor_world()
assert world.get_path_name().startswith('/Game/ExpeditionPortfolio/Maps/L_WhiteCastle.'), world.get_path_name()
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
scene = actors['WC_CastleExterior'].static_mesh_component
mesh = scene.static_mesh
assert mesh.get_path_name().startswith('/Game/ExpeditionPortfolio/Environment/WhiteCastle/PrivateGeometry/'), mesh.get_path_name()
assert unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_collision_complexity(mesh) == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
scene.set_collision_profile_name('BlockAll')
scene.set_editor_property('can_ever_affect_navigation', False)

# These coarse boxes cut across visible doorways, the staircase and open floors.
# Preserve the actors, disabling only their obsolete collision.
for label in ['WC_PalaceBoundary', 'WC_CrossingLeftBoundary',
              'WC_CrossingFrontBoundary', 'WC_CrossingRearBoundary',
              'WC_ArenaLeftBoundary']:
    actors[label].static_mesh_component.set_collision_profile_name('NoCollision')

assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log('WHITE_CASTLE_TRAVERSAL_FIXED SurfaceCollision=ComplexAsSimple ObsoleteBlockers=5')
