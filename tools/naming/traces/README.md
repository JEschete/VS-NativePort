# Trace logs

One short Markdown log per runtime-tracing scenario, named `<scenario>.md` after the scenario in the
`trace:<scenario>:<observation>` ledger tokens that cite it (Docs\NAMING_CONVENTIONS.md §10.1, #445).

Each log records:

- **Scenario:** what was played, from which save or replay inputs, and the source revision.
- **Setup:** the emulator or native build, and the breakpoints or watch addresses taken from `TRACE_QUEUE.csv`
  (`touched_by`, `watch`).
- **Observations:** one line per settled queue row: the row id, the observation token and what was seen (value,
  reader, frame).
- **Limits:** what the scenario did not exercise.

Keep captures, memory dumps, save states and other disc-derived data out of the repository; cite their local paths.
After editing the ledger rows, regenerate the queue with `python tools\naming\trace_queue.py`.
