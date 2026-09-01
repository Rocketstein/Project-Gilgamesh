# Developer Console System

## Purpose

This document describes the current developer-console implementation: how engine logs and command output reach the UI, how filtering works, how commands execute, and how the Output Log participates in the developer-tools dockspace.

The console is compiled only when `GILGAMESH_ENABLE_DEVELOPER_TOOLS` is enabled. The engine logging system itself remains independent of ImGui and the developer-tools library.

## Architecture

```text
Engine and application threads
  -> GILGAMESH_LOG
  -> Logger
      -> DebugOutputSink (_DEBUG only)
      -> ConsoleLogSink (developer tools only)
          -> ConsoleBuffer

Console command input
  -> OutputLogPanel
  -> pending command in main.cpp
  -> CommandRegistry on the next frame
  -> ConsoleCommandOutput
  -> ConsoleBuffer

ConsoleBuffer
  -> delta read by OutputLogPanel
  -> level and text filtering
  -> clipped ImGui table rendering
```

The important ownership boundary is that producers write entries into `ConsoleBuffer`; they do not know about ImGui. `OutputLogPanel` consumes the buffer and owns only its UI cache and search text. `ConsoleConfiguration` owns settings that must be shared between the UI and commands.

## Source map

| Responsibility | Source |
| --- | --- |
| Core logging types and levels | `Engine/Core/Logging/LogTypes.h` |
| Logger fan-out and sink registration | `Engine/Core/Logging/Logger.h`, `Logger.cpp` |
| Visual Studio debugger output | `Engine/Platform/Windows/Logging/DebugOutputSink.*` |
| Console entry types and bounded history | `DeveloperTools/Console/ConsoleBuffer.*` |
| Adapter from engine logs to console entries | `DeveloperTools/Console/ConsoleLogSink.*` |
| Shared console settings | `DeveloperTools/Console/ConsoleConfiguration.*` |
| Output Log UI, filtering, and command input | `DeveloperTools/Console/OutputLogPanel.*` |
| Command tokenization and dispatch | `DeveloperTools/Console/Commands/CommandLineTokenizer.*`, `CommandRegistry.*` |
| Built-in command definitions | `DeveloperTools/Console/Commands/BuiltInCommands.*` |
| Adapter from command responses to console entries | `DeveloperTools/Console/ConsoleCommandOutput.*` |
| Dockspace and initial layout | `DeveloperTools/Workspace/DeveloperToolsWorkspace.*` |
| Construction and per-frame orchestration | `main.cpp` |

## Entry model and buffer

Every displayed line is a `ConsoleEntry` containing:

- A monotonically increasing sequence number assigned by `ConsoleBuffer`
- A metadata variant identifying the entry type
- A message string
- A timestamp

There are three metadata variants:

1. `ConsoleLogMetadata` contains category, level, and source location.
2. `ConsoleCommandInputMetadata` identifies an entered command.
3. `ConsoleCommandOutputMetadata` contains a normal, warning, or error tone.

`ConsoleBuffer` is thread-safe and bounded. Its default capacity is 5,000 entries, but `main.cpp` currently constructs the application buffer with a capacity of 1,024. When full, pushing an entry discards the oldest entry before appending the new one.

Consumers use `ReadDelta()` rather than copying the entire buffer each frame. The panel supplies its last-seen sequence and receives:

- Newly appended entries
- The earliest sequence still retained by the buffer
- A boolean indicating whether the history changed

The panel removes locally cached entries that the bounded buffer has discarded, then appends the delta. Clearing reserves a new sequence number even though no entry is inserted; this ensures a consumer can detect that its cached history has become stale.

## Engine log path

`GILGAMESH_LOG(Category, Level, ...)` first calls `Logger::ShouldLog()`. If no sinks are registered, formatting and dispatch are skipped. Otherwise, `Logger` constructs one `LogEntry` and synchronously sends it to every registered sink.

With developer tools enabled, `ConsoleLogSink` converts each `LogEntry` into a `ConsoleEntry` while preserving its category, level, timestamp, message, and source location.

Debug builds also register `DebugOutputSink`. It independently writes a formatted copy to `OutputDebugStringA`, making logs visible in the Visual Studio Output window or DebugView. Removing that sink would not disable the in-process console, but it would remove the external diagnostic path for startup failures and crashes.

Sink registrations are RAII objects. Destroying a `LogSinkRegistration` unregisters its sink. Published sink lists are immutable snapshots, allowing logging threads to dispatch without holding the mutation mutex.

## Filtering

Filtering changes only what `OutputLogPanel` displays. It never removes entries from `ConsoleBuffer`.

### Log-level visibility

`ConsoleConfiguration` stores one visibility flag for each level:

- Trace
- Debug
- Info
- Warning
- Error
- Critical

Every level is enabled by default. The visibility flags are changed with commands rather than checkboxes:

```text
log.hidelevel debug
log.showlevel debug
```

Level arguments are ASCII case-insensitive, so `DEBUG`, `Debug`, and `debug` are equivalent. Invalid values return an execution error listing the accepted levels.

Only entries carrying `ConsoleLogMetadata` are affected by level visibility. Entered commands and command responses remain visible, ensuring that a command used to restore a level cannot hide itself.

`ConsoleConfiguration` increments `LogLevelVisibilityRevision()` only when a visibility value actually changes. `OutputLogPanel` compares that revision with its cached revision and rebuilds its filtered indices when necessary. The revision is deliberately specific to level visibility so unrelated future settings do not force a filter rebuild.

### Text search

Search uses allocation-free, ASCII case-insensitive substring matching. A log entry matches when the search text occurs in any of:

- Its message
- Its category name, such as `Renderer`
- Its level name, such as `Warning`

Command input and command output have no log category or level, so they match only against their message text.

Level visibility and search combine with logical AND. A structured log must have an enabled level and satisfy the text search.

The matching is intentionally ASCII-only. Messages are stored as UTF-8, but full Unicode case folding is not currently implemented.

### Filter cache

The panel does not copy matching entries into a second container. `RebuildFilteredIndices()` scans its cached entries in arrival order and stores the indexes of entries that pass both filters. The ImGui list clipper then renders only the visible slice of that index list.

The filtered index cache is rebuilt when:

- The buffer reports new, cleared, or discarded history
- The search input changes
- The log-level visibility revision changes

## Command system

### Submission lifecycle

`OutputLogPanel::Draw()` returns a trimmed command string when Enter is pressed. `main.cpp` stores it as `pendingCommand`. At the beginning of the next frame, before building the new ImGui UI, the application:

1. Writes the entered command into the buffer.
2. Executes it through `CommandRegistry`.
3. Writes any returned result into the buffer.

Deferring execution prevents command handlers from mutating the buffer while the previous frame's console widgets are being constructed.

### Parsing and dispatch

`CommandLineTokenizer` splits on whitespace and supports quoted arguments. Quotes are removed, and `\"` and `\\` escape a quote or backslash. An unterminated quote produces a tokenization error.

Command names are ASCII case-insensitive and may contain lowercase letters, digits, `_`, `-`, and `.`. Arguments otherwise preserve their spelling; individual commands decide whether their arguments are case-sensitive.

Each `CommandDefinition` supplies:

- A unique name
- A short description
- A canonical usage string
- A handler

A handler receives argument views, a reference to the registry, and an `ICommandOutput`. It returns success, usage error, or execution error. When a usage error has no custom message, the registry generates `Usage: ...` from the command definition.

### Built-in commands

| Command | Behavior |
| --- | --- |
| `clear` | Clears the shared console history. |
| `help` | Lists commands alphabetically. |
| `help <command>` | Shows a command's description and usage. |
| `echo <message>` | Writes its arguments back as one line. |
| `log.showlevel <level>` | Enables display of a log level. |
| `log.hidelevel <level>` | Disables display of a log level. |

`ConsoleCommandOutput` converts handler output into the same shared entry stream. Information, warning, and error output use different display tones. The `clear` command clears the command echo as well as all previous history and returns no result line.

### Adding a command

Add always-available commands to `CreateBuiltInCommandRegistry()` in `BuiltInCommands.cpp`. Keep dependencies explicit by passing UI-independent services or configuration objects into the registry factory and capturing them in handlers. Do not make an ImGui panel register its own commands.

For example, a command should modify `ConsoleConfiguration`, not `OutputLogPanel`. The panel observes configuration state and remains unaware of the command system.

## Output Log UI

The Output Log window uses the stable ImGui name `Output Log###OutputLog`. Text before `###` is the visible title; text after it provides a stable identity for docking and saved layouts.

Structured logs render category and level prefixes in distinct colors. Hovering a structured log shows its source file, line, and function. Command input is rendered with a `>` prefix, while command responses use their output tone.

New entries scroll to the bottom only when the view was already at the bottom. Scrolling upward therefore prevents incoming entries from pulling the user away from older history.

## Docking workspace

`DeveloperToolsWorkspace` submits a viewport-sized dockspace before any developer-tool windows are drawn. The central node is transparent, allowing the renderer output to remain visible wherever no tool is docked.

On the first run, `BuildDefaultLayout()` creates a bottom node occupying 30 percent of the viewport and docks Output Log there. This default is applied only when the dockspace node does not already exist. Afterward, normal ImGui docking allows the window to be:

- Docked at the top, bottom, left, or right
- Tabbed with another tool by dropping it on the center target
- Moved between tab groups
- Floated outside the dockspace
- Resized through dock-node splitters

DockBuilder is currently an internal ImGui API. Its use is isolated to `DeveloperToolsWorkspace.cpp`; individual panels should use only ordinary `ImGui::Begin()` windows.

ImGui persists docking nodes, split ratios, tabs, and window positions in `imgui.ini`. The default filename is relative to the process working directory. A stable application-data location has not yet been configured.

### Adding another dockable tool

To add a tool such as Content Browser:

1. Give it a stable name such as `Content Browser###ContentBrowser`.
2. Draw its ordinary ImGui window after `DeveloperToolsWorkspace::DrawDockSpace()`.
3. Add its sources to `DeveloperTools/CMakeLists.txt`.
4. Optionally add its exact stable name to `BuildDefaultLayout()`.

Docking two window names into the same default node makes them tabs automatically:

```cpp
ImGui::DockBuilderDockWindow(
    OutputLogPanel::WindowName,
    bottomNode);

ImGui::DockBuilderDockWindow(
    ContentBrowserPanel::WindowName,
    bottomNode);
```

Workspace placement and visibility should not be stored in `ConsoleConfiguration`; that configuration is reserved for console-specific behavior.

## Current limitations and extension points

- There is no command history navigation or autocomplete.
- Search has no regular expressions, category-only selectors, or Unicode case folding.
- There is no UI command for listing the current visible-level state.
- Tool-window visibility does not yet have a shared View menu.
- The ImGui settings path is not yet anchored to a stable application-data directory.
- Commands execute synchronously on the main thread and should remain short-running.
- Console history exists only in memory; there is no persistent file sink or crash log.

When the implementation changes, update this document alongside the relevant source so it remains the current architectural reference.
