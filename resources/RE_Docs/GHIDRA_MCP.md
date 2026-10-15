# GhidraMCP: a narrower agent bridge

Research date: 2026-10-14 (US Eastern). Documentation reviewed; not installed,
registered, or exercised against this project.

## What it provides

[LaurieWired/GhidraMCP](https://github.com/LaurieWired/GhidraMCP) is a Ghidra
plugin plus an MCP bridge. Upstream documents binary decompilation/analysis,
function and data renaming, and listing functions, classes, imports, and
exports. Its README identifies Apache-2.0 licensing.

Unlike [REA](REA.md), this is focused on exposing a Ghidra workspace rather than
providing a broad collection of application-analysis providers.

**Recommendation: optional, after a successful manual PS1 Ghidra pilot.**
The main benefit would be letting an agent query an already-correct analysis
project instead of repeatedly copying disassembly and pseudocode by hand.

## Why it might be more appropriate than REA here

### Broader console use

The bridge is not inherently PlayStation-specific. Its usefulness follows the
Ghidra workspace's actual CPU language, loader, memory mapping, analysis, and
plugin compatibility. This makes it a candidate for other systems in the
[cross-system matrix](CROSS_SYSTEM_MATRIX.md), once their imports are validated.
The bridge itself does not add cartridge mappers, a 65816 processor, Game Boy
CPU semantics, a PS2 vector-unit model, or correct bank/overlay handling.

REA's documented Windows provider does not admit PS1 inputs. A Ghidra workspace
created with a PS1 loader is a plausible alternative route, but **interoperation
between GhidraMCP and that extension was not verified**. Neither project's
README is an end-to-end Vagrant Story compatibility claim.

The GUI path is the better first experiment: manually establish the loader,
overlay mapping, function boundaries, and GTE behavior, then ask the bridge to
query known results. Do not start by assuming the bridge's headless importer
will choose the correct PS1 loader and options.

## Documented prerequisites and client options

At research time, upstream specifies:

- Ghidra 12.1.4 and a matching GhidraMCP extension archive.
- `uv`, or Python 3.10+ for the ordinary bridge installation path.
- A separate optional headless/PyGhidra path with Python 3.10-3.13.
- GUI plugin installation through `File -> Install Extensions`, a restart,
  and enabling `GhidraMCPPlugin`.
- A default Ghidra bridge endpoint at `http://127.0.0.1:8080/`.
- MCP clients using stdio, or an explicitly launched streamable-HTTP MCP server.
- A VS Code configuration example using a `servers` object, not the
  `mcpServers` object used by some other clients.

Use the JDK required by the selected Ghidra distribution. A plugin source-build
requirement is not necessarily the same as the runtime requirement for every
Ghidra version.

The upstream examples execute code from a moving Git URL. For a project trial,
review and pin the bridge revision and extension release instead. No workspace
MCP configuration was created as part of this documentation task.

## Safe pilot and verification

1. Finish the manual [Ghidra import checks](GHIDRA.md).
2. Copy the analysis database before allowing agent access.
3. Bind both HTTP interfaces, if used, to loopback. Do not expose them to a LAN
   or assume that a localhost example supplies authentication.
4. Initially permit only read/query operations. The advertised rename tools
   mutate the analysis database; it is not an inherently read-only interface.
5. Query one known function's address and pseudocode. Compare with the GUI,
   including the currently selected program/overlay.
6. Test tool discovery in the actual VS Code client. A registered server or
   working connection in another agent does not establish availability here.
7. Save useful annotations explicitly and retain tool/version provenance.
   Review changes before promoting any names or inferred types to source.

Even a local MCP server can return retail-derived information to a remote
model. Do not transmit sensitive code, dumps, assets, or credentials to
third-party systems. A fully local analysis workflow remains the default.

## Limits

This bridge does not provide historical compiler matching, emulate the GTE,
resolve overlay lifetime automatically, or certify a native implementation.
Its headless persistence option is useful only after verifying that the
correct program and analysis settings actually survive between runs.

## Sources

- [Upstream README: features, plugin, VS Code, transports, headless setup](https://github.com/LaurieWired/GhidraMCP/blob/main/README.md)
- [Extension releases](https://github.com/LaurieWired/GhidraMCP/releases)
- [PS1 loader, a separate integration to validate](https://github.com/lab313ru/ghidra_psx_ldr)
