#!/usr/bin/env python3
"""Sound generator for Typewrite OS fonts."""

import numpy as np
import soundfile as sf
from scipy import signal
import math
import os

SAMPLE_RATE = 44100


def generate_tone(freq, duration, attack=0.01, decay=0.1, sustain=0.7, release=0.1, volume=0.5):
    """Generate an ADSR envelope tone."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    envelope = np.ones(n_samples)
    attack_samples = int(SAMPLE_RATE * attack)
    decay_samples = int(SAMPLE_RATE * decay)
    release_samples = int(SAMPLE_RATE * release)
    
    # Attack
    envelope[:attack_samples] = np.linspace(0, 1, attack_samples)
    # Decay to sustain
    envelope[attack_samples:attack_samples+decay_samples] = np.linspace(1, sustain, decay_samples)
    # Sustain
    sustain_end = n_samples - release_samples
    envelope[attack_samples+decay_samples:sustain_end] = sustain
    # Release
    envelope[sustain_end:] = np.linspace(sustain, 0, release_samples)
    
    wave = np.sin(2 * math.pi * freq * t) * envelope * volume
    return wave.astype(np.float32)


def generate_noise(duration, volume=0.3):
    """Generate white noise."""
    n_samples = int(SAMPLE_RATE * duration)
    noise = np.random.uniform(-1, 1, n_samples) * volume
    return noise.astype(np.float32)


def generate_clack(duration=0.08, pitch_variation=50):
    """Typewriter/key clack - short noise burst with pitch."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    freq = 800 + np.random.uniform(-pitch_variation, pitch_variation)
    tone = np.sin(2 * math.pi * freq * t)
    
    # Envelope - quick attack, fast decay
    envelope = np.exp(-t * 40)
    
    # Add some noise
    noise = np.random.uniform(-1, 1, n_samples) * 0.3
    
    wave = (tone * 0.7 + noise) * envelope * 0.6
    return wave.astype(np.float32)


def generate_carriage_return(duration=0.6):
    """Typewriter carriage return - mechanical whir."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Low frequency mechanical hum
    freq = 60 + 20 * np.sin(2 * math.pi * 3 * t)
    tone = np.sin(2 * math.pi * freq * t)
    
    # Add noise for mechanical texture
    noise = np.random.uniform(-1, 1, n_samples) * 0.2
    
    # Envelope
    envelope = np.exp(-t * 2)
    
    wave = (tone * 0.5 + noise) * envelope * 0.5
    return wave.astype(np.float32)


def generate_bell(duration=0.5, freq=800):
    """Typewriter bell - high ping."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Main tone + harmonic
    wave = np.sin(2 * math.pi * freq * t) + 0.5 * np.sin(2 * math.pi * freq * 2 * t)
    
    # Quick decay
    envelope = np.exp(-t * 8)
    
    return (wave * envelope * 0.4).astype(np.float32)


def generate_c64_blip(duration=0.1, freq=440):
    """C64/Atari style blip - square wave with quick decay."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Square wave approximation
    wave = signal.square(2 * math.pi * freq * t) * 0.5
    
    # Quick envelope
    envelope = np.exp(-t * 25)
    
    return (wave * envelope * 0.5).astype(np.float32)


def generate_coin_sound(duration=0.3):
    """Arcade coin pickup - two-tone ascending."""
    tone1 = generate_tone(987, 0.08, attack=0.005, decay=0.05, sustain=0, release=0)
    tone2 = generate_tone(1318, 0.15, attack=0.005, decay=0.1, sustain=0, release=0.05)
    
    wave = np.concatenate([tone1, tone2])
    return wave.astype(np.float32)


def generate_menu_blip(duration=0.05, freq=660):
    """Menu selection blip."""
    return generate_tone(freq, duration, attack=0.002, decay=0.02, sustain=0, release=0, volume=0.4)


def generate_boot_sound(duration=1.0):
    """C64 boot - ascending tones."""
    tones = [261, 329, 392, 523, 659]  # C4, E4, G4, C5, E5
    wave = np.array([])
    
    for i, freq in enumerate(tones):
        t = generate_tone(freq, 0.15, attack=0.01, decay=0.05, sustain=0.3, release=0.1, volume=0.3)
        wave = np.concatenate([wave, t])
    
    return wave.astype(np.float32)


def generate_crt_power_on(duration=0.8):
    """CRT power on - warmup with hum."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Low hum that rises in pitch
    freq = 60 + 100 * t
    tone = np.sin(2 * math.pi * freq * t)
    
    # Add some crackle
    crackle = np.random.uniform(-1, 1, n_samples) * 0.1 * (1 - t/duration)
    
    # Fade in
    envelope = np.linspace(0, 1, n_samples) * np.exp(-t * 0.5)
    
    wave = (tone * 0.3 + crackle) * envelope * 0.5
    return wave.astype(np.float32)


def generate_phosphor_buzz(duration=0.5):
    """CRT phosphor buzz."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # 60Hz hum + harmonics
    wave = (np.sin(2 * math.pi * 60 * t) * 0.5 + 
            np.sin(2 * math.pi * 120 * t) * 0.25 +
            np.sin(2 * math.pi * 180 * t) * 0.125)
    
    # Slight modulation
    wave *= 1 + 0.1 * np.sin(2 * math.pi * 5 * t)
    
    return (wave * 0.3).astype(np.float32)


def generate_keyboard_click(duration=0.03, freq=2000):
    """IBM style keyboard click - high freq short burst."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    wave = np.sin(2 * math.pi * freq * t) + np.random.uniform(-1, 1, n_samples) * 0.3
    envelope = np.exp(-t * 80)
    
    return (wave * envelope * 0.4).astype(np.float32)


def generate_disk_spin(duration=1.0):
    """Disk drive spin - steady motor hum."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Motor whine
    freq = 120 + 10 * t
    tone = np.sin(2 * math.pi * freq * t)
    
    # Add noise texture
    noise = np.random.uniform(-1, 1, n_samples) * 0.1
    
    # Envelope
    envelope = np.exp(-t * 0.5)
    
    wave = (tone * 0.3 + noise) * envelope * 0.4
    return wave.astype(np.float32)


def generate_post_beep(duration=0.4):
    """POST beep - classic IBM style."""
    return generate_tone(1000, duration, attack=0.01, decay=0.1, sustain=0.5, release=0.1, volume=0.5)


def generate_ui_tap(duration=0.05, freq=800):
    """Soft UI tap for Inter/Helvetica."""
    return generate_tone(freq, duration, attack=0.002, decay=0.03, sustain=0, release=0, volume=0.25)


def generate_notification_chime(duration=0.3):
    """Notification chime - two soft tones."""
    tone1 = generate_tone(523, 0.12, attack=0.01, decay=0.05, sustain=0.3, release=0.05, volume=0.3)
    tone2 = generate_tone(659, 0.15, attack=0.01, decay=0.05, sustain=0.3, release=0.1, volume=0.3)
    
    # Combine
    combined = np.zeros(max(len(tone1), len(tone2)))
    combined[:len(tone1)] += tone1
    combined[:len(tone2)] += tone2
    
    return combined.astype(np.float32)


def generate_pencil_scratch(duration=0.15):
    """Virgil hand-drawn - pencil scratch texture."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Friction-like noise with some tonal content
    noise = np.random.uniform(-1, 1, n_samples)
    
    # Bandpass filter to give it some "lead" quality
    b, a = signal.butter(2, [1000, 3000], btype='band', fs=SAMPLE_RATE)
    filtered = signal.filtfilt(b, a, noise)
    
    # Envelope
    envelope = np.exp(-t * 15)
    
    wave = filtered * envelope * 0.5
    return wave.astype(np.float32)


def generate_paper_rustle(duration=0.2):
    """Paper rustle for Virgil."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    
    # Filtered noise
    noise = np.random.uniform(-1, 1, n_samples)
    
    # Lowpass for paper-like sound
    b, a = signal.butter(2, 2000, btype='low', fs=SAMPLE_RATE)
    filtered = signal.filtfilt(b, a, noise)
    
    envelope = np.exp(-t * 8)
    
    return (filtered * envelope * 0.3).astype(np.float32)


def generate_simple_blip(duration=0.05, freq=440):
    """Simple minimal blip."""
    return generate_tone(freq, duration, attack=0.001, decay=0.03, sustain=0, release=0, volume=0.3)


# === GENERATOR DISPATCH ===
SOUNDS = {
    # Typewriter (Special Elite, Courier Prime)
    "typewriter_key": generate_clack,
    "typewriter_carriage": generate_carriage_return,
    "typewriter_bell": generate_bell,
    
    # Arcade (Press Start 2P)
    "arcade_coin": generate_coin_sound,
    "arcade_menu": generate_menu_blip,
    "arcade_boot": generate_boot_sound,
    "arcade_blip": lambda: generate_c64_blip(),
    
    # CRT Terminal (VT323, Share Tech Mono)
    "crt_power_on": generate_crt_power_on,
    "crt_buzz": generate_phosphor_buzz,
    "terminal_blip": lambda: generate_c64_blip(freq=880),
    
    # IBM (IBM Plex Mono)
    "ibm_keyboard": generate_keyboard_click,
    "ibm_disk": generate_disk_spin,
    "ibm_post": generate_post_beep,
    
    # UI (Inter/Helvetica)
    "ui_tap": generate_ui_tap,
    "ui_chime": generate_notification_chime,
    
    # Hand-drawn (Virgil)
    "virgil_pencil": generate_pencil_scratch,
    "virgil_paper": generate_paper_rustle,
    
    # Simple
    "simple_blip": generate_simple_blip,
}


def generate_sound(name, output_dir="."):
    """Generate a sound and save to file."""
    if name not in SOUNDS:
        print(f"Unknown sound: {name}")
        print(f"Available: {list(SOUNDS.keys())}")
        return
    
    generator = SOUNDS[name]
    wave = generator()
    
    # Normalize
    wave = wave / np.max(np.abs(wave)) * 0.9
    
    filename = os.path.join(output_dir, f"{name}.wav")
    sf.write(filename, wave, SAMPLE_RATE)
    print(f"Generated: {filename}")


def generate_all(output_dir="."):
    """Generate all sounds."""
    os.makedirs(output_dir, exist_ok=True)
    for name in SOUNDS:
        generate_sound(name, output_dir)
    print(f"\nGenerated {len(SOUNDS)} sounds in {output_dir}/")


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        output = sys.argv[1] if len(sys.argv) > 1 else "."
        generate_all(output)
    else:
        print("Usage: python3 soundgen.py [output_dir]")
        print(f"Available sounds: {list(SOUNDS.keys())}")
        print("\nGenerate all: python3 soundgen.py ./my_sounds")
        print("Generate one:  python3 soundgen.py ./typewriter_key")
