"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Creates only the baseline pattern and connects the prototype enemy's component.
Existing pattern values are preserved on reruns; a different existing list is rejected.
"""
import unreal


def setup():
    path = "/Game/ExpeditionPortfolio/Combat/Patterns/DA_EnemyBasicAttack"
    pattern = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if pattern is None:
        pattern = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_EnemyBasicAttack",
            "/Game/ExpeditionPortfolio/Combat/Patterns",
            unreal.EnemyAttackPatternData,
            unreal.DataAssetFactory(),
        )
        if pattern is None:
            raise RuntimeError("Cannot create baseline enemy pattern")
    if not isinstance(pattern, unreal.EnemyAttackPatternData):
        raise RuntimeError("Existing baseline asset has a different class")

    bp = unreal.EditorAssetLibrary.load_asset(
        "/Game/ExpeditionPortfolio/Combat/Enemies/BP_PrototypeEnemy"
    )
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    components = []
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if isinstance(obj, unreal.CombatantComponent):
            components.append(obj)
    if len(components) != 1:
        raise RuntimeError("Expected exactly one CombatantComponent: " + str(components))
    component = components[0]
    current = list(component.get_editor_property("enemy_attack_patterns"))
    if current and current != [pattern]:
        raise RuntimeError("Refusing to replace an existing enemy pattern list")
    component.set_editor_property("enemy_attack_patterns", [pattern])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_loaded_asset(pattern, only_if_is_dirty=False):
        raise RuntimeError("Could not save pattern")
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError("Could not save enemy blueprint")
    # Reload its component data after compiling to ensure the template retained the list.
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    verified = False
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if isinstance(obj, unreal.CombatantComponent):
            verified = list(obj.get_editor_property("enemy_attack_patterns")) == [pattern]
    if not verified:
        raise RuntimeError("Compiled component did not retain the pattern reference")
    # Verify the placed enemy inherits the saved template; do not save the level.
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level.load_level("/Game/ThirdPerson/Lvl_ThirdPerson"):
        raise RuntimeError("Could not load the current combat test level")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    enemy_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/ExpeditionPortfolio/Combat/Enemies/BP_PrototypeEnemy"
    )
    placed_count = 0
    for actor in actors:
        if actor.get_class() == enemy_class:
            placed_count += 1
            instance = actor.get_component_by_class(unreal.CombatantComponent)
            if instance is None or list(instance.get_editor_property("enemy_attack_patterns")) != [pattern]:
                raise RuntimeError("Placed enemy has a different pattern list: " + actor.get_name())
    if placed_count == 0:
        raise RuntimeError("No prototype enemy found in current test level")
    unreal.log("ENEMY_PATTERN_LEVEL_OK count=" + str(placed_count))
    unreal.log("ENEMY_PATTERN_SETUP_OK " + path)


setup()
