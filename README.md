# OJfix

OJfix is a small Windows background helper for online judge workflows. It watches clipboard text, sends text or a direct GDI screenshot to an OpenAI-compatible chat completions API, and writes the model response back to the clipboard.

The app has no visible UI in Release builds. It runs as a single background process and is controlled by global hotkeys.

## Features

- Monitors clipboard plain-text changes.
- Sends the latest clipboard text to a model.
- Captures the full virtual desktop with GDI/WinAPI and sends it as an image prompt.
- Writes model output back to the clipboard.
- Supports multiple system prompts: Java, MSVC C++, GNU G++ C++17, and Python 3.
- Uses streaming responses and primary/fallback endpoint failover.
- Release build uses the Windows subsystem, static runtime, optimization, icon resources, and optional UPX compression.
- Enforces single-instance execution with a named mutex.

## Hotkeys

| Hotkey | Action |
| --- | --- |
| `Ctrl + Alt + 1` | Send current clipboard text to the model |
| `Ctrl + Alt + 2` | Capture the screen with GDI and ask the model |
| `Ctrl + Alt + 3` | Switch system prompt |
| `Ctrl + Alt + 4` | Toggle prompt-switch notifications |
| `Ctrl + Alt + 5` | Exit the background process |

Prompt-switch notifications are disabled by default.

## Configuration

OJfix reads API settings from environment variables first, then from `ojfix.ini` next to `OJfix.exe`, then from safe defaults.

Environment variables:

| Name | Description |
| --- | --- |
| `OJFIX_API_KEY` | API key for the OpenAI-compatible endpoint |
| `OJFIX_PRIMARY_API_BASE_URL` | Primary base URL, for example `https://api.openai.com/v1` |
| `OJFIX_FALLBACK_API_BASE_URL` | Optional fallback base URL |
| `OJFIX_API_MODEL` | Model name |

Example `ojfix.ini`:

```ini
[api]
key=sk-your-api-key
primary_base_url=https://api.openai.com/v1
fallback_base_url=
model=gpt-4.1-mini
```

The repository intentionally does not contain any real API keys. Keep local `ojfix.ini` files out of git.

## Build

Requirements:

- Windows 10 or later
- Visual Studio 2022 with MSVC v143
- Optional: UPX for compressed Release packaging

Build Debug:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\OJfix.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

Build Release:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\OJfix.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Package Release with UPX:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

The packaged executable is written to `dist\OJfix.exe`.

## Repository Layout

```text
include/ojfix/        Public project headers
src/                  C++ implementation files
resources/            RC file and icon assets
scripts/              Build and packaging scripts
config/               Example runtime configuration
```

## Logs

Logs are written to:

```text
%LOCALAPPDATA%\OJfix\OJfix.log
```

## License

MIT
