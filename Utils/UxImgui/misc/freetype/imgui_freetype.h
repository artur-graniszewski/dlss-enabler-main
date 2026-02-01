// dear imgui: FreeType font builder (used as a replacement for the stb_truetype builder)
// (headers)

#pragma once
#include "../../imgui.h"      // IMGUI_API
#ifndef IMGUI_DISABLE

// Usage:
// - Add '#define IMGUI_ENABLE_FREETYPE' in your imconfig to automatically enable support
//   for imgui_freetype in imgui. It is equivalent to selecting the default loader with:
//      io.Fonts.FontLoader = UxImGuiFreeType::GetFontLoader()

// Optional support for OpenType SVG fonts:
// - Add '#define IMGUI_ENABLE_FREETYPE_PLUTOSVG' to use plutosvg (not provided). See #7927.
// - Add '#define IMGUI_ENABLE_FREETYPE_LUNASVG' to use lunasvg (not provided). See #6591.

// Forward declarations
struct UxImFontAtlas;
struct UxImFontLoader;

// Hinting greatly impacts visuals (and glyph sizes).
// - By default, hinting is enabled and the font's native hinter is preferred over the auto-hinter.
// - When disabled, FreeType generates blurrier glyphs, more or less matches the stb_truetype.h
// - The Default hinting mode usually looks good, but may distort glyphs in an unusual way.
// - The Light hinting mode generates fuzzier glyphs but better matches Microsoft's rasterizer.
// You can set those flags globally in UxImFontAtlas::FontBuilderFlags
// You can set those flags on a per font basis in UxImFontConfig::FontBuilderFlags
enum UxImGuiFreeTypeBuilderFlags
{
    UxImGuiFreeTypeBuilderFlags_NoHinting     = 1 << 0,   // Disable hinting. This generally generates 'blurrier' bitmap glyphs when the glyph are rendered in any of the anti-aliased modes.
    UxImGuiFreeTypeBuilderFlags_NoAutoHint    = 1 << 1,   // Disable auto-hinter.
    UxImGuiFreeTypeBuilderFlags_ForceAutoHint = 1 << 2,   // Indicates that the auto-hinter is preferred over the font's native hinter.
    UxImGuiFreeTypeBuilderFlags_LightHinting  = 1 << 3,   // A lighter hinting algorithm for gray-level modes. Many generated glyphs are fuzzier but better resemble their original shape. This is achieved by snapping glyphs to the pixel grid only vertically (Y-axis), as is done by Microsoft's ClearType and Adobe's proprietary font renderer. This preserves inter-glyph spacing in horizontal text.
    UxImGuiFreeTypeBuilderFlags_MonoHinting   = 1 << 4,   // Strong hinting algorithm that should only be used for monochrome output.
    UxImGuiFreeTypeBuilderFlags_Bold          = 1 << 5,   // Styling: Should we artificially embolden the font?
    UxImGuiFreeTypeBuilderFlags_Oblique       = 1 << 6,   // Styling: Should we slant the font, emulating italic style?
    UxImGuiFreeTypeBuilderFlags_Monochrome    = 1 << 7,   // Disable anti-aliasing. Combine this with MonoHinting for best results!
    UxImGuiFreeTypeBuilderFlags_LoadColor     = 1 << 8,   // Enable FreeType color-layered glyphs
    UxImGuiFreeTypeBuilderFlags_Bitmap        = 1 << 9    // Enable FreeType bitmap glyphs
};

namespace UxImGuiFreeType
{
    // This is automatically assigned when using '#define IMGUI_ENABLE_FREETYPE'.
    // If you need to dynamically select between multiple builders:
    // - you can manually assign this builder with 'atlas->FontLoader = UxImGuiFreeType::GetFontLoader()'
    // - prefer deep-copying this into your own UxImFontLoader instance if you use hot-reloading that messes up static data.
    IMGUI_API const UxImFontLoader*       GetFontLoader();

    // Override allocators. By default UxImGuiFreeType will use IM_ALLOC()/IM_FREE()
    // However, as FreeType does lots of allocations we provide a way for the user to redirect it to a separate memory heap if desired.
    IMGUI_API void                      SetAllocatorFunctions(void* (*alloc_func)(size_t sz, void* user_data), void (*free_func)(void* ptr, void* user_data), void* user_data = nullptr);

    // Display UI to edit FontBuilderFlags in UxImFontAtlas (shared) or UxImFontConfig (single source)
    IMGUI_API bool                      DebugEditFontBuilderFlags(unsigned int* p_font_loader_flags);

    // Obsolete names (will be removed)
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    //IMGUI_API const UxImFontBuilderIO* GetBuilderForFreeType(); // Renamed/changed in 1.92. Change 'io.Fonts->FontBuilderIO = UxImGuiFreeType::GetBuilderForFreeType()' to 'io.Fonts.FontLoader = UxImGuiFreeType::GetFontLoader()' if you need runtime selection.
    //static inline bool BuildFontAtlas(UxImFontAtlas* atlas, unsigned int flags = 0) { atlas->FontBuilderIO = GetBuilderForFreeType(); atlas->FontBuilderFlags = flags; return atlas->Build(); } // Prefer using '#define IMGUI_ENABLE_FREETYPE'
#endif
}

#endif // #ifndef IMGUI_DISABLE

