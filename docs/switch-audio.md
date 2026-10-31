# Audio on the Switch

This page explains how LM-NX plays sound, and how the crackling during thunder was found and fixed.
For the shared sound engine, formats and mixing rules, see [AUDIO.md](AUDIO.md).
Technical words are explained in the [glossary](glossary.md).

## From the game to the speakers

The original game prepares sound through JAudio, its sound library. The port provides the GameCube services that
JAudio expects, including disc reads, audio memory and the DSP (the GameCube's sound processor). This is
[high-level emulation](glossary.md#hle): native code implements those services around the recompiled game.

The output passes through these stages:

1. The game loads sound effects and reads streamed audio from its disc image.
2. The sound engine decodes and mixes them. Its normal stereo output is approximately 32,028 samples per second.
3. `src/recomp/audio_out.cpp` handles channel order, volume, output layout and resampling. Resampling changes the
   number of samples per second to match the output device.
4. The small SDL3 compatibility layer in `platforms/switch/sdl3_shim/` sends the result to libnx's `audout` service.
   The physical Switch output is 48,000 samples per second, stereo, with signed 16-bit samples.

The shared output supports a windowed-sinc resampler: a filter that avoids adding high-frequency copies of the
original sound when changing its sample rate. The Switch transport also accepts floating-point or 16-bit input
and converts supported channel layouts into stereo. The shared engine's surround options do not imply discrete
multichannel output from this transport.

The transport keeps a bounded queue and four output buffers of 1,024 stereo frames each. One buffer covers about
21.3 ms at 48 kHz. A dedicated output thread waits for the system to finish a buffer, fills it and submits it again.
It runs at priority `0x2B`, ahead of ordinary runtime workers. See [platform-notes.md](platform-notes.md).

If the queue runs dry while sound should be playing, the transport inserts silence and counts the missing samples.
An `underrun` report means this happened; it does not identify the cause by itself. A damaged decoded stream can
crackle even when the output queue never empties.

## The thunder crackling investigation, 2026-10-30 to 2026-10-31

### Symptom

Thunder made a loud crackling sound during the arrival at the mansion, especially on the first arrival after
loading a save. It was intermittent. It could also occur in the PC version, so a Switch output-driver fault was
only one possibility.

The affected sound came from the mansion's streamed audio, rather than from a general failure of all sound effects.
The important question became: did good audio arrive late, or did the decoder receive the wrong compressed data?

### Making an intermittent sound inspectable

Temporary diagnostic builds recorded several parts of the path:

- output queue levels and empty-buffer events;
- audio voice starts and their decoder state;
- changes to the GameCube's audio memory, called ARAM;
- disc read sizes, offsets and duration;
- a rolling recording of the last **90 seconds** of audio on the console.

That recording mattered because the crackle could happen before a manual recording started. Keeping recent sound
in a ring buffer meant the event could be saved after it was heard. These were investigation tools; the rolling
recorder and its menu control were removed after the fix. They are not features of the public settings menu.

The captured sound was compared with the stream decoded from the owner's disc. The comparison looked at small
time windows, their loudness and the alignment of their waveforms. This separated a missing piece of sound from a
wrongly decoded piece. The source stream was good; the console had played the wrong sequence of chunks.

### What was ruled out

The investigation checked queue starvation, late mixing, audio memory being overwritten, clipping (a signal above
the allowed maximum), races in voice data, and stale sound left in a ring buffer. None explained the recording and
the disc trace together.

One plausible change gave audio reads priority over large room-loading reads. Normal reads were split into pieces,
allowing a waiting small or high-priority read to go first. This was retained for disc-service behavior, but
**it did not fix the thunder**. Treating that build as the solution would have closed the investigation too early.

### The cause: chunk zero was read twice

The affected stream is stereo ADPCM at 44.1 kHz. ADPCM stores differences from previous samples, so decoding depends
on a small history carried from one block to the next. The game reads this stream in **5,760-byte chunks**.

The original stream loader performs these operations in order:

1. Mark a stream buffer busy.
2. Call `DVDReadAsyncPrio`, the GameCube function for requesting an asynchronous disc read.
3. Advance its count of bytes already requested.

An asynchronous request normally returns quickly; its completion is reported later. In the port, the host read
inside that request could take milliseconds while the SD card was also loading the mansion. The implementation
released the guest CPU during that host read, allowing other scheduled game work to run.

That opened a gap between steps 2 and 3. An earlier completion and the stream callback could run in the gap, see
another stream buffer available, and request the next chunk using the **old file offset**. The offset had not yet
been advanced by the first call.

The trace recorded two asynchronous reads of 5,760 bytes at offset **32**, in the same game frame. Instead of the
sequence `0, 1, 2, 3`, the decoder received `0, 0, 2, 3`. Chunk one was skipped. Because ADPCM uses previous samples
as history, the error continued after the repeated chunk and produced crackles until that history recovered.

Simulating this repeated first chunk reproduced the distinctive error in the recording. In the recorded comparison,
the waveforms matched again from about **12.05 seconds**, with correlation **1.00**. Correlation measures how closely
two waveforms follow each other; the alignment and the duplicated read together made this a stronger explanation
than simply hearing that one build sounded better.

### The fix

The implementation in [src/recomp/hle_dvd.cpp](../src/recomp/hle_dvd.cpp) now handles asynchronous reads of
**32 KiB or less without releasing the guest CPU**. The stream's 5,760-byte requests fit within that limit.
The caller can advance its file offset before another guest callback enters the loader.

This does not turn the completion callback into a synchronous callback. The read result is still posted through
the scheduler's delayed interrupt path, and the file remains busy until completion. Large reads may still release
the guest CPU when interrupts are enabled, so room loading does not unnecessarily block the game's other work.
Calls inside a guest critical section also keep that section intact.

The fix restores the ordering the original loader relies on. Increasing audio buffering could hide a late packet,
but could not repair a compressed stream whose second chunk had been replaced by its first.

### Confirmation on the console

On **2026-10-31**, repeated loads on the actual Switch confirmed that the thunder crackling was fixed. The earlier
disc-priority change had failed this same listening check; the small-read scheduling change passed it.

The temporary disc tracing, audio-memory logging and 90-second recorder were then removed. The ordinary output
underrun counter remains useful for a different class of failure: sound that runs out before the next buffer is
ready. There is no claim that every possible audio defect was tested throughout the entire game.

## Reporting a sound problem

Open settings with **L3 + R3** (press both sticks). Record the room or scene, whether the save was just loaded,
whether music and effects both fail, and whether the problem repeats after restarting. Keep the session logs from
the `logs/` folder next to `LM-NX.nro`.

A useful report distinguishes these symptoms:

| Symptom | What to investigate |
|---|---|
| Everything briefly goes silent | Output queue, scheduling and late mixing |
| One stream crackles while other sound continues | Compressed input, chunk ordering and decoder history |
| Left and right appear exchanged | Channel ordering and output settings |
| Sound stops after a load | Stream completion, callbacks and file lifetime |

For timing and repeatable comparisons, see [measuring.md](measuring.md). The lesson of the thunder fix is to trace
both the sound and the data that produced it: an audible defect can begin in the disc scheduler.
