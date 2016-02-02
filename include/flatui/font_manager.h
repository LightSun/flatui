// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <set>
#include "fplbase/renderer.h"
#include "flatui/internal/distance_computer.h"
#include "flatui/internal/glyph_cache.h"
#include "flatui/internal/flatui_util.h"
#include "flatui/internal/hb_complex_font.h"

/// @cond FLATUI_INTERNAL
// Use libunibreak for a line breaking.
#if !defined(FLATUI_USE_LIBUNIBREAK)
// For now, it's automatically turned on.
#define FLATUI_USE_LIBUNIBREAK 1
#endif  // !defined(FLATUI_USE_LIBUNIBREAK)

// Forward decls for FreeType.
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_GlyphSlotRec_ *FT_GlyphSlot;
/// @endcond

namespace flatui {

/// @file
/// @addtogroup flatui_font_manager
/// @{

/// @cond FLATUI_INTERNAL
// Forward decl.
class FaceData;
class FontTexture;
class FontBuffer;
class FontMetrics;
class WordEnumerator;
struct ScriptInfo;
/// @endcond

/// @var kFreeTypeUnit
///
/// @brief Constant to convert FreeType unit to pixel unit.
///
/// In FreeType & Harfbuzz, the position value unit is 1/64 px whereas
/// configurable in FlatUI. The constant is used to convert FreeType unit
/// to px.
const int32_t kFreeTypeUnit = 64;

/// @var kGlyphCacheWidth
///
/// @brief The Default size of the glyph cache width.
const int32_t kGlyphCacheWidth = 1024;

/// @var kGlyphCacheHeight
///
/// @brief The default size of the glyph cache height.
const int32_t kGlyphCacheHeight = 1024;

/// @var kLineHeightDefault
///
/// @brief Default value for a line height factor.
///
/// The line height is derived as the factor * a font height.
/// To change the line height, use `SetLineHeight()` API.
const float kLineHeightDefault = 1.2f;

/// @var kCaretPositionInvalid
///
/// @brief A sentinel value representing an invalid caret position.
const mathfu::vec2i kCaretPositionInvalid = mathfu::vec2i(-1, -1);

/// @var kDefaultLanguage
///
/// @brief The default language used for a line break.
const char *const kDefaultLanguage = "en";

/// @enum TextLayoutDirection
///
/// @brief Specify how to layout texts.
/// Default value is TextLayoutDirectionLTR.
///
enum TextLayoutDirection {
  kTextLayoutDirectionLTR = 0,
  kTextLayoutDirectionRTL = 1,
  kTextLayoutDirectionTTB = 2,
};

/// @enum TextAlignment
///
/// @brief Alignment of the text.
///
/// @note: Used for a typographic alignment in a label.
/// The enumeration is different from flatui::Alignment as it supports
/// justification setting.
/// Note that in RTL layout direction mode, the setting is flipped.
/// (e.g. kTextAlignmentLeft becomes kTextAlignmentRight)
/// kTextAlignLeft/Right/CenterJustify variants specify how the last line is
/// flushed left, right or centered.
///
/// **Enumerations**:
/// * `kTextAlignmentLeft` - Text is aligned to the left of the given area.
/// Default setting.
/// * `kTextAlignmentRight` - Text is aligned to the right of the given area.
/// * `kTextAlignmentCenter` - Text is aligned to the center of the given
/// area.
/// * `kTextAlignmentJustify` - Text is 'justified'. Spaces between words are
/// stretched to align both the left and right ends of each line of text.
/// * `kTextAlignmentLeftJustify` - An alias of kTextAlignmentJustify. The last
/// line of a paragraph is aligned to the left.
/// * `kTextAlignmentRightJustify` - Text is 'justified'. The last line of a
/// paragraph is aligned to the right.
/// * `kTextAlignmentCenterJustify` - Text is 'justified'. The last line of a
/// paragraph is aligned to the center.
///
/// For more detail of each settings, refer:
/// https://en.wikipedia.org/wiki/Typographic_alignment
enum TextAlignment {
  kTextAlignmentLeft = 0,
  kTextAlignmentRight = 1,
  kTextAlignmentCenter = 2,
  kTextAlignmentJustify = 4,
  kTextAlignmentLeftJustify = kTextAlignmentJustify,
  kTextAlignmentRightJustify = kTextAlignmentJustify | kTextAlignmentRight,
  kTextAlignmentCenterJustify = kTextAlignmentJustify | kTextAlignmentCenter,
};

/// @class FontBufferParameters
///
/// @brief This class that includes font buffer parameters. It is used as a key
/// in the unordered_map to look up FontBuffer.
class FontBufferParameters {
 public:
  /// @brief The default constructor for an empty FontBufferParameters.
  FontBufferParameters()
      : font_id_(kNullHash),
        text_id_(kNullHash),
        font_size_(0),
        size_(mathfu::kZeros2i),
        flags_value_(0) {}

  /// @brief Constructor for a FontBufferParameters.
  ///
  /// @param[in] font_id The HashedId for the font.
  /// @param[in] text_id The HashedID for the text.
  /// @param[in] font_size A float representing the size of the font.
  /// @param[in] size The requested size of the FontBuffer in pixels.
  /// The created FontBuffer size can be smaller than requested size as the
  /// FontBuffer size is calcuated by the layout and rendering result.
  /// @param[in] text_alignment A horizontal alignment of multi line
  /// buffer.
  /// @param[in] glyph_flags A flag determing SDF generation for the font.
  /// @param[in] caret_info A bool determining if the font buffer contains caret
  /// info.
  FontBufferParameters(HashedId font_id, HashedId text_id, float font_size,
                       const mathfu::vec2i &size, TextAlignment text_alignment,
                       GlyphFlags glyph_flags, bool caret_info) {
    font_id_ = font_id;
    text_id_ = text_id;
    font_size_ = font_size;
    size_ = size;
    flags_.text_alignement = text_alignment;
    flags_.glyph_flags = glyph_flags;
    flags_.caret_info = caret_info;
  }

  /// @brief The equal-to operator for comparing FontBufferParameters for
  /// equality.
  ///
  /// @param[in] other The other FontBufferParameters to check against for
  /// equality.
  ///
  /// @return Returns `true` if the two FontBufferParameters are equal.
  /// Otherwise it returns `false`.
  bool operator==(const FontBufferParameters &other) const {
    return (font_id_ == other.font_id_ && text_id_ == other.text_id_ &&
            font_size_ == other.font_size_ && size_.x() == other.size_.x() &&
            size_.y() == other.size_.y() && flags_value_ == other.flags_value_);
  }

  /// @brief The hash function for FontBufferParameters.
  ///
  /// @param[in] key A FontBufferParameters to use as the key for
  /// hashing.
  ///
  /// @return Returns a `size_t` of the hash of the FontBufferParameters.
  size_t operator()(const FontBufferParameters &key) const {
    // Note that font_id_ and text_id_ are already hashed values.
    size_t value = (font_id_ ^ (text_id_ << 1)) >> 1;
    value = value ^ (std::hash<float>()(key.font_size_) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(key.flags_value_) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(key.size_.x()) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(key.size_.y()) << 1) >> 1;
    return value;
  }

  /// @return Returns a hash value of the text.
  HashedId get_text_id() const { return text_id_; }

  /// @return Returns the size value.
  const mathfu::vec2i &get_size() const { return size_; }

  /// @return Returns the font size.
  float get_font_size() const { return font_size_; }

  /// @return Returns a text alignment info.
  TextAlignment get_text_alignment() const {
    return flags_.text_alignement;
  }

  /// @return Returns a glyph setting info.
  GlyphFlags get_glyph_flags() const {
    return flags_.glyph_flags;
  }

  /// @return Returns a flag to indicate if the buffer has caret info.
  bool get_caret_info_flag() const { return flags_.caret_info; }

  /// Retrieve a line length of the text based on given parameters.
  /// a fixed line length (get_size.x()) will be used if the text is justified
  /// or right aligned otherwise the line length will be determined by the text
  /// layout phase.
  /// @return Returns the expected line width.
  int32_t get_line_length() const {
    auto alignment = get_text_alignment();
    if (alignment == kTextAlignmentLeft || alignment == kTextAlignmentCenter) {
      return 0;
    } else {
      // Other settings will use max width of the given area.
      return size_.x() * kFreeTypeUnit;
    }
  }

  /// @return Returns the multi line setting.
  bool get_multi_line_setting() const {
    if (!size_.x()) {
      return false;
    }
    if (get_text_alignment() != kTextAlignmentLeft) {
      return true;
    } else {
      return size_.y() == 0 || size_.y() > font_size_;
    }
  }

 private:
  HashedId font_id_;
  HashedId text_id_;
  float font_size_;
  mathfu::vec2i size_;

  // A structure that defines bit fields to hold multiple flag values related to
  // the font buffer.
  struct FontBufferFlags {
    bool caret_info:1;
    GlyphFlags glyph_flags:2;
    TextAlignment text_alignement:3;
  };
  union {
    uint32_t flags_value_;
    FontBufferFlags flags_;
  };
};

/// @class FontManager
///
/// @brief FontManager manages font rendering with OpenGL utilizing freetype
/// and harfbuzz as a glyph rendering and layout back end.
///
/// It opens speficied OpenType/TrueType font and rasterize to OpenGL texture.
/// An application can use the generated texture for a text rendering.
///
/// @warning The class is not threadsafe, it's expected to be only used from
/// within OpenGL rendering thread.
class FontManager {
 public:
  /// @brief The default constructor for FontManager.
  FontManager();

  /// @brief Constructor for FontManager with a given cache size.
  ///
  /// @note The given size is rounded up to nearest power of 2 internally to be
  /// used as an OpenGL texture sizes.
  ///
  /// @param[in] cache_size The size of the cache, in pixels.
  FontManager(const mathfu::vec2i &cache_size);

  /// @brief The destructor for FontManager.
  ~FontManager();

  /// @brief Open a font face, TTF, OT font.
  ///
  /// In this version it supports only single face at a time.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `false` when failing to open font, such as
  /// a file open error, an invalid file format etc. Returns `true`
  /// if the font is opened successfully.
  bool Open(const char *font_name);

  /// @brief Discard a font face that has been opened via `Open()`.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `true` if the font was closed successfully. Otherwise
  /// it returns `false`.
  bool Close(const char *font_name);

  /// @brief Select the current font face. The font face will be used by a glyph
  /// rendering.
  ///
  /// @note The font face needs to have been opened by `Open()`.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `true` if the font was selected successfully. Otherwise it
  /// returns false.
  bool SelectFont(const char *font_name);

  /// @brief Select the current font faces with a fallback priority.
  ///
  /// @param[in] font_names An array of C-string corresponding to the name of
  /// the font. Font names in the array are stored in a priority order.
  /// @param[in] count A count of font names in the array.
  ///
  /// @return Returns `true` if the fonts were selected successfully. Otherwise
  /// it returns false.
  bool SelectFont(const char *font_names[], int32_t count);

  /// @brief Retrieve a texture with the given text.
  ///
  /// @note This API doesn't use the glyph cache, instead it writes the string
  /// image directly to the returned texture. The user can use this API when a
  /// font texture is used for a long time, such as a string image used in game
  /// HUD.
  ///
  /// @param[in] text A C-string in UTF-8 format with the text for the texture.
  /// @param[in] length The length of the text string.
  /// @param[in] ysize The height of the texture.
  ///
  /// @return Returns a pointer to the FontTexture.
  FontTexture *GetTexture(const char *text, uint32_t length, float ysize);

  /// @brief Retrieve a vertex buffer for a font rendering using glyph cache.
  ///
  /// @param[in] text A C-string in UTF-8 format with the text for the
  /// FontBuffer.
  /// @param[in] length The length of the text string.
  /// @param[in] parameters The FontBufferParameters specifying the parameters
  /// for the FontBuffer.
  ///
  /// @return Returns `nullptr` if the string does not fit in the glyph cache.
  ///  When this happens, caller may flush the glyph cache with
  /// `FlushAndUpdate()` call and re-try the `GetBuffer()` call.
  FontBuffer *GetBuffer(const char *text, size_t length,
                        const FontBufferParameters &parameters);

  /// @brief Set the renderer to be used to create texture instances.
  ///
  /// @param[in] renderer The Renderer to set for creating textures.
  void SetRenderer(fplbase::Renderer &renderer);

  /// @return Returns `true` if a font has been loaded. Otherwise it returns
  /// `false`.
  bool FontLoaded() { return face_initialized_; }

  /// @brief Indicate a start of new layout pass.
  ///
  /// Call the API each time the user starts a layout pass.
  ///
  /// @note The user of the font_manager class is expected to use the class in
  /// 2 passes: a layout pass and a render pass.
  ///
  /// During the layout pass, the user invokes `GetBuffer()` calls to fetch all
  /// required glyphs into the cache. Once in the cache, the user can retrieve
  /// the size of the strings to position them.
  ///
  /// Once the layout pass finishes, the user invokes `StartRenderPass()` to
  /// upload cache images to the atlas texture, and use the texture in the
  /// render pass. This design helps to minimize the frequency of the atlas
  /// texture upload.
  void StartLayoutPass();

  /// @brief Flush the existing glyph cache contents and start new layout pass.
  ///
  /// Call this API while in a layout pass when the glyph cache is fragmented.
  void FlushAndUpdate() { UpdatePass(true); }

  /// @brief Flush the existing FontBuffer in the cache.
  ///
  /// Call this API when FontBuffers are not used anymore.
  void FlushLayout() { map_buffers_.clear(); }

  /// @brief Indicates a start of new render pass.
  ///
  /// Call the API each time the user starts a render pass.
  void StartRenderPass() { UpdatePass(false); }

  /// @return Returns font atlas texture.
  fplbase::Texture *GetAtlasTexture() { return atlas_texture_.get(); }

  /// @brief The user can supply a size selector function to adjust glyph sizes
  /// when storing a glyph cache entry. By doing that, multiple strings with
  /// slightly different sizes can share the same glyph cache entry, so that the
  /// user can reduce a total # of cached entries.
  ///
  /// @param[in] selector The callback argument is a request for the glyph
  /// size in pixels. The callback should return the desired glyph size that
  /// should be used for the glyph sizes.
  void SetSizeSelector(std::function<int32_t(const int32_t)> selector) {
    size_selector_.swap(selector);
  }

  /// @param[in] locale  A C-string corresponding to the of the
  /// language defined in ISO 639 and the country code difined in ISO 3166
  /// separated
  /// by '-'. (e.g. 'en-US').
  ///
  /// @note The API sets language, script and layout direction used for
  /// following text renderings based on the locale.
  void SetLocale(const char *locale);

  /// @return Returns the current language setting as a std::string.
  const char *GetLanguage() { return language_.c_str(); }

  /// @brief Set a script used for a script layout.
  ///
  /// @param[in] language ISO 15924 based script code. Default setting is
  /// 'Latn' (Latin).
  /// For more detail, refer http://unicode.org/iso15924/iso15924-codes.html
  ///
  void SetScript(const char *script);

  /// @brief Set a script layout direction.
  ///
  /// @param[in] direction Text layout direction.
  ///            TextLayoutDirectionLTR & TextLayoutDirectionRTL are supported.
  ///
  void SetLayoutDirection(const TextLayoutDirection direction) {
    if (direction == kTextLayoutDirectionTTB) {
      fplbase::LogError("TextLayoutDirectionTTB is not supported yet.");
      return;
    }

    // Flush layout cache if we switch a direction.
    if (direction != layout_direction_) {
      FlushLayout();
    }
    layout_direction_ = direction;
  }

  /// @return Returns the current layout direciton.
  TextLayoutDirection GetLayoutDirection() { return layout_direction_; }

  /// @brief Set a line height for a multi-line text.
  ///
  /// @param[in] line_height A float representing the line height for a
  /// multi-line text.
  void SetLineHeight(float line_height) { line_height_ = line_height; }

  /// @return Returns the current font.
  HbFont *GetCurrentFont() { return current_font_; }

 private:
  // Pass indicating rendering pass.
  static const int32_t kRenderPass = -1;

  // Initialize static data associated with the class.
  void Initialize();

  // Clean up static data associated with the class.
  // Note that after the call, an application needs to call Initialize() again
  // to use the class.
  static void Terminate();

  // Expand a texture image buffer when the font metrics is changed.
  // Returns true if the image buffer was reallocated.
  static bool ExpandBuffer(int32_t width, int32_t height,
                           const FontMetrics &original_metrics,
                           const FontMetrics &new_metrics,
                           std::unique_ptr<uint8_t[]> *image);

  // Layout text and update harfbuzz_buf_.
  // Returns the width of the text layout in pixels.
  uint32_t LayoutText(const char *text, size_t length);

  // Calculate internal/external leading value and expand a buffer if
  // necessary.
  // Returns true if the size of metrics has been changed.
  bool UpdateMetrics(const FT_GlyphSlot glyph,
                     const FontMetrics &current_metrics,
                     FontMetrics *new_metrics);

  // Retrieve cached entry from the glyph cache.
  // If an entry is not found in the glyph cache, the API tries to create new
  // cache entry and returns it if succeeded.
  // Returns nullptr if,
  // - The font doesn't have the requested glyph.
  // - The glyph doesn't fit into the cache (even after trying to evict some
  // glyphs in cache based on LRU rule).
  // (e.g. Requested glyph size too large or the cache is highly fragmented.)
  const GlyphCacheEntry *GetCachedEntry(uint32_t code_point, uint32_t y_size,
                                        GlyphFlags flags);

  // Update font manager, check glyph cache if the texture atlas needs to be
  // updated.
  // If start_subpass == true,
  // the API uploads the current atlas texture, flushes cache and starts
  // a sub layout pass. Use the feature when the cache is full and needs to
  // flushed during a rendering pass.
  void UpdatePass(bool start_subpass);

  // Update UV value in the FontBuffer.
  // Returns nullptr if one of UV values couldn't be updated.
  FontBuffer *UpdateUV(int32_t ysize, GlyphFlags flags, FontBuffer *buffer);

  // Convert requested glyph size using SizeSelector if it's set.
  int32_t ConvertSize(int32_t size);

  // Retrieve a caret count in a specific glyph from linebreak and halfbuzz
  // glyph information.
  int32_t GetCaretPosCount(const WordEnumerator &enumerator,
                           const hb_glyph_info_t *info, int32_t glyph_count,
                           int32_t index);

  // Create FontBuffer with requested parameters.
  // The function may return nullptr if the glyph cache is full.
  FontBuffer *CreateBuffer(const char *text, uint32_t length,
                           const FontBufferParameters &parameters);

  // Update language related settings.
  void SetLanguageSettings();

  // Look up a supported locale.
  // Returns nullptr if the API doesn't find the specified locale.
  const ScriptInfo *FindLocale(const char *locale);

  // Check if speficied language is supported in the font manager engine.
  bool IsLanguageSupported(const char *language);

  // Renderer instance.
  fplbase::Renderer *renderer_;

  // flag indicating if a font file has loaded.
  bool face_initialized_;

  // Map that keeps opened face data instances.
  std::unordered_map<std::string, std::unique_ptr<FaceData>> map_faces_;

  // Pointer to active font face instance.
  HbFont *current_font_;

  // Texture cache for a rendered string image.
  // Using the FontBufferParameters as keys.
  // The map is used for GetTexture() API.
  std::unordered_map<FontBufferParameters, std::unique_ptr<FontTexture>,
                     FontBufferParameters> map_textures_;

  // Cache for a texture atlas + vertex array rendering.
  // Using the FontBufferParameters as keys.
  // The map is used for GetBuffer() API.
  std::unordered_map<FontBufferParameters, std::unique_ptr<FontBuffer>,
                     FontBufferParameters> map_buffers_;

  // Singleton instance of Freetype library.
  static FT_Library *ft_;

  // Harfbuzz buffer
  static hb_buffer_t *harfbuzz_buf_;

  // Unique pointer to a glyph cache.
  std::unique_ptr<GlyphCache<uint8_t>> glyph_cache_;

  // Current atlas texture's contents revision.
  uint32_t current_atlas_revision_;

  // Unique pointer to a font atlas texture.
  std::unique_ptr<fplbase::Texture> atlas_texture_;

  // Current pass counter.
  // Current implementation only supports up to 2 passes in a rendering cycle.
  int32_t current_pass_;

  // Size selector function object used to adjust a glyph size.
  std::function<int32_t(const int32_t)> size_selector_;

  // Language of input strings.
  // Used to determine line breaking depending on a language.
  uint32_t script_;
  std::string language_;
  std::string locale_;
  TextLayoutDirection layout_direction_;
  static const ScriptInfo script_table_[];
  static const char *language_table_[];

  // Line height for a multi line text.
  float line_height_;

  // Line break info buffer used in libunibreak.
  std::vector<char> wordbreak_info_;

  // An instance of signed distance field generator.
  // To avoid redundant initializations, the FontManager holds an instnce of the
  // class.
  DistanceComputer<uint8_t> sdf_computer_;
};

/// @class FontMetrics
///
/// @brief This class has additional properties for font metrics.
//
/// For details of font metrics, refer http://support.microsoft.com/kb/32667
/// In this class, ascender and descender values are retrieved from FreeType
/// font property.
///
/// And internal/external leading values are updated based on rendering glyph
/// information. When rendering a string, the leading values tracks max (min for
/// internal leading) value in the string.
class FontMetrics {
 public:
  /// The default constructor for FontMetrics.
  FontMetrics()
      : base_line_(0),
        internal_leading_(0),
        ascender_(0),
        descender_(0),
        external_leading_(0) {}

  /// The constructor with initialization parameters for FontMetrics.
  FontMetrics(int32_t base_line, int32_t internal_leading, int32_t ascender,
              int32_t descender, int32_t external_leading)
      : base_line_(base_line),
        internal_leading_(internal_leading),
        ascender_(ascender),
        descender_(descender),
        external_leading_(external_leading) {
    assert(internal_leading >= 0);
    assert(ascender >= 0);
    assert(descender <= 0);
    assert(external_leading <= 0);
  }

  /// The destructor for FontMetrics.
  ~FontMetrics() {}

  /// @return Returns the baseline value as an int32_t.
  int32_t base_line() const { return base_line_; }

  /// @brief set the baseline value.
  ///
  /// @param[in] base_line An int32_t to set as the baseline value.
  void set_base_line(int32_t base_line) { base_line_ = base_line; }

  /// @return Returns the internal leading parameter as an int32_t.
  int32_t internal_leading() const { return internal_leading_; }

  /// @brief Set the internal leading parameter value.
  ///
  /// @param[in] internal_leading An int32_t to set as the internal
  /// leading value.
  void set_internal_leading(int32_t internal_leading) {
    assert(internal_leading >= 0);
    internal_leading_ = internal_leading;
  }

  /// @return Returns the ascender value as an int32_t.
  int32_t ascender() const { return ascender_; }

  /// @brief Set the ascender value.
  ///
  /// @param[in] ascender An int32_t to set as the ascender value.
  void set_ascender(int32_t ascender) {
    assert(ascender >= 0);
    ascender_ = ascender;
  }

  /// @return Returns the descender value as an int32_t.
  int32_t descender() const { return descender_; }

  /// @brief Set the descender value.
  ///
  /// @param[in] descender An int32_t to set as the descender value.
  void set_descender(int32_t descender) {
    assert(descender <= 0);
    descender_ = descender;
  }

  /// @return Returns the external leading parameter value as an int32_t.
  int32_t external_leading() const { return external_leading_; }

  /// @brief Set the external leading parameter value.
  ///
  /// @param[in] external_leading An int32_t to set as the external
  /// leading value.
  void set_external_leading(int32_t external_leading) {
    assert(external_leading <= 0);
    external_leading_ = external_leading;
  }

  /// @return Returns the total height of the glyph.
  int32_t total() const {
    return internal_leading_ + ascender_ - descender_ - external_leading_;
  }

 private:
  // Baseline: Baseline of the glpyhs.
  // When rendering multiple glyphs in the same horizontal group,
  // baselines need be aligned.
  // Most of glyphs fit within the area of ascender + descender.
  // However some glyphs may render in internal/external leading area.
  // (e.g. Å, underlines)
  int32_t base_line_;

  // Internal leading: Positive value that describes the space above the
  // ascender.
  int32_t internal_leading_;

  // Ascender: Positive value that describes the size of the ascender above
  // the baseline.
  int32_t ascender_;

  // Descender: Negative value that describes the size of the descender below
  // the baseline.
  int32_t descender_;

  // External leading : Negative value that describes the space below
  // the descender.
  int32_t external_leading_;
};

/// @class FontTexture
///
/// @brief This class is the actual Texture for fonts.
class FontTexture : public fplbase::Texture {
 public:
  /// @brief The default constructor for a FontTexture.
  FontTexture() : fplbase::Texture(nullptr, fplbase::kFormatLuminance, false) {}

  /// @brief The destructor for FontTexture.
  ~FontTexture() {}

  /// @return Returns a const reference to the FontMetrics that specifies
  /// the metrics parameters for the FontTexture.
  const FontMetrics &metrics() const { return metrics_; }

  /// @brief Sets the FontMetrics that specifies the metrics parameters for
  /// the FontTexture.
  void set_metrics(const FontMetrics &metrics) { metrics_ = metrics; }

 private:
  FontMetrics metrics_;
};

/// @struct FontVertex
///
/// @brief This struct holds all the font vertex data.
struct FontVertex {
  /// @brief The constructor for a FontVertex.
  ///
  /// @param[in] x A float representing the `x` position of the vertex.
  /// @param[in] y A float representing the `y` position of the vertex.
  /// @param[in] z A float representing the `z` position of the vertex.
  /// @param[in] u A float representing the `u` value in the UV mapping.
  /// @param[in] v A float representing the `v` value in the UV mapping.
  FontVertex(float x, float y, float z, float u, float v) {
    position_.data[0] = x;
    position_.data[1] = y;
    position_.data[2] = z;
    uv_.data[0] = u;
    uv_.data[1] = v;
  }

  /// @cond FONT_MANAGER_INTERNAL
  mathfu::vec3_packed position_;
  mathfu::vec2_packed uv_;
  /// @endcond
};

/// @class FontBuffer
///
/// @brief this is used with the texture atlas rendering.
class FontBuffer {
 public:
  /// @var kIndiciesPerCodePoint
  ///
  /// @brief The number of indices per code point.
  static const int32_t kIndiciesPerCodePoint = 6;

  /// @var kVerticesPerCodePoint
  ///
  /// @brief The number of vertices per code point.
  static const int32_t kVerticesPerCodePoint = 4;

  /// @brief The default constructor for a FontBuffer.
  FontBuffer()
      : revision_(0), line_start_index_(0), line_start_caret_index_(0) {}

  /// @brief The constructor for FontBuffer with a given buffer size.
  ///
  /// @param[in] size A size of the FontBuffer in a number of glyphs.
  /// @param[in] caret_info Indicates if the FontBuffer also maintains a caret
  /// position buffer.
  ///
  /// @note Caret position does not match to glpyh position 1 to 1, because a
  /// glyph can have multiple caret positions (e.g. Single 'ff' glyph can have 2
  /// caret positions).
  ///
  /// Since it has a strong relationship to rendering positions, we store the
  /// caret position information in the FontBuffer.
  FontBuffer(uint32_t size, bool caret_info)
      : revision_(0), line_start_index_(0), line_start_caret_index_(0) {
    indices_.reserve(size * kIndiciesPerCodePoint);
    vertices_.reserve(size * kVerticesPerCodePoint);
    code_points_.reserve(size);
    if (caret_info) {
      caret_positions_.reserve(size + 1);
    }
  }

  /// The destructor for FontBuffer.
  ~FontBuffer() {}

  /// @return Returns the FontMetrics metrics parameters for the font
  /// texture.
  const FontMetrics &metrics() const { return metrics_; }

  /// @brief Sets the FontMetrics metrics parameters for the font
  /// texture.
  ///
  /// @param[in] metrics The FontMetrics to set for the font texture.
  void set_metrics(const FontMetrics &metrics) { metrics_ = metrics; }

  /// @return Returns the indices array as a std::vector<uint16_t>.
  std::vector<uint16_t> *get_indices() { return &indices_; }

  /// @return Returns the indices array as a const std::vector<uint16_t>.
  const std::vector<uint16_t> *get_indices() const { return &indices_; }

  /// @return Returns the vertices array as a std::vector<FontVertex>.
  std::vector<FontVertex> *get_vertices() { return &vertices_; }

  /// @return Returns the vertices array as a const std::vector<FontVertex>.
  const std::vector<FontVertex> *get_vertices() const { return &vertices_; }

  /// @return Returns the array of code points as a const std::vector<uint32_t>.
  const std::vector<uint32_t> *get_code_points() const { return &code_points_; }

  /// @return Returns the size of the string as a const vec2i reference.
  const mathfu::vec2i &get_size() const { return size_; }

  /// @brief Set the size of the string.
  ///
  /// @param[in] size A const vec2i reference to the size of the string.
  void set_size(const mathfu::vec2i &size) { size_ = size; }

  /// @return Returns the glyph cache revision counter as a uint32_t.
  ///
  /// @note Each time a contents of the glyph cache is updated, the revision of
  /// the cache is updated.
  ///
  /// If the cache revision and the buffer revision is different, the
  /// font_manager try to re-construct the buffer.
  uint32_t get_revision() const { return revision_; }

  /// @brief Sets the glyph cache revision counter.
  ///
  /// @param[in] revision The uint32_t containing the new counter to set.
  ///
  /// @note Each time a contents of the glyph cache is updated, the revision of
  /// the cache is updated.
  ///
  /// If the cache revision and the buffer revision is different, the
  /// font_manager try to re-construct the buffer.
  void set_revision(uint32_t revision) { revision_ = revision; }

  /// @return Returns the pass counter as an int32_t.
  ///
  /// @note In the render pass, this value is used if the user of the class
  /// needs to call `StartRenderPass()` to upload the atlas texture.
  int32_t get_pass() const { return pass_; }

  /// @return Returns the psas counter as an int32_t.
  ///
  /// @note In the render pass, this value is used if the user of the class
  /// needs to call `StartRenderPass()` to upload the atlas texture.
  void set_pass(int32_t pass) { pass_ = pass; }

  /// @brief Adds a codepoint of a glyph to the codepoint array.
  ///
  /// @param[in] codepoint A codepoint of the glyph.
  void AddCodepoint(uint32_t codepoint) { code_points_.push_back(codepoint); }

  /// @brief Adds 4 vertices to be used for a glyph rendering to the
  /// vertex array.
  ///
  /// @param[in] pos A vec2 containing the `x` and `y` position of the first,
  /// unscaled vertex.
  /// @param[in] base_line A const int32_t representing the baseline for the
  /// vertices.
  /// @param[in] scale A float used to scale the size and offset.
  /// @param[in] entry A const GlyphCacheEntry reference whose offset and size
  /// are used in the scaling.
  void AddVertices(const mathfu::vec2 &pos, int32_t base_line, float scale,
                   const GlyphCacheEntry &entry);

  /// @brief Add the given caret position to the buffer.
  ///
  /// @param[in] x The `x` position of the caret.
  /// @param[in] y The `y` position of the caret.
  void AddCaretPosition(int32_t x, int32_t y);

  ///
  /// @param[in] pos The position of the caret.
  void AddCaretPosition(const mathfu::vec2 &pos);

  /// @brief Tell the buffer a word boundary.
  /// It may use the information to justify text layout later.
  ///
  /// @param[in] parameters Text layout parameters used to update the layout.
  void AddWordBoundary(const FontBufferParameters &parameters);

  /// @brief Update UV information of a glyph entry.
  ///
  /// @param[in] index The index of the glyph entry that should be updated.
  /// @param[in] uv The `uv` vec4 should include the top-left corner of UV value
  /// as `x` and `y`, and the bottom-right of UV value as the `w` and `z`
  /// components of the vector.
  void UpdateUV(int32_t index, const mathfu::vec4 &uv);

  /// @brief Indicates a start of new line. It may update a previous line's
  /// buffer contents based on typography layout settings.
  ///
  /// @param[in] parameters Text layout parameters used to update the layout.
  /// @param[in] line_width Current line width.
  /// @param[in] last_line Indicate if the line is the last line in the
  /// paragraph.
  void UpdateLine(const FontBufferParameters &parameters,
                  TextLayoutDirection layout_direction, int32_t line_width,
                  bool last_line);

  /// @brief Verifies that the sizes of the arrays used in the buffer are
  /// correct.
  ///
  /// @warning Asserts if the array sizes are incorrect.
  ///
  /// @return Returns `true`.
  bool Verify() {
    assert(vertices_.size() == code_points_.size() * kVerticesPerCodePoint);
    assert(indices_.size() == code_points_.size() * kIndiciesPerCodePoint);
    return true;
  }

  /// @brief Retrieve a caret positions with a given index.
  ///
  /// @param[in] index The index of the caret position.
  ///
  /// @return Returns a vec2i containing the caret position. Otherwise it
  /// returns `kCaretPositionInvalid` if the buffer does not contain
  /// caret information at the given index, or if the index is out of range.
  mathfu::vec2i GetCaretPosition(size_t index) const {
    if (!caret_positions_.capacity() || index > caret_positions_.size())
      return kCaretPositionInvalid;
    return caret_positions_[index];
  }

  /// @return Returns a const reference to the caret positions buffer.
  const std::vector<mathfu::vec2i> &GetCaretPositions() const {
    return caret_positions_;
  }

  /// @return Returns `true` if the FontBuffer contains any caret positions.
  /// If the caret positions array has 0 elements, it will return `false`.
  bool HasCaretPositions() const { return caret_positions_.capacity() != 0; }

 private:
  // Font metrics information.
  FontMetrics metrics_;

  // Arrays for font indices, vertices and code points.
  // They are hold as a separate vector because OpenGL draw call needs them to
  // be a separate array.

  // Indices of the font buffer.
  std::vector<uint16_t> indices_;

  // Vertices data of the font buffer.
  std::vector<FontVertex> vertices_;

  // Code points used in the buffer. This array is used to fetch and update UV
  // entries when the glyph cache is flushed.
  std::vector<uint32_t> code_points_;

  // Caret positions in the buffer. We need to track them differently than a
  // vertices information because we support ligatures so that single glyph
  // can include multiple caret positions.
  std::vector<mathfu::vec2i> caret_positions_;

  // Word boundary information. This information is used only with a typography
  // layout with a justification.
  std::vector<uint32_t> word_boundary_;
  std::vector<uint32_t> word_boundary_caret_;

  // Size of the string in pixels.
  mathfu::vec2i size_;

  // Revision of the FontBuffer corresponding glyph cache revision.
  // Caller needs to check the revision value if glyph texture has referencing
  // entries by checking the revision.
  uint32_t revision_;

  // Pass id. Each pass should have it's own texture atlas contents.
  int32_t pass_;

  // Start index of current line.
  uint32_t line_start_index_;
  uint32_t line_start_caret_index_;
};

/// @struct ScriptInfo
///
/// @brief This struct holds the script information used for a text layout.
///
struct ScriptInfo {
  /// @var locale
  /// @brief A C-string corresponding to the of the
  /// language defined in ISO 639 and the country code difined in ISO 3166
  /// separated
  /// by '-'. (e.g. 'en-US').
  const char *locale;

  /// @var script
  /// @brief ISO 15924 Script code.
  const char *script;
  /// @var direction
  /// @brief Script layout direciton.
  TextLayoutDirection direction;
};
/// @}

/// @class FontShader
///
/// @brief Helper class to handle shaders for a font rendering.
/// The class keeps a reference to a shader, and a location of uniforms with a
/// fixed names.
/// A caller is responsive not to call set_* APIs that specified shader doesn't
/// support.
class FontShader {
public:
  void set(fplbase::Shader *shader) {
    assert(shader);
    shader_ = shader;
    pos_offset_ = shader->FindUniform("pos_offset");
    color_ = shader->FindUniform("color");
    clipping_ = shader->FindUniform("clipping");
    threshold_ = shader->FindUniform("threshold");

  }
  void set_renderer(const fplbase::Renderer &renderer) {
    shader_->Set(renderer);
  }
  void set_position_offset(const mathfu::vec3 &vec) {
    assert(pos_offset_ >= 0);
    shader_->SetUniform(pos_offset_, vec);
  }
  void set_color(const mathfu::vec4 &vec) {
    assert(color_ >= 0);
    shader_->SetUniform(color_, vec);
  }
  void set_clipping(const mathfu::vec4 &vec) {
    assert(clipping_ >= 0);
    shader_->SetUniform(clipping_, vec);
  }
  void set_threshold(float f) {
    assert(threshold_ >= 0);
    shader_->SetUniform(threshold_, &f, 1);
  }
private:
  fplbase::Shader *shader_;
  fplbase::UniformHandle pos_offset_;
  fplbase::UniformHandle color_;
  fplbase::UniformHandle clipping_;
  fplbase::UniformHandle threshold_;
};
/// @}

}  // namespace flatui

#endif  // FONT_MANAGER_H
