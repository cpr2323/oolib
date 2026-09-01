# oolib

Shared JUCE utility code, extracted from the `Source/Utility` folders of
**A8Manager**, **ClutchEdit** and **SquidManager**.

Every file here is self-contained: the only includes are `<JuceHeader.h>` and
other headers in this folder. Nothing reaches into an owning application.

## Using it

These sources `#include <JuceHeader.h>`, so they have to be compiled as part of
a JUCE target rather than built as a standalone library. Add oolib as a
submodule and pull the source list into your own target:

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

## Deliberately not included yet

`DirectoryValueTree.cpp/.h` and `DirectoryDataProperties.cpp/.h` are still owned
by the individual applications.

A8Manager and SquidManager each rewrote `DirectoryValueTree` independently, one
day apart, to solve the same ValueTree thread-safety problem in different ways.
A8Manager's version also depends directly on application code
(`Assimil8or/Audio/AudioManager.h`, `Assimil8or/FileTypeHelpers.h`,
`SystemServices.h`), and SquidManager's has a Squid-specific sort rule for names
beginning with `bank `. Their `DirectoryDataProperties::TypeIndex` enums are also
incompatible: A8Manager has `folder, systemFile, presetFile, audioFile,
unknownFile` while SquidManager has `unknownFile, folder, systemFile, audioFile`.

These belong in oolib once the class is properly abstracted for client
configuration — types registered at runtime, with callbacks for identifying and
processing them. That work is deliberately separate from this initial extraction.
