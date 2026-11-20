# Editable shortcuts (stage 28)

`shortcuts.xml` lives beside the LGPT executable, like `config.xml` and
`mapping.xml`. For the X64 development build this is `projects/shortcuts.xml`.
LGPT reads it once when creating the application window. Restart LGPT after
editing it. Release ZIPs include this file alongside the executable or, on
macOS, alongside `LittleGPTracker.app`. Installations without the file use the
compiled default bindings.

## Coverage

This migration covers all fifteen Groove actions and the Mixer controls,
including state-dependent Mute/Solo release bindings (22 Mixer entries), plus
all FX controls (12 entries) and both Table contexts (32 entries, including
selection mode), and Chain (48 entries, including sequence states and release
bindings). The command selector shared by Phrase and Table has six bindings.
The startup project picker, project name entry, and confirmation dialogs also support editable bindings.
Project settings use `project.*` bindings for navigation, field edits, activation,
tap tempo (B), tempo nudges (B+Left/Right), playback/render and return to Song.
Instrument slot changes (B+Left/Right for one slot, B+Down/Up for sixteen),
view changes (R+Left to Phrase, R+Down to Table) and playback (START or
R+START from the beginning) use `instrument.*` bindings. On the Sample, Table and Print FX rows,
press A twice to import, allocate a table or print effects. The first press
uses `begin_new`. The second uses the action for the focused row. The Edit
Sample row opens the full-page editor with one press of `open_editor`. To clone an
assigned sampler or MIDI table, focus Table, hold L and press B (`begin_clone`), then
release B and press A while still holding L (`clone_table`). The instrument
now points to the copy. The four direction bindings move focus. The four
`edit_*` bindings adjust the focused parameter (A plus a direction by default).
On the Engine row, hold `engine_modifier` (A) and use the navigation bindings
to change engines. `clear_sample` purges the focused sampler instrument on its
Sample row. `clear_table` clears the table assignment. Both default to B+A.
Song cursor movement uses `song.down/up/left/right`. B+Down/Up moves the song
offset by sixteen rows. L+Down/Up jumps to the next/previous section of the
current track. In a selection the four `song.selection_*` direction bindings
move the selection endpoint. On X64, a movement binding must explicitly require
SELECT to work while SELECT is held. The recording shortcut takes priority.
In Song, A (`song.insert`) recalls the last Chain on an empty step or remembers
the selected Chain. Press A again (`song.next_unused`) to assign a free Chain.
To clone the selected Chain, hold L, tap B (`song.begin_clone_selection`), then
tap A (`song.clone`). Its Phrases are initially shared with the original.
Keep L held and tap A again (`song.deep_clone`) for independent Phrase copies.
Repeated references to the same Phrase share one copy within the new Chain.
Song mute (R+B), solo (R+A) and unmute all (R+L) have separate normal and
selection bindings. Mute and solo affect every channel in the selected range.
Release B or A first while holding R to undo the toggle. Release R first to
latch it. When changing modifiers, set `song.mute_release` or `song.solo_release`
to the button that stays held for momentary operation.
B+Left or B+Right toggles Song/Live mode. START sends the usual playback
request; R+START requests a stop during playback. In Live mode these address
the current channel or the selected channel range (`song.selection_play` and
`song.selection_stop`). Outside a selection, B+START requests an immediate
trigger, L+START launches the current row across all channels in Live mode,
and L+Left/Right nudges the tempo down/up.
R+Right opens the Chain under the cursor if the cell contains one. R+Up opens
Project settings and R+Down opens Mixer. Edit `song.chain`, `song.project`
and `song.mixer` for normal navigation, or their `song.selection_*` entries
for navigation while a selection is active.
In Song, A+Left/Right changes the Chain number by one, and A+Down/Up by
sixteen. When remapping these, set `song.edit_hold` to their shared modifier
to reserve only the final Chain number. B+A cuts a cell and L+A inserts the
clipboard. While selecting, B+L extends the selection, B copies, and A+L cuts.
Cuts move later rows up within the selected tracks. Pasting inserts rows and
clips the clipboard at the right and bottom edges of the song.
In Phrase, the arrows move the cursor. In selection mode the four
`phrase.selection_*` directions move its endpoint within the current Phrase.
In normal mode B+Left/Right moves to a neighbouring channel with a Chain on
the current Song row. B+Up/Down moves to the adjacent Chain row if it holds
a Phrase, keeping the cursor on the same Phrase step.
R+Left opens Chain and R+Right opens Instrument. Each has a separate
`phrase.selection_*` binding while selecting. Instrument uses the instrument
on the cursor row, then the nearest row with an instrument, then the last
instrument remembered by the Phrase editor. In normal mode R+Down opens Table
and R+Up opens Groove. A TABL command on the current row selects the table.
If both command columns contain TABL, the first one takes precedence.
In the Song sequencer mode, START in Phrase toggles Phrase playback on the
current track. R+START toggles Song playback from the current Song and Chain
position. R+B toggles mute in normal editing. During a selection R+B still
interpolates. R+A solos the current track, and R+L unmutes all tracks.
Selection has separate playback, solo and unmute-all bindings. For momentary
mute/solo, release B/A while R remains held. Release R first to latch.
In Phrase, `edit_left/right` and `edit_down/up` adjust the current cell. In
parameter columns Left/Right selects a hex digit and Up/Down changes it. In
command columns Left/Right cycles commands, and `command_picker_up/down`
opens the picker. The four `selection_edit_*` bindings adjust selected notes,
velocities and instruments. All default to A plus the corresponding direction.
`audition` (A) previews the note or instrument on press while stopped.
`audition_hold` (A) keeps preview active through subsequent button presses.
In Phrase, B+A cuts at the cursor. Cutting a note also takes its velocity and
instrument. Cutting a command also takes its parameter. L+A pastes, wrapping
from step 0F to 00 within the same Phrase. To select, press B+L and then move
the cursor. B+L extends the active selection across all columns, then across
all steps. B copies, A+L cuts, and B+R interpolates between the endpoints of
a single note, velocity or parameter column. The initial B+L binding
(`phrase.begin_clone_selection`) also arms the existing clone gesture.
A (`phrase.insert`) recalls the last note, instrument or command on an empty
cell, or remembers the value on an occupied cell. In the instrument column,
press A again (`phrase.next_instrument`) to select a free sampler slot. In
either TABL parameter column, the second press (`phrase.next_table`) assigns a
free table. To clone, hold L, tap B, release B and tap A while still holding L.
In the note, velocity and instrument columns this copies the row's instrument
(`phrase.clone_instrument`). On a TABL parameter it copies the referenced
table (`phrase.clone_table`). The current row then references the copy.

The shared matcher handles the currently held buttons and the existing press
or release event. Chain retains its existing sequence state machine with
configurable transition bindings. The matcher does not synthesize button events,
add a double-click interval, change autorepeat timing, or implement a separate
sequence recognizer.
Subsequent migrations need explicit action/state definitions and regression
tests for those behaviors before exposing them in this file.

## Editing a binding

For example, change Groove's clear action from B+A to L+A:

```xml
<BIND action="groove.clear" keys="L+A" exclude="B" event="press" />
```

Replace the existing entry; do not add a second entry for the same action.
This action changes only Groove's current step. Other views retain B+A.

- `action` is a stable action name, listed below.
- `keys` lists buttons that must **all** be held, joined with `+`.
- `exclude` lists buttons that must **all** be absent. It defaults to empty
  when omitted. Keep the exclusions when changing a default binding unless
  you intentionally want it to operate with additional modifiers.
- `event="press"` matches button-down events, including existing arrow repeats.
- `event="release"` matches button-up events against the buttons **still held
  after release**. It does not identify which button was released. For example,
  `keys="L" exclude="A"` matches releasing A while L remains held, but also
  releasing another button while those conditions hold. Use this only when
  that behavior is intended; release-specific gestures need further migration.

Supported names are `A`, `B`, `L`, `R`, `START`, `SELECT`, `LEFT`, `RIGHT`, `UP`,
and `DOWN`, with exact capitalization and no whitespace inside `keys` or
`exclude`. These are logical pad buttons; physical keyboard/controller mappings
remain in `config.xml` and `mapping.xml`.

In Groove, all matching actions run in the established handler order, not XML order.
Mixer, FX and Table selection mode have consuming actions and priorities as
described below.
Overlapping bindings are allowed because existing controls can trigger several
actions together. For example, the original B+A+RIGHT advances the groove and
clears its current step. Changing one action does not automatically suppress
another action with an overlapping binding. To make L+A exclusively clear,
add L to the `exclude` list of any other binding that would also match it.

Holding a direction retains the existing `KEYDELAY`/`KEYREPEAT` behavior.
Mapping an action to a button without a direction does not give it autorepeat.
There is no hot reload or graphical shortcut editor in this stage.

## Groove actions

| Action | Default | Effect |
| --- | --- | --- |
| `groove.previous` | B+LEFT | Previous groove |
| `groove.next` | B+RIGHT | Next groove |
| `groove.previous_16` | B+DOWN | Go back 16 grooves |
| `groove.next_16` | B+UP | Go forward 16 grooves |
| `groove.clear` | B+A | Clear current step |
| `groove.decrease` | A+LEFT | Decrease current step by one tick |
| `groove.increase` | A+RIGHT | Increase current step by one tick |
| `groove.decrease_sync` | A+DOWN | Decrease the high hex digit |
| `groove.increase_sync` | A+UP | Increase the high hex digit |
| `groove.insert` | A alone | Initialize empty step |
| `groove.phrase` | R+DOWN | Switch to Phrase view |
| `groove.play_from_start` | R+START | Existing modified Phrase playback action |
| `groove.down` | DOWN | Next step |
| `groove.up` | UP | Previous step |
| `groove.play` | START | Existing Phrase playback action |

The XML exclusions preserve the old modifier precedence: B, then A, then R.
Groove steps range from `01` to `FF` ticks. Empty steps display as `--`.

## Mixer actions

| Action (`mixer.` prefix) | Default | Effect |
| --- | --- | --- |
| `mute` | R+B | Toggle current channel's mute |
| `solo` | R+A, without B | Toggle current channel's solo |
| `unmute_all` | R+L | Unmute all channels |
| `previous_view` | R+UP | Return to previous view |
| `fx` | R+DOWN | Open effects |
| `table` | R+RIGHT | Open table |
| `play_from_start` | R+START | Existing modified Song playback action |
| `clear` | B+A | Restore volume to FF or turn HPF/LPF/send off |
| `edit` | A | Enter value-edit handling, including LPF notification |
| `increase_coarse` | A+UP | Increase send/volume by 16; increase LPF by 10% |
| `decrease_coarse` | A+DOWN | Decrease send/volume by 16; decrease LPF by 10% |
| `increase_fine` | A+RIGHT | Increase send/volume by 1, LPF by 10 Hz; next HPF mode |
| `decrease_fine` | A+LEFT | Decrease send/volume by 1, LPF by 10 Hz; previous HPF mode |
| `up`, `down`, `left`, `right` | Corresponding direction | Move row/channel |
| `play` | START | Existing Song playback action |
| `selection_play_from_start` | R+START | Modified playback in existing selection state |
| `selection_play` | START | Playback in existing selection state |
| `mute_release` | R still held, release event | Toggle mute again when Mute mode is active |
| `solo_release` | R still held, release event | Toggle solo again when Solo mode is active |

The XML exclusions reproduce the original priorities. On press, Mixer first
checks mute, solo, unmute-all, previous-view, FX, table, modified playback, and
clear in that order; the first match consumes the event. Then it handles value
adjustments or `edit`, consuming the event. Finally it checks cursor movement
and normal playback. Selection mode uses its two separate playback bindings.
The selection state is retained for compatibility; these bindings do not add
a way to start a Mixer selection.

Each directional value adjustment can be remapped independently of `edit`.
The selected row still determines the operation and clamping/off behavior.
Simultaneous adjustments retain the previous per-row handling: send combines
deltas, volume uses the last matching direction (up, down, right, left), LPF
uses the first matching direction in that order, and HPF prefers right to left.
Coarse adjustments do not change HPF. `edit` alone preserves the old A-only
behavior, including displaying the LPF value.

Mixer press bindings must keep `event="press"`; its two release bindings must
keep `event="release"`. A different event is rejected rather than silently
ignored. Mute/Solo modes remain in the view's existing state machine. For the
default gesture, release B/A while holding R to undo the temporary mute/solo;
release R first to leave it latched. The release bindings are considered only
while the corresponding mode is active. As before, releasing any other button
while R is held can also trigger that mode's release action.

When changing the modifier for Mute/Solo, edit the press **and** release binding.
For example, use L+B for Mute with L as the release modifier:

```xml
<BIND action="mixer.mute" keys="L+B" exclude="" event="press" />
<BIND action="mixer.mute_release" keys="L" exclude="" event="release" />
```

This changes only Mixer. Mute/Solo controls in other views are not migrated yet.

## FX actions

| Action (`fx.` prefix) | Default | Effect |
| --- | --- | --- |
| `mixer` | R+UP | Return to Mixer |
| `play_from_start` | R+START, without UP | Existing modified Song playback action |
| `reset` | B+A, without R | Reset focused parameter to its default |
| `play` | START, without R/B | Existing Song playback action |
| `decrease_coarse` | A+DOWN | Decrease focused parameter by 16 |
| `increase_coarse` | A+UP | Increase focused parameter by 16 |
| `decrease_fine` | A+LEFT | Decrease focused parameter by 1 |
| `increase_fine` | A+RIGHT | Increase focused parameter by 1 |
| `down`, `up` | DOWN / UP | Select next/previous parameter, wrapping at the ends |
| `right`, `left` | RIGHT / LEFT | Horizontal field navigation (same field in the current single-column layout) |

FX bindings require `event="press"`. Releases do nothing, as before. The XML
exclusions retain the original modifier and playback priorities. The handler
checks `mixer`, `play_from_start`, `reset`, and `play` in that order; the first
match consumes the event. Value adjustments then consume the event before
field navigation. Each adjustment can be remapped independently.

The existing field code applies simultaneous adjustments in the order down,
up, left, right, clamping to 0–255 after each step. Navigation also retains the
existing field traversal order (down, up, right, left). A alone has no action
on these numeric fields and therefore has no separate binding. Reset restores
the parameter's default, not necessarily zero.

For example, replace `fx.increase_fine` with this entry to increase the focused
parameter using L+RIGHT:

```xml
<BIND action="fx.increase_fine" keys="L+RIGHT" exclude="R+B+START" event="press" />
```

## Table actions

Both Table screens share these bindings: the screen reached from Phrase and
the screen reached from Instrument. All require `event="press"`; releases are
ignored, as before. Once the selector opens, its own bindings take over.

| Action (`table.` prefix) | Default | Effect in normal mode |
| --- | --- | --- |
| `previous`, `next` | B+LEFT / B+RIGHT | Previous/next table |
| `previous16`, `next16` | B+DOWN / B+UP | Move back/forward 16 tables |
| `cut` | B+A | Cut current cell; commands include their parameter |
| `select` | B+L | Enter selection mode |
| `decrease_coarse`, `increase_coarse` | A+DOWN / A+UP | Open command selector on command columns; decrease/increase selected hexadecimal digit on parameter columns |
| `decrease_fine`, `increase_fine` | A+LEFT / A+RIGHT | Previous/next command on command columns; select higher/lower hexadecimal digit on parameter columns |
| `insert` | A alone | Existing last-value insertion action |
| `paste` | A+L | Paste clipboard and advance cursor by clipboard height |
| `parent` | R+UP | Return to Phrase or Instrument, depending on Table context |
| `view_left` | R+LEFT | From Instrument Table to Phrase Table; from Phrase Table to Mixer |
| `view_right` | R+RIGHT | From Phrase Table to Instrument Table |
| `play_from_start` | R+START | Existing modified Phrase playback action |
| `down`, `up`, `left`, `right` | Direction | Move cursor |
| `play` | START | Existing Phrase playback action |

The `coarse`/`fine` action names follow the other views, but Table's parameter
editor operates on hexadecimal digits rather than fixed +/-16 and +/-1 values.
The existing value/command editing routines and clipboard behavior are retained.

| Action (`table.selection_` prefix) | Default | Effect in selection mode |
| --- | --- | --- |
| `extend` | B+L | Expand to full row, then full table on the next press |
| `interpolate` | B+R, without L | Interpolate between endpoints of a single parameter-column selection |
| `copy` | B, without L/R | Copy selection and return to its origin in normal mode |
| `cut` | A+L, without B | Cut selection and return to its origin in normal mode |
| `parent` | R+UP, without A/B | Return to Phrase, preserving the existing behavior in both Table contexts |
| `play_from_start` | R+START, without A/B | Modified Phrase playback |
| `down`, `up`, `left`, `right` | Direction, without A/B/L/R | Change selection endpoint |
| `play` | START, without A/B/L/R | Phrase playback |

Normal-mode actions execute in the listed order, including multiple matching
actions. Default exclusions retain the previous B, A, R priority. Selection
mode is chosen at the start of a press event; entering selection does not also
extend it on that same event. The selection anchor is initialized on the next
press, as before. Releases between presses do not change the selection.

In selection mode, `extend`, `interpolate`, `copy`, and `cut` are checked in that
order and consume the event on the first match. The remaining selection actions
then follow the listed order. Unlike normal cursor movement, selection movement
is blocked by L in the default configuration.

To change the repeated selection gesture, change both `table.select` and
`table.selection_extend`. For example, set both to `keys="SELECT+B"` to use
SELECT+B to enter and then extend the selection. Normal paste and selection cut
are separate actions even though both default to L+A; edit both if you want to
move that shared gesture. Other views still use their original selection keys.

## Chain actions

Chain has normal, selection, pending-insert, and pending-clone states. Bindings
are considered in the state that already owns the gesture; changing a key does
not remove that state requirement. All bindings below use `event="press"`
except the two explicit Mute/Solo release bindings.

| Action (`chain.` prefix) | Default | Effect in normal mode |
| --- | --- | --- |
| `previous_channel`, `next_channel` | B+LEFT / B+RIGHT | Find a populated neighboring channel in Song |
| `previous_chain`, `next_chain` | B+UP / B+DOWN | Find the previous/next populated Song row in the current channel |
| `cut` | B+A | Cut current cell |
| `begin_clone_selection` | B+L | Enter the pending clone/selection state |
| `mute` | B+R | Toggle current channel mute |
| `decrease_coarse`, `increase_coarse` | A+DOWN / A+UP | Change phrase number by 16, or transpose by 12 semitones |
| `decrease_fine`, `increase_fine` | A+LEFT / A+RIGHT | Change focused value by 1 |
| `paste` | A+L | Paste clipboard and advance by its height |
| `insert` | A alone | Insert/remember last phrase; arm next-unused insertion on the phrase column |
| `solo` | A+R, without B | Toggle current channel solo |
| `song` | R+LEFT | Open Song |
| `phrase` | R+RIGHT | Open current phrase if the row is populated |
| `mixer` | R+DOWN | Open Mixer |
| `play_from_start` | R+START | Existing modified Chain playback action |
| `unmute_all` | R+L | Unmute all channels |
| `down`, `up`, `left`, `right` | Direction, without A/B/R/L | Move cursor |
| `play` | START, without A/B/R/L | Existing Chain playback action |

| Action (`chain.selection_` prefix) | Default | Effect in selection mode |
| --- | --- | --- |
| `copy` | B alone | Copy and return to selection origin |
| `mute` | B+R | Existing channel mute action |
| `extend` | B+L | Expand to both columns, then all rows |
| `decrease_coarse`, `increase_coarse` | A+DOWN / A+UP | Change selected values by 16 or 12 according to the current column |
| `decrease_fine`, `increase_fine` | A+LEFT / A+RIGHT | Change selected values by 1 |
| `cut` | A+L | Cut selection and return to its origin |
| `solo` | A+R | Existing channel solo action |
| `song`, `phrase`, `mixer` | R+LEFT / R+RIGHT / R+DOWN | Same view navigation as normal mode |
| `play_from_start`, `unmute_all` | R+START / R+L | Modified playback / unmute all |
| `down`, `up`, `left`, `right` | Direction, without A/B/R | Move selection endpoint; L is allowed here, as before |
| `play` | START, without A/B/R | Chain playback |

Default exclusions retain B, A, R priority. Within the selected handler,
matching actions run in the table order (not XML order). Overlapping custom
bindings can therefore perform more than one action. Existing selection and
mute/solo state transitions are preserved, including their interaction.

### Chain sequences and release bindings

| Action (`chain.` prefix) | Default | State-specific role |
| --- | --- | --- |
| `next_unused` | A alone | After `insert` on the phrase column, allocate the next unused phrase |
| `clone` | A+L | After `begin_clone_selection`, clone the current phrase |
| `edit_hold` | A held | Keep phrase-number editing pending until a later normal-mode press without this binding |
| `mute_release` | R still held, release event | Undo temporary mute when Mute mode is active |
| `solo_release` | R still held, release event | Undo temporary solo when Solo mode is active |

The default next-unused gesture is A, release, A. There is no timing threshold:
it uses the existing pending-insert state. A subsequent press that does not
match `next_unused` proceeds through the normal state handling instead.

For cloning, hold L, press B, release B, then press A while L remains held.
After `begin_clone_selection`, a press that does not match `clone` instead
enters selection mode and is immediately processed using the selection bindings.
For example, a direction starts extending a selection, and another L+B expands
the selection to both columns. Releases alone do not cancel the pending state.
Chain does not have Song's deep-clone action.

After `next_unused` or `clone` matches, its configured required buttons are
removed before processing the remaining held buttons. This preserves the old
behavior with extra directions and prevents the completed gesture from also
inserting/pasting. The gesture is consumed even if no free phrase is available.

Change related bindings together. For example, change `insert` and
`next_unused` to the same exact new key for a remapped double press; change
`begin_clone_selection`, `clone`, and `selection_extend` when moving the L-based
clone/selection gesture. Use `exclude` to avoid overlaps with other actions.

`edit_hold` is an editing-state condition, not a data-changing action. The
existing allocator marks the final edited phrase as used on the next normal-mode
press without this condition, not on release. If moving value editing away from
A, update `edit_hold` to the new editing modifier too.

Mute/Solo release bindings work like their Mixer equivalents: release B/A first
while R remains held for a temporary action; release R first to latch it. When
moving the modifier, update the normal action, selection action, and corresponding
release binding. Other views retain their own bindings.

## Command selector

The `command_selector.up`, `.down`, `.left` and `.right` bindings move the
highlight and preview the selected command. The defaults are the corresponding
directions. A confirms (`command_selector.confirm`). B cancels and restores
the command that was present when the dialog opened (`command_selector.cancel`).
All six bindings use `event="press"`. Overlapping chords resolve in the order
up, down, left, right, confirm, cancel. For example, holding A while pressing
an arrow moves the highlight and keeps the selector open.

## Startup project picker

The eight `project_picker.*` entries control the startup project list. UP/DOWN
select one entry and B+UP/DOWN moves by 20 entries with wraparound. LEFT/RIGHT
cycle through Load, New and Exit. A activates the highlighted button. B+A opens
the deletion confirmation for the selected entry. The prompt takes priority
over paging, which takes priority over activation and single-step navigation.
All bindings use `event="press"`.

## New project name

The `new_project.*` entries apply to both New and Save As. In the form, A activates
the focused name field or button (Random, Ok, Cancel). A+UP/DOWN increments or
decrements the current character. UP/DOWN switch between the name and buttons.
LEFT/RIGHT move the text cursor or the button selection.

Inside the on-screen keyboard, `keyboard_*` bindings take over. Arrows select a
key, A inputs it, B erases, and L/R move the text cursor. START or the on-screen
OK key returns to the form. Choose Ok on the form to accept the name. Both
contexts use press events. Direction repeats retain the normal pad repeat rate.

## Confirmation dialogs

In dialogs with Ok, Yes, No or Cancel buttons, LEFT and RIGHT cycle through
the available answers. A accepts the highlighted answer. A Yes/No dialog opens
with No highlighted. If A and a direction arrive together, the dialog returns
the answer highlighted before that movement.

The three `message_box.*` press bindings each have a matching `_release`
entry. The original dialog also responds to keys still held when another key
is released. Releasing A alone leaves no keys held. To move confirmation to
SELECT throughout the gesture, set both `message_box.confirm` and
`message_box.confirm_release` to `keys="SELECT"`, keeping their event types.

## Sample import browser

The `sample_browser.*` entries control the file list and Listen, Import and Exit.
UP/DOWN move one entry and stop the preview. B+UP/DOWN move by 15 entries.
LEFT/RIGHT change the highlighted button. A activates it. A+UP/DOWN move first,
then perform the highlighted action on the new entry. This can import a file
when Import is highlighted.

START previews a file or opens a selected folder. START+UP/DOWN browse with
preview. START+RIGHT imports the selected file and START+LEFT goes to the parent
folder within the sample library. The combined actions (for example
`preview_next`) have their own bindings and work independently of `preview`.
All entries use press events.

## Recording on Linux X64

In Song, `song.record` (SELECT+A) starts capture on an empty slot with playback
stopped and rendering disabled. Record Options opens from Project as a full-page
view and keeps P highlighted in the navigator. UP/DOWN select Channels,
Metronome, Metronome volume or Count in. A+LEFT/RIGHT change the selected value;
R+LEFT returns to Project (`record_options.*`). The former `record_options.toggle`
entry is still accepted in older shortcut files but no longer changes a value.

On Linux X64, open Settings from Project and use A+LEFT/RIGHT on Font to cycle
through Original, Digital and Monster. A custom startup font is also available
in the cycle. The choice applies immediately and is restored on restart from
`font_selection` beside the executable. R+LEFT returns
to Project. Settings shares these bindings with Record Options.

During count-in, START cancels. During capture, START stops and opens the take
decision. These are separate bindings (`record.cancel_count_in`, `record.stop`).
UP/DOWN select save, discard or record again, and A activates the selection.
A or B closes a recording error. All recording bindings use press events.
When changing a chord, remove its new required buttons from `exclude`.

## Sample editor

The twelve `sample_editor.*` bindings apply inside the waveform editor.
B returns to Instrument, START previews or stops, and L toggles zoom. DOWN/RIGHT and UP/LEFT
move between rows. The four `edit_*` bindings (A plus a direction) adjust the
selection digits, operation or filename according to the current row.

The `tap` binding arms on press. Process, Undo, Save As, Overwrite and Exit
execute on the final release of an isolated tap within `SHORT_PRESS_MS`.
Additional buttons or an editing action cancel the tap. For a multi-button tap,
hold the full chord briefly and then release all its buttons. Choose keys whose
individual presses are free on that row. The opening press is consumed.

## Validation and testing

Missing actions use their compiled defaults. Invalid XML, unknown actions or
binding attributes, duplicate action entries, unknown/repeated button names,
empty key combinations, and required/excluded conflicts reject the complete
configuration. Startup then logs the problem and keeps the defaults.
Removing `shortcuts.xml` restores defaults on the next launch.

Run `bash tests/run_shortcuts_test.sh` from the repository root. This builds the
X64 application and tests the default and XML action masks against the original
Groove branch logic for all 1,024 pad states and both event types. It also checks
edited bindings, invalid configurations, atomic rejection, and release-mask
semantics. The Mixer test compares all 1,024 pad states and both event types
against its old branch priorities, and invokes the real Mixer handler for
volume/filter/send editing, clearing, cursor movement, repeated input, both
Mute/Solo release orders, and changed bindings. It uses a stub window and audio
service; it does not exercise a displayed GUI, audio playback, or real controller
hardware. The FX regression compares the real handler with the frozen original
handler for every pad state and both event types on all five parameters, at
values 0, 100, and 255, using both XML and fallback defaults. It checks parameter
values, focus, view switching and captured playback requests, plus edited
bindings and repeat/release behavior. Playback requests are intercepted by the
test; no audio playback is performed. The Table tests compare binding resolution
for all 1,024 pad states, both event types and both editing modes, against the
old branch logic. Tests with the real handler cover hexadecimal digit editing,
selection expansion, interpolation, copy/cut/paste sequences, command-and-parameter
cutting, both Table navigation contexts, and edited bindings. Playback and modal
opening requests are intercepted; the command popup UI is not exercised.
Chain tests compare all pad states in normal/selection modes and the state-specific
binding predicates. Real-handler tests cover next-unused insertion, full phrase
cloning (including velocity and both command lanes), selection/clipboard sequences,
value editing and allocation, channel/Chain and view navigation, Mute/Solo release
orders, exhausted allocation, and remapped insert/clone gestures with remaining
buttons. Chain playback requests are intercepted rather than played.

Before proposing a PR, the human author must read the entire diff, test the
changed behavior on a real device (record hardware and OS), understand and be
able to explain the design, and take responsibility for investigating later
regressions. Other supported platform builds and device tests remain required.
