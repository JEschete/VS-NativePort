// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg077.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg153.h"
#include "Effects/Plugins/Plg154.h"
#include "Effects/Plugins/Plg260.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/Ring.h"

// decomp: PLG069.BIN func_800F98E0 (src/EFFECT/PLG069.BIN/E0.c:2): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_MESH10_FUNCTION Effects_Plg069_Mesh10
#define VS_MESH10_VERTICES Effects_Plg069_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg069_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

// decomp: PLG077.BIN func_800F9818 (src/EFFECT/PLG077.BIN/18.c:8): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_MESH10_FUNCTION Effects_Plg077_Mesh10
#define VS_MESH10_VERTICES Effects_Plg077_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg077_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

// decomp: PLG129.BIN func_800FA424 (src/EFFECT/PLG129.BIN/0.c:7): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_RING_FUNCTION Effects_Plg129_Ring
#define VS_RING_COLORS Effects_Plg129_ringColorFactors
#define VS_RING_TRANSPOSED
#define VS_MESH10_FUNCTION Effects_Plg129_Mesh10
#define VS_MESH10_VERTICES Effects_Plg129_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg129_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

// decomp: PLG137.BIN func_800F98F8 (src/EFFECT/PLG137.BIN/F8.c:2): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_MESH10_FUNCTION Effects_Plg137_Mesh10
#define VS_MESH10_VERTICES Effects_Plg137_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg137_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

// decomp: PLG153.BIN func_800F9910 (src/EFFECT/PLG153.BIN/110.c:2): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_MESH10_FUNCTION Effects_Plg153_Mesh10
#define VS_MESH10_VERTICES Effects_Plg153_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg153_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

// decomp: PLG154.BIN func_800F9800 (src/EFFECT/PLG154.BIN/0.c:8): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_MESH10_FUNCTION Effects_Plg154_Mesh10
#define VS_MESH10_VERTICES Effects_Plg154_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg154_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

// decomp: PLG260.BIN func_800F9800 (src/EFFECT/PLG260.BIN/0.c:5): VS_MESH10_FUNCTION, instance of Effects/Templates/Mesh10.inl
#define VS_MESH10_FUNCTION Effects_Plg260_Mesh10
#define VS_MESH10_VERTICES Effects_Plg260_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg260_mesh10Faces
#include "Effects/Templates/Mesh10.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES

