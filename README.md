# UWUAutocorrect

`uwuifier.exe` accepts the same two positional arguments as before:

```bat
uwuifier.exe input.txt output.txt
```

It reads UTF-8 text from the input file and writes UTF-8 text to the output
file. This keeps the existing AutoHotkey v2 script working unchanged.

## Modes

The default mode is `classic`, which uses the original local C++ uwuifier.

To enable Ollama, copy `uwuifier.conf.example` to `uwuifier.conf` next to
`uwuifier.exe` and change:

```ini
mode=hybrid
ollama_model=llama3.2
```

Available modes:

- `classic`: original fast local behavior
- `ai`: sends the original text to Ollama
- `hybrid`: runs classic uwuification first, then asks Ollama to polish it

If Ollama is unavailable, slow, or returns an invalid response, the executable
falls back to classic mode by default.

## Config

Settings can be placed in `uwuifier.conf` next to the executable, in the current
working directory, or set as environment variables.

Config keys:

```ini
mode=classic
ollama_url=http://127.0.0.1:11434
ollama_model=llama3.2
ollama_timeout_ms=8000
ollama_temperature=0.35
ollama_max_tokens=512
fallback_to_classic=true
```

Environment variables:

```bat
setx UWUIFY_MODE hybrid
setx UWUIFY_OLLAMA_MODEL llama3.2
setx UWUIFY_OLLAMA_TIMEOUT_MS 8000
setx UWUIFY_OLLAMA_MAX_TOKENS 512
```

Start Ollama before using `ai` or `hybrid` mode:

```bat
ollama serve
ollama pull llama3.2
```
