"""Prepare only VFX referenced by character profiles, parry feedback and items.

Does not rebuild maps or lighting profiles. Run outside PIE in one editor.
"""
from pathlib import Path
import runpy
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'End PIE before authoring'
tools=runpy.run_path(str(Path(__file__).with_name('SetupCommonRendering.py')),run_name='rendering_tools')
bindings,systems,changes=tools['setup_vfx']()
unreal.log('ACTIVE_VFX_PREPARED Bindings=%d Systems=%d UpdatedSlots=%d'%(len(bindings),len(systems),len(changes)))
