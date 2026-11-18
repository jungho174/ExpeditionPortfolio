"""Author a playable WhiteCastle bridge encounter and an open future-boss courtyard.

Run in one editor, outside PIE. Vendor assets and the original combat map stay intact.
Uses the local scenery mesh's cooked surface collision for architectural detail,
with simple supporting floors and boundaries aligned to visible geometry.
"""
import json, math
from pathlib import Path
import unreal

MAP = '/Game/ExpeditionPortfolio/Maps/L_WhiteCastle'
TEMPLATE = '/Game/WhiteCastle/Scenes/ExampleGathering'
SCALE = .75
FLOOR = 7919.531953
LIGHT_SCALE = 1.0 / 1024.0
SCENE_EV = 1.5
lib = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'End PIE before authoring the map'
if lib.does_asset_exist(MAP):
    assert levels.load_level(MAP)
else:
    assert levels.new_level_from_template(MAP, TEMPLATE)
world = editor.get_editor_world()
assert world.get_path_name().startswith(MAP + '.'), world.get_path_name()
existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
assert 'WC_GateEncounter_Group' not in existing, 'Grouped encounter already configured. Edit its Composition; this initial map builder would restore the retired single-enemy layout.'

def load(path):
    assert lib.does_asset_exist(path), path
    return lib.load_asset(path)

def actor(label, cls, position, rotation=(0, 0, 0), folder='WhiteCastle'):
    obj = existing.get(label)
    if not obj:
        obj = actors.spawn_actor_from_class(cls, unreal.Vector(*position))
        assert obj, label
        obj.set_actor_label(label)
        existing[label] = obj
    obj.set_actor_location(unreal.Vector(*position), False, True)
    obj.set_actor_rotation(unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), False)
    obj.set_folder_path(folder)
    return obj

def collision(component, profile):
    component.set_collision_profile_name(profile)
    component.set_editor_property('can_ever_affect_navigation', False)

cube = load('/Engine/BasicShapes/Cube')

def box(label, center, size, hidden=True, material=None, pawn_only=False):
    obj = actor(label, unreal.StaticMeshActor, center, folder='WhiteCastle/Collision' if hidden else 'WhiteCastle/Details')
    comp = obj.static_mesh_component
    comp.set_static_mesh(cube)
    obj.set_actor_scale3d(unreal.Vector(*(s / 100 for s in size)))
    comp.set_visibility(not hidden, True)
    collision(comp, 'BlockAll' if hidden else 'NoCollision')
    if pawn_only:
        comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionResponseType.ECR_IGNORE)
        comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_IGNORE)
    if material:
        comp.set_material(0, material)
    return obj

def module(label, name, center, scale=(1, 1, 1), yaw=0, bottom=False):
    mesh = load('/Game/WhiteCastle/Meshes/' + name)
    bounds = mesh.get_bounds()
    offset = bounds.origin * unreal.Vector(*scale)
    if bottom:
        offset.z = (bounds.origin.z - bounds.box_extent.z) * scale[2]
    rot = unreal.Rotator(pitch=0, yaw=yaw, roll=0)
    angle=math.radians(yaw)
    rotated=unreal.Vector(offset.x*math.cos(angle)-offset.y*math.sin(angle), offset.x*math.sin(angle)+offset.y*math.cos(angle), offset.z)
    origin = unreal.Vector(*center) - rotated
    obj = actor(label, unreal.StaticMeshActor, origin.to_tuple(), (0, yaw, 0), 'WhiteCastle/Courtyard')
    obj.set_actor_scale3d(unreal.Vector(*scale))
    obj.static_mesh_component.set_static_mesh(mesh)
    collision(obj.static_mesh_component, 'NoCollision')
    return obj

# Keep all geometry in the original pack; the level only contains placement and references.
scene = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)
             and a.static_mesh_component.static_mesh
             and (a.get_actor_label()=='WC_CastleExterior' or a.static_mesh_component.static_mesh.get_name() == 'SM_Scene'))
optimized='/Game/ExpeditionPortfolio/Environment/WhiteCastle/PrivateGeometry/SM_WhiteCastleScenery_Nanite'
if lib.does_asset_exist(optimized):
    scene.static_mesh_component.set_static_mesh(load(optimized))
scene.set_actor_label('WC_CastleExterior')
scene.set_folder_path('WhiteCastle/Architecture')
scene.set_actor_location(unreal.Vector(0, 10700 * SCALE, -FLOOR * SCALE), False, True)
scene.set_actor_scale3d(unreal.Vector(SCALE, SCALE, SCALE))
collision(scene.static_mesh_component, 'BlockAll')
scene.static_mesh_component.set_editor_property('visible_in_ray_tracing', False)
for obj in actors.get_all_level_actors():
    if obj.get_actor_label() in ['Floor', 'SM_SkySphere']:
        obj.set_actor_hidden_in_game(True)
        obj.set_actor_enable_collision(False)
        for comp in obj.get_components_by_class(unreal.PrimitiveComponent):
            comp.set_visibility(False, True)
    if isinstance(obj, unreal.PointLight):
        # These were decorative baked lights. Keep their relative placements in the new map.
        comp = obj.get_component_by_class(unreal.PointLightComponent)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        if not str(obj.get_actor_label()).startswith('WC_'):
            comp.set_intensity(comp.get_editor_property('intensity') * LIGHT_SCALE)
            pos = obj.get_actor_location()
            obj.set_actor_location(unreal.Vector((pos.x-2950)*SCALE, (pos.y+9500)*SCALE, (pos.z-FLOOR)*SCALE), False, True)
            obj.set_actor_label('WC_' + obj.get_actor_label())
        comp.set_editor_property('cast_shadows', False)
        obj.set_folder_path('WhiteCastle/Lighting')

# The road is lower than its side pavements. Invisible cubes provide inexpensive collision.
box('WC_BridgeRoadCollision', (0, 3300, -20), (400, 10200, 40))
for sign in [-1, 1]:
    box('WC_BridgePavementCollision_' + str(sign), (sign*495, 3300, 18.5), (590, 10200, 40))
    box('WC_BridgeRailingCollision_' + str(sign), (sign*800, 4400, 210), (70, 7800, 500), pawn_only=True)
    box('WC_GateWallCollision_' + str(sign), (sign*550, -200, 300), (650, 1800, 650), pawn_only=True)
box('WC_BridgeStartBoundary', (0, 8200, 240), (1700, 70, 550), pawn_only=True)
box('WC_GatePassageFloor', (0, -800, -20), (450, 2200, 40))
box('WC_InnerCrossingFloor', (2300, -1750, -50), (6100, 1500, 40))
box('WC_CourtyardFloorCollision', (4162.5, -3375, -49.67), (2380, 2380, 40))
box('WC_PalaceBoundary', (0, -4400, 350), (3400, 4400, 800), pawn_only=True)
box('WC_CrossingLeftBoundary', (-800, -1750, 300), (60, 1550, 700), pawn_only=True)
box('WC_CrossingFrontBoundary', (3050, -1000, 300), (5000, 60, 700), pawn_only=True)
box('WC_CrossingRearBoundary', (2350, -2570, 300), (1300, 60, 700), pawn_only=True)
box('WC_ArenaRightBoundary', (5375, -3125, 230), (60, 3300, 520))
box('WC_ArenaBackBoundary', (4162.5, -4590, 230), (2450, 60, 520))
box('WC_ArenaLeftBoundary', (2940, -3550, 300), (60, 2150, 700), pawn_only=True)
for label in ['WC_PalaceBoundary', 'WC_CrossingLeftBoundary', 'WC_CrossingFrontBoundary',
              'WC_CrossingRearBoundary', 'WC_ArenaLeftBoundary']:
    # Broad legacy boxes cross the actual stairs and open side passages.
    collision(existing[label].static_mesh_component, 'NoCollision')

# Project-owned, solid-color inlay material. No vendor geometry or textures are duplicated.
mat_path = '/Game/ExpeditionPortfolio/Environment/WhiteCastle/M_CourtyardGold'
if lib.does_asset_exist(mat_path):
    gold = load(mat_path)
else:
    gold = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CourtyardGold', '/Game/ExpeditionPortfolio/Environment/WhiteCastle', unreal.Material, unreal.MaterialFactoryNew())
    editing = unreal.MaterialEditingLibrary
    color = editing.create_material_expression(gold, unreal.MaterialExpressionConstant3Vector, -250, 0)
    color.set_editor_property('constant', unreal.LinearColor(.34, .20, .055, 1))
    metal = editing.create_material_expression(gold, unreal.MaterialExpressionConstant, -250, 110)
    metal.set_editor_property('r', .7)
    rough = editing.create_material_expression(gold, unreal.MaterialExpressionConstant, -250, 220)
    rough.set_editor_property('r', .4)
    editing.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    editing.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    editing.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    editing.recompile_material(gold)
    assert lib.save_loaded_asset(gold, only_if_is_dirty=False)

tile = 499.05137 * SCALE
for ix in range(6):
    for iy in range(6):
        module('WC_CourtPaving_%d_%d' % (ix, iy), 'SM_Floor_Plane_001',
               (4162.5 + (ix-2.5)*tile, -3375 + (iy-2.5)*tile, -27.5), (SCALE, SCALE, 1))
for distance, width in [(1075, 8), (1000, 3)]:
    for sign in [-1, 1]:
        box('WC_InlayX_%d_%d' % (distance, sign), (4162.5+sign*distance, -3375, -26.8), (width, 2*distance, 1), False, gold)
        box('WC_InlayY_%d_%d' % (distance, sign), (4162.5, -3375+sign*distance, -26.8), (2*distance, width, 1), False, gold)
for x in [3030, 5295]:
    for y in [-2240, -4510]:
        module('WC_CourtPillar_%d_%d' % (x,y), 'SM_Gat_WCArchElemCollumn2_Full', (x,y,-29.67), (.75,.75,.65), bottom=True)
for x in [3200, 5125]:
    module('WC_CourtBrazier_'+str(x), 'SM_Det_WCArchBraizer', (x,-2450,-29.67), (.8,.8,.8), bottom=True)
for i in range(9):
    x=4162.5+(i-4)*250
    y=-3375+(i-4)*250
    module('WC_CourtBackWall_'+str(i),'SM_Gat_WCArchElemWall_3',(x,-4570,-29.67),(1.01,1,.7),yaw=180,bottom=True)
    module('WC_CourtRightWall_'+str(i),'SM_Gat_WCArchElemWall_3',(5355,y,-29.67),(1.01,1,.7),yaw=90,bottom=True)
    if i%2==1:
        module('WC_CourtBackWindow_'+str(i),'SM_Gat_WCArchElemWindow1_Full',(x,-4520,40),(.85,1,.55),yaw=180,bottom=True)
        module('WC_CourtRightWindow_'+str(i),'SM_Gat_WCArchElemWindow1_Full',(5305,y,40),(.85,1,.55),yaw=90,bottom=True)
for i in range(4):
    module('WC_CourtBackPilaster_'+str(i),'SM_Gat_WCArchElemCollumn1_Full',(3037.5+i*750,-4510,-29.67),(.65,.65,.6),bottom=True)
    module('WC_CourtRightPilaster_'+str(i),'SM_Gat_WCArchElemCollumn1_Full',(5295,-4500+i*750,-29.67),(.65,.65,.6),bottom=True)

start = actor('WC_PlayerStart', unreal.PlayerStart, (0, 6400, 100), (0,-90,0), 'WhiteCastle/Gameplay')
golem_class = lib.load_blueprint_class('/Game/ExpeditionPortfolio/Combat/Enemies/BP_StoneGolem')
golem = actor('WC_GatekeeperGolem', golem_class, (0,950,165), (0,90,0), 'WhiteCastle/Gameplay')
retreat = actor('WC_GateRetreatPoint', unreal.TargetPoint, (0,2450,100), (0,-90,0), 'WhiteCastle/Gameplay')
trigger = actor('WC_GateEncounter', unreal.BattleEncounterTrigger, (0,1700,170), folder='WhiteCastle/Gameplay')
trigger.set_editor_property('enemy_actor', golem)
trigger.set_editor_property('retreat_point', retreat)
trigger.ensure_encounter_id()
trigger.get_editor_property('encounter_volume').set_box_extent(unreal.Vector(820,100,400), True)
actor('WC_FutureBossSpawn', unreal.TargetPoint, (4162.5,-3700,140), (0,90,0), 'WhiteCastle/FutureBoss')
actor('WC_FutureBossPlayerEntry', unreal.TargetPoint, (4162.5,-2700,100), (0,-90,0), 'WhiteCastle/FutureBoss')

world.get_world_settings().set_editor_property('default_game_mode', lib.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode'))
for obj in actors.get_all_level_actors():
    if isinstance(obj, unreal.DirectionalLight):
        light = obj.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_intensity(18000 * LIGHT_SCALE)
        light.set_editor_property('use_temperature', True)
        light.set_editor_property('temperature', 5400)
        light.set_editor_property('light_source_angle', 1.2)
        light.set_editor_property('atmosphere_sun_light', True)
        light.set_forward_shading_priority(1)
        obj.set_actor_rotation(unreal.Rotator(pitch=-35,yaw=-55,roll=0),False)
    if isinstance(obj, unreal.SkyLight):
        light = obj.get_component_by_class(unreal.SkyLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_intensity(1)
        light.set_editor_property('real_time_capture', True)
        light.recapture_sky()
    if isinstance(obj, unreal.ExponentialHeightFog):
        obj.set_actor_location(unreal.Vector(0,0,-450),False,True)
        fog = obj.get_component_by_class(unreal.ExponentialHeightFogComponent)
        fog.set_editor_property('fog_density', .004)
        fog.set_editor_property('fog_height_falloff', .25)
        fog.set_editor_property('start_distance', 2500)
        fog.set_editor_property('enable_volumetric_fog', True)
fill = actor('WC_CourtyardSkyFill', unreal.DirectionalLight, (0,0,1000), (0,0,0), 'WhiteCastle/Lighting')
fill.set_actor_rotation(unreal.Rotator(pitch=-60,yaw=125,roll=0),False)
fill_light=fill.get_component_by_class(unreal.DirectionalLightComponent)
fill_light.set_mobility(unreal.ComponentMobility.MOVABLE)
fill_light.set_intensity(2500 * LIGHT_SCALE)
fill_light.set_editor_property('cast_shadows',False)
fill_light.set_editor_property('atmosphere_sun_light',False)
fill_light.set_forward_shading_priority(0)
fill_light.set_editor_property('volumetric_scattering_intensity',0)
fill_light.set_light_color(unreal.LinearColor(.65,.77,1,1))
post = actor('WC_Exposure', unreal.PostProcessVolume, (0,0,0), folder='WhiteCastle/Lighting')
post.set_editor_property('unbound', True)
settings = post.get_editor_property('settings')
for name, value in [('auto_exposure_min_brightness',SCENE_EV), ('auto_exposure_max_brightness',SCENE_EV), ('auto_exposure_bias',0.), ('bloom_intensity',.2), ('motion_blur_amount',.15)]:
    settings.set_editor_property('override_'+name, True)
    settings.set_editor_property(name, value)
post.set_editor_property('settings', settings)
editor.set_level_viewport_camera_info(unreal.Vector(0,7000,750), unreal.Rotator(pitch=7,yaw=-90,roll=0))
assert levels.save_current_level()
layout = {'map':MAP, 'player_start':[0,6400,100], 'golem':[0,950,165], 'encounter':[0,1700,170],
          'retreat':[0,2450,100], 'courtyard':[4162.5,-3375,-29.67], 'boss_spawn':[4162.5,-3700,140],
          'courtyard_size':2250, 'encounter_id':unreal.GuidLibrary.conv_guid_to_string(trigger.get_editor_property('encounter_id'))}
Path(unreal.Paths.project_saved_dir()+'WhiteCastleLayout.json').write_text(json.dumps(layout,indent=2),encoding='utf-8')
unreal.log('WHITE_CASTLE_MAP_SAVED '+json.dumps(layout))
