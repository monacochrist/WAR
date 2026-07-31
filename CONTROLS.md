# Controls

## Modes

WAR has several modes. Press `Esc` to return to ROLL mode from any mode.

| Mode | Enter | Description |
|------|-------|-------------|
| **ROLL** | default | Main editing mode — move cursor, place/delete notes, capture audio |
| **VISUAL** | `v` | Select notes visually |
| **MIDI** | `m` | Play notes with keyboard, record MIDI |
| **WAV** | `gd` | View waveform of a captured note |
| **COMMAND** | `:` | Enter commands (`:w`, `:load`, etc.) |

---

The grid uses **1 cell = 1 sixteenth note**.
BPM is quarter notes per minute; `seconds_per_cell = 15.0 / bpm`.

## ROLL Mode — Cursor Movement

| Key | Action |
|-----|--------|
| `h` / `Left` | Move cursor left |
| `j` / `Down` | Move cursor down (decrease pitch) |
| `k` / `Up` | Move cursor up (increase pitch) |
| `l` / `Right` | Move cursor right |
| `w` | Jump to next note start on same row |
| `e` | Jump to end of current note, or end of next note on same row |
| `b` | Jump to previous note start on same row |
| `<A-h>` / `<A-Left>` | Leap left (one octave) |
| `<A-j>` / `<A-Down>` | Leap down (one octave) |
| `<A-k>` / `<A-Up>` | Leap up (one octave) |
| `<A-l>` / `<A-Right>` | Leap right (one octave) |

## ROLL Mode — Notes

| Key | Action |
|-----|--------|
| `z` | Place a note at cursor position |
| `x` | Delete note under cursor |
| `t` / `r` | Trim note under cursor to cursor position |
| `c` | Toggle crop mode for capture slot at cursor |
| `a` | Set cursor width to match capture slot duration |
| `s` | Reset step size to 1.0 |
| `f` + number | Multiply cursor width by prefix (fat) |
| `t` + number | Divide cursor width by prefix (thin) |
| `F` + number | Widen step size by prefix |
| `T` + number | Narrow step size by prefix |
| `S-s` | Split note at cursor: divides the capture slot audio and moves the right portion to the nearest empty slot above |
| `S-c` | Split note at playback bar position: same as S-s but splits at the playback bar column instead of the cursor |
| `u` | Undo last note modification |
| `<C-r>` | Redo last undone modification |
| `<C-Up>` | Increase gain for capture slot under cursor (+10) |
| `<C-Down>` | Decrease gain for capture slot under cursor (-10) |
| `<C-Left>` | Pan left for capture slot under cursor (-5) |
| `<C-Right>` | Pan right for capture slot under cursor (+5) |
| `<C-S-Up>` | Increase EQ cutoff toward low-pass |
| `<C-S-Down>` | Decrease EQ cutoff toward high-pass |
| `p` | Paste yanked notes at cursor position |

## ROLL Mode — Viewport

| Key | Action |
|-----|--------|
| `G` | Go to bottom of viewport |
| `gg` | Go to top of viewport (or row `{n}` with prefix, e.g. `60gg`) |
| `gt` | Jump to row 127 |
| `gm` | Jump to row 60 |
| `gb` | Jump to row 0 |
| `$` | Go to column (prefix + 3), or right bound without prefix |
| `0` | Go to left visible bound |
| `+` | Zoom in |
| `_` | Zoom out |
| `)` | Reset zoom |

## ROLL Mode — Audio Capture

| Key | Action |
|-----|--------|
| `Q` / `<S-q>` | Toggle capture — starts/stops recording audio to the current note/layer |
| `q` | During capture: save current segment, advance cursor to next row, continue capturing |
| `Space` | Preview the captured audio at cursor position |
| `<A-o>` | Open capture device selector popup — lists PipeWire capture sources from `pactl list sources short`, refreshed each open. Press Enter to select, Escape to cancel |
| `i` | Toggle ACROSS mode (pitch-shifts capture within radius) |
| `<A-r>` | Toggle RESAMPLE mode (ON: resample changes pitch+length, OFF: pitch shift preserves duration) |
| `:` | Enter command mode |

## HUD Popup

`C-h` toggles a centered 40×10 popup overlay. Navigate with vim keys:

| Key | Action |
|-----|--------|
| `C-h` | Toggle HUD popup on/off |
| `h` / `Left` | Pan text left (horizontal scroll) |
| `j` / `Down` | Move cursor down |
| `k` / `Up` | Move cursor up |
| `l` / `Right` | Pan text right (horizontal scroll) |
| `Enter` | Confirm selection (device selectors) |
| `Esc` | Close popup |

The popup is used by:
- **Capture device selector** (`A-o` during capture) — lists PipeWire sources, `*` marks current device
- **MIDI device selector** (`A-o` in MIDI mode) — lists ALSA sequencer devices, `*` marks current device
- **Generic popup** (`C-h`) — empty navigable grid

When a device list has more than 10 entries, the list scrolls vertically. Long device names pan horizontally with `h`/`l`.

## ROLL Mode — Playback Bar

| Key | Action |
|-----|--------|
| `<S-Space>` | Toggle playback bar (play/stop) |
| `<S-l>` | Toggle playback bar loop (restart after last note) |
| `<S-b>` | Toggle tap tempo mode — tap Space to set BPM |
| `<S-d>` | Reset playback bar to beginning |
| `<A-a>` | Move playback bar to cursor position |
| `n` | Set loop end marker at playback bar position |
| `N` (<S-n>) | Set loop start marker at playback bar position |

## ROLL Mode — HUD (Harpoon-style)

| Key | Action |
|-----|--------|
| `<A-e>` | Toggle HUD overlay |
| `n` | Save current cursor position to next HUD slot |
| `k` / `j` | Navigate HUD list |
| `Enter` | Jump to selected HUD slot and close HUD |
| `Esc` | Close HUD |

## ROLL Mode — Waveform Viewer

| Key | Action |
|-----|--------|
| `gd` | Open waveform view for the note under cursor |
| `h`/`j`/`k`/`l` | Navigate through waveform |
| `Space` | Preview the note audio |
| `<S-Space>` | Toggle playback bar |
| `Esc` | Close waveform view, return to ROLL mode |

## ROLL Mode — Layers

| Key | Action |
|-----|--------|
| `<A-1>` through `<A-9>` | Set active layer 1–9 |
| `<A-0>` | Set active layer 0 (none) |
| `<A-S-1>` through `<A-S-9>` | Toggle layer visibility on/off |

## ROLL Mode — Octave (Shift+number)

| Key | Action |
|-----|--------|
| `<S-0>` through `<S-9>` | Set octave 0–9 |
| `-` | Set octave -1 |

## ROLL Mode — Step Mode

| Key | Action |
|-----|--------|
| `F` + number | Set step to fat (multiply step by prefix) |
| `T` + number | Set step to thin (divide step by prefix) |

## ROLL Mode — Mode Switching

| Key | Action |
|-----|--------|
| `m` | Toggle MIDI mode |
| `v` | Toggle visual mode |
| `:` | Enter command mode |

---

## MIDI Mode

### Octave

| Key | Action |
|-----|--------|
| `0`–`9` | Set octave 0–9 |

### Play Keys (hold to play, release to stop)

| Key | Note |
|-----|------|
| `q` | C |
| `w` | C# |
| `e` | D |
| `r` | D# |
| `t` | E |
| `y` | F |
| `u` | F# |
| `i` | G |
| `o` | G# |
| `p` | A |
| `[` | A# |
| `]` | B |

### MIDI Controls

| Key | Action |
|-----|--------|
| `a` | Toggle recording (places notes from key presses; starts playback bar) |
| `l` | Toggle loop mode (held notes repeat) |
| `g` | Toggle toggle mode (press once to start, again to stop) |
| `<S-d>` | Reset playback bar to beginning |
| `<A-a>` | Move playback bar to cursor position |
| `<A-o>` | Open MIDI device selector popup — lists ALSA sequencer input devices from `aconnect -i`, refreshed each open. Press Enter to select, Escape to cancel |
| `<A-s>` | Toggle velocity sensitivity (SENSE) — when on, MIDI velocity maps to per-voice gain (velocity 64 = 0 dB) |
| `<C-m>` | Toggle MIDI controller playback (MCPLAY) — when on (default), connected controller Note On/Off triggers preview voices |

### MIDI Controller Input

WAR connects to a MIDI controller via ALSA sequencer. Select a device with `A-o` in MIDI mode, then browse with `j`/`k` and confirm with Enter.

- MIDI Note On/Off triggers preview voices (same as keyboard play keys) when controller playback is on
- Controller playback toggle (`C-m`, default on): off ignores Note On from the controller (events still drained; hanging notes release)
- Velocity sensitivity (toggle with `A-s`) scales playback gain: velocity 64 = unity, lower = quieter, higher = louder
- MIDI events are processed before the audio mixing loop for zero-frame latency
- Selected controller is saved to `global_war.config` and auto-connects on next launch
- Note On places a recorded note on the grid when recording is active (`a`)

### Layers (same as ROLL mode)

| Key | Action |
|-----|--------|
| `<A-1>` through `<A-9>` | Set active layer 1–9 |
| `<A-0>` | Set active layer 0 (none) |
| `<A-S-1>` through `<A-S-9>` | Toggle layer visibility on/off |

---

## Command Mode (press `:`)

| Command | Action |
|---------|--------|
| `:w <name>` | Save project file |
| `:load <name>` | Load project file |
| `:wwav <name>` | Export WAV audio |
| `:wmp3 <name>` | Export MP3 audio (requires ffmpeg) |
| `:bpm <value>` | Set BPM; type `:bpm` with no arg to view current BPM |
| `:loop <quarter_notes> <repeats>` | Loop notes (copy section length × repeats) |
| `:cd <path>` | Change directory |
| `:radius <n>` | Set ACROSS pitch-shift radius (notes above/below) |
| `:eq1 <value>` | Set EQ1 HPF/LPF for current slot (positive = HP, negative = LP, `off` = zero) |
| `:eq2 <value>` | Set EQ2 HPF/LPF for current slot (same as eq1) |
| `:eq1 status` / `:eq2 status` | Show current eq1/eq2 values |
| `:winst <name>` | Save instrument file for current cursor layer |
| `:loadinst <name>` | Load instrument file into current layer at cursor |
| `:mv <layer>` | Move capture slot at cursor row/layer to another layer |
| `:mvu <n>` | Move capture slot at cursor up n pitches |
| `:mvd <n>` | Move capture slot at cursor down n pitches |
| `:across <radius>` | Pitch-shift capture slot at cursor to nearby notes (within radius); respects RESAMPLE toggle |
| `:compress <on|off|params...>` | Toggle/set compressor (threshold, ratio, attack, release, makeup) |
| `:saturate <on|off|params...>` | Toggle/set saturator (drive, mix, makeup) |
| `:reverb <on|off|params...>` | Toggle/set reverb (decay, mix) |
| `:delay <on|off|params...>` | Toggle/set delay (time, feedback, mix) |
| `:chorus <on|off|params...>` | Toggle/set chorus (rate, depth, mix) |
| `:gate <on|off|params...>` | Toggle/set gate (threshold, attack, hold, release) |
| `:deesser <on|off|params...>` | Toggle/set de-esser (threshold, frequency) |
| `:effect usage` | Show effect parameter format |
| `:effect status` | Show current effect states for the slot |
| `:effect default` | Reset current effect to defaults |
| `:whatson` | List all active effects on current slot |
| `:offall` | Turn off all effects on current slot |
| `:clear` | Clear current slot (frees samples, resets all params) |
| `:clearall` | Clear ALL slots and note instances (reset project to default) |
| `:gp` | Move cursor to playback bar position |
| `:maj7` | Place a major 7th chord (root, +4, +7, +11) at cursor using cursor width |
| `:min7` | Place a minor 7th chord (0, +3, +7, +10) |
| `:min9` | Place a minor 9th chord (0, +3, +7, +10, +14) |
| `:9` | Place a dominant 9th chord (0, +4, +7, +10, +14) |
| `:maj9` | Place a major 9th chord (0, +4, +7, +11, +14) |
| `:6` | Place a major 6th chord (0, +4, +7, +9) |
| `:2` | Place a sus2 chord (0, +2, +7) |
| `:gain <0-200>` | Set gain for capture slot under cursor (100 = 1.0x) |
| `:pan <-100..100>` | Set pan for capture slot under cursor (0 = center) |
| `:cp <layer>` | Copy capture slot at cursor pitch/layer to another layer |
| `:q` | Quit the application |

Press `Esc` to exit command mode.

## Effect System

Each capture slot has 9 per-slot effect slots (`WAR_EFFECT_COUNT`). Effects are toggled and configured per-slot via commands.

### Effect Commands

Each effect supports the same sub-commands:
- `:effect on` — enable the effect
- `:effect off` — disable the effect
- `:effect <params>` — enable and set parameters
- `:effect usage` — show parameter format
- `:effect status` — show current state and values
- `:effect default` — reset to defaults (`off` with default params)

Effects process in series: Compress1 → Compress2 → Saturate → Gate → De-esser, followed by EQ1/EQ2 inline.

### Effect Parameters

| Effect | Command | Parameters |
|--------|---------|------------|
| **Compressor** | `:compress` | `threshold ratio attack(ms) release(ms) makeup` |
| **Compress2** | `:compress2` | Same as compress (second compressor in series) |
| **Saturator** | `:saturate` | `drive mix makeup(dB)` |
| **Reverb** | `:reverb` | `decay(0-1) mix(0-1)` |
| **Delay** | `:delay` | `time(ms) feedback(0-1) mix(0-1)` |
| **Chorus** | `:chorus` | `rate(Hz) depth(0-1) mix(0-1)` |
| **Gate** | `:gate` | `threshold attack(ms) hold(ms) release(ms)` |
| **De-esser** | `:deesser` | `threshold freq(Hz)` |

### Quick Controls

| Command | Action |
|---------|--------|
| `:whatson` | List all active effects on current slot with abbreviations |
| `:offall` | Disable all effects on current slot |
| `:clear` | Free current slot samples and reset all params |
| `:clearall` | Free ALL slot samples and reset project to defaults |
| `:eq1 <value>` | Set EQ1: positive = high-pass, negative = low-pass, `off` = zero |
| `:eq2 <value>` | Set EQ2 (same range as eq1) |

---

## Status Bar Indicators

| Label | Location | Meaning |
|-------|----------|---------|
| `G<value>` | Bottom bar | Gain for capture slot under cursor |
| `P<value>` | Bottom bar | Pan for capture slot under cursor |
| `A<value>` / `S<value>` / `R<value>` | Bottom bar | Attack, sustain, release for capture slot under cursor |
| `SENSE` | Bottom bar | Velocity sensitivity enabled (Alt+S to toggle) |
| `CROP` | Top bar | Crop mode active |
| `CAPTURE` | Middle bar | Audio capture in progress |
| `MIDI` | Middle bar | MIDI mode active |
| `MIDI RECORD C<N>` | Middle bar | MIDI recording active on capture mode N |
| `VISUAL` | Middle bar | Visual mode active |
| `TAP TEMPO` | Middle bar | Tap tempo mode (Shift+B) — Space to tap |
| `LOOP` | Top bar | Loop mode enabled (MIDI) |
| `ACROSS` | Top bar | ACROSS pitch-shift enabled |
| `TOGGLE` | Top bar | Toggle key mode enabled (MIDI) |
| `RESAMPLE` | Bottom bar | RESAMPLE mode enabled (pitch+length change); Alt+R toggles |
| `PB LOOP` | Bottom bar | Playback bar loop enabled |
| `123456789` | Top bar | Active layer visibility numbers |
| CWD path | Top bar | Current working directory |
| `row, col` | Top bar | Cursor position |

---

## Visual Mode

In visual mode, movement keys (`h`/`j`/`k`/`l`, `w`/`b`) extend the selection range. A blue highlight shows the selected area.

| Key | Action |
|-----|--------|
| `v` | Toggle visual mode on/off |
| `h`/`j`/`k`/`l` | Extend selection |
| `w`/`b` | Extend selection to next/prev note |
| `<S-h>`/`<S-j>`/`<S-k>`/`<S-l>` | Move selected notes by step size |
| `t` / `r` | Trim selected note |
| `x` | Delete selected note |
| `o` | Swap cursor to opposite end of selection |
| `y` | Yank (copy) selected notes |
| `p` | Paste yanked notes at cursor position |
| `n` | Save cursor position to HUD |
| `u` | Undo |
| `<C-r>` | Redo |
| `Esc` | Exit visual mode |

## Crop Mode

Press `c` on a row/layer that has a capture slot to enter crop mode. An orange **CROP** label appears on the top status bar. Arrow keys adjust offset markers freely; the actual crop is applied when exiting.

| Key | Action |
|-----|--------|
| `Left` | Move start marker left (restore/uncrop from start) |
| `Right` | Move start marker right (crop from start) |
| `Shift+Left` | Move end marker left (crop from end) |
| `Shift+Right` | Move end marker right (restore/uncrop from end) |
| `Space` | Preview the cropped range |
| `c` / `Esc` | Exit crop mode and apply the crop |

Crop adjustments are preview-only until exit. Left then Right (or vice versa) cancel out.

---

## WAV Mode

Opened with `gd` over a note. Shows the audio waveform for the capture slot at the cursor's pitch.

| Key | Action |
|-----|--------|
| `h`/`j`/`k`/`l` | Navigate through waveform |
| `Space` | Preview the note audio |
| `<S-Space>` | Toggle playback bar |
| `Esc` | Close waveform view |
