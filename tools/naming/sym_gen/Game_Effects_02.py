"""Game_Effects_02 globals and types from code read directly. Citations are resolved by exact text search (common.Gen).

Plugin globals: each plugin source binds a template macro to its own global (`#define VS_RING_COLORS D_...`) and then
includes the template header, whose use of the macro fixes the global's meaning; a few plugins index a ring color
table inline (`factorBytes = ((u_char*)D_...) + (column & 0x1C);`, the rings.h pattern). The generator finds each
global's binding in its own overlay directory (plugin overlays share addresses, so a name can recur in several) and
names it Effects::PlgNNN::<template member>, numbering repeats within one plugin. A template extern row and the
plugin's own definition of the same object (same overlay) are one object: the extern row is a view of the definition.
"""
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen, ROOT  # noqa: E402

g = Gen("Game_Effects_02")
c, G = g.c, g.G
L, M, H = "low", "med", "high"
E = "Native\\Game\\src\\Effects\\"
SRC = ROOT / "resources" / "rood-reverseWin" / "src"
ROWS = list(csv.DictReader(open(ROOT / "tools" / "naming" / "sym_batches" / "Game_Effects_02" / "globals.csv", encoding="utf-8")))

# macro: (member, disposition, purpose, template header, template line)
MACROS = {
    "RING_COLORS": ("ringColorFactors", "catalog", "Gives the ring template's color factors for each transparency mode.",
                    "EFFECT/rings.h", "factorBytes = (u_char*)VS_RING_COLORS + (column & 0x1C);"),
    "TILED_RING_COLORS": ("tiledRingColorFactors", "catalog", "Gives the tiled ring template's color factors for each transparency mode.",
                          "EFFECT/tiledRings.h", "factorBytes = &VS_TILED_RING_COLORS[column];"),
    "CLOSED_RING_COLORS": ("closedRingColorFactors", "catalog", "Gives the closed ring template's color factors for each transparency mode.",
                           "EFFECT/closedRings.h", "factorBytes = &VS_CLOSED_RING_COLORS[column];"),
    "SURFACE_VERTICES": ("surfaceVertices", "catalog", "Gives the surface template's 5 by 16 unit vertices, scaled by radius and height.",
                         "EFFECT/surfaces.h", "+ ((VS_SURFACE_VERTICES[row][column].vx * radiusValue) >> 12);"),
    "MIRROR_SURFACE_VERTICES": ("mirrorSurfaceVertices", "catalog",
                                "Gives the mirrored surface template's 5 by 16 unit vertices; the lower half is mirrored in height.",
                                "EFFECT/mirroredSurfaces.h", "+ ((VS_MIRROR_SURFACE_VERTICES[4 - row][column].vx * radiusValue)"),
    "MESH10_VERTICES": ("mesh10Vertices", "catalog", "Gives the mesh template's unit vertices (pad selects the vertex color).",
                        "EFFECT/meshes10.h", "(VS_MESH10_VERTICES[index].vx * scratch->unkA8.vx) >> 12;"),
    "MESH10_FACES": ("mesh10Faces", "catalog", "Gives the mesh template's triangles: three vertex indices and three texture offsets.",
                     "EFFECT/meshes10.h", "*(int*)&prim->poly.x0 = screen[VS_MESH10_FACES[index][0]];"),
    "PARTICLE_CORNER_STATE": ("particleEmissionCounter", "state",
                              "Steps the box corner (0-7) or ring position the particle template emits from next.",
                              "EFFECT/particleSetup.h", "VS_PARTICLE_CORNER_STATE = temp_a1 & 7;"),
    "TRAIL_CORNER_STATE": ("trailEmissionCounter", "state",
                           "Steps the box corner (0-7) or ring position the trail template emits from next.",
                           None, "VS_TRAIL_CORNER_STATE = temp_a1 & 7;"),
}
INLINE_RING = "factorBytes = ((u_char*){name}) + (column & 0x1C);"


def plugin_bindings(overlay: str, name: str):
    """(file, line, macro, include) for each binding of `name` in the overlay's sources, in source order."""
    out = []
    for p in sorted((SRC / overlay).glob("*.c")):
        rel = p.relative_to(SRC).as_posix()
        lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
        for i, ln in enumerate(lines):
            m = re.match(r"\s*#define VS_(\w+) " + re.escape(name) + r"\s*$", ln)
            if m:
                inc = next((re.search(r'"src/(EFFECT/\w+\.h)"', x).group(1) for x in lines[i + 1:]
                            if x.startswith("#include") and "src/EFFECT/" in x), None)
                out.append((rel, ln.strip(), m.group(1), inc))
            elif INLINE_RING.format(name=name) in ln:
                out.append((rel, ln.strip(), "RING_COLORS", None))
    return out


PLUGIN_ROWS = [r for r in ROWS if r["overlay"].startswith("EFFECT/PLG")]
canonical = {}
for r in PLUGIN_ROWS:
    key = (r["overlay"], r["name"])
    if r["segment"] == "c-defined":
        canonical[key] = r["id"]
counts: dict[tuple, int] = {}
unresolved = []
for r in PLUGIN_ROWS:
    key = (r["overlay"], r["name"])
    plg = re.search(r"PLG(\d+)", r["overlay"]).group(1)
    binds = plugin_bindings(r["overlay"], r["name"])
    if not binds:
        unresolved.append(r["id"])
        continue
    rel, line, macro, inc = binds[0]
    member, disp, purpose, header, hline = MACROS[macro]
    if macro == "TRAIL_CORNER_STATE":
        header = inc
    if key in canonical and canonical[key] != r["id"]:
        G(r["id"], "drop", f"Effects::Plg{plg}::{member}Extern", f"{E}Plugins\\Plg{plg}.c",
          f"Is the template's extern declaration of {canonical[key]}.", H,
          f"view-of:{canonical[key]};replaced-by:{canonical[key]};" + c((rel, line)))
        continue
    n = counts[(plg, member)] = counts.get((plg, member), 0) + 1
    final = f"Effects::Plg{plg}::{member}" + (str(n) if n > 1 else "")
    cites = [(rel, line)] + ([(header, hline)] if header else [])
    note = ";bound by the plugin source" if line.startswith("#define") else ";indexed inline like the rings.h template"
    G(r["id"], disp, final, f"{E}Plugins\\Plg{plg}.c", purpose, H if header or not line.startswith("#define") else M,
      c(*cites) + note)

if __name__ == "__main__":
    if unresolved:
        print("unresolved plugin globals:", unresolved)
    g.write(partial=True)
