# openFruit

A step sequencer and sample player for drum patterns. Create patterns one hit at a time, then arrange four pages into a song.

![openFruit step sequencer](docs/screenshots/preview.png)

[Download the latest release](https://github.com/alanmarcero/openFruit/releases/latest) · [Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

## Beta

All of these plugins are in beta. The macOS and Linux builds are not up to date, so the Windows build is the one to use for now. A plugin can change dramatically from one release to the next, but the goal is for each one to keep getting better.

## Play a pattern

Press **Play** to start the sequencer and arm host sync. The label beside it reads Stopped, Armed or Playing. Armed means the sequencer starts with the host transport, and a saved session stays armed. The citrus wheel turns once per four quarter notes and follows tempo changes and seeks. In standalone it follows the sequencer’s internal clock. Select **A**, **B**, **C**, or **D** to edit a page. Their startup phrases cover a straight floor groove, syncopated hats, halftime and a tom fill.

Click a sample name to audition it. Click any of its 16 pads to toggle a hit, drag across a row to paint hits, drag vertically for velocity, or right-drag or hold Alt and scroll for pitch. Right-click for probability, 1–8 ratchets, timing offset, sample start and end, reverse playback, and choke group. The menu ticks the current values, and Reset step clears them. Each pad draws a small mark for every setting that differs from the default. Right-click a track number or a pattern button to copy, paste, clear or shift that row or pattern. Mute and Solo are beside the sample, and a row that cannot be heard dims its pads. Select 4, 8, 12 or 16 tracks. **Mix** swaps the pads for each track's level and pan, and **Steps** brings them back. Larger patterns scroll vertically at compact sizes, and the editor reopens at the size you left it.

**Song mode** reveals four sections. Choose the page and length for each (1–32 bars). **Fill** plays page D during the final bar. While the song plays, its current section lights up with the bar it is on, and **Follow** shows the pattern that section plays. The playhead dims while the grid shows a pattern that is not playing. Global swing delays alternate steps. Accent sets periodic emphasis and its interval, from 2 to 16 steps. The master level and output trim (−24 to +12 dB) sit beside the transport.

![openFruit Song mode](assets/openFruit-song-mode-screenshot.png)

## Shape a track

**Edit** opens its MIDI channel, step rate (1/16, 1/32, 1/64), pitch (±24 semitones), level, pan, low-pass filter, nudge, groove, and attack, decay, sustain and release envelope. The arrows beside Done step to the previous or next track. Incoming MIDI plays the track on its selected channel. Each step also has ±24 semitones of tuning.

![openFruit native voice settings](assets/openFruit-track-settings-screenshot.png)

The Citrus kit has 16 synthesized one-shots. A swept kick, layered snare, retriggered clap and two metallic hats share the kit with tuned wood percussion, sub bass, chord stab, zap and ride. Pitched-up playback averages the source between output samples to reduce folded high frequencies in both forward and reverse playback.

## Browse and recall samples

**Browse** opens the sample library for that lane. Choose or drop a folder or audio file. Sort by duration, name or estimated pitch. **Autoplay** auditions each selection. **Preview** replays it, and **Load sample** assigns it to the track. The selected file also responds to incoming MIDI on the track’s channel.

![openFruit sample library](assets/openFruit-browser-screenshot.png)

The browser handles WAV/BWF, AIFF/AIFC, MP3, FLAC, Ogg Vorbis and additional formats supplied by the operating system. A saved song stores each loaded sample in the session as FLAC and keeps the original file path as a hint, so the song still opens on another computer. The same audio on more than one track is stored once. Short drum hits add little to the project. Long loops and large kits make the file bigger, because the audio travels with the song. An older session that only stored paths still opens from those files, and the next save stores the audio in the session. Projects saved by an earlier version reopen with the original factory kit and their stored patterns.

Undo and Redo (or Cmd/Ctrl+Z and Shift+Cmd/Ctrl+Z) step back through knob moves, step edits and kit loads. A and B keep two copies of the whole song, each with its own undo history. A>B and B>A copy one into the other. Presets saves a kit in Documents/Alan Marcero/openFruit/Presets. The list shows the factory kits and your kits, with search and favorites. Double-click a kit or press Enter to load it, or step through the list with the arrows beside the kit name. A star after the name marks edits since the last load or save. Factory kits stay read-only. The four editor themes use dark backgrounds.

## Formats

VST3 and LV2 for macOS, Windows, and Linux; AU for macOS; standalone apps for all three platforms.

## License

AGPL-3.0. See [LICENSE](LICENSE).
