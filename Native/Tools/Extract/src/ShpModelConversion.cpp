// The shp-gltf converter (ShpModelConversion.hpp).
#include "Extract/ShpModelConversion.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ActorAnimation.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
using Content::ActorAnimationDocument;
using Content::ConversionManifest;
using Content::ConversionOutput;
using Content::DataStore;
using Content::ImageDocument;
using Content::ImageFile;
using Content::kShpIndexCorners;
using Content::kShpMaxCorners;
using Content::kShpTriangleCorners;
using Content::ShpFile;
using Content::ShpModel;
using Content::ShpModelDocument;
using Content::ShpPolygon;
using Content::ShpTexture;
using Content::ShpTextureSize;
using Content::SourceDependency;

namespace {
namespace fs = std::filesystem;
using Json = Content::JsonDocument;
using Bytes = std::vector<std::byte>;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kShpDirectory = "OBJ";
constexpr std::string_view kModelIdPrefix = "model";
constexpr std::string_view kAnimationIdPrefix = "animation";
constexpr std::string_view kGenerator = "VS.Extract shp-gltf 2";
constexpr std::string_view kUnlitExtension = "KHR_materials_unlit";
constexpr std::string_view kVariantsExtension = "KHR_materials_variants";
// glTF buffer view targets, sampler filters and wrapping.
constexpr std::uint32_t kArrayBufferTarget = 34962;
constexpr std::uint32_t kElementArrayBufferTarget = 34963;
constexpr std::uint32_t kNearestFilter = 9728;
constexpr std::uint32_t kRepeatWrap = 10497;
constexpr std::size_t kViewAlignment = 4;
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint8_t kFullByte = 0xFF;
constexpr std::size_t kAxes = 3;
constexpr std::size_t kColorChannels = 3;
constexpr std::size_t kSkinSlots = 4;
// The SHP texture header: flags, width_byte, height_byte, palette_words.
constexpr std::size_t kTextureHeaderBytes = 4;
// u16 indices number up to this many corners.
constexpr std::size_t kMaxShortCorners = 0x10000;

void PutLittleEndian(Bytes& out, std::uint32_t value, std::size_t bytes) {
  for (std::size_t index = 0; index < bytes; ++index) {
    out.push_back(
        static_cast<std::byte>((value >> (index * kByteBits)) & kByteMask));
  }
}

void PutFloat(Bytes& out, float value) {
  PutLittleEndian(out, std::bit_cast<std::uint32_t>(value), sizeof(float));
}

// The disc file's stem: OBJ/4F.SHP -> 4F.
Result<std::string> StemOf(std::string_view disc_path) {
  auto stem = DiscStemOf(disc_path, kShpExtension);
  if (!stem) {
    return std::unexpected(
        std::format("'{}' is not an SHP character model", disc_path));
  }
  return std::move(*stem);
}

// --- The buffer.

struct AccessorSpec {
  std::uint32_t component_type = 0;
  bool normalized = false;
  std::size_t count = 0;
  std::string_view type;
  std::optional<std::uint32_t> target;
  Json min;  // null when not recorded
  Json max;
};

AccessorSpec SpecOf(std::string_view type, std::uint32_t component_type,
                    bool normalized) {
  return {.component_type = component_type,
          .normalized = normalized,
          .count = 0,
          .type = type,
          .target = std::nullopt,
          .min = Json(),
          .max = Json()};
}

class GltfBuffer {
 public:
  // `bytes` as a new buffer view starting on a 4-byte boundary.
  std::size_t AddView(std::span<const std::byte> bytes,
                      std::optional<std::uint32_t> target) {
    data_.resize(((data_.size() + kViewAlignment - 1) / kViewAlignment) *
                 kViewAlignment);
    Json view = Json::object();
    view["buffer"] = 0;
    view["byteOffset"] = data_.size();
    view["byteLength"] = bytes.size();
    if (target) view["target"] = *target;
    data_.insert(data_.end(), bytes.begin(), bytes.end());
    views_.push_back(std::move(view));
    return views_.size() - 1;
  }

  // An accessor over `bytes`, in a view of its own.
  std::size_t AddAccessor(const AccessorSpec& spec,
                          std::span<const std::byte> bytes) {
    Json accessor = Json::object();
    accessor["bufferView"] = AddView(bytes, spec.target);
    accessor["componentType"] = spec.component_type;
    if (spec.normalized) accessor["normalized"] = true;
    accessor["count"] = spec.count;
    accessor["type"] = std::string(spec.type);
    if (!spec.min.is_null()) accessor["min"] = spec.min;
    if (!spec.max.is_null()) accessor["max"] = spec.max;
    accessors_.push_back(std::move(accessor));
    return accessors_.size() - 1;
  }

  [[nodiscard]] const Bytes& Data() const { return data_; }
  [[nodiscard]] const Json& Views() const { return views_; }
  [[nodiscard]] const Json& Accessors() const { return accessors_; }

 private:
  Bytes data_;
  Json views_ = Json::array();
  Json accessors_ = Json::array();
};

// --- The mesh.

// One glTF vertex: a polygon corner.
struct Corner {
  std::uint16_t vertex = 0;
  std::uint16_t uv = 0;
  std::uint32_t color = 0;

  auto operator<=>(const Corner&) const = default;
};

// The polygons whose flags share kShpPrimitiveFlags, with their corners
// numbered in first use.
struct Primitive {
  std::uint8_t flags = 0;
  std::vector<Corner> corners;
  std::vector<std::uint32_t> indices;
  Json polygons = Json::array();
};

// A primitive's corner numbers, kept apart so moving a Primitive cannot throw.
using CornerNumbers = std::map<Corner, std::uint32_t>;

std::uint32_t NumberOf(const Corner& corner, Primitive& primitive,
                       CornerNumbers& numbers) {
  const auto [at, added] = numbers.try_emplace(
      corner, static_cast<std::uint32_t>(primitive.corners.size()));
  if (added) primitive.corners.push_back(corner);
  return at->second;
}

// What every primitive's corners read from the model.
struct MeshSource {
  const ShpModel* model = nullptr;
  const std::vector<std::int64_t>* origins = nullptr;
  std::vector<std::uint16_t> bones;  // by SHP vertex
  ShpTextureSize uv_scale;
};

// Each vertex's bone. The reader rebuilds the groups as runs of vertices on
// one bone, so every group must hold vertices and differ from the one before.
Result<std::vector<std::uint16_t>> VertexBones(const ShpModel& shp) {
  std::vector<std::uint16_t> bones;
  bones.reserve(shp.vertices.size());
  for (std::size_t group = 0; group < shp.groups.size(); ++group) {
    const Content::ShpGroup& entry = shp.groups[group];
    if (entry.end <= bones.size() || entry.end > shp.vertices.size() ||
        entry.bone >= shp.bones.size() ||
        (group != 0 && shp.groups[group - 1].bone == entry.bone)) {
      return std::unexpected(std::format(
          "vertex group {} (bone {}, ending at vertex {}) is empty, past the "
          "{} vertices, past the {} bones or on its predecessor's bone; the "
          "glTF model keeps groups as runs of vertices on one bone",
          group, entry.bone, entry.end, shp.vertices.size(), shp.bones.size()));
    }
    bones.resize(entry.end, entry.bone);
  }
  if (bones.size() != shp.vertices.size()) {
    return std::unexpected(std::format(
        "the vertex groups end at vertex {}; the model has {} vertices",
        bones.size(), shp.vertices.size()));
  }
  return bones;
}

Check AddPolygon(const ShpModel& shp, const ShpPolygon& polygon,
                 std::size_t ordinal, Primitive& primitive,
                 CornerNumbers& numbered, std::vector<bool>& used) {
  if (polygon.corners != kShpTriangleCorners &&
      polygon.corners != kShpMaxCorners) {
    return std::unexpected(
        std::format("polygon {} has {} corners", ordinal, polygon.corners));
  }
  std::array<std::uint32_t, kShpMaxCorners> numbers{};
  for (std::size_t corner = 0; corner < polygon.corners; ++corner) {
    const std::uint16_t vertex = polygon.vertices.at(corner);
    if (vertex >= shp.vertices.size()) {
      return std::unexpected(std::format("polygon {} names vertex {} of {}",
                                         ordinal, vertex, shp.vertices.size()));
    }
    used[vertex] = true;
    numbers.at(corner) = NumberOf(
        {.vertex = vertex,
         .uv = polygon.uvs.at(Content::ShpUvSlot(polygon.corners, corner)),
         .color = shp.colored ? polygon.colors.at(corner) : 0},
        primitive, numbered);
  }
  const std::size_t needed = polygon.corners == kShpTriangleCorners
                                 ? kShpTriangleCorners
                                 : kShpIndexCorners.size();
  for (std::size_t slot = 0; slot < needed; ++slot) {
    primitive.indices.push_back(numbers.at(kShpIndexCorners.at(slot)));
  }
  primitive.polygons.push_back(
      Json::array({ordinal, polygon.corners, polygon.flags, polygon.extra}));
  return {};
}

// The primitives in ascending flags, every polygon in ordinal order.
Result<std::vector<Primitive>> PlanPrimitives(const ShpModel& shp) {
  std::map<std::uint8_t, Primitive> by_flags;
  std::map<std::uint8_t, CornerNumbers> numbers;
  std::vector<bool> used(shp.vertices.size());
  std::size_t ordinal = 0;
  for (const std::vector<ShpPolygon>& part : shp.parts) {
    for (const ShpPolygon& polygon : part) {
      const auto flags = static_cast<std::uint8_t>(polygon.flags &
                                                   Content::kShpPrimitiveFlags);
      Primitive& primitive = by_flags[flags];
      primitive.flags = flags;
      if (auto added = AddPolygon(shp, polygon, ordinal, primitive,
                                  numbers[flags], used);
          !added) {
        return std::unexpected(added.error());
      }
      ++ordinal;
    }
  }
  if (ordinal == 0) {
    return std::unexpected(std::string("the model has no polygons"));
  }
  if (const auto unused = std::ranges::find(used, false);
      unused != used.end()) {
    return std::unexpected(std::format(
        "vertex {} is in no polygon; the glTF model keeps only polygon "
        "corners",
        std::distance(used.begin(), unused)));
  }
  std::vector<Primitive> primitives;
  primitives.reserve(by_flags.size());
  for (auto& [flags, primitive] : by_flags) {
    primitives.push_back(std::move(primitive));
  }
  return primitives;
}

// One primitive's vertex attributes, packed as their accessors hold them.
struct AttributeBytes {
  Bytes positions;
  Bytes joints;
  Bytes weights;
  Bytes vertices;
  Bytes uvs;
  Bytes colors;
  std::array<std::int64_t, kAxes> low{};
  std::array<std::int64_t, kAxes> high{};
};

void PutCorner(const MeshSource& source, const Corner& corner,
               AttributeBytes& out, bool first) {
  const Content::ShpVertex& vertex = source.model->vertices[corner.vertex];
  const std::uint16_t bone = source.bones[corner.vertex];
  const std::array<std::int64_t, kAxes> position{
      vertex.x + (*source.origins)[bone], vertex.y, vertex.z};
  for (std::size_t axis = 0; axis < kAxes; ++axis) {
    PutFloat(out.positions, static_cast<float>(position.at(axis)));
    out.low.at(axis) = first ? position.at(axis)
                             : std::min(out.low.at(axis), position.at(axis));
    out.high.at(axis) = first ? position.at(axis)
                              : std::max(out.high.at(axis), position.at(axis));
  }
  out.joints.push_back(static_cast<std::byte>(bone));
  out.weights.push_back(std::byte{kFullByte});
  out.joints.resize(out.joints.size() + kSkinSlots - 1);
  out.weights.resize(out.weights.size() + kSkinSlots - 1);
  PutFloat(out.vertices, static_cast<float>(corner.vertex));
  const std::uint32_t u = corner.uv & kByteMask;
  const std::uint32_t v = corner.uv >> kByteBits;
  PutFloat(out.uvs,
           static_cast<float>(static_cast<double>(u) / source.uv_scale.width));
  PutFloat(out.uvs,
           static_cast<float>(static_cast<double>(v) / source.uv_scale.height));
  for (std::size_t channel = 0; channel < kColorChannels; ++channel) {
    out.colors.push_back(static_cast<std::byte>(
        (corner.color >> (channel * kByteBits)) & kByteMask));
  }
  out.colors.push_back(std::byte{kFullByte});
}

Bytes IndexBytes(const Primitive& primitive, bool wide) {
  Bytes bytes;
  const std::size_t width =
      wide ? sizeof(std::uint32_t) : sizeof(std::uint16_t);
  bytes.reserve(primitive.indices.size() * width);
  for (const std::uint32_t index : primitive.indices) {
    PutLittleEndian(bytes, index, width);
  }
  return bytes;
}

Json AddPrimitive(const MeshSource& source, const Primitive& primitive,
                  std::size_t material, GltfBuffer& buffer) {
  AttributeBytes bytes;
  for (std::size_t index = 0; index < primitive.corners.size(); ++index) {
    PutCorner(source, primitive.corners[index], bytes, index == 0);
  }
  const std::size_t count = primitive.corners.size();
  const auto vector = [](const std::array<std::int64_t, kAxes>& values) {
    return Json::array({values[0], values[1], values[2]});
  };
  const auto attribute = [&](AccessorSpec spec, const Bytes& data) {
    spec.count = count;
    spec.target = kArrayBufferTarget;
    return buffer.AddAccessor(spec, data);
  };
  Json attributes = Json::object();
  AccessorSpec position = SpecOf("VEC3", Content::kGltfFloat, false);
  position.min = vector(bytes.low);
  position.max = vector(bytes.high);
  attributes["POSITION"] = attribute(position, bytes.positions);
  attributes["JOINTS_0"] = attribute(
      SpecOf("VEC4", Content::kGltfUnsignedByte, false), bytes.joints);
  attributes["WEIGHTS_0"] = attribute(
      SpecOf("VEC4", Content::kGltfUnsignedByte, true), bytes.weights);
  attributes[std::string(Content::kShpVertexAttribute)] =
      attribute(SpecOf("SCALAR", Content::kGltfFloat, false), bytes.vertices);
  attributes["TEXCOORD_0"] =
      attribute(SpecOf("VEC2", Content::kGltfFloat, false), bytes.uvs);
  if (source.model->colored) {
    attributes["COLOR_0"] = attribute(
        SpecOf("VEC4", Content::kGltfUnsignedByte, true), bytes.colors);
  }
  const bool wide = count > kMaxShortCorners;
  AccessorSpec index_spec = SpecOf(
      "SCALAR", wide ? Content::kGltfUnsignedInt : Content::kGltfUnsignedShort,
      false);
  index_spec.count = primitive.indices.size();
  index_spec.target = kElementArrayBufferTarget;
  const std::size_t indices =
      buffer.AddAccessor(index_spec, IndexBytes(primitive, wide));
  Json json = Json::object();
  json["attributes"] = std::move(attributes);
  json["indices"] = indices;
  json["material"] = material;
  json["mode"] = Content::kGltfTriangles;
  json["extras"] =
      Json{{std::string(Content::kShpExtrasKey),
            Json{{std::string(Content::kShpPolygonsKey), primitive.polygons}}}};
  return json;
}

// A preview of the polygon flags: texture `texture` unlit, blended when the
// polygons are semi-transparent and drawn from both sides without culling.
// A variant's copy adds its name to the material's.
Json MaterialOf(std::uint8_t flags, std::size_t texture = 0,
                std::string_view variant = {}) {
  Json material = Json::object();
  material["name"] = variant.empty()
                         ? std::format("flags_{:02x}", flags)
                         : std::format("flags_{:02x}_{}", flags, variant);
  material["pbrMetallicRoughness"] =
      Json{{"baseColorTexture", Json{{"index", texture}}},
           {"metallicFactor", 0.0},
           {"roughnessFactor", 1.0}};
  material["extensions"] = Json{{std::string(kUnlitExtension), Json::object()}};
  material["alphaMode"] =
      (flags & Content::kShpSemiTransparentFlag) != 0 ? "BLEND" : "MASK";
  material["doubleSided"] = (flags & Content::kShpDoubleSidedFlag) != 0;
  return material;
}

Bytes MatrixBytes(const std::vector<std::int64_t>& origins) {
  Bytes bytes;
  bytes.reserve(origins.size() * Content::kShpMatrixElements * sizeof(float));
  for (const std::int64_t origin : origins) {
    for (const float element : Content::ShpInverseBindMatrix(origin)) {
      PutFloat(bytes, element);
    }
  }
  return bytes;
}

struct GltfParts {
  Json meshes;
  Json materials;
};

// Variant v after the first shows texture v through a copy of each
// primitive's material, which the primitive maps v to; variant 0 maps to the
// primitive's own material.
void AddVariantMaterials(std::span<const ModelMaterialVariant> variants,
                         std::span<const Primitive> planned, Json& primitives,
                         Json& materials) {
  if (variants.empty()) return;
  for (std::size_t index = 0; index < planned.size(); ++index) {
    Json& primitive = primitives[index];
    Json mappings = Json::array({Json{{"material", primitive["material"]},
                                      {"variants", Json::array({0})}}});
    for (std::size_t variant = 1; variant < variants.size(); ++variant) {
      mappings.push_back(Json{{"material", materials.size()},
                              {"variants", Json::array({variant})}});
      materials.push_back(
          MaterialOf(planned[index].flags, variant, variants[variant].name));
    }
    primitive["extensions"] =
        Json{{std::string(kVariantsExtension), Json{{"mappings", mappings}}}};
  }
}

Result<GltfParts> BuildMesh(const ShpModel& model, const ModelGltfSpec& spec,
                            GltfBuffer& buffer) {
  auto origins = Content::ShpBoneOrigins(model);
  if (!origins) return std::unexpected(origins.error());
  auto bones = VertexBones(model);
  if (!bones) return std::unexpected(bones.error());
  if (spec.uv_scale.width == 0 || spec.uv_scale.height == 0) {
    return std::unexpected(
        std::string("the model's texture size, which scales its UVs, is not "
                    "known"));
  }
  auto planned = PlanPrimitives(model);
  if (!planned) return std::unexpected(planned.error());
  const MeshSource source{.model = &model,
                          .origins = &*origins,
                          .bones = std::move(*bones),
                          .uv_scale = spec.uv_scale};
  // The inverse bind matrices are accessor 0, where the skeleton names them.
  AccessorSpec matrices = SpecOf("MAT4", Content::kGltfFloat, false);
  matrices.count = model.bones.size();
  buffer.AddAccessor(matrices, MatrixBytes(*origins));
  GltfParts parts{.meshes = Json::array(), .materials = Json::array()};
  Json primitives = Json::array();
  for (const Primitive& primitive : *planned) {
    primitives.push_back(
        AddPrimitive(source, primitive, parts.materials.size(), buffer));
    parts.materials.push_back(MaterialOf(primitive.flags));
  }
  AddVariantMaterials(spec.variants, *planned, primitives, parts.materials);
  parts.meshes.push_back(Json{{"name", std::string(spec.stem)},
                              {"primitives", std::move(primitives)}});
  return parts;
}

Json GltfOf(const ModelGltfSpec& spec, Json payload, GltfParts parts,
            const GltfBuffer& buffer) {
  Json gltf = Json::object();
  gltf["asset"] =
      Json{{"version", "2.0"},
           {"generator", std::string(spec.generator)},
           {"extras",
            Json{{std::string(Content::kShpExtrasKey), std::move(payload)}}}};
  gltf["extensionsUsed"] = Json::array({std::string(kUnlitExtension)});
  for (const auto& member : spec.skeleton.items()) {
    gltf[member.key()] = member.value();
  }
  gltf["meshes"] = std::move(parts.meshes);
  gltf["materials"] = std::move(parts.materials);
  gltf["textures"] = Json::array({Json{{"sampler", 0}, {"source", 0}}});
  gltf["samplers"] = Json::array({Json{{"magFilter", kNearestFilter},
                                       {"minFilter", kNearestFilter},
                                       {"wrapS", kRepeatWrap},
                                       {"wrapT", kRepeatWrap}}});
  gltf["images"] = Json::array({Json{{"uri", std::string(spec.image_uri)}}});
  if (!spec.variants.empty()) {
    gltf["extensionsUsed"].push_back(std::string(kVariantsExtension));
    Json names = Json::array();
    for (std::size_t index = 0; index < spec.variants.size(); ++index) {
      names.push_back(Json{{"name", std::string(spec.variants[index].name)}});
      if (index == 0) continue;
      gltf["textures"].push_back(Json{{"sampler", 0}, {"source", index}});
      gltf["images"].push_back(
          Json{{"uri", std::string(spec.variants[index].image_uri)}});
    }
    gltf["extensions"] =
        Json{{std::string(kVariantsExtension), Json{{"variants", names}}}};
  }
  gltf["accessors"] = buffer.Accessors();
  gltf["bufferViews"] = buffer.Views();
  gltf["buffers"] = Json::array({Json{
      {"uri", std::string(spec.stem) + std::string(Content::kGltfBufferSuffix)},
      {"byteLength", buffer.Data().size()}}});
  return gltf;
}

// The model's own texture as image files named `stem`, if it has one.
Result<std::vector<ImageFile>> TextureFiles(const ShpModelDocument& document,
                                            std::string_view stem) {
  if (!document.shp.texture) return std::vector<ImageFile>{};
  const ShpTexture& texture = *document.shp.texture;
  auto encoded = Content::EncodeShp(document.shp);
  if (!encoded) return std::unexpected(encoded.error());
  const std::size_t texture_bytes =
      kTextureHeaderBytes + (texture.palettes.size() * sizeof(std::uint16_t)) +
      texture.pixels.size();
  const ImageDocument image{
      .id = document.image_id,
      .image = Content::ShpTextureImage(texture),
      .material = std::nullopt,
      .unreferenced = false,
      .provenance = {.source = document.source,
                     .source_sha256 = document.source_sha256,
                     .offset = encoded->size() - texture_bytes,
                     .tim = std::nullopt,
                     .texture_page = std::nullopt,
                     .preview_palette = std::nullopt,
                     .dependencies = {},
                     .note = {}}};
  return EncodeImageFiles(image, stem);
}

// The files must read back to `document` and rebuild the source bytes: the
// SHP and each bank it owns.
Check VerifyModel(const ShpModelDocument& document, std::string_view stem,
                  const std::vector<ImageFile>& files,
                  const ShpConversion& conversion) {
  const SourceFile& source = conversion.model;
  const std::span<const std::uint8_t> lengths = conversion.events.lengths;
  auto decoded = Content::DecodeShpModelFiles(
      stem, Content::FileListReader(files), lengths);
  if (!decoded) {
    return std::unexpected(
        std::format("{}: the converted model does not read back: {}",
                    source.path, decoded.error()));
  }
  if (*decoded != document) {
    return std::unexpected(
        std::format("{}: the converted model reads back to a different model",
                    source.path));
  }
  auto encoded = Content::EncodeShp(decoded->shp);
  if (!encoded) {
    return std::unexpected(std::format("{}: {}", source.path, encoded.error()));
  }
  if (*encoded != source.bytes) {
    return std::unexpected(
        std::format("{}: the converted model does not rebuild the source bytes",
                    source.path));
  }
  if (decoded->sequences.size() != conversion.sequences.size()) {
    return std::unexpected(std::format(
        "{}: the converted model holds {} banks; it owns {}", source.path,
        decoded->sequences.size(), conversion.sequences.size()));
  }
  for (std::size_t index = 0; index < conversion.sequences.size(); ++index) {
    const SourceFile& bank = conversion.sequences[index];
    auto rebuilt =
        Content::EncodeSeqFile(decoded->sequences[index].file, lengths);
    if (!rebuilt || *rebuilt != bank.bytes) {
      return std::unexpected(
          std::format("{}: the converted model does not rebuild {}",
                      source.path, bank.path));
    }
  }
  return {};
}

SourceDependency DependencyOf(const SourceFile& source) {
  return {.source = source.path, .source_sha256 = HashOf(source)};
}

// The table's file, when there is one.
std::optional<SourceDependency> EventsDependency(const SeqEventTable& events) {
  if (events.source.empty()) return std::nullopt;
  return SourceDependency{.source = events.source,
                          .source_sha256 = events.source_sha256};
}

void SortDependencies(std::vector<SourceDependency>& dependencies) {
  std::ranges::sort(dependencies, {}, &SourceDependency::source);
}

// The manifest entry `conversion` makes, without its files: its lender, the
// event table's file and its banks are its dependencies.
Result<ConversionOutput> ModelOutputOf(const ShpConversion& conversion) {
  auto id = ShpModelIdForDiscPath(conversion.model.path);
  if (!id) return std::unexpected(id.error());
  ConversionOutput output{
      .id = std::move(*id),
      .source = conversion.model.path,
      .source_sha256 = HashOf(conversion.model),
      .converter = std::string(Content::kShpModelConverterName),
      .files = {},
      .dependencies = {},
      .unreferenced = false,
      .note = {},
      .alias_of = {}};
  if (conversion.lender) {
    output.dependencies.push_back(DependencyOf(*conversion.lender));
  }
  if (auto events = EventsDependency(conversion.events)) {
    output.dependencies.push_back(std::move(*events));
  }
  for (const SourceFile& bank : conversion.sequences) {
    output.dependencies.push_back(DependencyOf(bank));
  }
  SortDependencies(output.dependencies);
  return output;
}

// The seq-gltf output of each bank `conversion` owns, without files: each
// depends on the model and the event table's file.
Result<std::vector<ConversionOutput>> CompanionsOf(
    const ShpConversion& conversion) {
  std::vector<SourceDependency> dependencies{DependencyOf(conversion.model)};
  if (auto events = EventsDependency(conversion.events)) {
    dependencies.push_back(std::move(*events));
  }
  SortDependencies(dependencies);
  std::vector<ConversionOutput> outputs;
  for (const SourceFile& bank : conversion.sequences) {
    auto id = SeqAnimationIdForDiscPath(bank.path);
    if (!id) return std::unexpected(id.error());
    outputs.push_back({.id = std::move(*id),
                       .source = bank.path,
                       .source_sha256 = HashOf(bank),
                       .converter = std::string(kSeqAnimationConverterName),
                       .files = {},
                       .dependencies = dependencies,
                       .unreferenced = false,
                       .note = {},
                       .alias_of = {}});
  }
  return outputs;
}

struct Lending {
  std::optional<bool> colored;  // the lender's polygon layout
  std::optional<ShpTextureSize> texture;
  std::string image_stem;  // whose PNG the materials show
};

// What the lender of a model that borrows its texture gives it.
Result<Lending> LendingOf(const ShpConversion& conversion,
                          std::optional<std::int32_t> lender) {
  const SourceFile& model = conversion.model;
  if (!lender) {
    if (conversion.lender) {
      return std::unexpected(std::format(
          "{} has its own texture, so it has no lender", model.path));
    }
    auto stem = StemOf(model.path);
    if (!stem) return std::unexpected(stem.error());
    return Lending{.colored = std::nullopt,
                   .texture = std::nullopt,
                   .image_stem = std::move(*stem)};
  }
  const std::string path = ShpLenderPath(*lender);
  if (!conversion.lender || conversion.lender->path != path) {
    return std::unexpected(std::format(
        "{} borrows the texture of {}, which was not given", model.path, path));
  }
  auto decoded = Content::DecodeShp(conversion.lender->bytes);
  if (!decoded) {
    return std::unexpected(std::format("{}: {}", path, decoded.error()));
  }
  if (!decoded->texture) {
    return std::unexpected(
        std::format("{} borrows the texture of {}, which borrows its own",
                    model.path, path));
  }
  auto stem = StemOf(path);
  if (!stem) return std::unexpected(stem.error());
  return Lending{
      .colored = decoded->colored,
      .texture = ShpTextureSize{.width = decoded->texture->Width(),
                                .height = decoded->texture->Height()},
      .image_stem = std::move(*stem)};
}

Result<ShpModelDocument> DocumentOf(const ShpConversion& conversion,
                                    const Lending& lending) {
  const SourceFile& model = conversion.model;
  auto shp = Content::DecodeShp(model.bytes, lending.colored);
  if (!shp) {
    return std::unexpected(std::format("{}: {}", model.path, shp.error()));
  }
  auto id = ShpModelIdForDiscPath(model.path);
  if (!id) return std::unexpected(id.error());
  ShpModelDocument document{.id = std::move(*id),
                            .source = model.path,
                            .source_sha256 = HashOf(model),
                            .image_id = {},
                            .lender_texture = lending.texture,
                            .shp = std::move(*shp),
                            .sequences = {}};
  if (document.shp.texture) {
    auto image = ImageIdForDiscPath(model.path);
    if (!image) return std::unexpected(image.error());
    document.image_id = std::move(*image);
  }
  for (const SourceFile& bank : conversion.sequences) {
    auto file = Content::DecodeSeqFile(bank.bytes, conversion.events.lengths);
    if (!file) {
      return std::unexpected(std::format("{}: {}", bank.path, file.error()));
    }
    if (file->bones != document.shp.bones.size()) {
      return std::unexpected(
          std::format("{} animates {} bones; its model {} has {}", bank.path,
                      file->bones, model.path, document.shp.bones.size()));
    }
    document.sequences.push_back({.source = bank.path,
                                  .source_sha256 = HashOf(bank),
                                  .file = std::move(*file)});
  }
  return document;
}

using SourceMap = std::map<std::string, SourceFile, std::less<>>;

// What the models convert from: every SHP and SEQ files.tsv lists, by disc
// path, and BATTLE.PRG when it lists it.
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct ModelSources {
  SourceMap models;
  SourceMap banks;
  std::map<std::uint32_t, const DataStore::Entry*> bank_sectors;  // by LBA
  std::optional<SourceFile> program;
};

Result<ModelSources> ReadModelSources(
    const fs::path& root, const std::vector<DataStore::Entry>& entries) {
  ModelSources sources;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm) continue;
    const bool model = entry.path.ends_with(kShpExtension);
    const bool bank = entry.path.ends_with(kSeqExtension);
    const bool program = entry.path == kBattleProgramPath;
    if (!model && !bank && !program) continue;
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    if (program) {
      sources.program = std::move(*source);
      continue;
    }
    if (bank) sources.bank_sectors.emplace(entry.lba, &entry);
    (model ? sources.models : sources.banks)
        .emplace(source->path, std::move(*source));
  }
  return sources;
}

// --- Which model owns each SEQ bank.

// The model fields' bank sizes are bytes; the game reads whole sectors.
constexpr std::uint32_t kSectorShift = 11;
// BATTLE.PRG's material bank tables (ActorAssets.cpp SequenceFile): the
// sector offsets (halfwords) and sector counts (bytes) of the banks the
// player's battle sequences load for each weapon material, then of the ones
// model 2's special sequences load, four per material. The game indexes them
// from material 1; row 0 holds no bank.
// decomp: D_800E8E8E (D_800E8E8E@BATTLE/BATTLE.PRG)
constexpr std::size_t kMaterialBankOffsetsAt = 0x8068E;
// decomp: D_800E8EA3 (D_800E8EA3@BATTLE/BATTLE.PRG)
constexpr std::size_t kMaterialBankSectorsAt = 0x806A3;
// decomp: D_800E8EA8 (D_800E8EA8@BATTLE/BATTLE.PRG)
constexpr std::size_t kMaterialModelBankOffsetsAt = 0x806A8;
// decomp: D_800E8EFC (D_800E8EFC@BATTLE/BATTLE.PRG)
constexpr std::size_t kMaterialModelBankSectorsAt = 0x806FC;
constexpr std::uint32_t kMaterialBankFirstSector = 0x18043;
constexpr std::uint32_t kMaterialModelBankFirstSector = 0x1943E;
constexpr std::size_t kFirstMaterial = 1;
constexpr std::size_t kLastMaterial = 10;
constexpr std::size_t kModelsPerMaterial = 4;
// The models that load them: the player and model 2.
constexpr std::int32_t kMaterialBankModel = 0;
constexpr std::int32_t kMaterialModelBankModel = 2;
static_assert(kMaterialModelBankSectorsAt +
                  ((kLastMaterial + 1) * kModelsPerMaterial) <=
              Content::kSeqEventLengthsAt);

class BankOwners {
 public:
  explicit BankOwners(const ModelSources& sources) : sources_(sources) {}

  // `owner`'s field `field` names `sectors` sectors at `lba`: that bank, or
  // nothing when it names no sectors.
  Check Claim(std::string_view owner, std::uint32_t lba, std::uint32_t sectors,
              std::string_view field) {
    if (sectors == 0) return {};
    const auto found = sources_.bank_sectors.find(lba);
    if (found == sources_.bank_sectors.end() ||
        found->second->SectorCount() != sectors) {
      return std::unexpected(
          std::format("{}: {} names {} sectors at LBA {}, which is no SEQ "
                      "files.tsv lists",
                      owner, field, sectors, lba));
    }
    const std::string& bank = found->second->path;
    const auto [claimed, added] = owner_of_.try_emplace(bank, owner);
    if (!added && claimed->second != owner) {
      return std::unexpected(std::format("{} is loaded by both {} and {}", bank,
                                         claimed->second, owner));
    }
    return {};
  }

  // The banks each model owns, sorted by path; every bank needs an owner
  // files.tsv lists.
  Result<std::map<std::string, std::vector<std::string>, std::less<>>> ByModel()
      const {
    std::map<std::string, std::vector<std::string>, std::less<>> banks;
    for (const auto& [bank, source] : sources_.banks) {
      const auto owner = owner_of_.find(bank);
      if (owner == owner_of_.end()) {
        return std::unexpected(std::format("{} is loaded by no model", bank));
      }
      if (!sources_.models.contains(owner->second)) {
        return std::unexpected(
            std::format("{} is loaded by {}, which files.tsv does not list",
                        bank, owner->second));
      }
      banks[owner->second].push_back(bank);
    }
    return banks;
  }

 private:
  const ModelSources& sources_;
  std::map<std::string, std::string, std::less<>> owner_of_;
};

// The banks the model's battle and special sequence fields name.
Check ClaimModelBanks(BankOwners& owners, const SourceFile& model) {
  constexpr std::size_t kBlockEnd =
      Content::kShpCountsBytes + Content::kShpLayoutBlockBytes;
  if (model.bytes.size() < kBlockEnd) {
    return std::unexpected(std::format(
        "{} is too short for its texture layout block", model.path));
  }
  Content::ShpLayoutBlock block{};
  std::ranges::copy(
      std::span(model.bytes).subspan(Content::kShpCountsBytes, block.size()),
      block.begin());
  const Json layout = Content::ShpLayoutJson(block);
  constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
      kFields{{{"battle_sequence_lbas", "battle_sequence_sizes"},
               {"special_sequence_lbas", "special_sequence_sizes"}}};
  for (const auto& [lbas_name, sizes_name] : kFields) {
    const Json& lbas = layout.at(std::string(lbas_name));
    const Json& sizes = layout.at(std::string(sizes_name));
    for (std::size_t index = 0; index < lbas.size(); ++index) {
      if (auto claimed =
              owners.Claim(model.path, lbas.at(index).get<std::uint32_t>(),
                           sizes.at(index).get<std::uint32_t>() >> kSectorShift,
                           std::format("{}[{}]", lbas_name, index));
          !claimed) {
        return claimed;
      }
    }
  }
  return {};
}

std::uint32_t ProgramByte(const SourceFile& program, std::size_t at) {
  return std::to_integer<std::uint32_t>(program.bytes[at]);
}

std::uint32_t ProgramHalfword(const SourceFile& program, std::size_t at) {
  return ProgramByte(program, at) | (ProgramByte(program, at + 1) << kByteBits);
}

// The banks BATTLE.PRG's material tables name.
Check ClaimMaterialBanks(BankOwners& owners, const SourceFile& program) {
  const std::string player = ShpLenderPath(kMaterialBankModel);
  const std::string model = ShpLenderPath(kMaterialModelBankModel);
  for (std::size_t material = kFirstMaterial; material <= kLastMaterial;
       ++material) {
    if (auto claimed = owners.Claim(
            player,
            kMaterialBankFirstSector +
                ProgramHalfword(program,
                                kMaterialBankOffsetsAt +
                                    (material * sizeof(std::uint16_t))),
            ProgramByte(program, kMaterialBankSectorsAt + material),
            std::format("material bank {}", material));
        !claimed) {
      return claimed;
    }
    for (std::size_t column = 0; column < kModelsPerMaterial; ++column) {
      const std::size_t row = (material * kModelsPerMaterial) + column;
      if (auto claimed = owners.Claim(
              model,
              kMaterialModelBankFirstSector +
                  ProgramHalfword(program, kMaterialModelBankOffsetsAt +
                                               (row * sizeof(std::uint16_t))),
              ProgramByte(program, kMaterialModelBankSectorsAt + row),
              std::format("material {} model bank {}", material, column));
          !claimed) {
        return claimed;
      }
    }
  }
  return {};
}

// The event stride table, and the banks each model owns.
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct BankPlan {
  SeqEventTable events;
  std::map<std::string, std::vector<std::string>, std::less<>> banks;
};

Result<BankPlan> PlanBanks(const ModelSources& sources) {
  BankPlan plan;
  BankOwners owners(sources);
  if (sources.program) {
    const SourceFile& program = *sources.program;
    constexpr std::size_t kTableEnd =
        Content::kSeqEventLengthsAt + Content::kSeqEventLengthCount;
    if (program.bytes.size() < kTableEnd) {
      return std::unexpected(
          std::format("{} is too short for the event stride table at {:#x}",
                      program.path, Content::kSeqEventLengthsAt));
    }
    const auto table = std::span(program.bytes)
                           .subspan(Content::kSeqEventLengthsAt,
                                    Content::kSeqEventLengthCount);
    plan.events = {.source = program.path,
                   .source_sha256 = HashOf(program),
                   .lengths = {}};
    std::ranges::transform(
        table, std::back_inserter(plan.events.lengths),
        [](std::byte value) { return std::to_integer<std::uint8_t>(value); });
    if (auto claimed = ClaimMaterialBanks(owners, program); !claimed) {
      return std::unexpected(claimed.error());
    }
  }
  for (const auto& [path, model] : sources.models) {
    if (auto claimed = ClaimModelBanks(owners, model); !claimed) {
      return std::unexpected(claimed.error());
    }
  }
  auto banks = owners.ByModel();
  if (!banks) return std::unexpected(banks.error());
  plan.banks = std::move(*banks);
  return plan;
}

Result<ShpConversion> PlanConversion(const SourceFile& model,
                                     const ModelSources& sources,
                                     const BankPlan& plan) {
  auto peeked = Content::PeekShpTexture(model.bytes);
  if (!peeked) {
    return std::unexpected(std::format("{}: {}", model.path, peeked.error()));
  }
  ShpConversion conversion{.model = model,
                           .lender = std::nullopt,
                           .sequences = {},
                           .events = plan.events};
  if (peeked->lender) {
    const std::string path = ShpLenderPath(*peeked->lender);
    const auto lender = sources.models.find(path);
    if (lender == sources.models.end()) {
      return std::unexpected(std::format(
          "{} borrows the texture of {}, which files.tsv does not list",
          model.path, path));
    }
    conversion.lender = lender->second;
  }
  if (const auto owned = plan.banks.find(model.path);
      owned != plan.banks.end()) {
    for (const std::string& bank : owned->second) {
      conversion.sequences.push_back(sources.banks.find(bank)->second);
    }
  }
  return conversion;
}

Check WriteModels(const fs::path& directory,
                  const std::vector<ConvertedModel>& converted) {
  std::vector<OutputBytes> files;
  for (const ConvertedModel& model : converted) {
    for (const ImageFile& file : model.files) {
      files.push_back({.name = file.name, .bytes = file.bytes});
    }
  }
  if (files.empty()) return {};
  return WriteOutputFiles(directory, files);
}

}  // namespace

std::expected<std::string, std::string> ShpModelIdForDiscPath(
    std::string_view disc_path) {
  if (auto stem = StemOf(disc_path); !stem) {
    return std::unexpected(stem.error());
  }
  auto image = ImageIdForDiscPath(disc_path);
  if (!image) return std::unexpected(image.error());
  // image.<directories>.<stem> -> model.<directories>.<stem>
  std::string id =
      std::format("{}{}", kModelIdPrefix,
                  std::string_view(*image).substr(image->find('.')));
  if (!Content::IsContentId(id)) {
    return std::unexpected(std::format(
        "'{}' gives the model id '{}', which is not valid", disc_path, id));
  }
  return id;
}

std::string ShpLenderPath(std::int32_t lender) {
  return std::format("{}/{:02X}{}", kShpDirectory, lender, kShpExtension);
}

std::expected<std::string, std::string> SeqAnimationIdForDiscPath(
    std::string_view disc_path) {
  if (!DiscStemOf(disc_path, kSeqExtension)) {
    return std::unexpected(
        std::format("'{}' is not a SEQ animation bank", disc_path));
  }
  auto image = ImageIdForDiscPath(disc_path);
  if (!image) return std::unexpected(image.error());
  // image.<directories>.<stem> -> animation.<directories>.<stem>
  std::string id =
      std::format("{}{}", kAnimationIdPrefix,
                  std::string_view(*image).substr(image->find('.')));
  if (!Content::IsContentId(id)) {
    return std::unexpected(std::format(
        "'{}' gives the animation id '{}', which is not valid", disc_path, id));
  }
  return id;
}

std::optional<std::string> DiscStemOf(std::string_view disc_path,
                                      std::string_view extension) {
  const std::size_t slash = disc_path.rfind('/');
  const std::string_view name =
      slash == std::string_view::npos ? disc_path : disc_path.substr(slash + 1);
  if (!name.ends_with(extension) || name.size() == extension.size()) {
    return std::nullopt;
  }
  return std::string(name.substr(0, name.size() - extension.size()));
}

std::expected<std::vector<ImageFile>, std::string> EncodeModelGltf(
    const Content::ShpModel& model, const ModelGltfSpec& spec) {
  GltfBuffer buffer;
  auto parts = BuildMesh(model, spec, buffer);
  if (!parts) return std::unexpected(parts.error());
  const ModelViewAdder add_view =
      [&buffer](
          std::span<const std::byte> bytes) -> std::optional<std::size_t> {
    if (bytes.empty()) return std::nullopt;
    return buffer.AddView(bytes, std::nullopt);
  };
  Json payload = spec.payload(add_view);
  const std::string text =
      Content::FormatJsonText(
          GltfOf(spec, std::move(payload), std::move(*parts), buffer)) +
      "\n";
  const auto text_bytes = std::as_bytes(std::span(text));
  std::vector<ImageFile> files;
  files.push_back(
      {.name = std::string(spec.stem) + std::string(Content::kGltfSuffix),
       .bytes = Bytes(text_bytes.begin(), text_bytes.end())});
  files.push_back(
      {.name = std::string(spec.stem) + std::string(Content::kGltfBufferSuffix),
       .bytes = buffer.Data()});
  return files;
}

std::expected<std::vector<ImageFile>, std::string> EncodeShpModelFiles(
    const ShpModelDocument& document, std::string_view stem,
    std::string_view image_uri, std::span<const std::uint8_t> event_lengths) {
  const ShpFile& shp = document.shp;
  auto sequence = Content::ShpSequenceJson(shp, event_lengths);
  if (!sequence) {
    return std::unexpected(
        std::format("{}: {}", document.source, sequence.error()));
  }
  const ModelGltfSpec spec{
      .stem = stem,
      .image_uri = image_uri,
      .generator = kGenerator,
      .uv_scale = Content::ShpUvScale(document).value_or(ShpTextureSize{}),
      .skeleton = Content::ShpSkeletonGltf(shp, 0),
      .payload =
          [&](const ModelViewAdder& add_view) {
            const std::optional<std::size_t> sound = add_view(shp.sound);
            return Content::ShpModelPayload(document, sound, *sequence);
          },
      .variants = {}};
  auto files = EncodeModelGltf(shp, spec);
  if (!files) {
    return std::unexpected(
        std::format("{}: {}", document.source, files.error()));
  }
  auto texture = TextureFiles(document, stem);
  if (!texture) {
    return std::unexpected(
        std::format("{}: {}", document.source, texture.error()));
  }
  std::ranges::move(*texture, std::back_inserter(*files));
  return files;
}

std::expected<ConvertedModel, std::string> ConvertShpModel(
    const ShpConversion& conversion) {
  const SourceFile& model = conversion.model;
  auto peeked = Content::PeekShpTexture(model.bytes);
  if (!peeked) {
    return std::unexpected(std::format("{}: {}", model.path, peeked.error()));
  }
  auto lending = LendingOf(conversion, peeked->lender);
  if (!lending) return std::unexpected(lending.error());
  auto document = DocumentOf(conversion, *lending);
  if (!document) return std::unexpected(document.error());
  auto stem = StemOf(model.path);
  if (!stem) return std::unexpected(stem.error());
  auto files = EncodeShpModelFiles(
      *document, *stem,
      lending->image_stem + std::string(Content::kImagePngSuffix),
      conversion.events.lengths);
  if (!files) return std::unexpected(files.error());
  if (auto verified = VerifyModel(*document, *stem, *files, conversion);
      !verified) {
    return std::unexpected(verified.error());
  }
  auto output = ModelOutputOf(conversion);
  if (!output) return std::unexpected(output.error());
  auto companions = CompanionsOf(conversion);
  if (!companions) return std::unexpected(companions.error());
  ConvertedModel converted = ConvertedModelOf(
      std::move(*output), std::move(*files), Content::kShpModelDirectory);
  // Each bank's output lists the glTF and its buffer, which hold it.
  for (ConversionOutput& companion : *companions) {
    std::ranges::copy_if(converted.output.files,
                         std::back_inserter(companion.files),
                         [](const std::string& file) {
                           return file.ends_with(Content::kGltfSuffix) ||
                                  file.ends_with(Content::kGltfBufferSuffix);
                         });
  }
  converted.companions = std::move(*companions);
  return converted;
}

ConvertedModel ConvertedModelOf(ConversionOutput output,
                                std::vector<ImageFile> files,
                                std::string_view directory) {
  for (const ImageFile& file : files) {
    output.files.push_back(std::format("{}/{}", directory, file.name));
  }
  return ConvertedModel{
      .output = std::move(output), .files = std::move(files), .companions = {}};
}

std::expected<ModelConversionReport, std::string> RunModelConverter(
    const fs::path& root, const RunStart& start, const ModelConverterSpec& spec,
    std::span<const ModelJob> jobs) {
  const ConversionManifest& previous = start.previous;
  std::vector<ConverterBatch> batches{
      StartBatch(previous, spec.name, spec.version, spec.force)};
  if (spec.companion) {
    batches.push_back(StartBatch(previous, spec.companion->name,
                                 spec.companion->version, spec.force));
  }
  ConverterBatch& models = batches.front();
  const auto unrecorded = [&spec](const std::string& id) {
    return std::unexpected(
        std::format("{} has companion outputs, which the {} converter does "
                    "not record",
                    id, spec.name));
  };
  ModelConversionReport report;
  std::vector<ConvertedModel> converted;
  for (const ModelJob& job : jobs) {
    if (!job.companions.empty() && !spec.companion) {
      return unrecorded(job.expected.id);
    }
    const ConversionOutput* kept =
        models.reuse ? MatchingOutput(previous, root, job.expected) : nullptr;
    std::vector<const ConversionOutput*> kept_companions;
    if (kept != nullptr && !job.companions.empty()) {
      if (!batches.back().reuse) kept = nullptr;
      for (const ConversionOutput& companion : job.companions) {
        if (kept == nullptr) break;
        const ConversionOutput* match =
            MatchingOutput(previous, root, companion);
        if (match == nullptr) kept = nullptr;
        kept_companions.push_back(match);
      }
    }
    if (kept != nullptr) {
      models.outputs.push_back(*kept);
      for (const ConversionOutput* companion : kept_companions) {
        batches.back().outputs.push_back(*companion);
      }
      ++report.unchanged;
      continue;
    }
    auto model = job.convert();
    if (!model) return std::unexpected(model.error());
    if (!model->companions.empty() && !spec.companion) {
      return unrecorded(model->output.id);
    }
    models.outputs.push_back(model->output);
    std::ranges::copy(model->companions,
                      std::back_inserter(batches.back().outputs));
    converted.push_back(std::move(*model));
  }
  const fs::path directory = root / Platform::Utf8Path(spec.directory);
  if (auto written = WriteModels(directory, converted); !written) {
    return std::unexpected(written.error());
  }
  auto removed = PublishBatches(root, previous, batches);
  if (!removed) return std::unexpected(removed.error());
  report.converted = converted.size();
  report.removed = *removed;
  return report;
}

std::expected<ModelConversionReport, std::string> ConvertDiscModels(
    const fs::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto sources = ReadModelSources(root, start->entries);
  if (!sources) return std::unexpected(sources.error());
  auto plan = PlanBanks(*sources);
  if (!plan) return std::unexpected(plan.error());
  std::vector<ModelJob> jobs;
  for (const auto& [path, source] : sources->models) {
    auto conversion = PlanConversion(source, *sources, *plan);
    if (!conversion) return std::unexpected(conversion.error());
    auto expected = ModelOutputOf(*conversion);
    if (!expected) return std::unexpected(expected.error());
    auto companions = CompanionsOf(*conversion);
    if (!companions) return std::unexpected(companions.error());
    jobs.push_back({.expected = std::move(*expected),
                    .companions = std::move(*companions),
                    .convert = [conversion = std::move(*conversion)] {
                      return ConvertShpModel(conversion);
                    }});
  }
  return RunModelConverter(
      root, *start,
      {.name = Content::kShpModelConverterName,
       .version = Content::kShpModelConverterVersion,
       .directory = Content::kShpModelDirectory,
       .force = force,
       .companion =
           ModelCompanionConverter{.name = kSeqAnimationConverterName,
                                   .version = kSeqAnimationConverterVersion}},
      jobs);
}

}  // namespace VS::Extract
