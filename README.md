# oolib

Shared JUCE utility code, extracted from the `Source/Utility` folders of
**A8Manager**, **ClutchEdit** and **SquidManager**.

Every file here is self-contained: the only includes are `<JuceHeader.h>` and
other headers in this folder. Nothing reaches into an owning application.

## Layout

```
oolib/
    Core/         small standalone primitives (Crc, LambdaThread)
    Debug/        logging and diagnostics (DebugLog, DumpStack, ValueTreeMonitor, WatchDogTimer)
    Directory/    directory scanning (DirectoryValueTree, DirectoryDataProperties)
    GUI/          components, look and feel, and GUI helpers
    Properties/   the shared application state schema (Root / Persistent / Runtime)
    ValueTree/    ValueTree infrastructure (Wrapper, Helpers, File)
```

Sources include each other by their namespaced path:

```cpp
#include "oolib/ValueTree/ValueTreeWrapper.h"
#include "oolib/GUI/CustomComboBox.h"
```

Nothing in the source refers to where oolib is checked out, so relocating it changes only the
`target_include_directories` line below. The `oolib/` prefix also keeps these headers from colliding
with a consuming project's own `GUI/` or `Properties/` folders.

## Using it

These sources `#include <JuceHeader.h>`, so they have to be compiled as part of a JUCE target
rather than built as a standalone library. Add oolib as a submodule and pull the source list into
your own target:

```cmake
include (submodules/oolib/oolib.cmake)

target_sources (MyApp PRIVATE ${OOLIB_SOURCES})
target_include_directories (MyApp PRIVATE ${OOLIB_INCLUDE_DIR})
```

## How the initial contents were resolved

27 files were byte-identical in every project that had them and were taken as-is.
4 existed in only one project (`MruListProperties` from A8Manager,
`RoundedSlideSwitch` from SquidManager) and were carried over unchanged. The
rest had diverged and were reconciled:

| File | Resolution |
| --- | --- |
| `CustomComponentMouseHandler.cpp/.h` | ClutchEdit copy (only removed a stale TODO comment). |
| `RuntimeRootProperties.h` | ClutchEdit copy (whitespace re-alignment only). |
| `ValueTreeWrapper.h` | A8Manager copy — a strict superset, adding `setForwardOffMessageThreadWrites` / `forwardedToMessageThread` so setters may be called off the message thread. |
| `ValueTreeHelpers.cpp/.h` | Two-way merge. From A8Manager: `callOnMessageThread`, `getMessageThreadSnapshot`, `replaceChildrenOnMessageThread`. From SquidManager: the `compareChidren` -> `compareChildren` spelling fix, skipping properties that begin with `_`, and the live (uncommented) `DebugLog` output. |
| `CustomTextEditor.h` | ClutchEdit copy, which was already the union of all subclasses across the three projects (`Int`, `Int32`, `Int64`, `Float`, `Double`). It also carries ClutchEdit's ordering in `setValue`: `setText` is called *before* `updateDataCallback`. A8Manager and SquidManager previously used the opposite order. |
| `ValueTreeMonitor.cpp/.h` | SquidManager's design, which routes output through a pluggable `outputFunction` instead of a hardcoded `Logger::outputDebugString`. Its refactor was incomplete and has been finished here: `valueTreeChildAdded`, `valueTreeChildRemoved` and `valueTreeParentChanged` built a log string and never emitted it, and `valueTreeChildOrderChanged` emitted a fragment without its header. All six callbacks now emit. |
| `FileSelectLabel.cpp/.h` | SquidManager copy (adds `canMultiSelect` and a `fileChooserOptions` member). The hardcoded, app-specific file chooser title was replaced with a `setDialogTitle` setter and a generic default. |

## Directory scanning

`DirectoryValueTree` scans a folder into a ValueTree and keeps it current. It carries no knowledge of
any particular application's file types. Clients describe those by registering them, each as a name
plus up to three callbacks:

| Callback | Purpose |
| --- | --- |
| `predicate` | identifies a file as being of this type |
| `decorator` | adds type specific properties to an entry, only during a full scan |
| `comparator` | orders entries within this type's section of a folder listing |

`registerFileType` returns the id that appears as the `type` property of matching `FileProperties`
entries, replacing the old compile time `TypeIndex` enum. Two ids exist without being registered:
`unknownTypeId` (0) when no predicate matches, and `folderTypeId` (1) for folders, which are
identified structurally rather than by a predicate.

Registered types are published into the `DirectoryDataProperties` tree, so any client holding that
tree can resolve a name to an id via `DirectoryDataProperties::getFileTypeId` without the registering
code having to pass the value around.

```cpp
directoryValueTree.init (runtimeRootProperties.getValueTree ());

const auto systemTypeId { directoryValueTree.registerFileType ("system",
    [] (juce::File file) { return FileTypeHelpers::isSystemFile (file); }) };
const auto audioTypeId  { directoryValueTree.registerFileType ("audio",
    [] (juce::File file) { return file.getFileExtension ().toLowerCase () == ".wav"; },
    [this] (juce::ValueTree entryVT, juce::File file) { /* add bit depth, sample rate, ... */ }) };

directoryValueTree.setSortOrder ({ DirectoryValueTree::folderTypeId, systemTypeId, audioTypeId,
                                   DirectoryValueTree::unknownTypeId });
```

Predicates are tried in registration order, so register cheap tests before expensive ones. Sort order
is separate from registration order, and defaults to folders, then registered types in registration
order, then unknown. `setComparatorForType` is the only way to attach a comparator to the built-in
folder and unknown sections, since those are never registered.

Register between `init ()` and the first scan. Registering later would resize the section list out
from under a running sort, and is asserted against.

**Threading:** predicates, decorators and comparators are called on the scan thread, never on the
message thread.

## Client migration notes

The three projects have not been updated to use oolib yet. When they are:

- Every `#include "Utility/X.h"` becomes `#include "oolib/<Category>/X.h"`. The category for each
  file is in the Layout section above.

- `FileSelectLabel` gained `setDialogTitle`. Without it, callers get a generic prompt instead of their
  app specific one. Affects `ZoneEditor.h` in A8Manager and `ChannelEditorComponent.h` in SquidManager.
- `ValueTreeHelpers::compareChidrenAndThierPropertiesUnordered` is now spelled `compareChildren...`.
  No callers outside the old Utility folders.
- `CustomTextEditor::setValue` calls `setText` before `updateDataCallback`. A8Manager and SquidManager
  previously used the opposite order, so both need testing.
- `DirectoryDataProperties::TypeIndex` is gone. Every `TypeIndex::x` reference becomes an id obtained
  from registration or from `DirectoryDataProperties::getFileTypeId`. 15 references across 3 files in
  A8Manager, 9 across 2 files in SquidManager.
- SquidManager's `systemFile` type was already unreachable: nothing produced it, so its `FileView`
  test for it could never be true. It simply goes away.
