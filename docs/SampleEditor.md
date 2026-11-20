# Sample editor

These are the default controls. Edit the `sample_editor.*` entries in `shortcuts.xml` and restart LGPT to change them.


In the instrument view, select **Edit** below **sample:** and press A to open
the full-page Sample Editor. The navigator keeps **I** highlighted.
The editor works on a private copy of the loaded mono/stereo WAV sample.
SoundFont instruments are not supported.

- Arrows select Start and End in each selection row, then Process, Undo, Name, Save As, Overwrite, or Exit.
- On Start/End, hold A and use Left/Right to choose a hexadecimal digit;
  A+Up/Down changes that digit's value. The rightmost digit moves exactly one
  sample frame (both channels together for stereo).
- **Select by Beats** shows both boundaries in decimal beats at the current
  tracker tempo. At 120 BPM, `0001.5000` is 0.75 seconds from the sample start.
  A+Left/Right selects a digit, skipping the decimal point. A+Up/Down changes
  its value by one step. The smallest possible movement is one source frame.
- Start is inclusive; End is exclusive. The initial selection is the full sample.
- L toggles between the full waveform and the selected region. This first version
  uses a min/max envelope per native display pixel on SDL/SDL2; stereo channels
  share the envelope. Older display backends retain the character-grid fallback. The numerical selection is exact even when smaller than a column.
- START plays the selection, including unsaved edits. A vertical play cursor
  follows the preview in both waveform views and disappears when it stops.
  START stops an active preview or starts it again after it ends.
  Preview uses the existing prelisten volume setting.
- On Process, A+Left/Right chooses a command; a separate short A press and release executes it.
  Holding A or changing a value/function with A held never executes an action.
- Undo is selectable to the right of Process. A press and release restores the
  last successful process, including selection, zoom and unsaved-change state.
  It has one level; a successful save clears it.
- On Name, A+Left/Right moves the character cursor, A+Up/Down changes the character.
  Names have up to 16 characters; `.wav` is added automatically.
- A on Save As writes a new file into the project's sample directory and assigns
  it to the current instrument. Existing filenames are refused.
- A on Overwrite asks for confirmation, replaces the source file, and refreshes
  every instrument using it. Saving stops song playback and resets the affected
  instruments' start/loop boundaries to the new full sample.
- B or Exit returns to the instrument view, asking before discarding unsaved edits.

Crop keeps the selection. Delete removes it and joins the remaining audio;
removing the entire sample is refused (use Silence instead). Normalize scales
both channels by the same peak gain. Silence sets the selection to zero.
Fade In and Fade Out use linear ramps with exact endpoints; a one-frame fade
produces silence. Processing never changes frames outside the selection except
for Crop/Delete. After those two commands the new full sample is selected.

Files are saved as 16-bit PCM WAV at the original sample rate and channel count.
Additional WAV metadata is not retained. A temporary file is completed before
replacing the destination; a failed save leaves the edits available for retry.
Exit without saving to discard all unsaved edits.

## Developer validation

Run the independent PCM/WAV tests from the repository root:

```sh
g++ -std=gnu++03 -Wall -Isources tests/sample_editor_test.cpp -o /tmp/sample_editor_test
/tmp/sample_editor_test
g++ -std=gnu++03 -Wall -Isources tests/sample_editor_action_test.cpp -o /tmp/sample_editor_action_test
/tmp/sample_editor_action_test
```

Before a PR, manually exercise navigation, preview, every command, Save As,
Overwrite (including two instruments sharing one sample), and discard on actual
hardware. Record hardware and OS. Compilation and PCM tests do not establish
that UI/audio behavior works on a device.

## Short A press duration

`<SHORT_PRESS_MS value="250" />` in `config.xml` is the general short-press setting.
The sample editor uses it as the maximum A tap duration in milliseconds for Process, Undo, Save As, Overwrite and Exit.
The default is 250 ms. Valid values are 1–2000; missing or invalid values use
the default. A release at the limit still counts as a tap. Holding longer does
nothing, and A+arrow combinations never execute an action. Restart LGPT after
changing the configuration.
