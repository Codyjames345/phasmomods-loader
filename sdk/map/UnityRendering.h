#pragma once

// Unity's rendering: renderers and their materials, the shader properties a material's colours and
// textures are set by, textures and render textures, and lights. Not obfuscated, so every entry is by
// real name or, for an engine function, its icall name. Spectral Flame recolours flames with them.
// What it needs only to copy a flame's sprite to a grey texture (the textures, render textures,
// Graphics.Blit and the textures' HideFlags in UnityObject.h) is optional: a mod checks it with
// use::resolved() and, without it, tints the game's own sprite, as Spectral Flame did before it read
// textures.

#include "entry.h"
#include "game_api.h"
#include "Transform.h"

namespace pm::map {

inline constexpr const char* UnityRendering_int[] = {"int"};
inline constexpr const char* UnityRendering_int_int[] = {"int", "int"};
inline constexpr const char* UnityRendering_bool_bool[] = {"bool", "bool"};

/// Unity's base class of renderers (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Renderer{.key = "Renderer", .name = "Renderer",
                                             .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The renderer's material as its asset is shared, unchanged. Inferred: Unity's documented property
/// getter.
inline constexpr pm::sdk::EntryDesc Renderer_get_sharedMaterial{
    .key = "Renderer.get_sharedMaterial", .kind = pm::sdk::Kind::Method, .owner = "Renderer",
    .name = "get_sharedMaterial"};

/// The renderer's own material: the first call gives the renderer a copy of the shared one, so a
/// change shows on this renderer alone. Candle.Update melts the wax through it (read in its decompiled
/// code). Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Renderer_get_material{.key = "Renderer.get_material", .kind = pm::sdk::Kind::Method,
                                                          .owner = "Renderer", .name = "get_material"};

/// Whether the renderer draws. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Renderer_get_enabled{.key = "Renderer.get_enabled", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Renderer", .name = "get_enabled"};

inline constexpr const char* UnityRendering_bool[] = {"bool"};

/// Switches the renderer's drawing on or off. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Renderer_set_enabled{.key = "Renderer.set_enabled", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Renderer", .name = "set_enabled",
                                                         .params = UnityRendering_bool};

/// Unity's light component (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Light{.key = "Light", .name = "Light", .assembly = "UnityEngine.CoreModule",
                                          .ns = "UnityEngine"};

/// The light's colour. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Light_get_color{.key = "Light.get_color", .kind = pm::sdk::Kind::Method,
                                                    .owner = "Light", .name = "get_color"};

inline constexpr const char* Light_Color[] = {"Color"};

/// Sets the light's colour. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Light_set_color{.key = "Light.set_color", .kind = pm::sdk::Kind::Method,
                                                    .owner = "Light", .name = "set_color", .params = Light_Color};

/// The light's brightness. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Light_get_intensity{.key = "Light.get_intensity", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Light", .name = "get_intensity"};

inline constexpr const char* Light_float[] = {"float"};

/// Sets the light's brightness. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Light_set_intensity{.key = "Light.set_intensity", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Light", .name = "set_intensity",
                                                        .params = Light_float};

/// The materials the renderer shares with every other user of them, a Material[]. Inferred: Unity's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc Renderer_get_sharedMaterials{.key = "Renderer.get_sharedMaterials",
                                                                 .kind = pm::sdk::Kind::Method, .owner = "Renderer",
                                                                 .name = "get_sharedMaterials"};

inline constexpr const char* Renderer_MaterialArray[] = {"Material[]"};

/// Sets the renderer's shared materials from a Material[]. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Renderer_set_sharedMaterials{
    .key = "Renderer.set_sharedMaterials", .kind = pm::sdk::Kind::Method, .owner = "Renderer",
    .name = "set_sharedMaterials", .params = Renderer_MaterialArray};

/// Unity's mesh (UnityEngine.CoreModule), an Object. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Mesh{.key = "Mesh", .name = "Mesh", .assembly = "UnityEngine.CoreModule",
                                         .ns = "UnityEngine"};

/// The constructor Mesh(): a new, empty mesh. Run it on an object the Mesh type made. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Mesh_ctor{.key = "Mesh.ctor", .kind = pm::sdk::Kind::Method, .owner = "Mesh",
                                              .name = ".ctor"};

/// A renderer that draws a mesh deformed by a skeleton (UnityEngine.CoreModule): a character's body.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc SkinnedMeshRenderer{.key = "SkinnedMeshRenderer", .name = "SkinnedMeshRenderer",
                                                        .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

inline constexpr const char* SkinnedMeshRenderer_BakeMesh_params[] = {"Mesh", "bool"};

/// The engine function behind SkinnedMeshRenderer.BakeMesh(mesh, useScale): writes the renderer's mesh,
/// as it is posed now, into `mesh`. The runtime registers it even where the game's code strips the
/// managed method. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc SkinnedMeshRenderer_BakeMesh{.key = "SkinnedMeshRenderer.BakeMesh",
                                                                 .kind = pm::sdk::Kind::Icall,
                                                                 .name = "UnityEngine.SkinnedMeshRenderer::BakeMesh",
                                                                 .params = SkinnedMeshRenderer_BakeMesh_params};

/// The component that holds the mesh a MeshRenderer draws (UnityEngine.CoreModule). Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc MeshFilter{.key = "MeshFilter", .name = "MeshFilter",
                                               .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

inline constexpr const char* MeshFilter_Mesh[] = {"Mesh"};

/// Sets the mesh the filter holds, shared, not copied. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc MeshFilter_set_sharedMesh{.key = "MeshFilter.set_sharedMesh",
                                                              .kind = pm::sdk::Kind::Method, .owner = "MeshFilter",
                                                              .name = "set_sharedMesh", .params = MeshFilter_Mesh};

/// A renderer that draws its GameObject's MeshFilter's mesh (UnityEngine.CoreModule). Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc MeshRenderer{.key = "MeshRenderer", .name = "MeshRenderer",
                                                 .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The kinds of light (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc LightType{.key = "LightType", .name = "LightType",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// A light that shines in every direction from its position, to its range. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc LightType_Point{.key = "LightType.Point", .kind = pm::sdk::Kind::EnumValue,
                                                    .flags = pm::sdk::kOptional, .owner = "LightType",
                                                    .name = "Point"};

inline constexpr const char* Light_LightType[] = {"LightType"};

/// Sets the kind of light. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Light_set_type{.key = "Light.set_type", .kind = pm::sdk::Kind::Method,
                                                   .flags = pm::sdk::kOptional, .owner = "Light", .name = "set_type",
                                                   .params = Light_LightType};

/// Sets how far a point or spot light reaches, in metres. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Light_set_range{.key = "Light.set_range", .kind = pm::sdk::Kind::Method,
                                                    .flags = pm::sdk::kOptional, .owner = "Light", .name = "set_range",
                                                    .params = Light_float};

/// Unity's material (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Material{.key = "Material", .name = "Material",
                                             .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The texture the material's shader property (by its id, Shader.PropertyToID) holds. Private: the body
/// of Material.GetTexture. Inferred: from its name, beside Unity's documented GetTexture.
inline constexpr pm::sdk::EntryDesc Material_GetTextureImpl{
    .key = "Material.GetTextureImpl", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional, .owner = "Material",
    .name = "GetTextureImpl", .params = UnityRendering_int};

inline constexpr const char* Material_SetTextureImpl_params[] = {"int", "Texture"};

/// Sets the texture of a shader property (by its id). Private: the body of Material.SetTexture.
/// Inferred: from its name, beside Unity's documented SetTexture.
inline constexpr pm::sdk::EntryDesc Material_SetTextureImpl{
    .key = "Material.SetTextureImpl", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional, .owner = "Material",
    .name = "SetTextureImpl", .params = Material_SetTextureImpl_params};

/// Whether the material's shader has the property (by its id): the engine function of
/// Material.HasProperty(int). Inferred: Unity's documented API; Spectral Flame has called it by this
/// name in the game since 2026-10-20.
inline constexpr pm::sdk::EntryDesc Material_HasProperty{.key = "Material.HasProperty", .kind = pm::sdk::Kind::Icall,
                                                         .name = "UnityEngine.Material::HasProperty",
                                                         .params = UnityRendering_int};

inline constexpr const char* Material_ColorImpl_params[] = {"int", "Color"};

/// The colour of a shader property (by its id), written to the Color its second argument points at:
/// the engine function behind Material.GetColor. Inferred: from its name; Spectral Flame has called
/// it by this name in the game since 2026-10-20.
inline constexpr pm::sdk::EntryDesc Material_GetColorImpl{.key = "Material.GetColorImpl", .kind = pm::sdk::Kind::Icall,
                                                          .name = "UnityEngine.Material::GetColorImpl_Injected",
                                                          .params = Material_ColorImpl_params};

/// Sets the colour of a shader property (by its id) from the Color its second argument points at: the
/// engine function behind Material.SetColor. Inferred: from its name; Spectral Flame has called it by
/// this name in the game since 2026-10-20.
inline constexpr pm::sdk::EntryDesc Material_SetColorImpl{.key = "Material.SetColorImpl", .kind = pm::sdk::Kind::Icall,
                                                          .name = "UnityEngine.Material::SetColorImpl_Injected",
                                                          .params = Material_ColorImpl_params};

inline constexpr const char* Shader_PropertyToID_params[] = {"string"};

/// The id of a shader property's name, the same for every material while the game runs: the engine
/// function of Shader.PropertyToID. Static. Inferred: Unity's documented API; Spectral Flame has
/// called it by this name in the game since 2026-10-20.
inline constexpr pm::sdk::EntryDesc Shader_PropertyToID{.key = "Shader.PropertyToID", .kind = pm::sdk::Kind::Icall,
                                                        .flags = pm::sdk::kStatic,
                                                        .name = "UnityEngine.Shader::PropertyToID",
                                                        .params = Shader_PropertyToID_params};

// The flame shaders' properties (ShaderProperty entries, sdk/systems/scene.md): each a property a shader
// of the game's has, by its name, checked offline in the shaders of the Tanglewood_Street_House
// scene's assets (its shared assets and the shared assets they refer to: "Candle Flame" lies in the
// main menu's, the other two in the scene's own in build 25811160). The host reads each one's id at
// activation through Shader.PropertyToID; whether a material has it is the material's to answer
// (Material.HasProperty). Optional: a mod checks one before using it, and goes without what it colours.

/// The Candle Flame shader's tint, a Color: the shader shows its sprite (_CandleSprite) times it. The
/// game's candle flame materials ("Candle flame", "Candle flame player") and its lighters' use the
/// shader. Live: Spectral Flame set it and the flame showed the colour (2026-10-22, build 25811160).
inline constexpr pm::sdk::EntryDesc ShaderProperty_CandleFlame_TintColour{
    .key = "ShaderProperty.CandleFlame.TintColour", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Live, .flags = pm::sdk::kOptional, .name = "_TintColour",
    .scene = "Tanglewood_Street_House", .component = "Candle Flame"};

/// The Candle Flame shader's sprite, a Texture: the flame's picture, orange and yellow around a white
/// core in the game's own. Live: Spectral Flame swapped it for a grey copy in the game (2026-10-22,
/// build 25811160).
inline constexpr pm::sdk::EntryDesc ShaderProperty_CandleFlame_CandleSprite{
    .key = "ShaderProperty.CandleFlame.CandleSprite", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Live, .flags = pm::sdk::kOptional, .name = "_CandleSprite",
    .scene = "Tanglewood_Street_House", .component = "Candle Flame"};

/// The Lantern Flame shader graph's second colour, a Color: the lantern's flame is noise between its
/// two colours, this one the warm one in the game's "Lantern Flame" material. Live: Spectral Flame set
/// it in the game (2026-10-23, build 25811160).
inline constexpr pm::sdk::EntryDesc ShaderProperty_LanternFlame_Color2{
    .key = "ShaderProperty.LanternFlame.Color2", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Live, .flags = pm::sdk::kOptional, .name = "_Color_2",
    .scene = "Tanglewood_Street_House", .component = "Shader Graphs/Lantern Flame"};

/// The Lantern Flame shader graph's first colour, a Color, the blue one in the game's "Lantern Flame"
/// material. Live: Spectral Flame set it in the game (2026-10-23, build 25811160).
inline constexpr pm::sdk::EntryDesc ShaderProperty_LanternFlame_Color1{
    .key = "ShaderProperty.LanternFlame.Color1", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Live, .flags = pm::sdk::kOptional, .name = "_Color_1",
    .scene = "Tanglewood_Street_House", .component = "Shader Graphs/Lantern Flame"};

/// The Candle Melt shader's emission colour, a Color: the candle wax's glow, which Candle.UseNetworked
/// sets to Candle.candleEmissionColor when the candle is lit and to black when it goes out. The Tier 1
/// and 2 candles' wax materials ("Candle", "Candle T2") use the shader. Decompiled
/// (Candle.UseNetworked).
inline constexpr pm::sdk::EntryDesc ShaderProperty_CandleMelt_EmissionColor{
    .key = "ShaderProperty.CandleMelt.EmissionColor", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .name = "_EmissionColor",
    .scene = "Tanglewood_Street_House", .component = "Candle Melt"};

/// Unity's base class of textures (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Texture{.key = "Texture", .name = "Texture", .assembly = "UnityEngine.CoreModule",
                                            .ns = "UnityEngine"};

/// The texture's width in pixels. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Texture_get_width{.key = "Texture.get_width", .kind = pm::sdk::Kind::Method,
                                                      .flags = pm::sdk::kOptional, .owner = "Texture",
                                                      .name = "get_width"};

/// The texture's height in pixels. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Texture_get_height{.key = "Texture.get_height", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kOptional, .owner = "Texture",
                                                       .name = "get_height"};

/// How the texture wraps, a TextureWrapMode. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Texture_get_wrapMode{.key = "Texture.get_wrapMode", .kind = pm::sdk::Kind::Method,
                                                         .flags = pm::sdk::kOptional, .owner = "Texture",
                                                         .name = "get_wrapMode"};

/// Sets how the texture wraps. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Texture_set_wrapMode{.key = "Texture.set_wrapMode", .kind = pm::sdk::Kind::Method,
                                                         .flags = pm::sdk::kOptional, .owner = "Texture",
                                                         .name = "set_wrapMode"};

/// How the texture is filtered, a FilterMode. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Texture_get_filterMode{.key = "Texture.get_filterMode",
                                                           .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional,
                                                           .owner = "Texture", .name = "get_filterMode"};

/// Sets how the texture is filtered. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Texture_set_filterMode{.key = "Texture.set_filterMode",
                                                           .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional,
                                                           .owner = "Texture", .name = "set_filterMode"};

/// Unity's 2D texture (UnityEngine.CoreModule), a Texture. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Texture2D{.key = "Texture2D", .flags = pm::sdk::kOptional, .name = "Texture2D",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The constructor Texture2D(width, height): RGBA32 with mipmaps, readable. Run it on an object the
/// Texture2D type made. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Texture2D_ctor{.key = "Texture2D.ctor", .kind = pm::sdk::Kind::Method,
                                                   .flags = pm::sdk::kOptional, .owner = "Texture2D", .name = ".ctor",
                                                   .params = UnityRendering_int_int};

inline constexpr const char* Texture2D_ReadPixels_params[] = {"Rect", "int", "int"};

/// Copies a rectangle of the active RenderTexture into the texture at a corner. Inferred: Unity's
/// documented API (the overload without recalculateMipMaps).
inline constexpr pm::sdk::EntryDesc Texture2D_ReadPixels{.key = "Texture2D.ReadPixels", .kind = pm::sdk::Kind::Method,
                                                         .flags = pm::sdk::kOptional, .owner = "Texture2D",
                                                         .name = "ReadPixels", .params = Texture2D_ReadPixels_params};

/// The texels of one mip level, a Color32[]. Texture2D.GetPixels32() calls the same engine function
/// with level 0 (decompiled). Decompiled.
inline constexpr pm::sdk::EntryDesc Texture2D_GetPixels32{
    .key = "Texture2D.GetPixels32", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kOptional, .owner = "Texture2D", .name = "GetPixels32", .params = UnityRendering_int};

inline constexpr const char* Texture2D_SetPixels32_params[] = {"Color32[]", "int"};

/// Sets the texels of one mip level from a Color32[]. Texture2D.SetPixels32(colors) calls the same
/// engine function with level 0 (decompiled). Decompiled.
inline constexpr pm::sdk::EntryDesc Texture2D_SetPixels32{
    .key = "Texture2D.SetPixels32", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kOptional, .owner = "Texture2D", .name = "SetPixels32", .params = Texture2D_SetPixels32_params};

/// Uploads the texture's texels to the GPU, its mipmaps made when `updateMipmaps`, and its copy in
/// memory dropped when `makeNoLongerReadable`. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Texture2D_Apply{.key = "Texture2D.Apply", .kind = pm::sdk::Kind::Method,
                                                    .flags = pm::sdk::kOptional, .owner = "Texture2D", .name = "Apply",
                                                    .params = UnityRendering_bool_bool};

/// Unity's render texture (UnityEngine.CoreModule), a Texture. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderTexture{.key = "RenderTexture", .name = "RenderTexture",
                                                  .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The pixel formats of a render texture (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderTextureFormat{.key = "RenderTextureFormat", .name = "RenderTextureFormat",
                                                        .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// Four 8-bit channels, the layout of an RGBA32 texture's texels. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderTextureFormat_ARGB32{.key = "RenderTextureFormat.ARGB32",
                                                               .kind = pm::sdk::Kind::EnumValue,
                                                               .flags = pm::sdk::kOptional,
                                                               .owner = "RenderTextureFormat", .name = "ARGB32"};

/// How a render texture's colours are converted (UnityEngine.CoreModule). Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc RenderTextureReadWrite{.key = "RenderTextureReadWrite",
                                                           .name = "RenderTextureReadWrite",
                                                           .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// sRGB: colours are read back as an sRGB texture stores them. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderTextureReadWrite_sRGB{.key = "RenderTextureReadWrite.sRGB",
                                                                .kind = pm::sdk::Kind::EnumValue,
                                                                .flags = pm::sdk::kOptional,
                                                                .owner = "RenderTextureReadWrite", .name = "sRGB"};

inline constexpr const char* RenderTexture_GetTemporary_params[] = {"int", "int", "int", "RenderTextureFormat",
                                                                    "RenderTextureReadWrite"};

/// A temporary render texture of a size, depth buffer bits, format and colour space, from Unity's pool;
/// give it back with ReleaseTemporary. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderTexture_GetTemporary{
    .key = "RenderTexture.GetTemporary", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic | pm::sdk::kOptional,
    .owner = "RenderTexture", .name = "GetTemporary", .params = RenderTexture_GetTemporary_params};

/// Gives a temporary render texture back to the pool. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderTexture_ReleaseTemporary{
    .key = "RenderTexture.ReleaseTemporary", .kind = pm::sdk::Kind::Method,
    .flags = pm::sdk::kStatic | pm::sdk::kOptional, .owner = "RenderTexture", .name = "ReleaseTemporary"};

/// The render texture drawing and ReadPixels use now, null for the screen. Static. Inferred: Unity's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc RenderTexture_get_active{
    .key = "RenderTexture.get_active", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic | pm::sdk::kOptional,
    .owner = "RenderTexture", .name = "get_active"};

/// Sets the active render texture. Static. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc RenderTexture_set_active{
    .key = "RenderTexture.set_active", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic | pm::sdk::kOptional,
    .owner = "RenderTexture", .name = "set_active"};

/// Unity's drawing functions (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Graphics{.key = "Graphics", .name = "Graphics",
                                             .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

inline constexpr const char* Graphics_Blit_params[] = {"Texture", "RenderTexture"};

/// Draws a texture into a render texture, whatever the texture's own format or readability. Static.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Graphics_Blit{.key = "Graphics.Blit", .kind = pm::sdk::Kind::Method,
                                                  .flags = pm::sdk::kStatic | pm::sdk::kOptional, .owner = "Graphics",
                                                  .name = "Blit", .params = Graphics_Blit_params};

// Building a mesh at run time, for a SkinnedMeshRenderer to draw and Cloth to simulate (sdk/api/models.h). Each setter
// takes a whole managed array and replaces that channel. Set the vertices before the triangles, whose indices must lie
// within them; setting the triangles also recalculates the mesh's bounds.

/// Unity's BoneWeight (UnityEngine.CoreModule), a struct: four weights, then four bone indices. Its type makes the
/// BoneWeight[] Mesh.set_boneWeights takes. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc BoneWeight{.key = "BoneWeight", .name = "BoneWeight",
                                               .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// Unity's Matrix4x4 (UnityEngine.CoreModule), a struct of sixteen floats by column. Its type makes the Matrix4x4[]
/// Mesh.set_bindposes takes. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Matrix4x4{.key = "Matrix4x4", .name = "Matrix4x4",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

inline constexpr const char* Mesh_Vector3Array[] = {"Vector3[]"};
inline constexpr const char* Mesh_Vector2Array[] = {"Vector2[]"};
inline constexpr const char* Mesh_Int32Array[] = {"Int32[]"};
inline constexpr const char* Mesh_BoneWeightArray[] = {"BoneWeight[]"};
inline constexpr const char* Mesh_Matrix4x4Array[] = {"Matrix4x4[]"};

/// Sets the mesh's vertex positions, a Vector3[] in the mesh's own space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Mesh_set_vertices{.key = "Mesh.set_vertices", .kind = pm::sdk::Kind::Method,
                                                      .owner = "Mesh", .name = "set_vertices",
                                                      .params = Mesh_Vector3Array};

/// Sets the mesh's vertex normals, a Vector3[] as long as its vertices. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Mesh_set_normals{.key = "Mesh.set_normals", .kind = pm::sdk::Kind::Method,
                                                     .owner = "Mesh", .name = "set_normals",
                                                     .params = Mesh_Vector3Array};

/// Sets the mesh's first texture coordinates, a Vector2[] as long as its vertices. Inferred: Unity's documented
/// property setter.
inline constexpr pm::sdk::EntryDesc Mesh_set_uv{.key = "Mesh.set_uv", .kind = pm::sdk::Kind::Method, .owner = "Mesh",
                                                .name = "set_uv", .params = Mesh_Vector2Array};

/// Sets the mesh's triangles, an Int32[] of three vertex indices each; a triangle whose vertices run clockwise as seen
/// faces the viewer. Also recalculates the bounds. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Mesh_set_triangles{.key = "Mesh.set_triangles", .kind = pm::sdk::Kind::Method,
                                                       .owner = "Mesh", .name = "set_triangles",
                                                       .params = Mesh_Int32Array};

/// Sets each vertex's bone weights, a BoneWeight[] as long as its vertices, for a SkinnedMeshRenderer. Inferred:
/// Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Mesh_set_boneWeights{.key = "Mesh.set_boneWeights", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Mesh", .name = "set_boneWeights",
                                                         .params = Mesh_BoneWeightArray};

/// Sets each bone's bind pose, a Matrix4x4[] with one per bone of the renderer: the inverse of the bone's transform
/// relative to the mesh when the mesh was bound. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Mesh_set_bindposes{.key = "Mesh.set_bindposes", .kind = pm::sdk::Kind::Method,
                                                       .owner = "Mesh", .name = "set_bindposes",
                                                       .params = Mesh_Matrix4x4Array};

inline constexpr const char* SkinnedMeshRenderer_Mesh[] = {"Mesh"};
inline constexpr const char* SkinnedMeshRenderer_TransformArray[] = {"Transform[]"};
inline constexpr const char* SkinnedMeshRenderer_Transform[] = {"Transform"};

/// Sets the mesh the renderer draws. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc SkinnedMeshRenderer_set_sharedMesh{
    .key = "SkinnedMeshRenderer.set_sharedMesh", .kind = pm::sdk::Kind::Method, .owner = "SkinnedMeshRenderer",
    .name = "set_sharedMesh", .params = SkinnedMeshRenderer_Mesh};

/// Sets the bones the mesh's bone weights index, a Transform[]. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc SkinnedMeshRenderer_set_bones{
    .key = "SkinnedMeshRenderer.set_bones", .kind = pm::sdk::Kind::Method, .owner = "SkinnedMeshRenderer",
    .name = "set_bones", .params = SkinnedMeshRenderer_TransformArray};

/// Sets the bone the renderer's bounds follow. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc SkinnedMeshRenderer_set_rootBone{
    .key = "SkinnedMeshRenderer.set_rootBone", .kind = pm::sdk::Kind::Method, .owner = "SkinnedMeshRenderer",
    .name = "set_rootBone", .params = SkinnedMeshRenderer_Transform};

/// Whether the renderer keeps updating its mesh, and its bounds, while no camera sees it. Inferred: Unity's documented
/// property setter.
inline constexpr pm::sdk::EntryDesc SkinnedMeshRenderer_set_updateWhenOffscreen{
    .key = "SkinnedMeshRenderer.set_updateWhenOffscreen", .kind = pm::sdk::Kind::Method,
    .owner = "SkinnedMeshRenderer", .name = "set_updateWhenOffscreen", .params = UnityRendering_bool};

inline constexpr const char* Material_Shader[] = {"Shader"};
inline constexpr const char* Material_SetFloatImpl_params[] = {"int", "float"};

/// The constructor Material(Shader): a new material of that shader with the shader's default values. Run it on an
/// object the Material type made. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Material_ctor_Shader{.key = "Material.ctor(Shader)", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Material", .name = ".ctor",
                                                         .params = Material_Shader};

/// The material's shader. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Material_get_shader{.key = "Material.get_shader", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Material", .name = "get_shader"};

/// Sets a float shader property (by its id): the engine function behind Material.SetFloat. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Material_SetFloatImpl{.key = "Material.SetFloatImpl", .kind = pm::sdk::Kind::Icall,
                                                          .name = "UnityEngine.Material::SetFloatImpl",
                                                          .params = Material_SetFloatImpl_params};

/// Unity's face culling modes (UnityEngine.CoreModule, UnityEngine.Rendering), the values a shader's cull property
/// takes. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CullMode{.key = "CullMode", .name = "CullMode",
                                             .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine.Rendering"};

/// No culling: both sides of each triangle are drawn. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CullMode_Off{.key = "CullMode.Off", .kind = pm::sdk::Kind::EnumValue,
                                                 .owner = "CullMode", .name = "Off"};

// The properties of URP's Lit shader, "Universal Render Pipeline/Lit", the lit shader of the game's investigator
// bodies: every material of the PCPlayer prefab's body uses it (read from the asset files). A material made with
// Material(Shader) from a body material's shader takes the shader's defaults, so only what a mod changes needs
// setting. Optional, as every ShaderProperty: a mod checks one before using it.

/// The Lit shader's base colour, a Color: the surface's colour, times its base texture (white when it has none).
/// Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc ShaderProperty_Lit_BaseColor{
    .key = "ShaderProperty.Lit.BaseColor", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .name = "_BaseColor",
    .scene = "Tanglewood_Street_House", .component = "Universal Render Pipeline/Lit"};

/// The Lit shader's face culling, a float holding a CullMode: the game's body materials cull back faces, and
/// CullMode.Off draws both sides, as a sheet seen from inside needs. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc ShaderProperty_Lit_Cull{
    .key = "ShaderProperty.Lit.Cull", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .name = "_Cull",
    .scene = "Tanglewood_Street_House", .component = "Universal Render Pipeline/Lit"};

/// The Lit shader's smoothness, a float from 0 (matte) to 1 (mirror): how sharp its highlights and reflections are. The
/// shader's default is 0.5. The game's untextured fabric (its denim jacket material) sets 0 with metallic 0; its
/// textured fabrics take theirs from a gloss map. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc ShaderProperty_Lit_Smoothness{
    .key = "ShaderProperty.Lit.Smoothness", .kind = pm::sdk::Kind::ShaderProperty,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .name = "_Smoothness",
    .scene = "Tanglewood_Street_House", .component = "Universal Render Pipeline/Lit"};

}  // namespace pm::map

/// @file
/// @brief The game API for rendering (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null, all zero) without
/// touching the game. Every object they return is a managed reference: keep it across frames only in a
/// pm::api::Held.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Four floats, as Unity's Color: red, green, blue and alpha, above 1 for an HDR colour.
struct Color {
  float r;  ///< red, 0 to 1 (above 1 for HDR)
  float g;  ///< green, 0 to 1 (above 1 for HDR)
  float b;  ///< blue, 0 to 1 (above 1 for HDR)
  float a;  ///< alpha, 0 (clear) to 1 (opaque)
};

/// Four bytes, as Unity's Color32: red, green, blue and alpha, as an RGBA32 texture stores them.
struct Color32 {
  uint8_t r;  ///< red, 0 to 255
  uint8_t g;  ///< green, 0 to 255
  uint8_t b;  ///< blue, 0 to 255
  uint8_t a;  ///< alpha, 0 (clear) to 255 (opaque)
};

/// A renderer. `renderer` below is one.
struct Renderer {
  /// The Renderer type: type_object() for Component.GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Renderer> type{};
  /// get_sharedMaterial(renderer). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Renderer_get_sharedMaterial, void*(void*)> get_sharedMaterial{};
  /// get_material(renderer): its own material. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Renderer_get_material, void*(void*)> get_material{};
  /// get_enabled(renderer). Unresolved: false.
  static constexpr pm::api::Method<pm::map::Renderer_get_enabled, bool(void*)> get_enabled{};
  /// set_enabled(renderer, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Renderer_set_enabled, void(void*, bool)> set_enabled{};
  /// get_sharedMaterials(renderer): a Material[] (pm::api::RefArray). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Renderer_get_sharedMaterials, void*(void*)> get_sharedMaterials{};
  /// set_sharedMaterials(renderer, materials), a Material[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Renderer_set_sharedMaterials, void(void*, void*)> set_sharedMaterials{};
};

/// A light. `light` below is the Light.
struct Light {
  /// The Light type: type_object() for Component.GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Light> type{};
  /// get_color(light). Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Light_get_color, Color(void*)> get_color{};
  /// set_color(light, &colour): the Color passed by address, which the callee may change, so pass a
  /// copy. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Light_set_color, void(void*, Color*)> set_color{};
  /// get_intensity(light). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Light_get_intensity, float(void*)> get_intensity{};
  /// set_intensity(light, intensity). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Light_set_intensity, void(void*, float)> set_intensity{};
  /// set_type(light, kind), a LightType number. Optional: check available(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Light_set_type, void(void*, int32_t)> set_type{};
  /// set_range(light, metres). Optional: check available(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Light_set_range, void(void*, float)> set_range{};
};

/// A mesh.
struct Mesh {
  /// The Mesh type: make() for a new one, then ctor. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Mesh> type{};
  /// ctor(mesh): runs Mesh() on an object Mesh::type made. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_ctor, void(void*)> ctor{};
  /// set_vertices(mesh, vertices), a Vector3[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_set_vertices, void(void*, void*)> set_vertices{};
  /// set_normals(mesh, normals), a Vector3[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_set_normals, void(void*, void*)> set_normals{};
  /// set_uv(mesh, uv), a Vector2[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_set_uv, void(void*, void*)> set_uv{};
  /// set_triangles(mesh, indices), an Int32[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_set_triangles, void(void*, void*)> set_triangles{};
  /// set_boneWeights(mesh, weights), a BoneWeight[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_set_boneWeights, void(void*, void*)> set_boneWeights{};
  /// set_bindposes(mesh, poses), a Matrix4x4[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Mesh_set_bindposes, void(void*, void*)> set_bindposes{};
};

/// Four weights, then four bone indices, as Unity's BoneWeight.
struct BoneWeight {
  /// The type: make_array() for a BoneWeight[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::BoneWeight> type{};
  float weight0;  ///< The first bone's weight, 0 to 1; the four add up to 1.
  float weight1;  ///< The second bone's weight.
  float weight2;  ///< The third bone's weight.
  float weight3;  ///< The fourth bone's weight.
  int32_t bone0;  ///< The first bone's index in the renderer's bones.
  int32_t bone1;  ///< The second bone's index.
  int32_t bone2;  ///< The third bone's index.
  int32_t bone3;  ///< The fourth bone's index.
};

/// Sixteen floats by column, as Unity's Matrix4x4.
struct Matrix4x4 {
  /// The type: make_array() for a Matrix4x4[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Matrix4x4> type{};
  float m[16];  ///< The elements by column: m[0] to m[3] the first column.
};

/// A skinned renderer, a character's body. `skinned` below is the SkinnedMeshRenderer.
struct SkinnedMeshRenderer {
  /// The type: type_object() for Component.GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::SkinnedMeshRenderer> type{};
  /// BakeMesh(skinned, mesh, useScale): writes the posed mesh into `mesh`. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::SkinnedMeshRenderer_BakeMesh, void(void*, void*, bool)> BakeMesh{};
  /// set_sharedMesh(skinned, mesh). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SkinnedMeshRenderer_set_sharedMesh, void(void*, void*)> set_sharedMesh{};
  /// set_bones(skinned, bones), a Transform[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SkinnedMeshRenderer_set_bones, void(void*, void*)> set_bones{};
  /// set_rootBone(skinned, transform). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SkinnedMeshRenderer_set_rootBone, void(void*, void*)> set_rootBone{};
  /// set_updateWhenOffscreen(skinned, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SkinnedMeshRenderer_set_updateWhenOffscreen, void(void*, bool)>
      set_updateWhenOffscreen{};
};

/// The holder of a MeshRenderer's mesh. `filter` below is the MeshFilter.
struct MeshFilter {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::MeshFilter> type{};
  /// set_sharedMesh(filter, mesh). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MeshFilter_set_sharedMesh, void(void*, void*)> set_sharedMesh{};
};

/// A renderer of a MeshFilter's mesh.
struct MeshRenderer {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::MeshRenderer> type{};
};

/// The kinds of light.
namespace LightType {
/// A point light: Point(). Optional: check available(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::LightType_Point> Point{};
}  // namespace LightType

/// A material. `material` below is the Material, `id` a property id from Shader::PropertyToID.
struct Material {
  /// GetTextureImpl(material, id). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Material_GetTextureImpl, void*(void*, int32_t)> GetTextureImpl{};
  /// SetTextureImpl(material, id, texture). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Material_SetTextureImpl, void(void*, int32_t, void*)> SetTextureImpl{};
  /// HasProperty(material, id). Unresolved: false.
  static constexpr pm::api::Icall<pm::map::Material_HasProperty, bool(void*, int32_t)> HasProperty{};
  /// GetColorImpl(material, id, &out). Unresolved: leaves `out` as it was.
  static constexpr pm::api::Icall<pm::map::Material_GetColorImpl, void(void*, int32_t, Color*)> GetColorImpl{};
  /// SetColorImpl(material, id, &colour): the Color by reference, so pass a copy. Unresolved: does
  /// nothing.
  static constexpr pm::api::Icall<pm::map::Material_SetColorImpl, void(void*, int32_t, Color*)> SetColorImpl{};
  /// The type: make() for a new one, then ctor_Shader; make_array() for a Material[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Material> type{};
  /// ctor_Shader(material, shader): runs Material(Shader) on an object Material::type made. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Material_ctor_Shader, void(void*, void*)> ctor_Shader{};
  /// get_shader(material). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Material_get_shader, void*(void*)> get_shader{};
  /// SetFloatImpl(material, id, value). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Material_SetFloatImpl, void(void*, int32_t, float)> SetFloatImpl{};
};

/// Unity's face culling modes, for a shader's cull property.
namespace CullMode {
/// Off(): both sides drawn. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::CullMode_Off> Off{};
}  // namespace CullMode

/// The properties of URP's Lit shader, the game's investigator bodies' shader, for Material's by-id functions:
/// BaseColor.id(). Check available() first. Unresolved: 0.
struct LitShader {
  /// The base colour, a Color.
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_Lit_BaseColor> BaseColor{};
  /// The face culling, a float holding a CullMode number.
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_Lit_Cull> Cull{};
  /// The smoothness, a float from 0 (matte) to 1.
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_Lit_Smoothness> Smoothness{};
};

/// Shaders.
struct Shader {
  /// PropertyToID(name), `name` a System.String (pm::api::make_string). Unresolved: 0.
  static constexpr pm::api::Icall<pm::map::Shader_PropertyToID, int32_t(void*)> PropertyToID{};
};

/// The Candle Flame shader's properties, for Material's by-id functions: TintColour.id(). Check
/// available() first. Unresolved: 0.
struct CandleFlame {
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_CandleFlame_TintColour> TintColour{};  ///< The TintColour property: TintColour.id()
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_CandleFlame_CandleSprite> CandleSprite{};  ///< The CandleSprite property: CandleSprite.id()
};

/// The Lantern Flame shader graph's properties: Color2.id(). Check available() first. Unresolved: 0.
struct LanternFlame {
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_LanternFlame_Color2> Color2{};  ///< The Color2 property: Color2.id()
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_LanternFlame_Color1> Color1{};  ///< The Color1 property: Color1.id()
};

/// The Candle Melt shader's properties (candle wax): EmissionColor.id(). Check available() first.
/// Unresolved: 0.
struct CandleMelt {
  static constexpr pm::api::ShaderProperty<pm::map::ShaderProperty_CandleMelt_EmissionColor> EmissionColor{};  ///< The EmissionColor property: EmissionColor.id()
};

/// A texture. `texture` below is one.
struct Texture {
  /// get_width(texture). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Texture_get_width, int32_t(void*)> get_width{};
  /// get_height(texture). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Texture_get_height, int32_t(void*)> get_height{};
  /// get_wrapMode(texture). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Texture_get_wrapMode, int32_t(void*)> get_wrapMode{};
  /// set_wrapMode(texture, mode). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Texture_set_wrapMode, void(void*, int32_t)> set_wrapMode{};
  /// get_filterMode(texture). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Texture_get_filterMode, int32_t(void*)> get_filterMode{};
  /// set_filterMode(texture, mode). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Texture_set_filterMode, void(void*, int32_t)> set_filterMode{};
};

/// A 2D texture. `texture` below is the Texture2D.
struct Texture2D {
  /// The Texture2D type: make() a new one for ctor. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Texture2D> type{};
  /// ctor(texture, width, height). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Texture2D_ctor, void(void*, int32_t, int32_t)> ctor{};
  /// ReadPixels(texture, &rect, x, y): the Rect passed by address, which the callee may change, so pass
  /// a copy. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Texture2D_ReadPixels, void(void*, Rect*, int32_t, int32_t)>
      ReadPixels{};
  /// GetPixels32(texture, level): a Color32[], read as a pm::api::Array<Color32>. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Texture2D_GetPixels32, void*(void*, int32_t)> GetPixels32{};
  /// SetPixels32(texture, colours, level). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Texture2D_SetPixels32, void(void*, void*, int32_t)> SetPixels32{};
  /// Apply(texture, updateMipmaps, makeNoLongerReadable). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Texture2D_Apply, void(void*, bool, bool)> Apply{};
};

/// The pixel formats of a render texture.
namespace RenderTextureFormat {
/// Four 8-bit channels: ARGB32(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::RenderTextureFormat_ARGB32> ARGB32{};
}  // namespace RenderTextureFormat

/// How a render texture's colours are converted.
namespace RenderTextureReadWrite {
/// As an sRGB texture stores them: sRGB(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::RenderTextureReadWrite_sRGB> sRGB{};
}  // namespace RenderTextureReadWrite

/// Render textures.
struct RenderTexture {
  /// GetTemporary(width, height, depthBits, format, readWrite). Unresolved: null.
  static constexpr pm::api::Method<pm::map::RenderTexture_GetTemporary,
                                   void*(int32_t, int32_t, int32_t, int32_t, int32_t)>
      GetTemporary{};
  /// ReleaseTemporary(renderTexture). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RenderTexture_ReleaseTemporary, void(void*)> ReleaseTemporary{};
  /// get_active(). Unresolved: null.
  static constexpr pm::api::Method<pm::map::RenderTexture_get_active, void*()> get_active{};
  /// set_active(renderTexture), null for the screen. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RenderTexture_set_active, void(void*)> set_active{};
};

/// Drawing.
struct Graphics {
  /// Blit(texture, renderTexture). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Graphics_Blit, void(void*, void*)> Blit{};
};

/// @}
}  // namespace phasmo
