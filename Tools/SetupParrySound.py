"""Build an original short, low-bodied parry impact and bind it without touching pack audio."""
import math
import random
import struct
import wave
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
source = project / 'SourceArt' / 'Audio' / 'ParryImpact_Tight.wav'
source.parent.mkdir(parents=True, exist_ok=True)
rate = 48000
duration = 0.22
rng = random.Random(7331)
samples = []
low = 0.0
previous = 0.0
body_phase = 0.0
for i in range(round(rate * duration)):
    t = i / rate
    noise = rng.uniform(-1.0, 1.0)
    # A dark transient instead of a sustained, tuned gong resonance.
    low += 0.35 * (noise - low)
    snap = low - previous * 0.55
    previous = low
    body_phase += 2 * math.pi * (95 + 145 * math.exp(-t / 0.008)) / rate
    body = math.sin(body_phase) * math.exp(-t / 0.024)
    metal = sum(math.sin(2 * math.pi * f * t + p) * math.exp(-t / decay)
                for f, p, decay in [(830, 0.1, .012), (1379, .8, .010), (2231, 1.7, .007)]) / 3
    attack = min(1.0, t / 0.0005)
    value = attack * (0.62 * body + 0.30 * metal + snap * (1.0 * math.exp(-t / .006) + .35 * math.exp(-t / .018)))
    samples.append(math.tanh(value * 1.3))
peak = max(abs(v) for v in samples)
gain = (10 ** (-3 / 20)) / peak
pcm = b''.join(struct.pack('<h', round(max(-1, min(1, v * gain)) * 32767)) for v in samples)
with wave.open(str(source), 'wb') as output:
    output.setnchannels(1)
    output.setsampwidth(2)
    output.setframerate(rate)
    output.writeframes(pcm)

lib = unreal.EditorAssetLibrary
root = '/Game/ExpeditionPortfolio/Animation/Defense'
path = root + '/SW_ParryImpact_Tight'
if not lib.does_asset_exist(path):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', root)
    task.set_editor_property('destination_name', 'SW_ParryImpact_Tight')
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = lib.load_asset(path)
assert sound, path
data = lib.load_asset(root + '/DA_ParryCounter')
assert data
current = data.get_editor_property('parry_impact_sound')
if current is None or current.get_name() in ['SW_Hit_01', 'SW_ParryImpact_Tight']:
    data.set_editor_property('parry_impact_sound', sound)
    assert lib.save_loaded_asset(data, only_if_is_dirty=False)
unreal.log('PARRY_SOUND_READY asset=' + sound.get_path_name() + ' duration=' + str(sound.get_editor_property('duration')))
unreal.log('PARRY_SOUND_SOURCE frames=' + str(len(samples)) + ' peak_db=-3.0 tail_at_100ms=' + str(max(abs(v * gain) for v in samples[int(rate * .1):])))
