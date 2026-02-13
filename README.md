# binagent (C)

`binagent` takes binary text from an AI agent and materializes two files:
- the raw binary text exactly as the agent produced it
- the decoded HTML generated from that binary

The tool does not design UI or inject templates. The agent controls the HTML structure.

## Why this exists

Many AI tools return text. This project enforces a strict contract where the model returns only binary bytes, and the C program converts that into a viewable HTML file.

Because AI outputs are probabilistic, two runs can and should produce different HTML structures, layouts, and behavior, even with similar prompts.

## Binary format contract

The agent output must be plain text where:
- each token is exactly 8 bits
- only `0` and `1` are allowed
- tokens are separated by whitespace
- no markdown, no prose, no code fences

Example token sequence:

```text
00111100 01101000 01110100 01101101 01101100 00111110
```

## Build

```bash
make
```

## Usage

```bash
./binagent materialize [--binary-in FILE|-] [--binary-out FILE] [--html-out FILE]
```

Defaults:
- `--binary-out output.binary.txt`
- `--html-out output.html`
- if `--binary-in` is omitted, input is read from `stdin`

## Typical workflow

1. Ask your AI agent for strict binary output only.
2. Save the response as a file (or pipe it directly).
3. Run `materialize`.
4. Open the generated HTML.

### File input

```bash
./binagent materialize \
  --binary-in agent_output.binary.txt \
  --binary-out run.binary.txt \
  --html-out run.html
```

### Stdin input

```bash
./binagent materialize \
  --binary-out run.binary.txt \
  --html-out run.html < agent_output.binary.txt
```

## Recommended agent prompt

```text
Output ONLY strict binary bytes for a complete standalone HTML file with inline CSS and JS.
Rules:
- each token must be exactly 8 bits, using only 0 or 1
- tokens must be separated by spaces
- no markdown, no code fences, no explanations, no extra text
Return only the binary stream.
```

## Smoke test

```bash
./scripts/smoke.sh
```

## Repository layout

- `src/main.c`: binary parser + materialization command
- `Makefile`: build target
- `scripts/smoke.sh`: quick end-to-end verification
- `agent_output.binary.txt`: sample binary fixture
- `agent_output.html`: sample decoded result
