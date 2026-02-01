// dear imgui, v1.92.0 WIP
// (headers)

// Help:
// - See links below.
// - Call and read UxImGui::ShowDemoWindow() in imgui_demo.cpp. All applications in examples/ are doing that.
// - Read top of imgui.cpp for more details, links and comments.
// - Add '#define IMGUI_DEFINE_MATH_OPERATORS' before including this file (or in imconfig.h) to access courtesy maths operators for UxImVec2 and UxImVec4.

// Resources:
// - FAQ ........................ https://dearimgui.com/faq (in repository as docs/FAQ.md)
// - Homepage ................... https://github.com/ocornut/imgui
// - Releases & changelog ....... https://github.com/ocornut/imgui/releases
// - Gallery .................... https://github.com/ocornut/imgui/issues?q=label%3Agallery (please post your screenshots/video there!)
// - Wiki ....................... https://github.com/ocornut/imgui/wiki (lots of good stuff there)
//   - Getting Started            https://github.com/ocornut/imgui/wiki/Getting-Started (how to integrate in an existing app by adding ~25 lines of code)
//   - Third-party Extensions     https://github.com/ocornut/imgui/wiki/Useful-Extensions (UxImPlot & many more)
//   - Bindings/Backends          https://github.com/ocornut/imgui/wiki/Bindings (language bindings, backends for various tech/engines)
//   - Glossary                   https://github.com/ocornut/imgui/wiki/Glossary
//   - Debug Tools                https://github.com/ocornut/imgui/wiki/Debug-Tools
//   - Software using Dear UxImGui  https://github.com/ocornut/imgui/wiki/Software-using-dear-imgui
// - Issues & support ........... https://github.com/ocornut/imgui/issues
// - Test Engine & Automation ... https://github.com/ocornut/imgui_test_engine (test suite, test engine to automate your apps)

// For first-time users having issues compiling/linking/running/loading fonts:
// please post in https://github.com/ocornut/imgui/discussions if you cannot find a solution in resources above.
// Everything else should be asked in 'Issues'! We are building a database of cross-linked knowledge there.

// Library Version
// (Integer encoded as XYYZZ for use in #if preprocessor conditionals, e.g. '#if IMGUI_VERSION_NUM >= 12345')
#define IMGUI_VERSION       "1.92.0 WIP"
#define IMGUI_VERSION_NUM   19196
#define IMGUI_HAS_TABLE
#define IMGUI_HAS_VIEWPORT          // Viewport WIP branch
#define IMGUI_HAS_DOCK              // Docking WIP branch
#define IMGUI_HAS_TEXTURES          // 1.92+ WIP branch with UxImGuiBackendFlags_RendererHasTextures

/*

Index of this file:
// [SECTION] Header mess
// [SECTION] Forward declarations and basic types
// [SECTION] Texture identifiers (UxImTextureID, UxImTextureRef)
// [SECTION] Dear UxImGui end-user API functions
// [SECTION] Flags & Enumerations
// [SECTION] Tables API flags and structures (UxImGuiTableFlags, UxImGuiTableColumnFlags, UxImGuiTableRowFlags, UxImGuiTableBgTarget, UxImGuiTableSortSpecs, UxImGuiTableColumnSortSpecs)
// [SECTION] Helpers: Debug log, Memory allocations macros, UxImVector<>
// [SECTION] UxImGuiStyle
// [SECTION] UxImGuiIO
// [SECTION] Misc data structures (UxImGuiInputTextCallbackData, UxImGuiSizeCallbackData, UxImGuiWindowClass, UxImGuiPayload)
// [SECTION] Helpers (UxImGuiOnceUponAFrame, UxImGuiTextFilter, UxImGuiTextBuffer, UxImGuiStorage, UxImGuiListClipper, Math Operators, UxImColor)
// [SECTION] Multi-Select API flags and structures (UxImGuiMultiSelectFlags, UxImGuiMultiSelectIO, UxImGuiSelectionRequest, UxImGuiSelectionBasicStorage, UxImGuiSelectionExternalStorage)
// [SECTION] Drawing API (UxImDrawCallback, UxImDrawCmd, UxImDrawIdx, UxImDrawVert, UxImDrawChannel, UxImDrawListSplitter, UxImDrawFlags, UxImDrawListFlags, UxImDrawList, UxImDrawData)
// [SECTION] Texture API (UxImTextureFormat, UxImTextureStatus, UxImTextureRect, UxImTextureData)
// [SECTION] Font API (UxImFontConfig, UxImFontGlyph, UxImFontGlyphRangesBuilder, UxImFontAtlasFlags, UxImFontAtlas, UxImFontBaked, UxImFont)
// [SECTION] Viewports (UxImGuiViewportFlags, UxImGuiViewport)
// [SECTION] UxImGuiPlatformIO + other Platform Dependent Interfaces (UxImGuiPlatformMonitor, UxImGuiPlatformImeData)
// [SECTION] Obsolete functions and types

*/

#pragma once

// Configuration file with compile-time options
// (edit imconfig.h or '#define IMGUI_USER_CONFIG "myfilename.h" from your build system)
#ifdef IMGUI_USER_CONFIG
#include IMGUI_USER_CONFIG
#endif
#include "imconfig.h"

#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Header mess
//-----------------------------------------------------------------------------

// Includes
#include <float.h>                  // FLT_MIN, FLT_MAX
#include <stdarg.h>                 // va_list, va_start, va_end
#include <stddef.h>                 // ptrdiff_t, NULL
#include <string.h>                 // memset, memmove, memcpy, strlen, strchr, strcpy, strcmp

// Define attributes of all API symbols declarations (e.g. for DLL under Windows)
// IMGUI_API is used for core imgui functions, IMGUI_IMPL_API is used for the default backends files (imgui_impl_xxx.h)
// Using dear imgui via a shared library is not recommended: we don't guarantee backward nor forward ABI compatibility + this is a call-heavy library and function call overhead adds up.
#ifndef IMGUI_API
#define IMGUI_API
#endif
#ifndef IMGUI_IMPL_API
#define IMGUI_IMPL_API              IMGUI_API
#endif

// Helper Macros
#ifndef IM_ASSERT
#include <assert.h>
#define IM_ASSERT(_EXPR)            assert(_EXPR)                               // You can override the default assert handler by editing imconfig.h
#endif
#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR) / sizeof(*(_ARR))))     // Size of a static C-style array. Don't use on pointers!
#define IM_UNUSED(_VAR)             ((void)(_VAR))                              // Used to silence "unused variable warnings". Often useful as asserts may be stripped out from final builds.

// Check that version and structures layouts are matching between compiled imgui code and caller. Read comments above DebugCheckVersionAndDataLayout() for details.
#define IMGUI_CHECKVERSION()        UxImGui::DebugCheckVersionAndDataLayout(IMGUI_VERSION, sizeof(UxImGuiIO), sizeof(UxImGuiStyle), sizeof(UxImVec2), sizeof(UxImVec4), sizeof(UxImDrawVert), sizeof(UxImDrawIdx))

// Helper Macros - IM_FMTARGS, IM_FMTLIST: Apply printf-style warnings to our formatting functions.
// (MSVC provides an equivalent mechanism via SAL Annotations but it would require the macros in a different
//  location. e.g. #include <sal.h> + void myprintf(_Printf_format_string_ const char* format, ...))
#if !defined(IMGUI_USE_STB_SPRINTF) && defined(__MINGW32__) && !defined(__clang__)
#define IM_FMTARGS(FMT)             __attribute__((format(gnu_printf, FMT, FMT+1)))
#define IM_FMTLIST(FMT)             __attribute__((format(gnu_printf, FMT, 0)))
#elif !defined(IMGUI_USE_STB_SPRINTF) && (defined(__clang__) || defined(__GNUC__))
#define IM_FMTARGS(FMT)             __attribute__((format(printf, FMT, FMT+1)))
#define IM_FMTLIST(FMT)             __attribute__((format(printf, FMT, 0)))
#else
#define IM_FMTARGS(FMT)
#define IM_FMTLIST(FMT)
#endif

// Disable some of MSVC most aggressive Debug runtime checks in function header/footer (used in some simple/low-level functions)
#if defined(_MSC_VER) && !defined(__clang__)  && !defined(__INTEL_COMPILER) && !defined(IMGUI_DEBUG_PARANOID)
#define IM_MSVC_RUNTIME_CHECKS_OFF      __pragma(runtime_checks("",off))     __pragma(check_stack(off)) __pragma(strict_gs_check(push,off))
#define IM_MSVC_RUNTIME_CHECKS_RESTORE  __pragma(runtime_checks("",restore)) __pragma(check_stack())    __pragma(strict_gs_check(pop))
#else
#define IM_MSVC_RUNTIME_CHECKS_OFF
#define IM_MSVC_RUNTIME_CHECKS_RESTORE
#endif

// Warnings
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 26495)    // [Static Analyzer] Variable 'XXX' is uninitialized. Always initialize a member variable (type.6).
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast
#pragma clang diagnostic ignored "-Wfloat-equal"                    // warning: comparing floating point with == or != is unsafe
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant
#pragma clang diagnostic ignored "-Wreserved-identifier"            // warning: identifier '_Xxx' is reserved because it starts with '_' followed by a capital letter
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"            // warning: 'xxx' is an unsafe pointer used for buffer access
#pragma clang diagnostic ignored "-Wnontrivial-memaccess"           // warning: first argument in call to 'memset' is a pointer to non-trivially copyable type
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"                          // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wfloat-equal"                      // warning: comparing floating-point with '==' or '!=' is unsafe
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward declarations and basic types
//-----------------------------------------------------------------------------

// Scalar data types
typedef unsigned int        UxImGuiID;// A unique ID used by widgets (typically the result of hashing a stack of string)
typedef signed char         UxImS8;   // 8-bit signed integer
typedef unsigned char       UxImU8;   // 8-bit unsigned integer
typedef signed short        UxImS16;  // 16-bit signed integer
typedef unsigned short      UxImU16;  // 16-bit unsigned integer
typedef signed int          UxImS32;  // 32-bit signed integer == int
typedef unsigned int        UxImU32;  // 32-bit unsigned integer (often used to store packed colors)
typedef signed   long long  UxImS64;  // 64-bit signed integer
typedef unsigned long long  UxImU64;  // 64-bit unsigned integer

// Forward declarations: UxImDrawList, UxImFontAtlas layer
struct UxImDrawChannel;               // Temporary storage to output draw commands out of order, used by UxImDrawListSplitter and UxImDrawList::ChannelsSplit()
struct UxImDrawCmd;                   // A single draw command within a parent UxImDrawList (generally maps to 1 GPU draw call, unless it is a callback)
struct UxImDrawData;                  // All draw command lists required to render the frame + pos/size coordinates to use for the projection matrix.
struct UxImDrawList;                  // A single draw command list (generally one per window, conceptually you may see this as a dynamic "mesh" builder)
struct UxImDrawListSharedData;        // Data shared among multiple draw lists (typically owned by parent UxImGui context, but you may create one yourself)
struct UxImDrawListSplitter;          // Helper to split a draw list into different layers which can be drawn into out of order, then flattened back.
struct UxImDrawVert;                  // A single vertex (pos + uv + col = 20 bytes by default. Override layout with IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT)
struct UxImFont;                      // Runtime data for a single font within a parent UxImFontAtlas
struct UxImFontAtlas;                 // Runtime data for multiple fonts, bake multiple fonts into a single texture, TTF/OTF font loader
struct UxImFontAtlasBuilder;          // Opaque storage for building a UxImFontAtlas
struct UxImFontAtlasRect;             // Output of UxImFontAtlas::GetCustomRect() when using custom rectangles.
struct UxImFontBaked;                 // Baked data for a UxImFont at a given size.
struct UxImFontConfig;                // Configuration data when adding a font or merging fonts
struct UxImFontGlyph;                 // A single font glyph (code point + coordinates within in UxImFontAtlas + offset)
struct UxImFontGlyphRangesBuilder;    // Helper to build glyph ranges from text/string data
struct UxImFontLoader;                // Opaque interface to a font loading backend (stb_truetype, FreeType etc.).
struct UxImTextureData;               // Specs and pixel storage for a texture used by Dear UxImGui.
struct UxImTextureRect;               // Coordinates of a rectangle within a texture.
struct UxImColor;                     // Helper functions to create a color that can be converted to either u32 or float4 (*OBSOLETE* please avoid using)

// Forward declarations: UxImGui layer
struct UxImGuiContext;                // Dear UxImGui context (opaque structure, unless including imgui_internal.h)
struct UxImGuiIO;                     // Main configuration and I/O between your application and UxImGui (also see: UxImGuiPlatformIO)
struct UxImGuiInputTextCallbackData;  // Shared state of InputText() when using custom UxImGuiInputTextCallback (rare/advanced use)
struct UxImGuiKeyData;                // Storage for UxImGuiIO and IsKeyDown(), IsKeyPressed() etc functions.
struct UxImGuiListClipper;            // Helper to manually clip large list of items
struct UxImGuiMultiSelectIO;          // Structure to interact with a BeginMultiSelect()/EndMultiSelect() block
struct UxImGuiOnceUponAFrame;         // Helper for running a block of code not more than once a frame
struct UxImGuiPayload;                // User data payload for drag and drop operations
struct UxImGuiPlatformIO;             // Interface between platform/renderer backends and UxImGui (e.g. Clipboard, IME, Multi-Viewport support). Extends UxImGuiIO.
struct UxImGuiPlatformImeData;        // Platform IME data for io.PlatformSetImeDataFn() function.
struct UxImGuiPlatformMonitor;        // Multi-viewport support: user-provided bounds for each connected monitor/display. Used when positioning popups and tooltips to avoid them straddling monitors
struct UxImGuiSelectionBasicStorage;  // Optional helper to store multi-selection state + apply multi-selection requests.
struct UxImGuiSelectionExternalStorage;//Optional helper to apply multi-selection requests to existing randomly accessible storage.
struct UxImGuiSelectionRequest;       // A selection request (stored in UxImGuiMultiSelectIO)
struct UxImGuiSizeCallbackData;       // Callback data when using SetNextWindowSizeConstraints() (rare/advanced use)
struct UxImGuiStorage;                // Helper for key->value storage (container sorted by key)
struct UxImGuiStoragePair;            // Helper for key->value storage (pair)
struct UxImGuiStyle;                  // Runtime data for styling/colors
struct UxImGuiTableSortSpecs;         // Sorting specifications for a table (often handling sort specs for a single column, occasionally more)
struct UxImGuiTableColumnSortSpecs;   // Sorting specification for one column of a table
struct UxImGuiTextBuffer;             // Helper to hold and append into a text buffer (~string builder)
struct UxImGuiTextFilter;             // Helper to parse and apply text filters (e.g. "aaaaa[,bbbbb][,ccccc]")
struct UxImGuiViewport;               // A Platform Window (always 1 unless multi-viewport are enabled. One per platform window to output to). In the future may represent Platform Monitor
struct UxImGuiWindowClass;            // Window class (rare/advanced uses: provide hints to the platform backend via altered viewport flags and parent/child info)

// Enumerations
// - We don't use strongly typed enums much because they add constraints (can't extend in private code, can't store typed in bit fields, extra casting on iteration)
// - Tip: Use your programming IDE navigation facilities on the names in the _central column_ below to find the actual flags/enum lists!
//   - In Visual Studio: CTRL+comma ("Edit.GoToAll") can follow symbols inside comments, whereas CTRL+F12 ("Edit.GoToImplementation") cannot.
//   - In Visual Studio w/ Visual Assist installed: ALT+G ("VAssistX.GoToImplementation") can also follow symbols inside comments.
//   - In VS Code, CLion, etc.: CTRL+click can follow symbols inside comments.
enum UxImGuiDir : int;                // -> enum UxImGuiDir              // Enum: A cardinal direction (Left, Right, Up, Down)
enum UxImGuiKey : int;                // -> enum UxImGuiKey              // Enum: A key identifier (UxImGuiKey_XXX or UxImGuiMod_XXX value)
enum UxImGuiMouseSource : int;        // -> enum UxImGuiMouseSource      // Enum; A mouse input source identifier (Mouse, TouchScreen, Pen)
enum UxImGuiSortDirection : UxImU8;     // -> enum UxImGuiSortDirection    // Enum: A sorting direction (ascending or descending)
typedef int UxImGuiCol;               // -> enum UxImGuiCol_             // Enum: A color identifier for styling
typedef int UxImGuiCond;              // -> enum UxImGuiCond_            // Enum: A condition for many Set*() functions
typedef int UxImGuiDataType;          // -> enum UxImGuiDataType_        // Enum: A primary data type
typedef int UxImGuiMouseButton;       // -> enum UxImGuiMouseButton_     // Enum: A mouse button identifier (0=left, 1=right, 2=middle)
typedef int UxImGuiMouseCursor;       // -> enum UxImGuiMouseCursor_     // Enum: A mouse cursor shape
typedef int UxImGuiStyleVar;          // -> enum UxImGuiStyleVar_        // Enum: A variable identifier for styling
typedef int UxImGuiTableBgTarget;     // -> enum UxImGuiTableBgTarget_   // Enum: A color target for TableSetBgColor()

// Flags (declared as int to allow using as flags without overhead, and to not pollute the top of this file)
// - Tip: Use your programming IDE navigation facilities on the names in the _central column_ below to find the actual flags/enum lists!
//   - In Visual Studio: CTRL+comma ("Edit.GoToAll") can follow symbols inside comments, whereas CTRL+F12 ("Edit.GoToImplementation") cannot.
//   - In Visual Studio w/ Visual Assist installed: ALT+G ("VAssistX.GoToImplementation") can also follow symbols inside comments.
//   - In VS Code, CLion, etc.: CTRL+click can follow symbols inside comments.
typedef int UxImDrawFlags;            // -> enum UxImDrawFlags_          // Flags: for UxImDrawList functions
typedef int UxImDrawListFlags;        // -> enum UxImDrawListFlags_      // Flags: for UxImDrawList instance
typedef int UxImFontFlags;            // -> enum UxImFontFlags_          // Flags: for UxImFont
typedef int UxImFontAtlasFlags;       // -> enum UxImFontAtlasFlags_     // Flags: for UxImFontAtlas
typedef int UxImGuiBackendFlags;      // -> enum UxImGuiBackendFlags_    // Flags: for io.BackendFlags
typedef int UxImGuiButtonFlags;       // -> enum UxImGuiButtonFlags_     // Flags: for InvisibleButton()
typedef int UxImGuiChildFlags;        // -> enum UxImGuiChildFlags_      // Flags: for BeginChild()
typedef int UxImGuiColorEditFlags;    // -> enum UxImGuiColorEditFlags_  // Flags: for ColorEdit4(), ColorPicker4() etc.
typedef int UxImGuiConfigFlags;       // -> enum UxImGuiConfigFlags_     // Flags: for io.ConfigFlags
typedef int UxImGuiComboFlags;        // -> enum UxImGuiComboFlags_      // Flags: for BeginCombo()
typedef int UxImGuiDockNodeFlags;     // -> enum UxImGuiDockNodeFlags_   // Flags: for DockSpace()
typedef int UxImGuiDragDropFlags;     // -> enum UxImGuiDragDropFlags_   // Flags: for BeginDragDropSource(), AcceptDragDropPayload()
typedef int UxImGuiFocusedFlags;      // -> enum UxImGuiFocusedFlags_    // Flags: for IsWindowFocused()
typedef int UxImGuiHoveredFlags;      // -> enum UxImGuiHoveredFlags_    // Flags: for IsItemHovered(), IsWindowHovered() etc.
typedef int UxImGuiInputFlags;        // -> enum UxImGuiInputFlags_      // Flags: for Shortcut(), SetNextItemShortcut()
typedef int UxImGuiInputTextFlags;    // -> enum UxImGuiInputTextFlags_  // Flags: for InputText(), InputTextMultiline()
typedef int UxImGuiItemFlags;         // -> enum UxImGuiItemFlags_       // Flags: for PushItemFlag(), shared by all items
typedef int UxImGuiKeyChord;          // -> UxImGuiKey | UxImGuiMod_XXX    // Flags: for IsKeyChordPressed(), Shortcut() etc. an UxImGuiKey optionally OR-ed with one or more UxImGuiMod_XXX values.
typedef int UxImGuiPopupFlags;        // -> enum UxImGuiPopupFlags_      // Flags: for OpenPopup*(), BeginPopupContext*(), IsPopupOpen()
typedef int UxImGuiMultiSelectFlags;  // -> enum UxImGuiMultiSelectFlags_// Flags: for BeginMultiSelect()
typedef int UxImGuiSelectableFlags;   // -> enum UxImGuiSelectableFlags_ // Flags: for Selectable()
typedef int UxImGuiSliderFlags;       // -> enum UxImGuiSliderFlags_     // Flags: for DragFloat(), DragInt(), SliderFloat(), SliderInt() etc.
typedef int UxImGuiTabBarFlags;       // -> enum UxImGuiTabBarFlags_     // Flags: for BeginTabBar()
typedef int UxImGuiTabItemFlags;      // -> enum UxImGuiTabItemFlags_    // Flags: for BeginTabItem()
typedef int UxImGuiTableFlags;        // -> enum UxImGuiTableFlags_      // Flags: For BeginTable()
typedef int UxImGuiTableColumnFlags;  // -> enum UxImGuiTableColumnFlags_// Flags: For TableSetupColumn()
typedef int UxImGuiTableRowFlags;     // -> enum UxImGuiTableRowFlags_   // Flags: For TableNextRow()
typedef int UxImGuiTreeNodeFlags;     // -> enum UxImGuiTreeNodeFlags_   // Flags: for TreeNode(), TreeNodeEx(), CollapsingHeader()
typedef int UxImGuiViewportFlags;     // -> enum UxImGuiViewportFlags_   // Flags: for UxImGuiViewport
typedef int UxImGuiWindowFlags;       // -> enum UxImGuiWindowFlags_     // Flags: for Begin(), BeginChild()

// Character types
// (we generally use UTF-8 encoded string in the API. This is storage specifically for a decoded character used for keyboard input and display)
typedef unsigned int UxImWchar32;     // A single decoded U32 character/code point. We encode them as multi bytes UTF-8 when used in strings.
typedef unsigned short UxImWchar16;   // A single decoded U16 character/code point. We encode them as multi bytes UTF-8 when used in strings.
#ifdef IMGUI_USE_WCHAR32            // UxImWchar [configurable type: override in imconfig.h with '#define IMGUI_USE_WCHAR32' to support Unicode planes 1-16]
typedef UxImWchar32 UxImWchar;
#else
typedef UxImWchar16 UxImWchar;
#endif

// Multi-Selection item index or identifier when using BeginMultiSelect()
// - Used by SetNextItemSelectionUserData() + and inside UxImGuiMultiSelectIO structure.
// - Most users are likely to use this store an item INDEX but this may be used to store a POINTER/ID as well. Read comments near UxImGuiMultiSelectIO for details.
typedef UxImS64 UxImGuiSelectionUserData;

// Callback and functions types
typedef int     (*UxImGuiInputTextCallback)(UxImGuiInputTextCallbackData* data);    // Callback function for UxImGui::InputText()
typedef void    (*UxImGuiSizeCallback)(UxImGuiSizeCallbackData* data);              // Callback function for UxImGui::SetNextWindowSizeConstraints()
typedef void*   (*UxImGuiMemAllocFunc)(size_t sz, void* user_data);               // Function signature for UxImGui::SetAllocatorFunctions()
typedef void    (*UxImGuiMemFreeFunc)(void* ptr, void* user_data);                // Function signature for UxImGui::SetAllocatorFunctions()

// UxImVec2: 2D vector used to store positions, sizes etc. [Compile-time configurable type]
// - This is a frequently used type in the API. Consider using IM_VEC2_CLASS_EXTRA to create implicit cast from/to our preferred type.
// - Add '#define IMGUI_DEFINE_MATH_OPERATORS' before including this file (or in imconfig.h) to access courtesy maths operators for UxImVec2 and UxImVec4.
IM_MSVC_RUNTIME_CHECKS_OFF
struct UxImVec2
{
    float                                   x, y;
    constexpr UxImVec2()                      : x(0.0f), y(0.0f) { }
    constexpr UxImVec2(float _x, float _y)    : x(_x), y(_y) { }
    float& operator[] (size_t idx)          { IM_ASSERT(idx == 0 || idx == 1); return ((float*)(void*)(char*)this)[idx]; } // We very rarely use this [] operator, so the assert overhead is fine.
    float  operator[] (size_t idx) const    { IM_ASSERT(idx == 0 || idx == 1); return ((const float*)(const void*)(const char*)this)[idx]; }
#ifdef IM_VEC2_CLASS_EXTRA
    IM_VEC2_CLASS_EXTRA     // Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math types and UxImVec2.
#endif
};

// UxImVec4: 4D vector used to store clipping rectangles, colors etc. [Compile-time configurable type]
struct UxImVec4
{
    float                                                     x, y, z, w;
    constexpr UxImVec4()                                        : x(0.0f), y(0.0f), z(0.0f), w(0.0f) { }
    constexpr UxImVec4(float _x, float _y, float _z, float _w)  : x(_x), y(_y), z(_z), w(_w) { }
#ifdef IM_VEC4_CLASS_EXTRA
    IM_VEC4_CLASS_EXTRA     // Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math types and UxImVec4.
#endif
};
IM_MSVC_RUNTIME_CHECKS_RESTORE

//-----------------------------------------------------------------------------
// [SECTION] Texture identifiers (UxImTextureID, UxImTextureRef)
//-----------------------------------------------------------------------------

// UxImTextureID = backend specific, low-level identifier for a texture uploaded in GPU/graphics system.
// [Compile-time configurable type]
// Overview:
// - When a Rendered Backend creates a texture, it store its native identifier into a UxImTextureID value.
//   (e.g. Used by DX11 backend to a `ID3D11ShaderResourceView*`; Used by OpenGL backends to store `GLuint';
//         Used by SDLGPU backend to store a `SDL_GPUTextureSamplerBinding*`, etc.).
// - User may submit their own textures to e.g. UxImGui::Image() function by passing the same type.
// - During the rendering loop, the Renderer Backend retrieve the UxImTextureID, which stored inside
//   UxImTextureRef, which is stored inside UxImDrawCmd.
// Configuring the type:
// - To use something other than a 64-bit value: add '#define UxImTextureID MyTextureType*' in your imconfig.h file.
// - This can be whatever to you want it to be! read the FAQ entry about textures for details.
// - You may decide to store a higher-level structure containing texture, sampler, shader etc. with various
//   constructors if you like. You will need to implement ==/!= operators.
// History:
// - In v1.91.4 (2024/10/08): the default type for UxImTextureID was changed from 'void*' to 'UxImU64'. This allowed backends requirig 64-bit worth of data to build on 32-bit architectures. Use intermediary intptr_t cast and read FAQ if you have casting warnings.
// - In v1.92.0 (2025/XX/XX): added UxImTextureRef which carry either a UxImTextureID either a pointer to internal texture atlas. All user facing functions taking UxImTextureID changed to UxImTextureRef
#ifndef UxImTextureID
typedef UxImU64 UxImTextureID;      // Default: store up to 64-bits (any pointer or integer). A majority of backends are ok with that.
#endif

// Define this if you need 0 to be a valid UxImTextureID for your backend.
#ifndef UxImTextureID_Invalid
#define UxImTextureID_Invalid     ((UxImTextureID)0)
#endif

// UxImTextureRef = higher-level identifier for a texture.
// The identifier is valid even before the texture has been uploaded to the GPU/graphics system.
// This is what gets passed to functions such as UxImGui::Image(), UxImDrawList::AddImage().
// This is what gets stored in draw commands (UxImDrawCmd) to identify a texture during rendering.
// - When a texture is created by user code (e.g. custom images), we directly stores the low-level UxImTextureID.
// - When a texture is created by the backend, we stores a UxImTextureData* which becomes an indirection
//   to extract the UxImTextureID value during rendering, after texture upload has happened.
// - There is no constructor to create a UxImTextureID from a UxImTextureData* as we don't expect this
//   to be useful to the end-user, and it would be erroneously called by many legacy code.
// - If you want to bind the current atlas when using custom rectangle, you can use io.Fonts->TexRef.
// - Binding generators for languages such as C (which don't have constructors), should provide a helper:
//      inline UxImTextureRef UxImTextureRefFromID(UxImTextureID tex_id) { UxImTextureRef tex_ref = { ._TexData = NULL, .TexID = tex_id }; return tex_ref; }
// In 1.92 we changed most drawing functions using UxImTextureID to use UxImTextureRef.
// We intentionally do not provide an implicit UxImTextureRef -> UxImTextureID cast operator because it is technically lossy to convert UxImTextureRef to UxImTextureID before rendering.
IM_MSVC_RUNTIME_CHECKS_OFF
struct UxImTextureRef
{
    UxImTextureRef()                          { _TexData = NULL; _TexID = UxImTextureID_Invalid; }
    UxImTextureRef(UxImTextureID tex_id)        { _TexData = NULL; _TexID = tex_id; }
#if !defined(IMGUI_DISABLE_OBSOLETE_FUNCTIONS) && !defined(UxImTextureID)
    UxImTextureRef(void* tex_id)              { _TexData = NULL; _TexID = (UxImTextureID)(size_t)tex_id; }  // For legacy backends casting to UxImTextureID
#endif

    inline UxImTextureID  GetTexID() const;   // == (_TexData ? _TexData->TexID : _TexID) // Implemented below in the file.

    // Members (either are set, never both!)
    UxImTextureData*      _TexData;           //      A texture, generally owned by a UxImFontAtlas. Will convert to UxImTextureID during render loop, after texture has been uploaded.
    UxImTextureID         _TexID;             // _OR_ Low-level backend texture identifier, if already uploaded or created by user/app. Generally provided to e.g. UxImGui::Image() calls.
};
IM_MSVC_RUNTIME_CHECKS_RESTORE

//-----------------------------------------------------------------------------
// [SECTION] Dear UxImGui end-user API functions
// (Note that UxImGui:: being a namespace, you can add extra UxImGui:: functions in your own separate file. Please don't modify imgui source files!)
//-----------------------------------------------------------------------------

namespace UxImGui
{
    // Context creation and access
    // - Each context create its own UxImFontAtlas by default. You may instance one yourself and pass it to CreateContext() to share a font atlas between contexts.
    // - DLL users: heaps and globals are not shared across DLL boundaries! You will need to call SetCurrentContext() + SetAllocatorFunctions()
    //   for each static/DLL boundary you are calling from. Read "Context and Memory Allocators" section of imgui.cpp for details.
    IMGUI_API UxImGuiContext* CreateContext(UxImFontAtlas* shared_font_atlas = NULL);
    IMGUI_API void          DestroyContext(UxImGuiContext* ctx = NULL);   // NULL = destroy current context
    IMGUI_API UxImGuiContext* GetCurrentContext();
    IMGUI_API void          SetCurrentContext(UxImGuiContext* ctx);

    // Main
    IMGUI_API UxImGuiIO&      GetIO();                                    // access the UxImGuiIO structure (mouse/keyboard/gamepad inputs, time, various configuration options/flags)
    IMGUI_API UxImGuiPlatformIO& GetPlatformIO();                         // access the UxImGuiPlatformIO structure (mostly hooks/functions to connect to platform/renderer and OS Clipboard, IME etc.)
    IMGUI_API UxImGuiStyle&   GetStyle();                                 // access the Style structure (colors, sizes). Always use PushStyleColor(), PushStyleVar() to modify style mid-frame!
    IMGUI_API void          NewFrame();                                 // start a new Dear UxImGui frame, you can submit any command from this point until Render()/EndFrame().
    IMGUI_API void          EndFrame();                                 // ends the Dear UxImGui frame. automatically called by Render(). If you don't need to render data (skipping rendering) you may call EndFrame() without Render()... but you'll have wasted CPU already! If you don't need to render, better to not create any windows and not call NewFrame() at all!
    IMGUI_API void          Render();                                   // ends the Dear UxImGui frame, finalize the draw data. You can then get call GetDrawData().
    IMGUI_API UxImDrawData*   GetDrawData();                              // valid after Render() and until the next call to NewFrame(). Call UxImGui_ImplXXXX_RenderDrawData() function in your Renderer Backend to render.

    // Demo, Debug, Information
    IMGUI_API void          ShowDemoWindow(bool* p_open = NULL);        // create Demo window. demonstrate most UxImGui features. call this to learn about the library! try to make it always available in your application!
    IMGUI_API void          ShowMetricsWindow(bool* p_open = NULL);     // create Metrics/Debugger window. display Dear UxImGui internals: windows, draw commands, various internal state, etc.
    IMGUI_API void          ShowDebugLogWindow(bool* p_open = NULL);    // create Debug Log window. display a simplified log of important dear imgui events.
    IMGUI_API void          ShowIDStackToolWindow(bool* p_open = NULL); // create Stack Tool window. hover items with mouse to query information about the source of their unique ID.
    IMGUI_API void          ShowAboutWindow(bool* p_open = NULL);       // create About window. display Dear UxImGui version, credits and build/system information.
    IMGUI_API void          ShowStyleEditor(UxImGuiStyle* ref = NULL);    // add style editor block (not a window). you can pass in a reference UxImGuiStyle structure to compare to, revert to and save to (else it uses the default style)
    IMGUI_API bool          ShowStyleSelector(const char* label);       // add style selector block (not a window), essentially a combo listing the default styles.
    IMGUI_API void          ShowFontSelector(const char* label);        // add font selector block (not a window), essentially a combo listing the loaded fonts.
    IMGUI_API void          ShowUserGuide();                            // add basic help/info block (not a window): how to manipulate UxImGui as an end-user (mouse/keyboard controls).
    IMGUI_API const char*   GetVersion();                               // get the compiled version string e.g. "1.80 WIP" (essentially the value for IMGUI_VERSION from the compiled version of imgui.cpp)

    // Styles
    IMGUI_API void          StyleColorsDark(UxImGuiStyle* dst = NULL);    // new, recommended style (default)
    IMGUI_API void          StyleColorsLight(UxImGuiStyle* dst = NULL);   // best used with borders and a custom, thicker font
    IMGUI_API void          StyleColorsClassic(UxImGuiStyle* dst = NULL); // classic imgui style

    // Windows
    // - Begin() = push window to the stack and start appending to it. End() = pop window from the stack.
    // - Passing 'bool* p_open != NULL' shows a window-closing widget in the upper-right corner of the window,
    //   which clicking will set the boolean to false when clicked.
    // - You may append multiple times to the same window during the same frame by calling Begin()/End() pairs multiple times.
    //   Some information such as 'flags' or 'p_open' will only be considered by the first call to Begin().
    // - Begin() return false to indicate the window is collapsed or fully clipped, so you may early out and omit submitting
    //   anything to the window. Always call a matching End() for each Begin() call, regardless of its return value!
    //   [Important: due to legacy reason, Begin/End and BeginChild/EndChild are inconsistent with all other functions
    //    such as BeginMenu/EndMenu, BeginPopup/EndPopup, etc. where the EndXXX call should only be called if the corresponding
    //    BeginXXX function returned true. Begin and BeginChild are the only odd ones out. Will be fixed in a future update.]
    // - Note that the bottom of window stack always contains a window called "Debug".
    IMGUI_API bool          Begin(const char* name, bool* p_open = NULL, UxImGuiWindowFlags flags = 0);
    IMGUI_API void          End();

    // Child Windows
    // - Use child windows to begin into a self-contained independent scrolling/clipping regions within a host window. Child windows can embed their own child.
    // - Before 1.90 (November 2023), the "UxImGuiChildFlags child_flags = 0" parameter was "bool border = false".
    //   This API is backward compatible with old code, as we guarantee that UxImGuiChildFlags_Borders == true.
    //   Consider updating your old code:
    //      BeginChild("Name", size, false)   -> Begin("Name", size, 0); or Begin("Name", size, UxImGuiChildFlags_None);
    //      BeginChild("Name", size, true)    -> Begin("Name", size, UxImGuiChildFlags_Borders);
    // - Manual sizing (each axis can use a different setting e.g. UxImVec2(0.0f, 400.0f)):
    //     == 0.0f: use remaining parent window size for this axis.
    //      > 0.0f: use specified size for this axis.
    //      < 0.0f: right/bottom-align to specified distance from available content boundaries.
    // - Specifying UxImGuiChildFlags_AutoResizeX or UxImGuiChildFlags_AutoResizeY makes the sizing automatic based on child contents.
    //   Combining both UxImGuiChildFlags_AutoResizeX _and_ UxImGuiChildFlags_AutoResizeY defeats purpose of a scrolling region and is NOT recommended.
    // - BeginChild() returns false to indicate the window is collapsed or fully clipped, so you may early out and omit submitting
    //   anything to the window. Always call a matching EndChild() for each BeginChild() call, regardless of its return value.
    //   [Important: due to legacy reason, Begin/End and BeginChild/EndChild are inconsistent with all other functions
    //    such as BeginMenu/EndMenu, BeginPopup/EndPopup, etc. where the EndXXX call should only be called if the corresponding
    //    BeginXXX function returned true. Begin and BeginChild are the only odd ones out. Will be fixed in a future update.]
    IMGUI_API bool          BeginChild(const char* str_id, const UxImVec2& size = UxImVec2(0, 0), UxImGuiChildFlags child_flags = 0, UxImGuiWindowFlags window_flags = 0);
    IMGUI_API bool          BeginChild(UxImGuiID id, const UxImVec2& size = UxImVec2(0, 0), UxImGuiChildFlags child_flags = 0, UxImGuiWindowFlags window_flags = 0);
    IMGUI_API void          EndChild();

    // Windows Utilities
    // - 'current window' = the window we are appending into while inside a Begin()/End() block. 'next window' = next window we will Begin() into.
    IMGUI_API bool          IsWindowAppearing();
    IMGUI_API bool          IsWindowCollapsed();
    IMGUI_API bool          IsWindowFocused(UxImGuiFocusedFlags flags=0); // is current window focused? or its root/child, depending on flags. see flags for options.
    IMGUI_API bool          IsWindowHovered(UxImGuiHoveredFlags flags=0); // is current window hovered and hoverable (e.g. not blocked by a popup/modal)? See UxImGuiHoveredFlags_ for options. IMPORTANT: If you are trying to check whether your mouse should be dispatched to Dear UxImGui or to your underlying app, you should not use this function! Use the 'io.WantCaptureMouse' boolean for that! Refer to FAQ entry "How can I tell whether to dispatch mouse/keyboard to Dear UxImGui or my application?" for details.
    IMGUI_API UxImDrawList*   GetWindowDrawList();                        // get draw list associated to the current window, to append your own drawing primitives
    IMGUI_API float         GetWindowDpiScale();                        // get DPI scale currently associated to the current window's viewport.
    IMGUI_API UxImVec2        GetWindowPos();                             // get current window position in screen space (IT IS UNLIKELY YOU EVER NEED TO USE THIS. Consider always using GetCursorScreenPos() and GetContentRegionAvail() instead)
    IMGUI_API UxImVec2        GetWindowSize();                            // get current window size (IT IS UNLIKELY YOU EVER NEED TO USE THIS. Consider always using GetCursorScreenPos() and GetContentRegionAvail() instead)
    IMGUI_API float         GetWindowWidth();                           // get current window width (IT IS UNLIKELY YOU EVER NEED TO USE THIS). Shortcut for GetWindowSize().x.
    IMGUI_API float         GetWindowHeight();                          // get current window height (IT IS UNLIKELY YOU EVER NEED TO USE THIS). Shortcut for GetWindowSize().y.
    IMGUI_API UxImGuiViewport*GetWindowViewport();                        // get viewport currently associated to the current window.

    // Window manipulation
    // - Prefer using SetNextXXX functions (before Begin) rather that SetXXX functions (after Begin).
    IMGUI_API void          SetNextWindowPos(const UxImVec2& pos, UxImGuiCond cond = 0, const UxImVec2& pivot = UxImVec2(0, 0)); // set next window position. call before Begin(). use pivot=(0.5f,0.5f) to center on given point, etc.
    IMGUI_API void          SetNextWindowSize(const UxImVec2& size, UxImGuiCond cond = 0);                  // set next window size. set axis to 0.0f to force an auto-fit on this axis. call before Begin()
    IMGUI_API void          SetNextWindowSizeConstraints(const UxImVec2& size_min, const UxImVec2& size_max, UxImGuiSizeCallback custom_callback = NULL, void* custom_callback_data = NULL); // set next window size limits. use 0.0f or FLT_MAX if you don't want limits. Use -1 for both min and max of same axis to preserve current size (which itself is a constraint). Use callback to apply non-trivial programmatic constraints.
    IMGUI_API void          SetNextWindowContentSize(const UxImVec2& size);                               // set next window content size (~ scrollable client area, which enforce the range of scrollbars). Not including window decorations (title bar, menu bar, etc.) nor WindowPadding. set an axis to 0.0f to leave it automatic. call before Begin()
    IMGUI_API void          SetNextWindowCollapsed(bool collapsed, UxImGuiCond cond = 0);                 // set next window collapsed state. call before Begin()
    IMGUI_API void          SetNextWindowFocus();                                                       // set next window to be focused / top-most. call before Begin()
    IMGUI_API void          SetNextWindowScroll(const UxImVec2& scroll);                                  // set next window scrolling value (use < 0.0f to not affect a given axis).
    IMGUI_API void          SetNextWindowBgAlpha(float alpha);                                          // set next window background color alpha. helper to easily override the Alpha component of UxImGuiCol_WindowBg/ChildBg/PopupBg. you may also use UxImGuiWindowFlags_NoBackground.
    IMGUI_API void          SetNextWindowViewport(UxImGuiID viewport_id);                                 // set next window viewport
    IMGUI_API void          SetWindowPos(const UxImVec2& pos, UxImGuiCond cond = 0);                        // (not recommended) set current window position - call within Begin()/End(). prefer using SetNextWindowPos(), as this may incur tearing and side-effects.
    IMGUI_API void          SetWindowSize(const UxImVec2& size, UxImGuiCond cond = 0);                      // (not recommended) set current window size - call within Begin()/End(). set to UxImVec2(0, 0) to force an auto-fit. prefer using SetNextWindowSize(), as this may incur tearing and minor side-effects.
    IMGUI_API void          SetWindowCollapsed(bool collapsed, UxImGuiCond cond = 0);                     // (not recommended) set current window collapsed state. prefer using SetNextWindowCollapsed().
    IMGUI_API void          SetWindowFocus();                                                           // (not recommended) set current window to be focused / top-most. prefer using SetNextWindowFocus().
    IMGUI_API void          SetWindowFontScale(float scale);                                            // [OBSOLETE] set font scale. Adjust IO.FontGlobalScale if you want to scale all windows. This is an old API! For correct scaling, prefer to reload font + rebuild UxImFontAtlas + call style.ScaleAllSizes().
    IMGUI_API void          SetWindowPos(const char* name, const UxImVec2& pos, UxImGuiCond cond = 0);      // set named window position.
    IMGUI_API void          SetWindowSize(const char* name, const UxImVec2& size, UxImGuiCond cond = 0);    // set named window size. set axis to 0.0f to force an auto-fit on this axis.
    IMGUI_API void          SetWindowCollapsed(const char* name, bool collapsed, UxImGuiCond cond = 0);   // set named window collapsed state
    IMGUI_API void          SetWindowFocus(const char* name);                                           // set named window to be focused / top-most. use NULL to remove focus.

    // Windows Scrolling
    // - Any change of Scroll will be applied at the beginning of next frame in the first call to Begin().
    // - You may instead use SetNextWindowScroll() prior to calling Begin() to avoid this delay, as an alternative to using SetScrollX()/SetScrollY().
    IMGUI_API float         GetScrollX();                                                   // get scrolling amount [0 .. GetScrollMaxX()]
    IMGUI_API float         GetScrollY();                                                   // get scrolling amount [0 .. GetScrollMaxY()]
    IMGUI_API void          SetScrollX(float scroll_x);                                     // set scrolling amount [0 .. GetScrollMaxX()]
    IMGUI_API void          SetScrollY(float scroll_y);                                     // set scrolling amount [0 .. GetScrollMaxY()]
    IMGUI_API float         GetScrollMaxX();                                                // get maximum scrolling amount ~~ ContentSize.x - WindowSize.x - DecorationsSize.x
    IMGUI_API float         GetScrollMaxY();                                                // get maximum scrolling amount ~~ ContentSize.y - WindowSize.y - DecorationsSize.y
    IMGUI_API void          SetScrollHereX(float center_x_ratio = 0.5f);                    // adjust scrolling amount to make current cursor position visible. center_x_ratio=0.0: left, 0.5: center, 1.0: right. When using to make a "default/current item" visible, consider using SetItemDefaultFocus() instead.
    IMGUI_API void          SetScrollHereY(float center_y_ratio = 0.5f);                    // adjust scrolling amount to make current cursor position visible. center_y_ratio=0.0: top, 0.5: center, 1.0: bottom. When using to make a "default/current item" visible, consider using SetItemDefaultFocus() instead.
    IMGUI_API void          SetScrollFromPosX(float local_x, float center_x_ratio = 0.5f);  // adjust scrolling amount to make given position visible. Generally GetCursorStartPos() + offset to compute a valid position.
    IMGUI_API void          SetScrollFromPosY(float local_y, float center_y_ratio = 0.5f);  // adjust scrolling amount to make given position visible. Generally GetCursorStartPos() + offset to compute a valid position.

    // Parameters stacks (font)
    // *IMPORTANT* before 1.92, fonts had a single size. They can now be dynamically be adjusted.
    // - Before 1.92: PushFont() always used font default size.
    // -  Since 1.92: PushFont() preserve the current shared font size.
    // - To use old behavior (single size font): use 'PushFont(font, font->DefaultSize)' in call site, or set 'UxImFontConfig::Flags |= UxImFontFlags_UseDefaultSize' before calling AddFont().
    IMGUI_API void          PushFont(UxImFont* font, float font_size = -1);                   // use NULL as a shortcut to push default font. Use <0.0f to keep current font size. Use font->DefaultSize to revert to font default size.
    IMGUI_API void          PopFont();
    IMGUI_API void          PushFontSize(float font_size);
    IMGUI_API void          PopFontSize();

    // Parameters stacks (shared)
    IMGUI_API void          PushStyleColor(UxImGuiCol idx, UxImU32 col);                        // modify a style color. always use this if you modify the style after NewFrame().
    IMGUI_API void          PushStyleColor(UxImGuiCol idx, const UxImVec4& col);
    IMGUI_API void          PopStyleColor(int count = 1);
    IMGUI_API void          PushStyleVar(UxImGuiStyleVar idx, float val);                     // modify a style float variable. always use this if you modify the style after NewFrame()!
    IMGUI_API void          PushStyleVar(UxImGuiStyleVar idx, const UxImVec2& val);             // modify a style UxImVec2 variable. "
    IMGUI_API void          PushStyleVarX(UxImGuiStyleVar idx, float val_x);                  // modify X component of a style UxImVec2 variable. "
    IMGUI_API void          PushStyleVarY(UxImGuiStyleVar idx, float val_y);                  // modify Y component of a style UxImVec2 variable. "
    IMGUI_API void          PopStyleVar(int count = 1);
    IMGUI_API void          PushItemFlag(UxImGuiItemFlags option, bool enabled);              // modify specified shared item flag, e.g. PushItemFlag(UxImGuiItemFlags_NoTabStop, true)
    IMGUI_API void          PopItemFlag();

    // Parameters stacks (current window)
    IMGUI_API void          PushItemWidth(float item_width);                                // push width of items for common large "item+label" widgets. >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -FLT_MIN always align width to the right side).
    IMGUI_API void          PopItemWidth();
    IMGUI_API void          SetNextItemWidth(float item_width);                             // set width of the _next_ common large "item+label" widget. >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -FLT_MIN always align width to the right side)
    IMGUI_API float         CalcItemWidth();                                                // width of item given pushed settings and current cursor position. NOT necessarily the width of last item unlike most 'Item' functions.
    IMGUI_API void          PushTextWrapPos(float wrap_local_pos_x = 0.0f);                 // push word-wrapping position for Text*() commands. < 0.0f: no wrapping; 0.0f: wrap to end of window (or column); > 0.0f: wrap at 'wrap_pos_x' position in window local space
    IMGUI_API void          PopTextWrapPos();

    // Style read access
    // - Use the ShowStyleEditor() function to interactively see/edit the colors.
    IMGUI_API UxImFont*       GetFont();                                                      // get current font
    IMGUI_API float         GetFontSize();                                                  // get current font size (= height in pixels) of current font with current scale applied
    IMGUI_API UxImVec2        GetFontTexUvWhitePixel();                                       // get UV coordinate for a white pixel, useful to draw custom shapes via the UxImDrawList API
    IMGUI_API UxImFontBaked*  GetFontBaked();                                                 // get current font bound at current size // == GetFont()->GetFontBaked(GetFontSize())
    IMGUI_API UxImU32         GetColorU32(UxImGuiCol idx, float alpha_mul = 1.0f);              // retrieve given style color with style alpha applied and optional extra alpha multiplier, packed as a 32-bit value suitable for UxImDrawList
    IMGUI_API UxImU32         GetColorU32(const UxImVec4& col);                                 // retrieve given color with style alpha applied, packed as a 32-bit value suitable for UxImDrawList
    IMGUI_API UxImU32         GetColorU32(UxImU32 col, float alpha_mul = 1.0f);                 // retrieve given color with style alpha applied, packed as a 32-bit value suitable for UxImDrawList
    IMGUI_API const UxImVec4& GetStyleColorVec4(UxImGuiCol idx);                                // retrieve style color as stored in UxImGuiStyle structure. use to feed back into PushStyleColor(), otherwise use GetColorU32() to get style color with style alpha baked in.

    // Layout cursor positioning
    // - By "cursor" we mean the current output position.
    // - The typical widget behavior is to output themselves at the current cursor position, then move the cursor one line down.
    // - You can call SameLine() between widgets to undo the last carriage return and output at the right of the preceding widget.
    // - YOU CAN DO 99% OF WHAT YOU NEED WITH ONLY GetCursorScreenPos() and GetContentRegionAvail().
    // - Attention! We currently have inconsistencies between window-local and absolute positions we will aim to fix with future API:
    //    - Absolute coordinate:        GetCursorScreenPos(), SetCursorScreenPos(), all UxImDrawList:: functions. -> this is the preferred way forward.
    //    - Window-local coordinates:   SameLine(offset), GetCursorPos(), SetCursorPos(), GetCursorStartPos(), PushTextWrapPos()
    //    - Window-local coordinates:   GetContentRegionMax(), GetWindowContentRegionMin(), GetWindowContentRegionMax() --> all obsoleted. YOU DON'T NEED THEM.
    // - GetCursorScreenPos() = GetCursorPos() + GetWindowPos(). GetWindowPos() is almost only ever useful to convert from window-local to absolute coordinates. Try not to use it.
    IMGUI_API UxImVec2        GetCursorScreenPos();                                           // cursor position, absolute coordinates. THIS IS YOUR BEST FRIEND (prefer using this rather than GetCursorPos(), also more useful to work with UxImDrawList API).
    IMGUI_API void          SetCursorScreenPos(const UxImVec2& pos);                          // cursor position, absolute coordinates. THIS IS YOUR BEST FRIEND.
    IMGUI_API UxImVec2        GetContentRegionAvail();                                        // available space from current position. THIS IS YOUR BEST FRIEND.
    IMGUI_API UxImVec2        GetCursorPos();                                                 // [window-local] cursor position in window-local coordinates. This is not your best friend.
    IMGUI_API float         GetCursorPosX();                                                // [window-local] "
    IMGUI_API float         GetCursorPosY();                                                // [window-local] "
    IMGUI_API void          SetCursorPos(const UxImVec2& local_pos);                          // [window-local] "
    IMGUI_API void          SetCursorPosX(float local_x);                                   // [window-local] "
    IMGUI_API void          SetCursorPosY(float local_y);                                   // [window-local] "
    IMGUI_API UxImVec2        GetCursorStartPos();                                            // [window-local] initial cursor position, in window-local coordinates. Call GetCursorScreenPos() after Begin() to get the absolute coordinates version.

    // Other layout functions
    IMGUI_API void          Separator();                                                    // separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.
    IMGUI_API void          SameLine(float offset_from_start_x=0.0f, float spacing=-1.0f);  // call between widgets or groups to layout them horizontally. X position given in window coordinates.
    IMGUI_API void          NewLine();                                                      // undo a SameLine() or force a new line when in a horizontal-layout context.
    IMGUI_API void          Spacing();                                                      // add vertical spacing.
    IMGUI_API void          Dummy(const UxImVec2& size);                                      // add a dummy item of given size. unlike InvisibleButton(), Dummy() won't take the mouse click or be navigable into.
    IMGUI_API void          Indent(float indent_w = 0.0f);                                  // move content position toward the right, by indent_w, or style.IndentSpacing if indent_w <= 0
    IMGUI_API void          Unindent(float indent_w = 0.0f);                                // move content position back to the left, by indent_w, or style.IndentSpacing if indent_w <= 0
    IMGUI_API void          BeginGroup();                                                   // lock horizontal starting position
    IMGUI_API void          EndGroup();                                                     // unlock horizontal starting position + capture the whole group bounding box into one "item" (so you can use IsItemHovered() or layout primitives such as SameLine() on whole group, etc.)
    IMGUI_API void          AlignTextToFramePadding();                                      // vertically align upcoming text baseline to FramePadding.y so that it will align properly to regularly framed items (call if you have text on a line before a framed item)
    IMGUI_API float         GetTextLineHeight();                                            // ~ FontSize
    IMGUI_API float         GetTextLineHeightWithSpacing();                                 // ~ FontSize + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of text)
    IMGUI_API float         GetFrameHeight();                                               // ~ FontSize + style.FramePadding.y * 2
    IMGUI_API float         GetFrameHeightWithSpacing();                                    // ~ FontSize + style.FramePadding.y * 2 + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of framed widgets)

    // ID stack/scopes
    // Read the FAQ (docs/FAQ.md or http://dearimgui.com/faq) for more details about how ID are handled in dear imgui.
    // - Those questions are answered and impacted by understanding of the ID stack system:
    //   - "Q: Why is my widget not reacting when I click on it?"
    //   - "Q: How can I have widgets with an empty label?"
    //   - "Q: How can I have multiple widgets with the same label?"
    // - Short version: ID are hashes of the entire ID stack. If you are creating widgets in a loop you most likely
    //   want to push a unique identifier (e.g. object pointer, loop index) to uniquely differentiate them.
    // - You can also use the "Label##foobar" syntax within widget label to distinguish them from each others.
    // - In this header file we use the "label"/"name" terminology to denote a string that will be displayed + used as an ID,
    //   whereas "str_id" denote a string that is only used as an ID and not normally displayed.
    IMGUI_API void          PushID(const char* str_id);                                     // push string into the ID stack (will hash string).
    IMGUI_API void          PushID(const char* str_id_begin, const char* str_id_end);       // push string into the ID stack (will hash string).
    IMGUI_API void          PushID(const void* ptr_id);                                     // push pointer into the ID stack (will hash pointer).
    IMGUI_API void          PushID(int int_id);                                             // push integer into the ID stack (will hash integer).
    IMGUI_API void          PopID();                                                        // pop from the ID stack.
    IMGUI_API UxImGuiID       GetID(const char* str_id);                                      // calculate unique ID (hash of whole ID stack + given parameter). e.g. if you want to query into UxImGuiStorage yourself
    IMGUI_API UxImGuiID       GetID(const char* str_id_begin, const char* str_id_end);
    IMGUI_API UxImGuiID       GetID(const void* ptr_id);
    IMGUI_API UxImGuiID       GetID(int int_id);

    // Widgets: Text
    IMGUI_API void          TextUnformatted(const char* text, const char* text_end = NULL); // raw text without formatting. Roughly equivalent to Text("%s", text) but: A) doesn't require null terminated string if 'text_end' is specified, B) it's faster, no memory copy is done, no buffer size limits, recommended for long chunks of text.
    IMGUI_API void          Text(const char* fmt, ...)                                      IM_FMTARGS(1); // formatted text
    IMGUI_API void          TextV(const char* fmt, va_list args)                            IM_FMTLIST(1);
    IMGUI_API void          TextColored(const UxImVec4& col, const char* fmt, ...)            IM_FMTARGS(2); // shortcut for PushStyleColor(UxImGuiCol_Text, col); Text(fmt, ...); PopStyleColor();
    IMGUI_API void          TextColoredV(const UxImVec4& col, const char* fmt, va_list args)  IM_FMTLIST(2);
    IMGUI_API void          TextDisabled(const char* fmt, ...)                              IM_FMTARGS(1); // shortcut for PushStyleColor(UxImGuiCol_Text, style.Colors[UxImGuiCol_TextDisabled]); Text(fmt, ...); PopStyleColor();
    IMGUI_API void          TextDisabledV(const char* fmt, va_list args)                    IM_FMTLIST(1);
    IMGUI_API void          TextWrapped(const char* fmt, ...)                               IM_FMTARGS(1); // shortcut for PushTextWrapPos(0.0f); Text(fmt, ...); PopTextWrapPos();. Note that this won't work on an auto-resizing window if there's no other widgets to extend the window width, yoy may need to set a size using SetNextWindowSize().
    IMGUI_API void          TextWrappedV(const char* fmt, va_list args)                     IM_FMTLIST(1);
    IMGUI_API void          LabelText(const char* label, const char* fmt, ...)              IM_FMTARGS(2); // display text+label aligned the same way as value+label widgets
    IMGUI_API void          LabelTextV(const char* label, const char* fmt, va_list args)    IM_FMTLIST(2);
    IMGUI_API void          BulletText(const char* fmt, ...)                                IM_FMTARGS(1); // shortcut for Bullet()+Text()
    IMGUI_API void          BulletTextV(const char* fmt, va_list args)                      IM_FMTLIST(1);
    IMGUI_API void          SeparatorText(const char* label);                               // currently: formatted text with a horizontal line

    // Widgets: Main
    // - Most widgets return true when the value has been changed or when pressed/selected
    // - You may also use one of the many IsItemXXX functions (e.g. IsItemActive, IsItemHovered, etc.) to query widget state.
    IMGUI_API bool          Button(const char* label, const UxImVec2& size = UxImVec2(0, 0));   // button
    IMGUI_API bool          SmallButton(const char* label);                                 // button with (FramePadding.y == 0) to easily embed within text
    IMGUI_API bool          InvisibleButton(const char* str_id, const UxImVec2& size, UxImGuiButtonFlags flags = 0); // flexible button behavior without the visuals, frequently useful to build custom behaviors using the public api (along with IsItemActive, IsItemHovered, etc.)
    IMGUI_API bool          ArrowButton(const char* str_id, UxImGuiDir dir);                  // square button with an arrow shape
    IMGUI_API bool          Checkbox(const char* label, bool* v);
    IMGUI_API bool          CheckboxFlags(const char* label, int* flags, int flags_value);
    IMGUI_API bool          CheckboxFlags(const char* label, unsigned int* flags, unsigned int flags_value);
    IMGUI_API bool          RadioButton(const char* label, bool active);                    // use with e.g. if (RadioButton("one", my_value==1)) { my_value = 1; }
    IMGUI_API bool          RadioButton(const char* label, int* v, int v_button);           // shortcut to handle the above pattern when value is an integer
    IMGUI_API void          ProgressBar(float fraction, const UxImVec2& size_arg = UxImVec2(-FLT_MIN, 0), const char* overlay = NULL);
    IMGUI_API void          Bullet();                                                       // draw a small circle + keep the cursor on the same line. advance cursor x position by GetTreeNodeToLabelSpacing(), same distance that TreeNode() uses
    IMGUI_API bool          TextLink(const char* label);                                    // hyperlink text button, return true when clicked
    IMGUI_API void          TextLinkOpenURL(const char* label, const char* url = NULL);     // hyperlink text button, automatically open file/url when clicked

    // Widgets: Images
    // - Read about UxImTextureID/UxImTextureRef  here: https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
    // - 'uv0' and 'uv1' are texture coordinates. Read about them from the same link above.
    // - Image() pads adds style.ImageBorderSize on each side, ImageButton() adds style.FramePadding on each side.
    // - ImageButton() draws a background based on regular Button() color + optionally an inner background if specified.
    IMGUI_API void          Image(UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0 = UxImVec2(0, 0), const UxImVec2& uv1 = UxImVec2(1, 1));
    IMGUI_API void          ImageWithBg(UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0 = UxImVec2(0, 0), const UxImVec2& uv1 = UxImVec2(1, 1), const UxImVec4& bg_col = UxImVec4(0, 0, 0, 0), const UxImVec4& tint_col = UxImVec4(1, 1, 1, 1));
    IMGUI_API bool          ImageButton(const char* str_id, UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0 = UxImVec2(0, 0), const UxImVec2& uv1 = UxImVec2(1, 1), const UxImVec4& bg_col = UxImVec4(0, 0, 0, 0), const UxImVec4& tint_col = UxImVec4(1, 1, 1, 1));

    // Widgets: Combo Box (Dropdown)
    // - The BeginCombo()/EndCombo() api allows you to manage your contents and selection state however you want it, by creating e.g. Selectable() items.
    // - The old Combo() api are helpers over BeginCombo()/EndCombo() which are kept available for convenience purpose. This is analogous to how ListBox are created.
    IMGUI_API bool          BeginCombo(const char* label, const char* preview_value, UxImGuiComboFlags flags = 0);
    IMGUI_API void          EndCombo(); // only call EndCombo() if BeginCombo() returns true!
    IMGUI_API bool          Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1);
    IMGUI_API bool          Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int popup_max_height_in_items = -1);      // Separate items with \0 within a string, end item-list with \0\0. e.g. "One\0Two\0Three\0"
    IMGUI_API bool          Combo(const char* label, int* current_item, const char* (*getter)(void* user_data, int idx), void* user_data, int items_count, int popup_max_height_in_items = -1);

    // Widgets: Drag Sliders
    // - CTRL+Click on any drag box to turn them into an input box. Manually input values aren't clamped by default and can go off-bounds. Use UxImGuiSliderFlags_AlwaysClamp to always clamp.
    // - For all the Float2/Float3/Float4/Int2/Int3/Int4 versions of every function, note that a 'float v[X]' function argument is the same as 'float* v',
    //   the array syntax is just a way to document the number of elements that are expected to be accessible. You can pass address of your first element out of a contiguous set, e.g. &myvector.x
    // - Adjust format string to decorate the value with a prefix, a suffix, or adapt the editing and display precision e.g. "%.3f" -> 1.234; "%5.2f secs" -> 01.23 secs; "Biscuit: %.0f" -> Biscuit: 1; etc.
    // - Format string may also be set to NULL or use the default format ("%f" or "%d").
    // - Speed are per-pixel of mouse movement (v_speed=0.2f: mouse needs to move by 5 pixels to increase value by 1). For keyboard/gamepad navigation, minimum speed is Max(v_speed, minimum_step_at_given_precision).
    // - Use v_min < v_max to clamp edits to given limits. Note that CTRL+Click manual input can override those limits if UxImGuiSliderFlags_AlwaysClamp is not used.
    // - Use v_max = FLT_MAX / INT_MAX etc to avoid clamping to a maximum, same with v_min = -FLT_MAX / INT_MIN to avoid clamping to a minimum.
    // - We use the same sets of flags for DragXXX() and SliderXXX() functions as the features are the same and it makes it easier to swap them.
    // - Legacy: Pre-1.78 there are DragXXX() function signatures that take a final `float power=1.0f' argument instead of the `UxImGuiSliderFlags flags=0' argument.
    //   If you get a warning converting a float to UxImGuiSliderFlags, read https://github.com/ocornut/imgui/issues/3361
    IMGUI_API bool          DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);     // If v_min >= v_max we have no bound
    IMGUI_API bool          DragFloat2(const char* label, float v[2], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragFloat4(const char* label, float v[4], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragFloatRange2(const char* label, float* v_current_min, float* v_current_max, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", const char* format_max = NULL, UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", UxImGuiSliderFlags flags = 0);  // If v_min >= v_max we have no bound
    IMGUI_API bool          DragInt2(const char* label, int v[2], float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragInt3(const char* label, int v[3], float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragInt4(const char* label, int v[4], float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragIntRange2(const char* label, int* v_current_min, int* v_current_max, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", const char* format_max = NULL, UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragScalar(const char* label, UxImGuiDataType data_type, void* p_data, float v_speed = 1.0f, const void* p_min = NULL, const void* p_max = NULL, const char* format = NULL, UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          DragScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, float v_speed = 1.0f, const void* p_min = NULL, const void* p_max = NULL, const char* format = NULL, UxImGuiSliderFlags flags = 0);

    // Widgets: Regular Sliders
    // - CTRL+Click on any slider to turn them into an input box. Manually input values aren't clamped by default and can go off-bounds. Use UxImGuiSliderFlags_AlwaysClamp to always clamp.
    // - Adjust format string to decorate the value with a prefix, a suffix, or adapt the editing and display precision e.g. "%.3f" -> 1.234; "%5.2f secs" -> 01.23 secs; "Biscuit: %.0f" -> Biscuit: 1; etc.
    // - Format string may also be set to NULL or use the default format ("%f" or "%d").
    // - Legacy: Pre-1.78 there are SliderXXX() function signatures that take a final `float power=1.0f' argument instead of the `UxImGuiSliderFlags flags=0' argument.
    //   If you get a warning converting a float to UxImGuiSliderFlags, read https://github.com/ocornut/imgui/issues/3361
    IMGUI_API bool          SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);     // adjust format to decorate the value with a prefix or a suffix for in-slider labels or unit display.
    IMGUI_API bool          SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderAngle(const char* label, float* v_rad, float v_degrees_min = -360.0f, float v_degrees_max = +360.0f, const char* format = "%.0f deg", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderScalar(const char* label, UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = NULL, UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          SliderScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, const char* format = NULL, UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          VSliderFloat(const char* label, const UxImVec2& size, float* v, float v_min, float v_max, const char* format = "%.3f", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          VSliderInt(const char* label, const UxImVec2& size, int* v, int v_min, int v_max, const char* format = "%d", UxImGuiSliderFlags flags = 0);
    IMGUI_API bool          VSliderScalar(const char* label, const UxImVec2& size, UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = NULL, UxImGuiSliderFlags flags = 0);

    // Widgets: Input with Keyboard
    // - If you want to use InputText() with std::string or any custom dynamic string type, see misc/cpp/imgui_stdlib.h and comments in imgui_demo.cpp.
    // - Most of the UxImGuiInputTextFlags flags are only useful for InputText() and not for InputFloatX, InputIntX, InputDouble etc.
    IMGUI_API bool          InputText(const char* label, char* buf, size_t buf_size, UxImGuiInputTextFlags flags = 0, UxImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    IMGUI_API bool          InputTextMultiline(const char* label, char* buf, size_t buf_size, const UxImVec2& size = UxImVec2(0, 0), UxImGuiInputTextFlags flags = 0, UxImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    IMGUI_API bool          InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, UxImGuiInputTextFlags flags = 0, UxImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    IMGUI_API bool          InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f", UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputFloat2(const char* label, float v[2], const char* format = "%.3f", UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputFloat3(const char* label, float v[3], const char* format = "%.3f", UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputFloat4(const char* label, float v[4], const char* format = "%.3f", UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputInt(const char* label, int* v, int step = 1, int step_fast = 100, UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputInt2(const char* label, int v[2], UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputInt3(const char* label, int v[3], UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputInt4(const char* label, int v[4], UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputDouble(const char* label, double* v, double step = 0.0, double step_fast = 0.0, const char* format = "%.6f", UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputScalar(const char* label, UxImGuiDataType data_type, void* p_data, const void* p_step = NULL, const void* p_step_fast = NULL, const char* format = NULL, UxImGuiInputTextFlags flags = 0);
    IMGUI_API bool          InputScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, const void* p_step = NULL, const void* p_step_fast = NULL, const char* format = NULL, UxImGuiInputTextFlags flags = 0);

    // Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little color square that can be left-clicked to open a picker, and right-clicked to open an option menu.)
    // - Note that in C++ a 'float v[X]' function argument is the _same_ as 'float* v', the array syntax is just a way to document the number of elements that are expected to be accessible.
    // - You can pass the address of a first float element out of a contiguous structure, e.g. &myvector.x
    IMGUI_API bool          ColorEdit3(const char* label, float col[3], UxImGuiColorEditFlags flags = 0);
    IMGUI_API bool          ColorEdit4(const char* label, float col[4], UxImGuiColorEditFlags flags = 0);
    IMGUI_API bool          ColorPicker3(const char* label, float col[3], UxImGuiColorEditFlags flags = 0);
    IMGUI_API bool          ColorPicker4(const char* label, float col[4], UxImGuiColorEditFlags flags = 0, const float* ref_col = NULL);
    IMGUI_API bool          ColorButton(const char* desc_id, const UxImVec4& col, UxImGuiColorEditFlags flags = 0, const UxImVec2& size = UxImVec2(0, 0)); // display a color square/button, hover for details, return true when pressed.
    IMGUI_API void          SetColorEditOptions(UxImGuiColorEditFlags flags);                     // initialize current options (generally on application startup) if you want to select a default format, picker type, etc. User will be able to change many settings, unless you pass the _NoOptions flag to your calls.

    // Widgets: Trees
    // - TreeNode functions return true when the node is open, in which case you need to also call TreePop() when you are finished displaying the tree node contents.
    IMGUI_API bool          TreeNode(const char* label);
    IMGUI_API bool          TreeNode(const char* str_id, const char* fmt, ...) IM_FMTARGS(2);   // helper variation to easily decorelate the id from the displayed string. Read the FAQ about why and how to use ID. to align arbitrary text at the same level as a TreeNode() you can use Bullet().
    IMGUI_API bool          TreeNode(const void* ptr_id, const char* fmt, ...) IM_FMTARGS(2);   // "
    IMGUI_API bool          TreeNodeV(const char* str_id, const char* fmt, va_list args) IM_FMTLIST(2);
    IMGUI_API bool          TreeNodeV(const void* ptr_id, const char* fmt, va_list args) IM_FMTLIST(2);
    IMGUI_API bool          TreeNodeEx(const char* label, UxImGuiTreeNodeFlags flags = 0);
    IMGUI_API bool          TreeNodeEx(const char* str_id, UxImGuiTreeNodeFlags flags, const char* fmt, ...) IM_FMTARGS(3);
    IMGUI_API bool          TreeNodeEx(const void* ptr_id, UxImGuiTreeNodeFlags flags, const char* fmt, ...) IM_FMTARGS(3);
    IMGUI_API bool          TreeNodeExV(const char* str_id, UxImGuiTreeNodeFlags flags, const char* fmt, va_list args) IM_FMTLIST(3);
    IMGUI_API bool          TreeNodeExV(const void* ptr_id, UxImGuiTreeNodeFlags flags, const char* fmt, va_list args) IM_FMTLIST(3);
    IMGUI_API void          TreePush(const char* str_id);                                       // ~ Indent()+PushID(). Already called by TreeNode() when returning true, but you can call TreePush/TreePop yourself if desired.
    IMGUI_API void          TreePush(const void* ptr_id);                                       // "
    IMGUI_API void          TreePop();                                                          // ~ Unindent()+PopID()
    IMGUI_API float         GetTreeNodeToLabelSpacing();                                        // horizontal distance preceding label when using TreeNode*() or Bullet() == (g.FontSize + style.FramePadding.x*2) for a regular unframed TreeNode
    IMGUI_API bool          CollapsingHeader(const char* label, UxImGuiTreeNodeFlags flags = 0);  // if returning 'true' the header is open. doesn't indent nor push on ID stack. user doesn't have to call TreePop().
    IMGUI_API bool          CollapsingHeader(const char* label, bool* p_visible, UxImGuiTreeNodeFlags flags = 0); // when 'p_visible != NULL': if '*p_visible==true' display an additional small close button on upper right of the header which will set the bool to false when clicked, if '*p_visible==false' don't display the header.
    IMGUI_API void          SetNextItemOpen(bool is_open, UxImGuiCond cond = 0);                  // set next TreeNode/CollapsingHeader open state.
    IMGUI_API void          SetNextItemStorageID(UxImGuiID storage_id);                           // set id to use for open/close storage (default to same as item id).

    // Widgets: Selectables
    // - A selectable highlights when hovered, and can display another color when selected.
    // - Neighbors selectable extend their highlight bounds in order to leave no gap between them. This is so a series of selected Selectable appear contiguous.
    IMGUI_API bool          Selectable(const char* label, bool selected = false, UxImGuiSelectableFlags flags = 0, const UxImVec2& size = UxImVec2(0, 0)); // "bool selected" carry the selection state (read-only). Selectable() is clicked is returns true so you can modify your selection state. size.x==0.0: use remaining width, size.x>0.0: specify width. size.y==0.0: use label height, size.y>0.0: specify height
    IMGUI_API bool          Selectable(const char* label, bool* p_selected, UxImGuiSelectableFlags flags = 0, const UxImVec2& size = UxImVec2(0, 0));      // "bool* p_selected" point to the selection state (read-write), as a convenient helper.

    // Multi-selection system for Selectable(), Checkbox(), TreeNode() functions [BETA]
    // - This enables standard multi-selection/range-selection idioms (CTRL+Mouse/Keyboard, SHIFT+Mouse/Keyboard, etc.) in a way that also allow a clipper to be used.
    // - UxImGuiSelectionUserData is often used to store your item index within the current view (but may store something else).
    // - Read comments near UxImGuiMultiSelectIO for instructions/details and see 'Demo->Widgets->Selection State & Multi-Select' for demo.
    // - TreeNode() is technically supported but... using this correctly is more complicated. You need some sort of linear/random access to your tree,
    //   which is suited to advanced trees setups already implementing filters and clipper. We will work simplifying the current demo.
    // - 'selection_size' and 'items_count' parameters are optional and used by a few features. If they are costly for you to compute, you may avoid them.
    IMGUI_API UxImGuiMultiSelectIO*   BeginMultiSelect(UxImGuiMultiSelectFlags flags, int selection_size = -1, int items_count = -1);
    IMGUI_API UxImGuiMultiSelectIO*   EndMultiSelect();
    IMGUI_API void                  SetNextItemSelectionUserData(UxImGuiSelectionUserData selection_user_data);
    IMGUI_API bool                  IsItemToggledSelection();                                   // Was the last item selection state toggled? Useful if you need the per-item information _before_ reaching EndMultiSelect(). We only returns toggle _event_ in order to handle clipping correctly.

    // Widgets: List Boxes
    // - This is essentially a thin wrapper to using BeginChild/EndChild with the UxImGuiChildFlags_FrameStyle flag for stylistic changes + displaying a label.
    // - If you don't need a label you can probably simply use BeginChild() with the UxImGuiChildFlags_FrameStyle flag for the same result.
    // - You can submit contents and manage your selection state however you want it, by creating e.g. Selectable() or any other items.
    // - The simplified/old ListBox() api are helpers over BeginListBox()/EndListBox() which are kept available for convenience purpose. This is analogous to how Combos are created.
    // - Choose frame width:   size.x > 0.0f: custom  /  size.x < 0.0f or -FLT_MIN: right-align   /  size.x = 0.0f (default): use current ItemWidth
    // - Choose frame height:  size.y > 0.0f: custom  /  size.y < 0.0f or -FLT_MIN: bottom-align  /  size.y = 0.0f (default): arbitrary default height which can fit ~7 items
    IMGUI_API bool          BeginListBox(const char* label, const UxImVec2& size = UxImVec2(0, 0)); // open a framed scrolling region
    IMGUI_API void          EndListBox();                                                       // only call EndListBox() if BeginListBox() returned true!
    IMGUI_API bool          ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items = -1);
    IMGUI_API bool          ListBox(const char* label, int* current_item, const char* (*getter)(void* user_data, int idx), void* user_data, int items_count, int height_in_items = -1);

    // Widgets: Data Plotting
    // - Consider using UxImPlot (https://github.com/epezent/implot) which is much better!
    IMGUI_API void          PlotLines(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, UxImVec2 graph_size = UxImVec2(0, 0), int stride = sizeof(float));
    IMGUI_API void          PlotLines(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, UxImVec2 graph_size = UxImVec2(0, 0));
    IMGUI_API void          PlotHistogram(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, UxImVec2 graph_size = UxImVec2(0, 0), int stride = sizeof(float));
    IMGUI_API void          PlotHistogram(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, UxImVec2 graph_size = UxImVec2(0, 0));

    // Widgets: Value() Helpers.
    // - Those are merely shortcut to calling Text() with a format string. Output single value in "name: value" format (tip: freely declare more in your code to handle your types. you can add functions to the UxImGui namespace)
    IMGUI_API void          Value(const char* prefix, bool b);
    IMGUI_API void          Value(const char* prefix, int v);
    IMGUI_API void          Value(const char* prefix, unsigned int v);
    IMGUI_API void          Value(const char* prefix, float v, const char* float_format = NULL);

    // Widgets: Menus
    // - Use BeginMenuBar() on a window UxImGuiWindowFlags_MenuBar to append to its menu bar.
    // - Use BeginMainMenuBar() to create a menu bar at the top of the screen and append to it.
    // - Use BeginMenu() to create a menu. You can call BeginMenu() multiple time with the same identifier to append more items to it.
    // - Not that MenuItem() keyboardshortcuts are displayed as a convenience but _not processed_ by Dear UxImGui at the moment.
    IMGUI_API bool          BeginMenuBar();                                                     // append to menu-bar of current window (requires UxImGuiWindowFlags_MenuBar flag set on parent window).
    IMGUI_API void          EndMenuBar();                                                       // only call EndMenuBar() if BeginMenuBar() returns true!
    IMGUI_API bool          BeginMainMenuBar();                                                 // create and append to a full screen menu-bar.
    IMGUI_API void          EndMainMenuBar();                                                   // only call EndMainMenuBar() if BeginMainMenuBar() returns true!
    IMGUI_API bool          BeginMenu(const char* label, bool enabled = true);                  // create a sub-menu entry. only call EndMenu() if this returns true!
    IMGUI_API void          EndMenu();                                                          // only call EndMenu() if BeginMenu() returns true!
    IMGUI_API bool          MenuItem(const char* label, const char* shortcut = NULL, bool selected = false, bool enabled = true);  // return true when activated.
    IMGUI_API bool          MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled = true);              // return true when activated + toggle (*p_selected) if p_selected != NULL

    // Tooltips
    // - Tooltips are windows following the mouse. They do not take focus away.
    // - A tooltip window can contain items of any types.
    // - SetTooltip() is more or less a shortcut for the 'if (BeginTooltip()) { Text(...); EndTooltip(); }' idiom (with a subtlety that it discard any previously submitted tooltip)
    IMGUI_API bool          BeginTooltip();                                                     // begin/append a tooltip window.
    IMGUI_API void          EndTooltip();                                                       // only call EndTooltip() if BeginTooltip()/BeginItemTooltip() returns true!
    IMGUI_API void          SetTooltip(const char* fmt, ...) IM_FMTARGS(1);                     // set a text-only tooltip. Often used after a UxImGui::IsItemHovered() check. Override any previous call to SetTooltip().
    IMGUI_API void          SetTooltipV(const char* fmt, va_list args) IM_FMTLIST(1);

    // Tooltips: helpers for showing a tooltip when hovering an item
    // - BeginItemTooltip() is a shortcut for the 'if (IsItemHovered(UxImGuiHoveredFlags_ForTooltip) && BeginTooltip())' idiom.
    // - SetItemTooltip() is a shortcut for the 'if (IsItemHovered(UxImGuiHoveredFlags_ForTooltip)) { SetTooltip(...); }' idiom.
    // - Where 'UxImGuiHoveredFlags_ForTooltip' itself is a shortcut to use 'style.HoverFlagsForTooltipMouse' or 'style.HoverFlagsForTooltipNav' depending on active input type. For mouse it defaults to 'UxImGuiHoveredFlags_Stationary | UxImGuiHoveredFlags_DelayShort'.
    IMGUI_API bool          BeginItemTooltip();                                                 // begin/append a tooltip window if preceding item was hovered.
    IMGUI_API void          SetItemTooltip(const char* fmt, ...) IM_FMTARGS(1);                 // set a text-only tooltip if preceding item was hovered. override any previous call to SetTooltip().
    IMGUI_API void          SetItemTooltipV(const char* fmt, va_list args) IM_FMTLIST(1);

    // Popups, Modals
    //  - They block normal mouse hovering detection (and therefore most mouse interactions) behind them.
    //  - If not modal: they can be closed by clicking anywhere outside them, or by pressing ESCAPE.
    //  - Their visibility state (~bool) is held internally instead of being held by the programmer as we are used to with regular Begin*() calls.
    //  - The 3 properties above are related: we need to retain popup visibility state in the library because popups may be closed as any time.
    //  - You can bypass the hovering restriction by using UxImGuiHoveredFlags_AllowWhenBlockedByPopup when calling IsItemHovered() or IsWindowHovered().
    //  - IMPORTANT: Popup identifiers are relative to the current ID stack, so OpenPopup and BeginPopup generally needs to be at the same level of the stack.
    //    This is sometimes leading to confusing mistakes. May rework this in the future.
    //  - BeginPopup(): query popup state, if open start appending into the window. Call EndPopup() afterwards if returned true. UxImGuiWindowFlags are forwarded to the window.
    //  - BeginPopupModal(): block every interaction behind the window, cannot be closed by user, add a dimming background, has a title bar.
    IMGUI_API bool          BeginPopup(const char* str_id, UxImGuiWindowFlags flags = 0);                         // return true if the popup is open, and you can start outputting to it.
    IMGUI_API bool          BeginPopupModal(const char* name, bool* p_open = NULL, UxImGuiWindowFlags flags = 0); // return true if the modal is open, and you can start outputting to it.
    IMGUI_API void          EndPopup();                                                                         // only call EndPopup() if BeginPopupXXX() returns true!

    // Popups: open/close functions
    //  - OpenPopup(): set popup state to open. UxImGuiPopupFlags are available for opening options.
    //  - If not modal: they can be closed by clicking anywhere outside them, or by pressing ESCAPE.
    //  - CloseCurrentPopup(): use inside the BeginPopup()/EndPopup() scope to close manually.
    //  - CloseCurrentPopup() is called by default by Selectable()/MenuItem() when activated (FIXME: need some options).
    //  - Use UxImGuiPopupFlags_NoOpenOverExistingPopup to avoid opening a popup if there's already one at the same level. This is equivalent to e.g. testing for !IsAnyPopupOpen() prior to OpenPopup().
    //  - Use IsWindowAppearing() after BeginPopup() to tell if a window just opened.
    //  - IMPORTANT: Notice that for OpenPopupOnItemClick() we exceptionally default flags to 1 (== UxImGuiPopupFlags_MouseButtonRight) for backward compatibility with older API taking 'int mouse_button = 1' parameter
    IMGUI_API void          OpenPopup(const char* str_id, UxImGuiPopupFlags popup_flags = 0);                     // call to mark popup as open (don't call every frame!).
    IMGUI_API void          OpenPopup(UxImGuiID id, UxImGuiPopupFlags popup_flags = 0);                             // id overload to facilitate calling from nested stacks
    IMGUI_API void          OpenPopupOnItemClick(const char* str_id = NULL, UxImGuiPopupFlags popup_flags = 1);   // helper to open popup when clicked on last item. Default to UxImGuiPopupFlags_MouseButtonRight == 1. (note: actually triggers on the mouse _released_ event to be consistent with popup behaviors)
    IMGUI_API void          CloseCurrentPopup();                                                                // manually close the popup we have begin-ed into.

    // Popups: open+begin combined functions helpers
    //  - Helpers to do OpenPopup+BeginPopup where the Open action is triggered by e.g. hovering an item and right-clicking.
    //  - They are convenient to easily create context menus, hence the name.
    //  - IMPORTANT: Notice that BeginPopupContextXXX takes UxImGuiPopupFlags just like OpenPopup() and unlike BeginPopup(). For full consistency, we may add UxImGuiWindowFlags to the BeginPopupContextXXX functions in the future.
    //  - IMPORTANT: Notice that we exceptionally default their flags to 1 (== UxImGuiPopupFlags_MouseButtonRight) for backward compatibility with older API taking 'int mouse_button = 1' parameter, so if you add other flags remember to re-add the UxImGuiPopupFlags_MouseButtonRight.
    IMGUI_API bool          BeginPopupContextItem(const char* str_id = NULL, UxImGuiPopupFlags popup_flags = 1);  // open+begin popup when clicked on last item. Use str_id==NULL to associate the popup to previous item. If you want to use that on a non-interactive item such as Text() you need to pass in an explicit ID here. read comments in .cpp!
    IMGUI_API bool          BeginPopupContextWindow(const char* str_id = NULL, UxImGuiPopupFlags popup_flags = 1);// open+begin popup when clicked on current window.
    IMGUI_API bool          BeginPopupContextVoid(const char* str_id = NULL, UxImGuiPopupFlags popup_flags = 1);  // open+begin popup when clicked in void (where there are no windows).

    // Popups: query functions
    //  - IsPopupOpen(): return true if the popup is open at the current BeginPopup() level of the popup stack.
    //  - IsPopupOpen() with UxImGuiPopupFlags_AnyPopupId: return true if any popup is open at the current BeginPopup() level of the popup stack.
    //  - IsPopupOpen() with UxImGuiPopupFlags_AnyPopupId + UxImGuiPopupFlags_AnyPopupLevel: return true if any popup is open.
    IMGUI_API bool          IsPopupOpen(const char* str_id, UxImGuiPopupFlags flags = 0);                         // return true if the popup is open.

    // Tables
    // - Full-featured replacement for old Columns API.
    // - See Demo->Tables for demo code. See top of imgui_tables.cpp for general commentary.
    // - See UxImGuiTableFlags_ and UxImGuiTableColumnFlags_ enums for a description of available flags.
    // The typical call flow is:
    // - 1. Call BeginTable(), early out if returning false.
    // - 2. Optionally call TableSetupColumn() to submit column name/flags/defaults.
    // - 3. Optionally call TableSetupScrollFreeze() to request scroll freezing of columns/rows.
    // - 4. Optionally call TableHeadersRow() to submit a header row. Names are pulled from TableSetupColumn() data.
    // - 5. Populate contents:
    //    - In most situations you can use TableNextRow() + TableSetColumnIndex(N) to start appending into a column.
    //    - If you are using tables as a sort of grid, where every column is holding the same type of contents,
    //      you may prefer using TableNextColumn() instead of TableNextRow() + TableSetColumnIndex().
    //      TableNextColumn() will automatically wrap-around into the next row if needed.
    //    - IMPORTANT: Comparatively to the old Columns() API, we need to call TableNextColumn() for the first column!
    //    - Summary of possible call flow:
    //        - TableNextRow() -> TableSetColumnIndex(0) -> Text("Hello 0") -> TableSetColumnIndex(1) -> Text("Hello 1")  // OK
    //        - TableNextRow() -> TableNextColumn()      -> Text("Hello 0") -> TableNextColumn()      -> Text("Hello 1")  // OK
    //        -                   TableNextColumn()      -> Text("Hello 0") -> TableNextColumn()      -> Text("Hello 1")  // OK: TableNextColumn() automatically gets to next row!
    //        - TableNextRow()                           -> Text("Hello 0")                                               // Not OK! Missing TableSetColumnIndex() or TableNextColumn()! Text will not appear!
    // - 5. Call EndTable()
    IMGUI_API bool          BeginTable(const char* str_id, int columns, UxImGuiTableFlags flags = 0, const UxImVec2& outer_size = UxImVec2(0.0f, 0.0f), float inner_width = 0.0f);
    IMGUI_API void          EndTable();                                         // only call EndTable() if BeginTable() returns true!
    IMGUI_API void          TableNextRow(UxImGuiTableRowFlags row_flags = 0, float min_row_height = 0.0f); // append into the first cell of a new row.
    IMGUI_API bool          TableNextColumn();                                  // append into the next column (or first column of next row if currently in last column). Return true when column is visible.
    IMGUI_API bool          TableSetColumnIndex(int column_n);                  // append into the specified column. Return true when column is visible.

    // Tables: Headers & Columns declaration
    // - Use TableSetupColumn() to specify label, resizing policy, default width/weight, id, various other flags etc.
    // - Use TableHeadersRow() to create a header row and automatically submit a TableHeader() for each column.
    //   Headers are required to perform: reordering, sorting, and opening the context menu.
    //   The context menu can also be made available in columns body using UxImGuiTableFlags_ContextMenuInBody.
    // - You may manually submit headers using TableNextRow() + TableHeader() calls, but this is only useful in
    //   some advanced use cases (e.g. adding custom widgets in header row).
    // - Use TableSetupScrollFreeze() to lock columns/rows so they stay visible when scrolled.
    IMGUI_API void          TableSetupColumn(const char* label, UxImGuiTableColumnFlags flags = 0, float init_width_or_weight = 0.0f, UxImGuiID user_id = 0);
    IMGUI_API void          TableSetupScrollFreeze(int cols, int rows);         // lock columns/rows so they stay visible when scrolled.
    IMGUI_API void          TableHeader(const char* label);                     // submit one header cell manually (rarely used)
    IMGUI_API void          TableHeadersRow();                                  // submit a row with headers cells based on data provided to TableSetupColumn() + submit context menu
    IMGUI_API void          TableAngledHeadersRow();                            // submit a row with angled headers for every column with the UxImGuiTableColumnFlags_AngledHeader flag. MUST BE FIRST ROW.

    // Tables: Sorting & Miscellaneous functions
    // - Sorting: call TableGetSortSpecs() to retrieve latest sort specs for the table. NULL when not sorting.
    //   When 'sort_specs->SpecsDirty == true' you should sort your data. It will be true when sorting specs have
    //   changed since last call, or the first time. Make sure to set 'SpecsDirty = false' after sorting,
    //   else you may wastefully sort your data every frame!
    // - Functions args 'int column_n' treat the default value of -1 as the same as passing the current column index.
    IMGUI_API UxImGuiTableSortSpecs*  TableGetSortSpecs();                        // get latest sort specs for the table (NULL if not sorting).  Lifetime: don't hold on this pointer over multiple frames or past any subsequent call to BeginTable().
    IMGUI_API int                   TableGetColumnCount();                      // return number of columns (value passed to BeginTable)
    IMGUI_API int                   TableGetColumnIndex();                      // return current column index.
    IMGUI_API int                   TableGetRowIndex();                         // return current row index.
    IMGUI_API const char*           TableGetColumnName(int column_n = -1);      // return "" if column didn't have a name declared by TableSetupColumn(). Pass -1 to use current column.
    IMGUI_API UxImGuiTableColumnFlags TableGetColumnFlags(int column_n = -1);     // return column flags so you can query their Enabled/Visible/Sorted/Hovered status flags. Pass -1 to use current column.
    IMGUI_API void                  TableSetColumnEnabled(int column_n, bool v);// change user accessible enabled/disabled state of a column. Set to false to hide the column. User can use the context menu to change this themselves (right-click in headers, or right-click in columns body with UxImGuiTableFlags_ContextMenuInBody)
    IMGUI_API int                   TableGetHoveredColumn();                    // return hovered column. return -1 when table is not hovered. return columns_count if the unused space at the right of visible columns is hovered. Can also use (TableGetColumnFlags() & UxImGuiTableColumnFlags_IsHovered) instead.
    IMGUI_API void                  TableSetBgColor(UxImGuiTableBgTarget target, UxImU32 color, int column_n = -1);  // change the color of a cell, row, or column. See UxImGuiTableBgTarget_ flags for details.

    // Legacy Columns API (prefer using Tables!)
    // - You can also use SameLine(pos_x) to mimic simplified columns.
    IMGUI_API void          Columns(int count = 1, const char* id = NULL, bool borders = true);
    IMGUI_API void          NextColumn();                                                       // next column, defaults to current row or next row if the current row is finished
    IMGUI_API int           GetColumnIndex();                                                   // get current column index
    IMGUI_API float         GetColumnWidth(int column_index = -1);                              // get column width (in pixels). pass -1 to use current column
    IMGUI_API void          SetColumnWidth(int column_index, float width);                      // set column width (in pixels). pass -1 to use current column
    IMGUI_API float         GetColumnOffset(int column_index = -1);                             // get position of column line (in pixels, from the left side of the contents region). pass -1 to use current column, otherwise 0..GetColumnsCount() inclusive. column 0 is typically 0.0f
    IMGUI_API void          SetColumnOffset(int column_index, float offset_x);                  // set position of column line (in pixels, from the left side of the contents region). pass -1 to use current column
    IMGUI_API int           GetColumnsCount();

    // Tab Bars, Tabs
    // - Note: Tabs are automatically created by the docking system (when in 'docking' branch). Use this to create tab bars/tabs yourself.
    IMGUI_API bool          BeginTabBar(const char* str_id, UxImGuiTabBarFlags flags = 0);        // create and append into a TabBar
    IMGUI_API void          EndTabBar();                                                        // only call EndTabBar() if BeginTabBar() returns true!
    IMGUI_API bool          BeginTabItem(const char* label, bool* p_open = NULL, UxImGuiTabItemFlags flags = 0); // create a Tab. Returns true if the Tab is selected.
    IMGUI_API void          EndTabItem();                                                       // only call EndTabItem() if BeginTabItem() returns true!
    IMGUI_API bool          TabItemButton(const char* label, UxImGuiTabItemFlags flags = 0);      // create a Tab behaving like a button. return true when clicked. cannot be selected in the tab bar.
    IMGUI_API void          SetTabItemClosed(const char* tab_or_docked_window_label);           // notify TabBar or Docking system of a closed tab/window ahead (useful to reduce visual flicker on reorderable tab bars). For tab-bar: call after BeginTabBar() and before Tab submissions. Otherwise call with a window name.

    // Docking
    // [BETA API] Enable with io.ConfigFlags |= UxImGuiConfigFlags_DockingEnable.
    // Note: You can use most Docking facilities without calling any API. You DO NOT need to call DockSpace() to use Docking!
    // - Drag from window title bar or their tab to dock/undock. Hold SHIFT to disable docking.
    // - Drag from window menu button (upper-left button) to undock an entire node (all windows).
    // - When io.ConfigDockingWithShift == true, you instead need to hold SHIFT to enable docking.
    // About dockspaces:
    // - Use DockSpaceOverViewport() to create a window covering the screen or a specific viewport + a dockspace inside it.
    //   This is often used with UxImGuiDockNodeFlags_PassthruCentralNode to make it transparent.
    // - Use DockSpace() to create an explicit dock node _within_ an existing window. See Docking demo for details.
    // - Important: Dockspaces need to be submitted _before_ any window they can host. Submit it early in your frame!
    // - Important: Dockspaces need to be kept alive if hidden, otherwise windows docked into it will be undocked.
    //   e.g. if you have multiple tabs with a dockspace inside each tab: submit the non-visible dockspaces with UxImGuiDockNodeFlags_KeepAliveOnly.
    IMGUI_API UxImGuiID       DockSpace(UxImGuiID dockspace_id, const UxImVec2& size = UxImVec2(0, 0), UxImGuiDockNodeFlags flags = 0, const UxImGuiWindowClass* window_class = NULL);
    IMGUI_API UxImGuiID       DockSpaceOverViewport(UxImGuiID dockspace_id = 0, const UxImGuiViewport* viewport = NULL, UxImGuiDockNodeFlags flags = 0, const UxImGuiWindowClass* window_class = NULL);
    IMGUI_API void          SetNextWindowDockID(UxImGuiID dock_id, UxImGuiCond cond = 0);           // set next window dock id
    IMGUI_API void          SetNextWindowClass(const UxImGuiWindowClass* window_class);           // set next window class (control docking compatibility + provide hints to platform backend via custom viewport flags and platform parent/child relationship)
    IMGUI_API UxImGuiID       GetWindowDockID();
    IMGUI_API bool          IsWindowDocked();                                                   // is current window docked into another window?

    // Logging/Capture
    // - All text output from the interface can be captured into tty/file/clipboard. By default, tree nodes are automatically opened during logging.
    IMGUI_API void          LogToTTY(int auto_open_depth = -1);                                 // start logging to tty (stdout)
    IMGUI_API void          LogToFile(int auto_open_depth = -1, const char* filename = NULL);   // start logging to file
    IMGUI_API void          LogToClipboard(int auto_open_depth = -1);                           // start logging to OS clipboard
    IMGUI_API void          LogFinish();                                                        // stop logging (close file, etc.)
    IMGUI_API void          LogButtons();                                                       // helper to display buttons for logging to tty/file/clipboard
    IMGUI_API void          LogText(const char* fmt, ...) IM_FMTARGS(1);                        // pass text data straight to log (without being displayed)
    IMGUI_API void          LogTextV(const char* fmt, va_list args) IM_FMTLIST(1);

    // Drag and Drop
    // - On source items, call BeginDragDropSource(), if it returns true also call SetDragDropPayload() + EndDragDropSource().
    // - On target candidates, call BeginDragDropTarget(), if it returns true also call AcceptDragDropPayload() + EndDragDropTarget().
    // - If you stop calling BeginDragDropSource() the payload is preserved however it won't have a preview tooltip (we currently display a fallback "..." tooltip, see #1725)
    // - An item can be both drag source and drop target.
    IMGUI_API bool          BeginDragDropSource(UxImGuiDragDropFlags flags = 0);                                      // call after submitting an item which may be dragged. when this return true, you can call SetDragDropPayload() + EndDragDropSource()
    IMGUI_API bool          SetDragDropPayload(const char* type, const void* data, size_t sz, UxImGuiCond cond = 0);  // type is a user defined string of maximum 32 characters. Strings starting with '_' are reserved for dear imgui internal types. Data is copied and held by imgui. Return true when payload has been accepted.
    IMGUI_API void          EndDragDropSource();                                                                    // only call EndDragDropSource() if BeginDragDropSource() returns true!
    IMGUI_API bool                  BeginDragDropTarget();                                                          // call after submitting an item that may receive a payload. If this returns true, you can call AcceptDragDropPayload() + EndDragDropTarget()
    IMGUI_API const UxImGuiPayload*   AcceptDragDropPayload(const char* type, UxImGuiDragDropFlags flags = 0);          // accept contents of a given type. If UxImGuiDragDropFlags_AcceptBeforeDelivery is set you can peek into the payload before the mouse button is released.
    IMGUI_API void                  EndDragDropTarget();                                                            // only call EndDragDropTarget() if BeginDragDropTarget() returns true!
    IMGUI_API const UxImGuiPayload*   GetDragDropPayload();                                                           // peek directly into the current payload from anywhere. returns NULL when drag and drop is finished or inactive. use UxImGuiPayload::IsDataType() to test for the payload type.

    // Disabling [BETA API]
    // - Disable all user interactions and dim items visuals (applying style.DisabledAlpha over current colors)
    // - Those can be nested but it cannot be used to enable an already disabled section (a single BeginDisabled(true) in the stack is enough to keep everything disabled)
    // - Tooltips windows by exception are opted out of disabling.
    // - BeginDisabled(false)/EndDisabled() essentially does nothing but is provided to facilitate use of boolean expressions (as a micro-optimization: if you have tens of thousands of BeginDisabled(false)/EndDisabled() pairs, you might want to reformulate your code to avoid making those calls)
    IMGUI_API void          BeginDisabled(bool disabled = true);
    IMGUI_API void          EndDisabled();

    // Clipping
    // - Mouse hovering is affected by UxImGui::PushClipRect() calls, unlike direct calls to UxImDrawList::PushClipRect() which are render only.
    IMGUI_API void          PushClipRect(const UxImVec2& clip_rect_min, const UxImVec2& clip_rect_max, bool intersect_with_current_clip_rect);
    IMGUI_API void          PopClipRect();

    // Focus, Activation
    IMGUI_API void          SetItemDefaultFocus();                                              // make last item the default focused item of a newly appearing window.
    IMGUI_API void          SetKeyboardFocusHere(int offset = 0);                               // focus keyboard on the next widget. Use positive 'offset' to access sub components of a multiple component widget. Use -1 to access previous widget.

    // Keyboard/Gamepad Navigation
    IMGUI_API void          SetNavCursorVisible(bool visible);                                  // alter visibility of keyboard/gamepad cursor. by default: show when using an arrow key, hide when clicking with mouse.

    // Overlapping mode
    IMGUI_API void          SetNextItemAllowOverlap();                                          // allow next item to be overlapped by a subsequent item. Useful with invisible buttons, selectable, treenode covering an area where subsequent items may need to be added. Note that both Selectable() and TreeNode() have dedicated flags doing this.

    // Item/Widgets Utilities and Query Functions
    // - Most of the functions are referring to the previous Item that has been submitted.
    // - See Demo Window under "Widgets->Querying Status" for an interactive visualization of most of those functions.
    IMGUI_API bool          IsItemHovered(UxImGuiHoveredFlags flags = 0);                         // is the last item hovered? (and usable, aka not blocked by a popup, etc.). See UxImGuiHoveredFlags for more options.
    IMGUI_API bool          IsItemActive();                                                     // is the last item active? (e.g. button being held, text field being edited. This will continuously return true while holding mouse button on an item. Items that don't interact will always return false)
    IMGUI_API bool          IsItemFocused();                                                    // is the last item focused for keyboard/gamepad navigation?
    IMGUI_API bool          IsItemClicked(UxImGuiMouseButton mouse_button = 0);                   // is the last item hovered and mouse clicked on? (**)  == IsMouseClicked(mouse_button) && IsItemHovered()Important. (**) this is NOT equivalent to the behavior of e.g. Button(). Read comments in function definition.
    IMGUI_API bool          IsItemVisible();                                                    // is the last item visible? (items may be out of sight because of clipping/scrolling)
    IMGUI_API bool          IsItemEdited();                                                     // did the last item modify its underlying value this frame? or was pressed? This is generally the same as the "bool" return value of many widgets.
    IMGUI_API bool          IsItemActivated();                                                  // was the last item just made active (item was previously inactive).
    IMGUI_API bool          IsItemDeactivated();                                                // was the last item just made inactive (item was previously active). Useful for Undo/Redo patterns with widgets that require continuous editing.
    IMGUI_API bool          IsItemDeactivatedAfterEdit();                                       // was the last item just made inactive and made a value change when it was active? (e.g. Slider/Drag moved). Useful for Undo/Redo patterns with widgets that require continuous editing. Note that you may get false positives (some widgets such as Combo()/ListBox()/Selectable() will return true even when clicking an already selected item).
    IMGUI_API bool          IsItemToggledOpen();                                                // was the last item open state toggled? set by TreeNode().
    IMGUI_API bool          IsAnyItemHovered();                                                 // is any item hovered?
    IMGUI_API bool          IsAnyItemActive();                                                  // is any item active?
    IMGUI_API bool          IsAnyItemFocused();                                                 // is any item focused?
    IMGUI_API UxImGuiID       GetItemID();                                                        // get ID of last item (~~ often same UxImGui::GetID(label) beforehand)
    IMGUI_API UxImVec2        GetItemRectMin();                                                   // get upper-left bounding rectangle of the last item (screen space)
    IMGUI_API UxImVec2        GetItemRectMax();                                                   // get lower-right bounding rectangle of the last item (screen space)
    IMGUI_API UxImVec2        GetItemRectSize();                                                  // get size of last item

    // Viewports
    // - Currently represents the Platform Window created by the application which is hosting our Dear UxImGui windows.
    // - In 'docking' branch with multi-viewport enabled, we extend this concept to have multiple active viewports.
    // - In the future we will extend this concept further to also represent Platform Monitor and support a "no main platform window" operation mode.
    IMGUI_API UxImGuiViewport* GetMainViewport();                                                 // return primary/default viewport. This can never be NULL.

    // Background/Foreground Draw Lists
    IMGUI_API UxImDrawList*   GetBackgroundDrawList(UxImGuiViewport* viewport = NULL);              // get background draw list for the given viewport or viewport associated to the current window. this draw list will be the first rendering one. Useful to quickly draw shapes/text behind dear imgui contents.
    IMGUI_API UxImDrawList*   GetForegroundDrawList(UxImGuiViewport* viewport = NULL);              // get foreground draw list for the given viewport or viewport associated to the current window. this draw list will be the top-most rendered one. Useful to quickly draw shapes/text over dear imgui contents.

    // Miscellaneous Utilities
    IMGUI_API bool          IsRectVisible(const UxImVec2& size);                                  // test if rectangle (of given size, starting from cursor position) is visible / not clipped.
    IMGUI_API bool          IsRectVisible(const UxImVec2& rect_min, const UxImVec2& rect_max);      // test if rectangle (in screen space) is visible / not clipped. to perform coarse clipping on user's side.
    IMGUI_API double        GetTime();                                                          // get global imgui time. incremented by io.DeltaTime every frame.
    IMGUI_API int           GetFrameCount();                                                    // get global imgui frame count. incremented by 1 every frame.
    IMGUI_API UxImDrawListSharedData* GetDrawListSharedData();                                    // you may use this when creating your own UxImDrawList instances.
    IMGUI_API const char*   GetStyleColorName(UxImGuiCol idx);                                    // get a string corresponding to the enum value (for display, saving, etc.).
    IMGUI_API void          SetStateStorage(UxImGuiStorage* storage);                             // replace current window storage with our own (if you want to manipulate it yourself, typically clear subsection of it)
    IMGUI_API UxImGuiStorage* GetStateStorage();

    // Text Utilities
    IMGUI_API UxImVec2        CalcTextSize(const char* text, const char* text_end = NULL, bool hide_text_after_double_hash = false, float wrap_width = -1.0f);

    // Color Utilities
    IMGUI_API UxImVec4        ColorConvertU32ToFloat4(UxImU32 in);
    IMGUI_API UxImU32         ColorConvertFloat4ToU32(const UxImVec4& in);
    IMGUI_API void          ColorConvertRGBtoHSV(float r, float g, float b, float& out_h, float& out_s, float& out_v);
    IMGUI_API void          ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g, float& out_b);

    // Inputs Utilities: Keyboard/Mouse/Gamepad
    // - the UxImGuiKey enum contains all possible keyboard, mouse and gamepad inputs (e.g. UxImGuiKey_A, UxImGuiKey_MouseLeft, UxImGuiKey_GamepadDpadUp...).
    // - (legacy: before v1.87, we used UxImGuiKey to carry native/user indices as defined by each backends. This was obsoleted in 1.87 (2022-02) and completely removed in 1.91.5 (2024-11). See https://github.com/ocornut/imgui/issues/4921)
    // - (legacy: any use of UxImGuiKey will assert when key < 512 to detect passing legacy native/user indices)
    IMGUI_API bool          IsKeyDown(UxImGuiKey key);                                            // is key being held.
    IMGUI_API bool          IsKeyPressed(UxImGuiKey key, bool repeat = true);                     // was key pressed (went from !Down to Down)? if repeat=true, uses io.KeyRepeatDelay / KeyRepeatRate
    IMGUI_API bool          IsKeyReleased(UxImGuiKey key);                                        // was key released (went from Down to !Down)?
    IMGUI_API bool          IsKeyChordPressed(UxImGuiKeyChord key_chord);                         // was key chord (mods + key) pressed, e.g. you can pass 'UxImGuiMod_Ctrl | UxImGuiKey_S' as a key-chord. This doesn't do any routing or focus check, please consider using Shortcut() function instead.
    IMGUI_API int           GetKeyPressedAmount(UxImGuiKey key, float repeat_delay, float rate);  // uses provided repeat rate/delay. return a count, most often 0 or 1 but might be >1 if RepeatRate is small enough that DeltaTime > RepeatRate
    IMGUI_API const char*   GetKeyName(UxImGuiKey key);                                           // [DEBUG] returns English name of the key. Those names are provided for debugging purpose and are not meant to be saved persistently nor compared.
    IMGUI_API void          SetNextFrameWantCaptureKeyboard(bool want_capture_keyboard);        // Override io.WantCaptureKeyboard flag next frame (said flag is left for your application to handle, typically when true it instructs your app to ignore inputs). e.g. force capture keyboard when your widget is being hovered. This is equivalent to setting "io.WantCaptureKeyboard = want_capture_keyboard"; after the next NewFrame() call.

    // Inputs Utilities: Shortcut Testing & Routing [BETA]
    // - UxImGuiKeyChord = a UxImGuiKey + optional UxImGuiMod_Alt/UxImGuiMod_Ctrl/UxImGuiMod_Shift/UxImGuiMod_Super.
    //       UxImGuiKey_C                          // Accepted by functions taking UxImGuiKey or UxImGuiKeyChord arguments)
    //       UxImGuiMod_Ctrl | UxImGuiKey_C          // Accepted by functions taking UxImGuiKeyChord arguments)
    //   only UxImGuiMod_XXX values are legal to combine with an UxImGuiKey. You CANNOT combine two UxImGuiKey values.
    // - The general idea is that several callers may register interest in a shortcut, and only one owner gets it.
    //      Parent   -> call Shortcut(Ctrl+S)    // When Parent is focused, Parent gets the shortcut.
    //        Child1 -> call Shortcut(Ctrl+S)    // When Child1 is focused, Child1 gets the shortcut (Child1 overrides Parent shortcuts)
    //        Child2 -> no call                  // When Child2 is focused, Parent gets the shortcut.
    //   The whole system is order independent, so if Child1 makes its calls before Parent, results will be identical.
    //   This is an important property as it facilitate working with foreign code or larger codebase.
    // - To understand the difference:
    //   - IsKeyChordPressed() compares mods and call IsKeyPressed() -> function has no side-effect.
    //   - Shortcut() submits a route, routes are resolved, if it currently can be routed it calls IsKeyChordPressed() -> function has (desirable) side-effects as it can prevents another call from getting the route.
    // - Visualize registered routes in 'Metrics/Debugger->Inputs'.
    IMGUI_API bool          Shortcut(UxImGuiKeyChord key_chord, UxImGuiInputFlags flags = 0);
    IMGUI_API void          SetNextItemShortcut(UxImGuiKeyChord key_chord, UxImGuiInputFlags flags = 0);

    // Inputs Utilities: Key/Input Ownership [BETA]
    // - One common use case would be to allow your items to disable standard inputs behaviors such
    //   as Tab or Alt key handling, Mouse Wheel scrolling, etc.
    //   e.g. Button(...); SetItemKeyOwner(UxImGuiKey_MouseWheelY); to make hovering/activating a button disable wheel for scrolling.
    // - Reminder UxImGuiKey enum include access to mouse buttons and gamepad, so key ownership can apply to them.
    // - Many related features are still in imgui_internal.h. For instance, most IsKeyXXX()/IsMouseXXX() functions have an owner-id-aware version.
    IMGUI_API void          SetItemKeyOwner(UxImGuiKey key);                                      // Set key owner to last item ID if it is hovered or active. Equivalent to 'if (IsItemHovered() || IsItemActive()) { SetKeyOwner(key, GetItemID());'.

    // Inputs Utilities: Mouse
    // - To refer to a mouse button, you may use named enums in your code e.g. UxImGuiMouseButton_Left, UxImGuiMouseButton_Right.
    // - You can also use regular integer: it is forever guaranteed that 0=Left, 1=Right, 2=Middle.
    // - Dragging operations are only reported after mouse has moved a certain distance away from the initial clicking position (see 'lock_threshold' and 'io.MouseDraggingThreshold')
    IMGUI_API bool          IsMouseDown(UxImGuiMouseButton button);                               // is mouse button held?
    IMGUI_API bool          IsMouseClicked(UxImGuiMouseButton button, bool repeat = false);       // did mouse button clicked? (went from !Down to Down). Same as GetMouseClickedCount() == 1.
    IMGUI_API bool          IsMouseReleased(UxImGuiMouseButton button);                           // did mouse button released? (went from Down to !Down)
    IMGUI_API bool          IsMouseDoubleClicked(UxImGuiMouseButton button);                      // did mouse button double-clicked? Same as GetMouseClickedCount() == 2. (note that a double-click will also report IsMouseClicked() == true)
    IMGUI_API bool          IsMouseReleasedWithDelay(UxImGuiMouseButton button, float delay);     // delayed mouse release (use very sparingly!). Generally used with 'delay >= io.MouseDoubleClickTime' + combined with a 'io.MouseClickedLastCount==1' test. This is a very rarely used UI idiom, but some apps use this: e.g. MS Explorer single click on an icon to rename.
    IMGUI_API int           GetMouseClickedCount(UxImGuiMouseButton button);                      // return the number of successive mouse-clicks at the time where a click happen (otherwise 0).
    IMGUI_API bool          IsMouseHoveringRect(const UxImVec2& r_min, const UxImVec2& r_max, bool clip = true);// is mouse hovering given bounding rect (in screen space). clipped by current clipping settings, but disregarding of other consideration of focus/window ordering/popup-block.
    IMGUI_API bool          IsMousePosValid(const UxImVec2* mouse_pos = NULL);                    // by convention we use (-FLT_MAX,-FLT_MAX) to denote that there is no mouse available
    IMGUI_API bool          IsAnyMouseDown();                                                   // [WILL OBSOLETE] is any mouse button held? This was designed for backends, but prefer having backend maintain a mask of held mouse buttons, because upcoming input queue system will make this invalid.
    IMGUI_API UxImVec2        GetMousePos();                                                      // shortcut to UxImGui::GetIO().MousePos provided by user, to be consistent with other calls
    IMGUI_API UxImVec2        GetMousePosOnOpeningCurrentPopup();                                 // retrieve mouse position at the time of opening popup we have BeginPopup() into (helper to avoid user backing that value themselves)
    IMGUI_API bool          IsMouseDragging(UxImGuiMouseButton button, float lock_threshold = -1.0f);         // is mouse dragging? (uses io.MouseDraggingThreshold if lock_threshold < 0.0f)
    IMGUI_API UxImVec2        GetMouseDragDelta(UxImGuiMouseButton button = 0, float lock_threshold = -1.0f);   // return the delta from the initial clicking position while the mouse button is pressed or was just released. This is locked and return 0.0f until the mouse moves past a distance threshold at least once (uses io.MouseDraggingThreshold if lock_threshold < 0.0f)
    IMGUI_API void          ResetMouseDragDelta(UxImGuiMouseButton button = 0);                   //
    IMGUI_API UxImGuiMouseCursor GetMouseCursor();                                                // get desired mouse cursor shape. Important: reset in UxImGui::NewFrame(), this is updated during the frame. valid before Render(). If you use software rendering by setting io.MouseDrawCursor UxImGui will render those for you
    IMGUI_API void          SetMouseCursor(UxImGuiMouseCursor cursor_type);                       // set desired mouse cursor shape
    IMGUI_API void          SetNextFrameWantCaptureMouse(bool want_capture_mouse);              // Override io.WantCaptureMouse flag next frame (said flag is left for your application to handle, typical when true it instructs your app to ignore inputs). This is equivalent to setting "io.WantCaptureMouse = want_capture_mouse;" after the next NewFrame() call.

    // Clipboard Utilities
    // - Also see the LogToClipboard() function to capture GUI into clipboard, or easily output text data to the clipboard.
    IMGUI_API const char*   GetClipboardText();
    IMGUI_API void          SetClipboardText(const char* text);

    // Settings/.Ini Utilities
    // - The disk functions are automatically called if io.IniFilename != NULL (default is "imgui.ini").
    // - Set io.IniFilename to NULL to load/save manually. Read io.WantSaveIniSettings description about handling .ini saving manually.
    // - Important: default value "imgui.ini" is relative to current working dir! Most apps will want to lock this to an absolute path (e.g. same path as executables).
    IMGUI_API void          LoadIniSettingsFromDisk(const char* ini_filename);                  // call after CreateContext() and before the first call to NewFrame(). NewFrame() automatically calls LoadIniSettingsFromDisk(io.IniFilename).
    IMGUI_API void          LoadIniSettingsFromMemory(const char* ini_data, size_t ini_size=0); // call after CreateContext() and before the first call to NewFrame() to provide .ini data from your own data source.
    IMGUI_API void          SaveIniSettingsToDisk(const char* ini_filename);                    // this is automatically called (if io.IniFilename is not empty) a few seconds after any modification that should be reflected in the .ini file (and also by DestroyContext).
    IMGUI_API const char*   SaveIniSettingsToMemory(size_t* out_ini_size = NULL);               // return a zero-terminated string with the .ini data which you can save by your own mean. call when io.WantSaveIniSettings is set, then save data by your own mean and clear io.WantSaveIniSettings.

    // Debug Utilities
    // - Your main debugging friend is the ShowMetricsWindow() function, which is also accessible from Demo->Tools->Metrics Debugger
    IMGUI_API void          DebugTextEncoding(const char* text);
    IMGUI_API void          DebugFlashStyleColor(UxImGuiCol idx);
    IMGUI_API void          DebugStartItemPicker();
    IMGUI_API bool          DebugCheckVersionAndDataLayout(const char* version_str, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_drawvert, size_t sz_drawidx); // This is called by IMGUI_CHECKVERSION() macro.
#ifndef IMGUI_DISABLE_DEBUG_TOOLS
    IMGUI_API void          DebugLog(const char* fmt, ...)           IM_FMTARGS(1); // Call via IMGUI_DEBUG_LOG() for maximum stripping in caller code!
    IMGUI_API void          DebugLogV(const char* fmt, va_list args) IM_FMTLIST(1);
#endif

    // Memory Allocators
    // - Those functions are not reliant on the current context.
    // - DLL users: heaps and globals are not shared across DLL boundaries! You will need to call SetCurrentContext() + SetAllocatorFunctions()
    //   for each static/DLL boundary you are calling from. Read "Context and Memory Allocators" section of imgui.cpp for more details.
    IMGUI_API void          SetAllocatorFunctions(UxImGuiMemAllocFunc alloc_func, UxImGuiMemFreeFunc free_func, void* user_data = NULL);
    IMGUI_API void          GetAllocatorFunctions(UxImGuiMemAllocFunc* p_alloc_func, UxImGuiMemFreeFunc* p_free_func, void** p_user_data);
    IMGUI_API void*         MemAlloc(size_t size);
    IMGUI_API void          MemFree(void* ptr);

    // (Optional) Platform/OS interface for multi-viewport support
    // Read comments around the UxImGuiPlatformIO structure for more details.
    // Note: You may use GetWindowViewport() to get the current viewport of the current window.
    IMGUI_API void          UpdatePlatformWindows();                                // call in main loop. will call CreateWindow/ResizeWindow/etc. platform functions for each secondary viewport, and DestroyWindow for each inactive viewport.
    IMGUI_API void          RenderPlatformWindowsDefault(void* platform_render_arg = NULL, void* renderer_render_arg = NULL); // call in main loop. will call RenderWindow/SwapBuffers platform functions for each secondary viewport which doesn't have the UxImGuiViewportFlags_Minimized flag set. May be reimplemented by user for custom rendering needs.
    IMGUI_API void          DestroyPlatformWindows();                               // call DestroyWindow platform functions for all viewports. call from backend Shutdown() if you need to close platform windows before imgui shutdown. otherwise will be called by DestroyContext().
    IMGUI_API UxImGuiViewport* FindViewportByID(UxImGuiID id);                          // this is a helper for backends.
    IMGUI_API UxImGuiViewport* FindViewportByPlatformHandle(void* platform_handle);   // this is a helper for backends. the type platform_handle is decided by the backend (e.g. HWND, MyWindow*, GLFWwindow* etc.)

} // namespace UxImGui

//-----------------------------------------------------------------------------
// [SECTION] Flags & Enumerations
//-----------------------------------------------------------------------------

// Flags for UxImGui::Begin()
// (Those are per-window flags. There are shared flags in UxImGuiIO: io.ConfigWindowsResizeFromEdges and io.ConfigWindowsMoveFromTitleBarOnly)
enum UxImGuiWindowFlags_
{
    UxImGuiWindowFlags_None                   = 0,
    UxImGuiWindowFlags_NoTitleBar             = 1 << 0,   // Disable title-bar
    UxImGuiWindowFlags_NoResize               = 1 << 1,   // Disable user resizing with the lower-right grip
    UxImGuiWindowFlags_NoMove                 = 1 << 2,   // Disable user moving the window
    UxImGuiWindowFlags_NoScrollbar            = 1 << 3,   // Disable scrollbars (window can still scroll with mouse or programmatically)
    UxImGuiWindowFlags_NoScrollWithMouse      = 1 << 4,   // Disable user vertically scrolling with mouse wheel. On child window, mouse wheel will be forwarded to the parent unless NoScrollbar is also set.
    UxImGuiWindowFlags_NoCollapse             = 1 << 5,   // Disable user collapsing window by double-clicking on it. Also referred to as Window Menu Button (e.g. within a docking node).
    UxImGuiWindowFlags_AlwaysAutoResize       = 1 << 6,   // Resize every window to its content every frame
    UxImGuiWindowFlags_NoBackground           = 1 << 7,   // Disable drawing background color (WindowBg, etc.) and outside border. Similar as using SetNextWindowBgAlpha(0.0f).
    UxImGuiWindowFlags_NoSavedSettings        = 1 << 8,   // Never load/save settings in .ini file
    UxImGuiWindowFlags_NoMouseInputs          = 1 << 9,   // Disable catching mouse, hovering test with pass through.
    UxImGuiWindowFlags_MenuBar                = 1 << 10,  // Has a menu-bar
    UxImGuiWindowFlags_HorizontalScrollbar    = 1 << 11,  // Allow horizontal scrollbar to appear (off by default). You may use SetNextWindowContentSize(UxImVec2(width,0.0f)); prior to calling Begin() to specify width. Read code in imgui_demo in the "Horizontal Scrolling" section.
    UxImGuiWindowFlags_NoFocusOnAppearing     = 1 << 12,  // Disable taking focus when transitioning from hidden to visible state
    UxImGuiWindowFlags_NoBringToFrontOnFocus  = 1 << 13,  // Disable bringing window to front when taking focus (e.g. clicking on it or programmatically giving it focus)
    UxImGuiWindowFlags_AlwaysVerticalScrollbar= 1 << 14,  // Always show vertical scrollbar (even if ContentSize.y < Size.y)
    UxImGuiWindowFlags_AlwaysHorizontalScrollbar=1<< 15,  // Always show horizontal scrollbar (even if ContentSize.x < Size.x)
    UxImGuiWindowFlags_NoNavInputs            = 1 << 16,  // No keyboard/gamepad navigation within the window
    UxImGuiWindowFlags_NoNavFocus             = 1 << 17,  // No focusing toward this window with keyboard/gamepad navigation (e.g. skipped by CTRL+TAB)
    UxImGuiWindowFlags_UnsavedDocument        = 1 << 18,  // Display a dot next to the title. When used in a tab/docking context, tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
    UxImGuiWindowFlags_NoDocking              = 1 << 19,  // Disable docking of this window
    UxImGuiWindowFlags_NoNav                  = UxImGuiWindowFlags_NoNavInputs | UxImGuiWindowFlags_NoNavFocus,
    UxImGuiWindowFlags_NoDecoration           = UxImGuiWindowFlags_NoTitleBar | UxImGuiWindowFlags_NoResize | UxImGuiWindowFlags_NoScrollbar | UxImGuiWindowFlags_NoCollapse,
    UxImGuiWindowFlags_NoInputs               = UxImGuiWindowFlags_NoMouseInputs | UxImGuiWindowFlags_NoNavInputs | UxImGuiWindowFlags_NoNavFocus,

    // [Internal]
    UxImGuiWindowFlags_DockNodeHost           = 1 << 23,  // Don't use! For internal use by Begin()/NewFrame()
    UxImGuiWindowFlags_ChildWindow            = 1 << 24,  // Don't use! For internal use by BeginChild()
    UxImGuiWindowFlags_Tooltip                = 1 << 25,  // Don't use! For internal use by BeginTooltip()
    UxImGuiWindowFlags_Popup                  = 1 << 26,  // Don't use! For internal use by BeginPopup()
    UxImGuiWindowFlags_Modal                  = 1 << 27,  // Don't use! For internal use by BeginPopupModal()
    UxImGuiWindowFlags_ChildMenu              = 1 << 28,  // Don't use! For internal use by BeginMenu()

    // Obsolete names
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiWindowFlags_NavFlattened           = 1 << 29,  // Obsoleted in 1.90.9: Use UxImGuiChildFlags_NavFlattened in BeginChild() call.
    UxImGuiWindowFlags_AlwaysUseWindowPadding = 1 << 30,  // Obsoleted in 1.90.0: Use UxImGuiChildFlags_AlwaysUseWindowPadding in BeginChild() call.
#endif
};

// Flags for UxImGui::BeginChild()
// (Legacy: bit 0 must always correspond to UxImGuiChildFlags_Borders to be backward compatible with old API using 'bool border = false'.
// About using AutoResizeX/AutoResizeY flags:
// - May be combined with SetNextWindowSizeConstraints() to set a min/max size for each axis (see "Demo->Child->Auto-resize with Constraints").
// - Size measurement for a given axis is only performed when the child window is within visible boundaries, or is just appearing.
//   - This allows BeginChild() to return false when not within boundaries (e.g. when scrolling), which is more optimal. BUT it won't update its auto-size while clipped.
//     While not perfect, it is a better default behavior as the always-on performance gain is more valuable than the occasional "resizing after becoming visible again" glitch.
//   - You may also use UxImGuiChildFlags_AlwaysAutoResize to force an update even when child window is not in view.
//     HOWEVER PLEASE UNDERSTAND THAT DOING SO WILL PREVENT BeginChild() FROM EVER RETURNING FALSE, disabling benefits of coarse clipping.
enum UxImGuiChildFlags_
{
    UxImGuiChildFlags_None                    = 0,
    UxImGuiChildFlags_Borders                 = 1 << 0,   // Show an outer border and enable WindowPadding. (IMPORTANT: this is always == 1 == true for legacy reason)
    UxImGuiChildFlags_AlwaysUseWindowPadding  = 1 << 1,   // Pad with style.WindowPadding even if no border are drawn (no padding by default for non-bordered child windows because it makes more sense)
    UxImGuiChildFlags_ResizeX                 = 1 << 2,   // Allow resize from right border (layout direction). Enable .ini saving (unless UxImGuiWindowFlags_NoSavedSettings passed to window flags)
    UxImGuiChildFlags_ResizeY                 = 1 << 3,   // Allow resize from bottom border (layout direction). "
    UxImGuiChildFlags_AutoResizeX             = 1 << 4,   // Enable auto-resizing width. Read "IMPORTANT: Size measurement" details above.
    UxImGuiChildFlags_AutoResizeY             = 1 << 5,   // Enable auto-resizing height. Read "IMPORTANT: Size measurement" details above.
    UxImGuiChildFlags_AlwaysAutoResize        = 1 << 6,   // Combined with AutoResizeX/AutoResizeY. Always measure size even when child is hidden, always return true, always disable clipping optimization! NOT RECOMMENDED.
    UxImGuiChildFlags_FrameStyle              = 1 << 7,   // Style the child window like a framed item: use FrameBg, FrameRounding, FrameBorderSize, FramePadding instead of ChildBg, ChildRounding, ChildBorderSize, WindowPadding.
    UxImGuiChildFlags_NavFlattened            = 1 << 8,   // [BETA] Share focus scope, allow keyboard/gamepad navigation to cross over parent border to this child or between sibling child windows.

    // Obsolete names
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiChildFlags_Border                  = UxImGuiChildFlags_Borders,  // Renamed in 1.91.1 (August 2024) for consistency.
#endif
};

// Flags for UxImGui::PushItemFlag()
// (Those are shared by all items)
enum UxImGuiItemFlags_
{
    UxImGuiItemFlags_None                     = 0,        // (Default)
    UxImGuiItemFlags_NoTabStop                = 1 << 0,   // false    // Disable keyboard tabbing. This is a "lighter" version of UxImGuiItemFlags_NoNav.
    UxImGuiItemFlags_NoNav                    = 1 << 1,   // false    // Disable any form of focusing (keyboard/gamepad directional navigation and SetKeyboardFocusHere() calls).
    UxImGuiItemFlags_NoNavDefaultFocus        = 1 << 2,   // false    // Disable item being a candidate for default focus (e.g. used by title bar items).
    UxImGuiItemFlags_ButtonRepeat             = 1 << 3,   // false    // Any button-like behavior will have repeat mode enabled (based on io.KeyRepeatDelay and io.KeyRepeatRate values). Note that you can also call IsItemActive() after any button to tell if it is being held.
    UxImGuiItemFlags_AutoClosePopups          = 1 << 4,   // true     // MenuItem()/Selectable() automatically close their parent popup window.
    UxImGuiItemFlags_AllowDuplicateId         = 1 << 5,   // false    // Allow submitting an item with the same identifier as an item already submitted this frame without triggering a warning tooltip if io.ConfigDebugHighlightIdConflicts is set.
};

// Flags for UxImGui::InputText()
// (Those are per-item flags. There are shared flags in UxImGuiIO: io.ConfigInputTextCursorBlink and io.ConfigInputTextEnterKeepActive)
enum UxImGuiInputTextFlags_
{
    // Basic filters (also see UxImGuiInputTextFlags_CallbackCharFilter)
    UxImGuiInputTextFlags_None                = 0,
    UxImGuiInputTextFlags_CharsDecimal        = 1 << 0,   // Allow 0123456789.+-*/
    UxImGuiInputTextFlags_CharsHexadecimal    = 1 << 1,   // Allow 0123456789ABCDEFabcdef
    UxImGuiInputTextFlags_CharsScientific     = 1 << 2,   // Allow 0123456789.+-*/eE (Scientific notation input)
    UxImGuiInputTextFlags_CharsUppercase      = 1 << 3,   // Turn a..z into A..Z
    UxImGuiInputTextFlags_CharsNoBlank        = 1 << 4,   // Filter out spaces, tabs

    // Inputs
    UxImGuiInputTextFlags_AllowTabInput       = 1 << 5,   // Pressing TAB input a '\t' character into the text field
    UxImGuiInputTextFlags_EnterReturnsTrue    = 1 << 6,   // Return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider using IsItemDeactivatedAfterEdit() instead!
    UxImGuiInputTextFlags_EscapeClearsAll     = 1 << 7,   // Escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    UxImGuiInputTextFlags_CtrlEnterForNewLine = 1 << 8,   // In multi-line mode, validate with Enter, add new line with Ctrl+Enter (default is opposite: validate with Ctrl+Enter, add line with Enter).

    // Other options
    UxImGuiInputTextFlags_ReadOnly            = 1 << 9,   // Read-only mode
    UxImGuiInputTextFlags_Password            = 1 << 10,  // Password mode, display all characters as '*', disable copy
    UxImGuiInputTextFlags_AlwaysOverwrite     = 1 << 11,  // Overwrite mode
    UxImGuiInputTextFlags_AutoSelectAll       = 1 << 12,  // Select entire text when first taking mouse focus
    UxImGuiInputTextFlags_ParseEmptyRefVal    = 1 << 13,  // InputFloat(), InputInt(), InputScalar() etc. only: parse empty string as zero value.
    UxImGuiInputTextFlags_DisplayEmptyRefVal  = 1 << 14,  // InputFloat(), InputInt(), InputScalar() etc. only: when value is zero, do not display it. Generally used with UxImGuiInputTextFlags_ParseEmptyRefVal.
    UxImGuiInputTextFlags_NoHorizontalScroll  = 1 << 15,  // Disable following the cursor horizontally
    UxImGuiInputTextFlags_NoUndoRedo          = 1 << 16,  // Disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().

    // Elide display / Alignment
    UxImGuiInputTextFlags_ElideLeft           = 1 << 17,  // When text doesn't fit, elide left side to ensure right side stays visible. Useful for path/filenames. Single-line only!

    // Callback features
    UxImGuiInputTextFlags_CallbackCompletion  = 1 << 18,  // Callback on pressing TAB (for completion handling)
    UxImGuiInputTextFlags_CallbackHistory     = 1 << 19,  // Callback on pressing Up/Down arrows (for history handling)
    UxImGuiInputTextFlags_CallbackAlways      = 1 << 20,  // Callback on each iteration. User code may query cursor position, modify text buffer.
    UxImGuiInputTextFlags_CallbackCharFilter  = 1 << 21,  // Callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
    UxImGuiInputTextFlags_CallbackResize      = 1 << 22,  // Callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
    UxImGuiInputTextFlags_CallbackEdit        = 1 << 23,  // Callback on any edit. Note that InputText() already returns true on edit + you can always use IsItemEdited(). The callback is useful to manipulate the underlying buffer while focus is active.

    // Obsolete names
    //UxImGuiInputTextFlags_AlwaysInsertMode  = UxImGuiInputTextFlags_AlwaysOverwrite   // [renamed in 1.82] name was not matching behavior
};

// Flags for UxImGui::TreeNodeEx(), UxImGui::CollapsingHeader*()
enum UxImGuiTreeNodeFlags_
{
    UxImGuiTreeNodeFlags_None                 = 0,
    UxImGuiTreeNodeFlags_Selected             = 1 << 0,   // Draw as selected
    UxImGuiTreeNodeFlags_Framed               = 1 << 1,   // Draw frame with background (e.g. for CollapsingHeader)
    UxImGuiTreeNodeFlags_AllowOverlap         = 1 << 2,   // Hit testing to allow subsequent widgets to overlap this one
    UxImGuiTreeNodeFlags_NoTreePushOnOpen     = 1 << 3,   // Don't do a TreePush() when open (e.g. for CollapsingHeader) = no extra indent nor pushing on ID stack
    UxImGuiTreeNodeFlags_NoAutoOpenOnLog      = 1 << 4,   // Don't automatically and temporarily open node when Logging is active (by default logging will automatically open tree nodes)
    UxImGuiTreeNodeFlags_DefaultOpen          = 1 << 5,   // Default node to be open
    UxImGuiTreeNodeFlags_OpenOnDoubleClick    = 1 << 6,   // Open on double-click instead of simple click (default for multi-select unless any _OpenOnXXX behavior is set explicitly). Both behaviors may be combined.
    UxImGuiTreeNodeFlags_OpenOnArrow          = 1 << 7,   // Open when clicking on the arrow part (default for multi-select unless any _OpenOnXXX behavior is set explicitly). Both behaviors may be combined.
    UxImGuiTreeNodeFlags_Leaf                 = 1 << 8,   // No collapsing, no arrow (use as a convenience for leaf nodes).
    UxImGuiTreeNodeFlags_Bullet               = 1 << 9,   // Display a bullet instead of arrow. IMPORTANT: node can still be marked open/close if you don't set the _Leaf flag!
    UxImGuiTreeNodeFlags_FramePadding         = 1 << 10,  // Use FramePadding (even for an unframed text node) to vertically align text baseline to regular widget height. Equivalent to calling AlignTextToFramePadding() before the node.
    UxImGuiTreeNodeFlags_SpanAvailWidth       = 1 << 11,  // Extend hit box to the right-most edge, even if not framed. This is not the default in order to allow adding other items on the same line without using AllowOverlap mode.
    UxImGuiTreeNodeFlags_SpanFullWidth        = 1 << 12,  // Extend hit box to the left-most and right-most edges (cover the indent area).
    UxImGuiTreeNodeFlags_SpanLabelWidth       = 1 << 13,  // Narrow hit box + narrow hovering highlight, will only cover the label text.
    UxImGuiTreeNodeFlags_SpanAllColumns       = 1 << 14,  // Frame will span all columns of its container table (label will still fit in current column)
    UxImGuiTreeNodeFlags_LabelSpanAllColumns  = 1 << 15,  // Label will span all columns of its container table
    //UxImGuiTreeNodeFlags_NoScrollOnOpen     = 1 << 16,  // FIXME: TODO: Disable automatic scroll on TreePop() if node got just open and contents is not visible
    UxImGuiTreeNodeFlags_NavLeftJumpsToParent = 1 << 17,  // Nav: left arrow moves back to parent. This is processed in TreePop() when there's an unfullfilled Left nav request remaining.
    UxImGuiTreeNodeFlags_CollapsingHeader     = UxImGuiTreeNodeFlags_Framed | UxImGuiTreeNodeFlags_NoTreePushOnOpen | UxImGuiTreeNodeFlags_NoAutoOpenOnLog,

    // [EXPERIMENTAL] Draw lines connecting TreeNode hierarchy. Discuss in GitHub issue #2920.
    // Default value is pulled from style.TreeLinesFlags. May be overridden in TreeNode calls.
    UxImGuiTreeNodeFlags_DrawLinesNone        = 1 << 18,  // No lines drawn
    UxImGuiTreeNodeFlags_DrawLinesFull        = 1 << 19,  // Horizontal lines to child nodes. Vertical line drawn down to TreePop() position: cover full contents. Faster (for large trees).
    UxImGuiTreeNodeFlags_DrawLinesToNodes     = 1 << 20,  // Horizontal lines to child nodes. Vertical line drawn down to bottom-most child node. Slower (for large trees).

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiTreeNodeFlags_NavLeftJumpsBackHere = UxImGuiTreeNodeFlags_NavLeftJumpsToParent,  // Renamed in 1.92.0
    UxImGuiTreeNodeFlags_SpanTextWidth        = UxImGuiTreeNodeFlags_SpanLabelWidth,        // Renamed in 1.90.7
    UxImGuiTreeNodeFlags_AllowItemOverlap     = UxImGuiTreeNodeFlags_AllowOverlap,          // Renamed in 1.89.7
#endif
};

// Flags for OpenPopup*(), BeginPopupContext*(), IsPopupOpen() functions.
// - To be backward compatible with older API which took an 'int mouse_button = 1' argument instead of 'UxImGuiPopupFlags flags',
//   we need to treat small flags values as a mouse button index, so we encode the mouse button in the first few bits of the flags.
//   It is therefore guaranteed to be legal to pass a mouse button index in UxImGuiPopupFlags.
// - For the same reason, we exceptionally default the UxImGuiPopupFlags argument of BeginPopupContextXXX functions to 1 instead of 0.
//   IMPORTANT: because the default parameter is 1 (==UxImGuiPopupFlags_MouseButtonRight), if you rely on the default parameter
//   and want to use another flag, you need to pass in the UxImGuiPopupFlags_MouseButtonRight flag explicitly.
// - Multiple buttons currently cannot be combined/or-ed in those functions (we could allow it later).
enum UxImGuiPopupFlags_
{
    UxImGuiPopupFlags_None                    = 0,
    UxImGuiPopupFlags_MouseButtonLeft         = 0,        // For BeginPopupContext*(): open on Left Mouse release. Guaranteed to always be == 0 (same as UxImGuiMouseButton_Left)
    UxImGuiPopupFlags_MouseButtonRight        = 1,        // For BeginPopupContext*(): open on Right Mouse release. Guaranteed to always be == 1 (same as UxImGuiMouseButton_Right)
    UxImGuiPopupFlags_MouseButtonMiddle       = 2,        // For BeginPopupContext*(): open on Middle Mouse release. Guaranteed to always be == 2 (same as UxImGuiMouseButton_Middle)
    UxImGuiPopupFlags_MouseButtonMask_        = 0x1F,
    UxImGuiPopupFlags_MouseButtonDefault_     = 1,
    UxImGuiPopupFlags_NoReopen                = 1 << 5,   // For OpenPopup*(), BeginPopupContext*(): don't reopen same popup if already open (won't reposition, won't reinitialize navigation)
    //UxImGuiPopupFlags_NoReopenAlwaysNavInit = 1 << 6,   // For OpenPopup*(), BeginPopupContext*(): focus and initialize navigation even when not reopening.
    UxImGuiPopupFlags_NoOpenOverExistingPopup = 1 << 7,   // For OpenPopup*(), BeginPopupContext*(): don't open if there's already a popup at the same level of the popup stack
    UxImGuiPopupFlags_NoOpenOverItems         = 1 << 8,   // For BeginPopupContextWindow(): don't return true when hovering items, only when hovering empty space
    UxImGuiPopupFlags_AnyPopupId              = 1 << 10,  // For IsPopupOpen(): ignore the UxImGuiID parameter and test for any popup.
    UxImGuiPopupFlags_AnyPopupLevel           = 1 << 11,  // For IsPopupOpen(): search/test at any level of the popup stack (default test in the current level)
    UxImGuiPopupFlags_AnyPopup                = UxImGuiPopupFlags_AnyPopupId | UxImGuiPopupFlags_AnyPopupLevel,
};

// Flags for UxImGui::Selectable()
enum UxImGuiSelectableFlags_
{
    UxImGuiSelectableFlags_None               = 0,
    UxImGuiSelectableFlags_NoAutoClosePopups  = 1 << 0,   // Clicking this doesn't close parent popup window (overrides UxImGuiItemFlags_AutoClosePopups)
    UxImGuiSelectableFlags_SpanAllColumns     = 1 << 1,   // Frame will span all columns of its container table (text will still fit in current column)
    UxImGuiSelectableFlags_AllowDoubleClick   = 1 << 2,   // Generate press events on double clicks too
    UxImGuiSelectableFlags_Disabled           = 1 << 3,   // Cannot be selected, display grayed out text
    UxImGuiSelectableFlags_AllowOverlap       = 1 << 4,   // (WIP) Hit testing to allow subsequent widgets to overlap this one
    UxImGuiSelectableFlags_Highlight          = 1 << 5,   // Make the item be displayed as if it is hovered

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiSelectableFlags_DontClosePopups    = UxImGuiSelectableFlags_NoAutoClosePopups,   // Renamed in 1.91.0
    UxImGuiSelectableFlags_AllowItemOverlap   = UxImGuiSelectableFlags_AllowOverlap,        // Renamed in 1.89.7
#endif
};

// Flags for UxImGui::BeginCombo()
enum UxImGuiComboFlags_
{
    UxImGuiComboFlags_None                    = 0,
    UxImGuiComboFlags_PopupAlignLeft          = 1 << 0,   // Align the popup toward the left by default
    UxImGuiComboFlags_HeightSmall             = 1 << 1,   // Max ~4 items visible. Tip: If you want your combo popup to be a specific size you can use SetNextWindowSizeConstraints() prior to calling BeginCombo()
    UxImGuiComboFlags_HeightRegular           = 1 << 2,   // Max ~8 items visible (default)
    UxImGuiComboFlags_HeightLarge             = 1 << 3,   // Max ~20 items visible
    UxImGuiComboFlags_HeightLargest           = 1 << 4,   // As many fitting items as possible
    UxImGuiComboFlags_NoArrowButton           = 1 << 5,   // Display on the preview box without the square arrow button
    UxImGuiComboFlags_NoPreview               = 1 << 6,   // Display only a square arrow button
    UxImGuiComboFlags_WidthFitPreview         = 1 << 7,   // Width dynamically calculated from preview contents
    UxImGuiComboFlags_HeightMask_             = UxImGuiComboFlags_HeightSmall | UxImGuiComboFlags_HeightRegular | UxImGuiComboFlags_HeightLarge | UxImGuiComboFlags_HeightLargest,
};

// Flags for UxImGui::BeginTabBar()
enum UxImGuiTabBarFlags_
{
    UxImGuiTabBarFlags_None                           = 0,
    UxImGuiTabBarFlags_Reorderable                    = 1 << 0,   // Allow manually dragging tabs to re-order them + New tabs are appended at the end of list
    UxImGuiTabBarFlags_AutoSelectNewTabs              = 1 << 1,   // Automatically select new tabs when they appear
    UxImGuiTabBarFlags_TabListPopupButton             = 1 << 2,   // Disable buttons to open the tab list popup
    UxImGuiTabBarFlags_NoCloseWithMiddleMouseButton   = 1 << 3,   // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You may handle this behavior manually on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.
    UxImGuiTabBarFlags_NoTabListScrollingButtons      = 1 << 4,   // Disable scrolling buttons (apply when fitting policy is UxImGuiTabBarFlags_FittingPolicyScroll)
    UxImGuiTabBarFlags_NoTooltip                      = 1 << 5,   // Disable tooltips when hovering a tab
    UxImGuiTabBarFlags_DrawSelectedOverline           = 1 << 6,   // Draw selected overline markers over selected tab
    UxImGuiTabBarFlags_FittingPolicyResizeDown        = 1 << 7,   // Resize tabs when they don't fit
    UxImGuiTabBarFlags_FittingPolicyScroll            = 1 << 8,   // Add scroll buttons when tabs don't fit
    UxImGuiTabBarFlags_FittingPolicyMask_             = UxImGuiTabBarFlags_FittingPolicyResizeDown | UxImGuiTabBarFlags_FittingPolicyScroll,
    UxImGuiTabBarFlags_FittingPolicyDefault_          = UxImGuiTabBarFlags_FittingPolicyResizeDown,
};

// Flags for UxImGui::BeginTabItem()
enum UxImGuiTabItemFlags_
{
    UxImGuiTabItemFlags_None                          = 0,
    UxImGuiTabItemFlags_UnsavedDocument               = 1 << 0,   // Display a dot next to the title + set UxImGuiTabItemFlags_NoAssumedClosure.
    UxImGuiTabItemFlags_SetSelected                   = 1 << 1,   // Trigger flag to programmatically make the tab selected when calling BeginTabItem()
    UxImGuiTabItemFlags_NoCloseWithMiddleMouseButton  = 1 << 2,   // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You may handle this behavior manually on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.
    UxImGuiTabItemFlags_NoPushId                      = 1 << 3,   // Don't call PushID()/PopID() on BeginTabItem()/EndTabItem()
    UxImGuiTabItemFlags_NoTooltip                     = 1 << 4,   // Disable tooltip for the given tab
    UxImGuiTabItemFlags_NoReorder                     = 1 << 5,   // Disable reordering this tab or having another tab cross over this tab
    UxImGuiTabItemFlags_Leading                       = 1 << 6,   // Enforce the tab position to the left of the tab bar (after the tab list popup button)
    UxImGuiTabItemFlags_Trailing                      = 1 << 7,   // Enforce the tab position to the right of the tab bar (before the scrolling buttons)
    UxImGuiTabItemFlags_NoAssumedClosure              = 1 << 8,   // Tab is selected when trying to close + closure is not immediately assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
};

// Flags for UxImGui::IsWindowFocused()
enum UxImGuiFocusedFlags_
{
    UxImGuiFocusedFlags_None                          = 0,
    UxImGuiFocusedFlags_ChildWindows                  = 1 << 0,   // Return true if any children of the window is focused
    UxImGuiFocusedFlags_RootWindow                    = 1 << 1,   // Test from root window (top most parent of the current hierarchy)
    UxImGuiFocusedFlags_AnyWindow                     = 1 << 2,   // Return true if any window is focused. Important: If you are trying to tell how to dispatch your low-level inputs, do NOT use this. Use 'io.WantCaptureMouse' instead! Please read the FAQ!
    UxImGuiFocusedFlags_NoPopupHierarchy              = 1 << 3,   // Do not consider popup hierarchy (do not treat popup emitter as parent of popup) (when used with _ChildWindows or _RootWindow)
    UxImGuiFocusedFlags_DockHierarchy                 = 1 << 4,   // Consider docking hierarchy (treat dockspace host as parent of docked window) (when used with _ChildWindows or _RootWindow)
    UxImGuiFocusedFlags_RootAndChildWindows           = UxImGuiFocusedFlags_RootWindow | UxImGuiFocusedFlags_ChildWindows,
};

// Flags for UxImGui::IsItemHovered(), UxImGui::IsWindowHovered()
// Note: if you are trying to check whether your mouse should be dispatched to Dear UxImGui or to your app, you should use 'io.WantCaptureMouse' instead! Please read the FAQ!
// Note: windows with the UxImGuiWindowFlags_NoInputs flag are ignored by IsWindowHovered() calls.
enum UxImGuiHoveredFlags_
{
    UxImGuiHoveredFlags_None                          = 0,        // Return true if directly over the item/window, not obstructed by another window, not obstructed by an active popup or modal blocking inputs under them.
    UxImGuiHoveredFlags_ChildWindows                  = 1 << 0,   // IsWindowHovered() only: Return true if any children of the window is hovered
    UxImGuiHoveredFlags_RootWindow                    = 1 << 1,   // IsWindowHovered() only: Test from root window (top most parent of the current hierarchy)
    UxImGuiHoveredFlags_AnyWindow                     = 1 << 2,   // IsWindowHovered() only: Return true if any window is hovered
    UxImGuiHoveredFlags_NoPopupHierarchy              = 1 << 3,   // IsWindowHovered() only: Do not consider popup hierarchy (do not treat popup emitter as parent of popup) (when used with _ChildWindows or _RootWindow)
    UxImGuiHoveredFlags_DockHierarchy                 = 1 << 4,   // IsWindowHovered() only: Consider docking hierarchy (treat dockspace host as parent of docked window) (when used with _ChildWindows or _RootWindow)
    UxImGuiHoveredFlags_AllowWhenBlockedByPopup       = 1 << 5,   // Return true even if a popup window is normally blocking access to this item/window
    //UxImGuiHoveredFlags_AllowWhenBlockedByModal     = 1 << 6,   // Return true even if a modal popup window is normally blocking access to this item/window. FIXME-TODO: Unavailable yet.
    UxImGuiHoveredFlags_AllowWhenBlockedByActiveItem  = 1 << 7,   // Return true even if an active item is blocking access to this item/window. Useful for Drag and Drop patterns.
    UxImGuiHoveredFlags_AllowWhenOverlappedByItem     = 1 << 8,   // IsItemHovered() only: Return true even if the item uses AllowOverlap mode and is overlapped by another hoverable item.
    UxImGuiHoveredFlags_AllowWhenOverlappedByWindow   = 1 << 9,   // IsItemHovered() only: Return true even if the position is obstructed or overlapped by another window.
    UxImGuiHoveredFlags_AllowWhenDisabled             = 1 << 10,  // IsItemHovered() only: Return true even if the item is disabled
    UxImGuiHoveredFlags_NoNavOverride                 = 1 << 11,  // IsItemHovered() only: Disable using keyboard/gamepad navigation state when active, always query mouse
    UxImGuiHoveredFlags_AllowWhenOverlapped           = UxImGuiHoveredFlags_AllowWhenOverlappedByItem | UxImGuiHoveredFlags_AllowWhenOverlappedByWindow,
    UxImGuiHoveredFlags_RectOnly                      = UxImGuiHoveredFlags_AllowWhenBlockedByPopup | UxImGuiHoveredFlags_AllowWhenBlockedByActiveItem | UxImGuiHoveredFlags_AllowWhenOverlapped,
    UxImGuiHoveredFlags_RootAndChildWindows           = UxImGuiHoveredFlags_RootWindow | UxImGuiHoveredFlags_ChildWindows,

    // Tooltips mode
    // - typically used in IsItemHovered() + SetTooltip() sequence.
    // - this is a shortcut to pull flags from 'style.HoverFlagsForTooltipMouse' or 'style.HoverFlagsForTooltipNav' where you can reconfigure desired behavior.
    //   e.g. 'TooltipHoveredFlagsForMouse' defaults to 'UxImGuiHoveredFlags_Stationary | UxImGuiHoveredFlags_DelayShort'.
    // - for frequently actioned or hovered items providing a tooltip, you want may to use UxImGuiHoveredFlags_ForTooltip (stationary + delay) so the tooltip doesn't show too often.
    // - for items which main purpose is to be hovered, or items with low affordance, or in less consistent apps, prefer no delay or shorter delay.
    UxImGuiHoveredFlags_ForTooltip                    = 1 << 12,  // Shortcut for standard flags when using IsItemHovered() + SetTooltip() sequence.

    // (Advanced) Mouse Hovering delays.
    // - generally you can use UxImGuiHoveredFlags_ForTooltip to use application-standardized flags.
    // - use those if you need specific overrides.
    UxImGuiHoveredFlags_Stationary                    = 1 << 13,  // Require mouse to be stationary for style.HoverStationaryDelay (~0.15 sec) _at least one time_. After this, can move on same item/window. Using the stationary test tends to reduces the need for a long delay.
    UxImGuiHoveredFlags_DelayNone                     = 1 << 14,  // IsItemHovered() only: Return true immediately (default). As this is the default you generally ignore this.
    UxImGuiHoveredFlags_DelayShort                    = 1 << 15,  // IsItemHovered() only: Return true after style.HoverDelayShort elapsed (~0.15 sec) (shared between items) + requires mouse to be stationary for style.HoverStationaryDelay (once per item).
    UxImGuiHoveredFlags_DelayNormal                   = 1 << 16,  // IsItemHovered() only: Return true after style.HoverDelayNormal elapsed (~0.40 sec) (shared between items) + requires mouse to be stationary for style.HoverStationaryDelay (once per item).
    UxImGuiHoveredFlags_NoSharedDelay                 = 1 << 17,  // IsItemHovered() only: Disable shared delay system where moving from one item to the next keeps the previous timer for a short time (standard for tooltips with long delays)
};

// Flags for UxImGui::DockSpace(), shared/inherited by child nodes.
// (Some flags can be applied to individual nodes directly)
// FIXME-DOCK: Also see UxImGuiDockNodeFlagsPrivate_ which may involve using the WIP and internal DockBuilder api.
enum UxImGuiDockNodeFlags_
{
    UxImGuiDockNodeFlags_None                         = 0,
    UxImGuiDockNodeFlags_KeepAliveOnly                = 1 << 0,   //       // Don't display the dockspace node but keep it alive. Windows docked into this dockspace node won't be undocked.
    //UxImGuiDockNodeFlags_NoCentralNode              = 1 << 1,   //       // Disable Central Node (the node which can stay empty)
    UxImGuiDockNodeFlags_NoDockingOverCentralNode     = 1 << 2,   //       // Disable docking over the Central Node, which will be always kept empty.
    UxImGuiDockNodeFlags_PassthruCentralNode          = 1 << 3,   //       // Enable passthru dockspace: 1) DockSpace() will render a UxImGuiCol_WindowBg background covering everything excepted the Central Node when empty. Meaning the host window should probably use SetNextWindowBgAlpha(0.0f) prior to Begin() when using this. 2) When Central Node is empty: let inputs pass-through + won't display a DockingEmptyBg background. See demo for details.
    UxImGuiDockNodeFlags_NoDockingSplit               = 1 << 4,   //       // Disable other windows/nodes from splitting this node.
    UxImGuiDockNodeFlags_NoResize                     = 1 << 5,   // Saved // Disable resizing node using the splitter/separators. Useful with programmatically setup dockspaces.
    UxImGuiDockNodeFlags_AutoHideTabBar               = 1 << 6,   //       // Tab bar will automatically hide when there is a single window in the dock node.
    UxImGuiDockNodeFlags_NoUndocking                  = 1 << 7,   //       // Disable undocking this node.

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiDockNodeFlags_NoSplit                      = UxImGuiDockNodeFlags_NoDockingSplit, // Renamed in 1.90
    UxImGuiDockNodeFlags_NoDockingInCentralNode       = UxImGuiDockNodeFlags_NoDockingOverCentralNode, // Renamed in 1.90
#endif
};

// Flags for UxImGui::BeginDragDropSource(), UxImGui::AcceptDragDropPayload()
enum UxImGuiDragDropFlags_
{
    UxImGuiDragDropFlags_None                         = 0,
    // BeginDragDropSource() flags
    UxImGuiDragDropFlags_SourceNoPreviewTooltip       = 1 << 0,   // Disable preview tooltip. By default, a successful call to BeginDragDropSource opens a tooltip so you can display a preview or description of the source contents. This flag disables this behavior.
    UxImGuiDragDropFlags_SourceNoDisableHover         = 1 << 1,   // By default, when dragging we clear data so that IsItemHovered() will return false, to avoid subsequent user code submitting tooltips. This flag disables this behavior so you can still call IsItemHovered() on the source item.
    UxImGuiDragDropFlags_SourceNoHoldToOpenOthers     = 1 << 2,   // Disable the behavior that allows to open tree nodes and collapsing header by holding over them while dragging a source item.
    UxImGuiDragDropFlags_SourceAllowNullID            = 1 << 3,   // Allow items such as Text(), Image() that have no unique identifier to be used as drag source, by manufacturing a temporary identifier based on their window-relative position. This is extremely unusual within the dear imgui ecosystem and so we made it explicit.
    UxImGuiDragDropFlags_SourceExtern                 = 1 << 4,   // External source (from outside of dear imgui), won't attempt to read current item/window info. Will always return true. Only one Extern source can be active simultaneously.
    UxImGuiDragDropFlags_PayloadAutoExpire            = 1 << 5,   // Automatically expire the payload if the source cease to be submitted (otherwise payloads are persisting while being dragged)
    UxImGuiDragDropFlags_PayloadNoCrossContext        = 1 << 6,   // Hint to specify that the payload may not be copied outside current dear imgui context.
    UxImGuiDragDropFlags_PayloadNoCrossProcess        = 1 << 7,   // Hint to specify that the payload may not be copied outside current process.
    // AcceptDragDropPayload() flags
    UxImGuiDragDropFlags_AcceptBeforeDelivery         = 1 << 10,  // AcceptDragDropPayload() will returns true even before the mouse button is released. You can then call IsDelivery() to test if the payload needs to be delivered.
    UxImGuiDragDropFlags_AcceptNoDrawDefaultRect      = 1 << 11,  // Do not draw the default highlight rectangle when hovering over target.
    UxImGuiDragDropFlags_AcceptNoPreviewTooltip       = 1 << 12,  // Request hiding the BeginDragDropSource tooltip from the BeginDragDropTarget site.
    UxImGuiDragDropFlags_AcceptPeekOnly               = UxImGuiDragDropFlags_AcceptBeforeDelivery | UxImGuiDragDropFlags_AcceptNoDrawDefaultRect, // For peeking ahead and inspecting the payload before delivery.

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiDragDropFlags_SourceAutoExpirePayload = UxImGuiDragDropFlags_PayloadAutoExpire, // Renamed in 1.90.9
#endif
};

// Standard Drag and Drop payload types. You can define you own payload types using short strings. Types starting with '_' are defined by Dear UxImGui.
#define IMGUI_PAYLOAD_TYPE_COLOR_3F     "_COL3F"    // float[3]: Standard type for colors, without alpha. User code may use this type.
#define IMGUI_PAYLOAD_TYPE_COLOR_4F     "_COL4F"    // float[4]: Standard type for colors. User code may use this type.

// A primary data type
enum UxImGuiDataType_
{
    UxImGuiDataType_S8,       // signed char / char (with sensible compilers)
    UxImGuiDataType_U8,       // unsigned char
    UxImGuiDataType_S16,      // short
    UxImGuiDataType_U16,      // unsigned short
    UxImGuiDataType_S32,      // int
    UxImGuiDataType_U32,      // unsigned int
    UxImGuiDataType_S64,      // long long / __int64
    UxImGuiDataType_U64,      // unsigned long long / unsigned __int64
    UxImGuiDataType_Float,    // float
    UxImGuiDataType_Double,   // double
    UxImGuiDataType_Bool,     // bool (provided for user convenience, not supported by scalar widgets)
    UxImGuiDataType_String,   // char* (provided for user convenience, not supported by scalar widgets)
    UxImGuiDataType_COUNT
};

// A cardinal direction
enum UxImGuiDir : int
{
    UxImGuiDir_None    = -1,
    UxImGuiDir_Left    = 0,
    UxImGuiDir_Right   = 1,
    UxImGuiDir_Up      = 2,
    UxImGuiDir_Down    = 3,
    UxImGuiDir_COUNT
};

// A sorting direction
enum UxImGuiSortDirection : UxImU8
{
    UxImGuiSortDirection_None         = 0,
    UxImGuiSortDirection_Ascending    = 1,    // Ascending = 0->9, A->Z etc.
    UxImGuiSortDirection_Descending   = 2     // Descending = 9->0, Z->A etc.
};

// A key identifier (UxImGuiKey_XXX or UxImGuiMod_XXX value): can represent Keyboard, Mouse and Gamepad values.
// All our named keys are >= 512. Keys value 0 to 511 are left unused and were legacy native/opaque key values (< 1.87).
// Support for legacy keys was completely removed in 1.91.5.
// Read details about the 1.87+ transition : https://github.com/ocornut/imgui/issues/4921
// Note that "Keys" related to physical keys and are not the same concept as input "Characters", the later are submitted via io.AddInputCharacter().
// The keyboard key enum values are named after the keys on a standard US keyboard, and on other keyboard types the keys reported may not match the keycaps.
enum UxImGuiKey : int
{
    // Keyboard
    UxImGuiKey_None = 0,
    UxImGuiKey_NamedKey_BEGIN = 512,  // First valid key value (other than 0)

    UxImGuiKey_Tab = 512,             // == UxImGuiKey_NamedKey_BEGIN
    UxImGuiKey_LeftArrow,
    UxImGuiKey_RightArrow,
    UxImGuiKey_UpArrow,
    UxImGuiKey_DownArrow,
    UxImGuiKey_PageUp,
    UxImGuiKey_PageDown,
    UxImGuiKey_Home,
    UxImGuiKey_End,
    UxImGuiKey_Insert,
    UxImGuiKey_Delete,
    UxImGuiKey_Backspace,
    UxImGuiKey_Space,
    UxImGuiKey_Enter,
    UxImGuiKey_Escape,
    UxImGuiKey_LeftCtrl, UxImGuiKey_LeftShift, UxImGuiKey_LeftAlt, UxImGuiKey_LeftSuper,
    UxImGuiKey_RightCtrl, UxImGuiKey_RightShift, UxImGuiKey_RightAlt, UxImGuiKey_RightSuper,
    UxImGuiKey_Menu,
    UxImGuiKey_0, UxImGuiKey_1, UxImGuiKey_2, UxImGuiKey_3, UxImGuiKey_4, UxImGuiKey_5, UxImGuiKey_6, UxImGuiKey_7, UxImGuiKey_8, UxImGuiKey_9,
    UxImGuiKey_A, UxImGuiKey_B, UxImGuiKey_C, UxImGuiKey_D, UxImGuiKey_E, UxImGuiKey_F, UxImGuiKey_G, UxImGuiKey_H, UxImGuiKey_I, UxImGuiKey_J,
    UxImGuiKey_K, UxImGuiKey_L, UxImGuiKey_M, UxImGuiKey_N, UxImGuiKey_O, UxImGuiKey_P, UxImGuiKey_Q, UxImGuiKey_R, UxImGuiKey_S, UxImGuiKey_T,
    UxImGuiKey_U, UxImGuiKey_V, UxImGuiKey_W, UxImGuiKey_X, UxImGuiKey_Y, UxImGuiKey_Z,
    UxImGuiKey_F1, UxImGuiKey_F2, UxImGuiKey_F3, UxImGuiKey_F4, UxImGuiKey_F5, UxImGuiKey_F6,
    UxImGuiKey_F7, UxImGuiKey_F8, UxImGuiKey_F9, UxImGuiKey_F10, UxImGuiKey_F11, UxImGuiKey_F12,
    UxImGuiKey_F13, UxImGuiKey_F14, UxImGuiKey_F15, UxImGuiKey_F16, UxImGuiKey_F17, UxImGuiKey_F18,
    UxImGuiKey_F19, UxImGuiKey_F20, UxImGuiKey_F21, UxImGuiKey_F22, UxImGuiKey_F23, UxImGuiKey_F24,
    UxImGuiKey_Apostrophe,        // '
    UxImGuiKey_Comma,             // ,
    UxImGuiKey_Minus,             // -
    UxImGuiKey_Period,            // .
    UxImGuiKey_Slash,             // /
    UxImGuiKey_Semicolon,         // ;
    UxImGuiKey_Equal,             // =
    UxImGuiKey_LeftBracket,       // [
    UxImGuiKey_Backslash,         // \ (this text inhibit multiline comment caused by backslash)
    UxImGuiKey_RightBracket,      // ]
    UxImGuiKey_GraveAccent,       // `
    UxImGuiKey_CapsLock,
    UxImGuiKey_ScrollLock,
    UxImGuiKey_NumLock,
    UxImGuiKey_PrintScreen,
    UxImGuiKey_Pause,
    UxImGuiKey_Keypad0, UxImGuiKey_Keypad1, UxImGuiKey_Keypad2, UxImGuiKey_Keypad3, UxImGuiKey_Keypad4,
    UxImGuiKey_Keypad5, UxImGuiKey_Keypad6, UxImGuiKey_Keypad7, UxImGuiKey_Keypad8, UxImGuiKey_Keypad9,
    UxImGuiKey_KeypadDecimal,
    UxImGuiKey_KeypadDivide,
    UxImGuiKey_KeypadMultiply,
    UxImGuiKey_KeypadSubtract,
    UxImGuiKey_KeypadAdd,
    UxImGuiKey_KeypadEnter,
    UxImGuiKey_KeypadEqual,
    UxImGuiKey_AppBack,               // Available on some keyboard/mouses. Often referred as "Browser Back"
    UxImGuiKey_AppForward,
    UxImGuiKey_Oem102,                // Non-US backslash.

    // Gamepad (some of those are analog values, 0.0f to 1.0f)                          // NAVIGATION ACTION
    // (download controller mapping PNG/PSD at http://dearimgui.com/controls_sheets)
    UxImGuiKey_GamepadStart,          // Menu (Xbox)      + (Switch)   Start/Options (PS)
    UxImGuiKey_GamepadBack,           // View (Xbox)      - (Switch)   Share (PS)
    UxImGuiKey_GamepadFaceLeft,       // X (Xbox)         Y (Switch)   Square (PS)        // Tap: Toggle Menu. Hold: Windowing mode (Focus/Move/Resize windows)
    UxImGuiKey_GamepadFaceRight,      // B (Xbox)         A (Switch)   Circle (PS)        // Cancel / Close / Exit
    UxImGuiKey_GamepadFaceUp,         // Y (Xbox)         X (Switch)   Triangle (PS)      // Text Input / On-screen Keyboard
    UxImGuiKey_GamepadFaceDown,       // A (Xbox)         B (Switch)   Cross (PS)         // Activate / Open / Toggle / Tweak
    UxImGuiKey_GamepadDpadLeft,       // D-pad Left                                       // Move / Tweak / Resize Window (in Windowing mode)
    UxImGuiKey_GamepadDpadRight,      // D-pad Right                                      // Move / Tweak / Resize Window (in Windowing mode)
    UxImGuiKey_GamepadDpadUp,         // D-pad Up                                         // Move / Tweak / Resize Window (in Windowing mode)
    UxImGuiKey_GamepadDpadDown,       // D-pad Down                                       // Move / Tweak / Resize Window (in Windowing mode)
    UxImGuiKey_GamepadL1,             // L Bumper (Xbox)  L (Switch)   L1 (PS)            // Tweak Slower / Focus Previous (in Windowing mode)
    UxImGuiKey_GamepadR1,             // R Bumper (Xbox)  R (Switch)   R1 (PS)            // Tweak Faster / Focus Next (in Windowing mode)
    UxImGuiKey_GamepadL2,             // L Trig. (Xbox)   ZL (Switch)  L2 (PS) [Analog]
    UxImGuiKey_GamepadR2,             // R Trig. (Xbox)   ZR (Switch)  R2 (PS) [Analog]
    UxImGuiKey_GamepadL3,             // L Stick (Xbox)   L3 (Switch)  L3 (PS)
    UxImGuiKey_GamepadR3,             // R Stick (Xbox)   R3 (Switch)  R3 (PS)
    UxImGuiKey_GamepadLStickLeft,     // [Analog]                                         // Move Window (in Windowing mode)
    UxImGuiKey_GamepadLStickRight,    // [Analog]                                         // Move Window (in Windowing mode)
    UxImGuiKey_GamepadLStickUp,       // [Analog]                                         // Move Window (in Windowing mode)
    UxImGuiKey_GamepadLStickDown,     // [Analog]                                         // Move Window (in Windowing mode)
    UxImGuiKey_GamepadRStickLeft,     // [Analog]
    UxImGuiKey_GamepadRStickRight,    // [Analog]
    UxImGuiKey_GamepadRStickUp,       // [Analog]
    UxImGuiKey_GamepadRStickDown,     // [Analog]

    // Aliases: Mouse Buttons (auto-submitted from AddMouseButtonEvent() calls)
    // - This is mirroring the data also written to io.MouseDown[], io.MouseWheel, in a format allowing them to be accessed via standard key API.
    UxImGuiKey_MouseLeft, UxImGuiKey_MouseRight, UxImGuiKey_MouseMiddle, UxImGuiKey_MouseX1, UxImGuiKey_MouseX2, UxImGuiKey_MouseWheelX, UxImGuiKey_MouseWheelY,

    // [Internal] Reserved for mod storage
    UxImGuiKey_ReservedForModCtrl, UxImGuiKey_ReservedForModShift, UxImGuiKey_ReservedForModAlt, UxImGuiKey_ReservedForModSuper,
    UxImGuiKey_NamedKey_END,

    // Keyboard Modifiers (explicitly submitted by backend via AddKeyEvent() calls)
    // - This is mirroring the data also written to io.KeyCtrl, io.KeyShift, io.KeyAlt, io.KeySuper, in a format allowing
    //   them to be accessed via standard key API, allowing calls such as IsKeyPressed(), IsKeyReleased(), querying duration etc.
    // - Code polling every key (e.g. an interface to detect a key press for input mapping) might want to ignore those
    //   and prefer using the real keys (e.g. UxImGuiKey_LeftCtrl, UxImGuiKey_RightCtrl instead of UxImGuiMod_Ctrl).
    // - In theory the value of keyboard modifiers should be roughly equivalent to a logical or of the equivalent left/right keys.
    //   In practice: it's complicated; mods are often provided from different sources. Keyboard layout, IME, sticky keys and
    //   backends tend to interfere and break that equivalence. The safer decision is to relay that ambiguity down to the end-user...
    // - On macOS, we swap Cmd(Super) and Ctrl keys at the time of the io.AddKeyEvent() call.
    UxImGuiMod_None                   = 0,
    UxImGuiMod_Ctrl                   = 1 << 12, // Ctrl (non-macOS), Cmd (macOS)
    UxImGuiMod_Shift                  = 1 << 13, // Shift
    UxImGuiMod_Alt                    = 1 << 14, // Option/Menu
    UxImGuiMod_Super                  = 1 << 15, // Windows/Super (non-macOS), Ctrl (macOS)
    UxImGuiMod_Mask_                  = 0xF000,  // 4-bits

    // [Internal] If you need to iterate all keys (for e.g. an input mapper) you may use UxImGuiKey_NamedKey_BEGIN..UxImGuiKey_NamedKey_END.
    UxImGuiKey_NamedKey_COUNT         = UxImGuiKey_NamedKey_END - UxImGuiKey_NamedKey_BEGIN,
    //UxImGuiKey_KeysData_SIZE        = UxImGuiKey_NamedKey_COUNT,  // Size of KeysData[]: only hold named keys
    //UxImGuiKey_KeysData_OFFSET      = UxImGuiKey_NamedKey_BEGIN,  // Accesses to io.KeysData[] must use (key - UxImGuiKey_NamedKey_BEGIN) index.

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiKey_COUNT                  = UxImGuiKey_NamedKey_END,    // Obsoleted in 1.91.5 because it was extremely misleading (since named keys don't start at 0 anymore)
    UxImGuiMod_Shortcut               = UxImGuiMod_Ctrl,            // Removed in 1.90.7, you can now simply use UxImGuiMod_Ctrl
    UxImGuiKey_ModCtrl = UxImGuiMod_Ctrl, UxImGuiKey_ModShift = UxImGuiMod_Shift, UxImGuiKey_ModAlt = UxImGuiMod_Alt, UxImGuiKey_ModSuper = UxImGuiMod_Super, // Renamed in 1.89
    //UxImGuiKey_KeyPadEnter = UxImGuiKey_KeypadEnter,              // Renamed in 1.87
#endif
};

// Flags for Shortcut(), SetNextItemShortcut(),
// (and for upcoming extended versions of IsKeyPressed(), IsMouseClicked(), Shortcut(), SetKeyOwner(), SetItemKeyOwner() that are still in imgui_internal.h)
// Don't mistake with UxImGuiInputTextFlags! (which is for UxImGui::InputText() function)
enum UxImGuiInputFlags_
{
    UxImGuiInputFlags_None                    = 0,
    UxImGuiInputFlags_Repeat                  = 1 << 0,   // Enable repeat. Return true on successive repeats. Default for legacy IsKeyPressed(). NOT Default for legacy IsMouseClicked(). MUST BE == 1.

    // Flags for Shortcut(), SetNextItemShortcut()
    // - Routing policies: RouteGlobal+OverActive >> RouteActive or RouteFocused (if owner is active item) >> RouteGlobal+OverFocused >> RouteFocused (if in focused window stack) >> RouteGlobal.
    // - Default policy is RouteFocused. Can select only 1 policy among all available.
    UxImGuiInputFlags_RouteActive             = 1 << 10,  // Route to active item only.
    UxImGuiInputFlags_RouteFocused            = 1 << 11,  // Route to windows in the focus stack (DEFAULT). Deep-most focused window takes inputs. Active item takes inputs over deep-most focused window.
    UxImGuiInputFlags_RouteGlobal             = 1 << 12,  // Global route (unless a focused window or active item registered the route).
    UxImGuiInputFlags_RouteAlways             = 1 << 13,  // Do not register route, poll keys directly.
    // - Routing options
    UxImGuiInputFlags_RouteOverFocused        = 1 << 14,  // Option: global route: higher priority than focused route (unless active item in focused route).
    UxImGuiInputFlags_RouteOverActive         = 1 << 15,  // Option: global route: higher priority than active item. Unlikely you need to use that: will interfere with every active items, e.g. CTRL+A registered by InputText will be overridden by this. May not be fully honored as user/internal code is likely to always assume they can access keys when active.
    UxImGuiInputFlags_RouteUnlessBgFocused    = 1 << 16,  // Option: global route: will not be applied if underlying background/void is focused (== no Dear UxImGui windows are focused). Useful for overlay applications.
    UxImGuiInputFlags_RouteFromRootWindow     = 1 << 17,  // Option: route evaluated from the point of view of root window rather than current window.

    // Flags for SetNextItemShortcut()
    UxImGuiInputFlags_Tooltip                 = 1 << 18,  // Automatically display a tooltip when hovering item [BETA] Unsure of right api (opt-in/opt-out)
};

// Configuration flags stored in io.ConfigFlags. Set by user/application.
enum UxImGuiConfigFlags_
{
    UxImGuiConfigFlags_None                   = 0,
    UxImGuiConfigFlags_NavEnableKeyboard      = 1 << 0,   // Master keyboard navigation enable flag. Enable full Tabbing + directional arrows + space/enter to activate.
    UxImGuiConfigFlags_NavEnableGamepad       = 1 << 1,   // Master gamepad navigation enable flag. Backend also needs to set UxImGuiBackendFlags_HasGamepad.
    UxImGuiConfigFlags_NoMouse                = 1 << 4,   // Instruct dear imgui to disable mouse inputs and interactions.
    UxImGuiConfigFlags_NoMouseCursorChange    = 1 << 5,   // Instruct backend to not alter mouse cursor shape and visibility. Use if the backend cursor changes are interfering with yours and you don't want to use SetMouseCursor() to change mouse cursor. You may want to honor requests from imgui by reading GetMouseCursor() yourself instead.
    UxImGuiConfigFlags_NoKeyboard             = 1 << 6,   // Instruct dear imgui to disable keyboard inputs and interactions. This is done by ignoring keyboard events and clearing existing states.

    // [BETA] Docking
    UxImGuiConfigFlags_DockingEnable          = 1 << 7,   // Docking enable flags.

    // [BETA] Viewports
    // When using viewports it is recommended that your default value for UxImGuiCol_WindowBg is opaque (Alpha=1.0) so transition to a viewport won't be noticeable.
    UxImGuiConfigFlags_ViewportsEnable        = 1 << 10,  // Viewport enable flags (require both UxImGuiBackendFlags_PlatformHasViewports + UxImGuiBackendFlags_RendererHasViewports set by the respective backends)
    UxImGuiConfigFlags_DpiEnableScaleViewports= 1 << 14,  // [BETA: Don't use] FIXME-DPI: Reposition and resize imgui windows when the DpiScale of a viewport changed (mostly useful for the main viewport hosting other window). Note that resizing the main window itself is up to your application.
    UxImGuiConfigFlags_DpiEnableScaleFonts    = 1 << 15,  // [BETA: Don't use] FIXME-DPI: Request bitmap-scaled fonts to match DpiScale. This is a very low-quality workaround. The correct way to handle DPI is _currently_ to replace the atlas and/or fonts in the Platform_OnChangedViewport callback, but this is all early work in progress.

    // User storage (to allow your backend/engine to communicate to code that may be shared between multiple projects. Those flags are NOT used by core Dear UxImGui)
    UxImGuiConfigFlags_IsSRGB                 = 1 << 20,  // Application is SRGB-aware.
    UxImGuiConfigFlags_IsTouchScreen          = 1 << 21,  // Application is using a touch screen instead of a mouse.

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiConfigFlags_NavEnableSetMousePos   = 1 << 2,   // [moved/renamed in 1.91.4] -> use bool io.ConfigNavMoveSetMousePos
    UxImGuiConfigFlags_NavNoCaptureKeyboard   = 1 << 3,   // [moved/renamed in 1.91.4] -> use bool io.ConfigNavCaptureKeyboard
#endif
};

// Backend capabilities flags stored in io.BackendFlags. Set by imgui_impl_xxx or custom backend.
enum UxImGuiBackendFlags_
{
    UxImGuiBackendFlags_None                  = 0,
    UxImGuiBackendFlags_HasGamepad            = 1 << 0,   // Backend Platform supports gamepad and currently has one connected.
    UxImGuiBackendFlags_HasMouseCursors       = 1 << 1,   // Backend Platform supports honoring GetMouseCursor() value to change the OS cursor shape.
    UxImGuiBackendFlags_HasSetMousePos        = 1 << 2,   // Backend Platform supports io.WantSetMousePos requests to reposition the OS mouse position (only used if io.ConfigNavMoveSetMousePos is set).
    UxImGuiBackendFlags_RendererHasVtxOffset  = 1 << 3,   // Backend Renderer supports UxImDrawCmd::VtxOffset. This enables output of large meshes (64K+ vertices) while still using 16-bit indices.
    UxImGuiBackendFlags_RendererHasTextures   = 1 << 4,   // Backend Renderer supports UxImTextureData requests to create/update/destroy textures. This enables incremental texture updates and texture reloads.

    // [BETA] Viewports
    UxImGuiBackendFlags_PlatformHasViewports  = 1 << 10,  // Backend Platform supports multiple viewports.
    UxImGuiBackendFlags_HasMouseHoveredViewport=1 << 11,  // Backend Platform supports calling io.AddMouseViewportEvent() with the viewport under the mouse. IF POSSIBLE, ignore viewports with the UxImGuiViewportFlags_NoInputs flag (Win32 backend, GLFW 3.30+ backend can do this, SDL backend cannot). If this cannot be done, Dear UxImGui needs to use a flawed heuristic to find the viewport under.
    UxImGuiBackendFlags_RendererHasViewports  = 1 << 12,  // Backend Renderer supports multiple viewports.
};

// Enumeration for PushStyleColor() / PopStyleColor()
enum UxImGuiCol_
{
    UxImGuiCol_Text,
    UxImGuiCol_TextDisabled,
    UxImGuiCol_WindowBg,              // Background of normal windows
    UxImGuiCol_ChildBg,               // Background of child windows
    UxImGuiCol_PopupBg,               // Background of popups, menus, tooltips windows
    UxImGuiCol_Border,
    UxImGuiCol_BorderShadow,
    UxImGuiCol_FrameBg,               // Background of checkbox, radio button, plot, slider, text input
    UxImGuiCol_FrameBgHovered,
    UxImGuiCol_FrameBgActive,
    UxImGuiCol_TitleBg,               // Title bar
    UxImGuiCol_TitleBgActive,         // Title bar when focused
    UxImGuiCol_TitleBgCollapsed,      // Title bar when collapsed
    UxImGuiCol_MenuBarBg,
    UxImGuiCol_ScrollbarBg,
    UxImGuiCol_ScrollbarGrab,
    UxImGuiCol_ScrollbarGrabHovered,
    UxImGuiCol_ScrollbarGrabActive,
    UxImGuiCol_CheckMark,             // Checkbox tick and RadioButton circle
    UxImGuiCol_SliderGrab,
    UxImGuiCol_SliderGrabActive,
    UxImGuiCol_Button,
    UxImGuiCol_ButtonHovered,
    UxImGuiCol_ButtonActive,
    UxImGuiCol_Header,                // Header* colors are used for CollapsingHeader, TreeNode, Selectable, MenuItem
    UxImGuiCol_HeaderHovered,
    UxImGuiCol_HeaderActive,
    UxImGuiCol_Separator,
    UxImGuiCol_SeparatorHovered,
    UxImGuiCol_SeparatorActive,
    UxImGuiCol_ResizeGrip,            // Resize grip in lower-right and lower-left corners of windows.
    UxImGuiCol_ResizeGripHovered,
    UxImGuiCol_ResizeGripActive,
    UxImGuiCol_InputTextCursor,       // InputText cursor/caret
    UxImGuiCol_TabHovered,            // Tab background, when hovered
    UxImGuiCol_Tab,                   // Tab background, when tab-bar is focused & tab is unselected
    UxImGuiCol_TabSelected,           // Tab background, when tab-bar is focused & tab is selected
    UxImGuiCol_TabSelectedOverline,   // Tab horizontal overline, when tab-bar is focused & tab is selected
    UxImGuiCol_TabDimmed,             // Tab background, when tab-bar is unfocused & tab is unselected
    UxImGuiCol_TabDimmedSelected,     // Tab background, when tab-bar is unfocused & tab is selected
    UxImGuiCol_TabDimmedSelectedOverline,//..horizontal overline, when tab-bar is unfocused & tab is selected
    UxImGuiCol_DockingPreview,        // Preview overlay color when about to docking something
    UxImGuiCol_DockingEmptyBg,        // Background color for empty node (e.g. CentralNode with no window docked into it)
    UxImGuiCol_PlotLines,
    UxImGuiCol_PlotLinesHovered,
    UxImGuiCol_PlotHistogram,
    UxImGuiCol_PlotHistogramHovered,
    UxImGuiCol_TableHeaderBg,         // Table header background
    UxImGuiCol_TableBorderStrong,     // Table outer and header borders (prefer using Alpha=1.0 here)
    UxImGuiCol_TableBorderLight,      // Table inner borders (prefer using Alpha=1.0 here)
    UxImGuiCol_TableRowBg,            // Table row background (even rows)
    UxImGuiCol_TableRowBgAlt,         // Table row background (odd rows)
    UxImGuiCol_TextLink,              // Hyperlink color
    UxImGuiCol_TextSelectedBg,        // Selected text inside an InputText
    UxImGuiCol_TreeLines,             // Tree node hierarchy outlines when using UxImGuiTreeNodeFlags_DrawLines
    UxImGuiCol_DragDropTarget,        // Rectangle highlighting a drop target
    UxImGuiCol_NavCursor,             // Color of keyboard/gamepad navigation cursor/rectangle, when visible
    UxImGuiCol_NavWindowingHighlight, // Highlight window when using CTRL+TAB
    UxImGuiCol_NavWindowingDimBg,     // Darken/colorize entire screen behind the CTRL+TAB window list, when active
    UxImGuiCol_ModalWindowDimBg,      // Darken/colorize entire screen behind a modal window, when one is active
    UxImGuiCol_COUNT,

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiCol_TabActive = UxImGuiCol_TabSelected,                  // [renamed in 1.90.9]
    UxImGuiCol_TabUnfocused = UxImGuiCol_TabDimmed,                 // [renamed in 1.90.9]
    UxImGuiCol_TabUnfocusedActive = UxImGuiCol_TabDimmedSelected,   // [renamed in 1.90.9]
    UxImGuiCol_NavHighlight = UxImGuiCol_NavCursor,                 // [renamed in 1.91.4]
#endif
};

// Enumeration for PushStyleVar() / PopStyleVar() to temporarily modify the UxImGuiStyle structure.
// - The enum only refers to fields of UxImGuiStyle which makes sense to be pushed/popped inside UI code.
//   During initialization or between frames, feel free to just poke into UxImGuiStyle directly.
// - Tip: Use your programming IDE navigation facilities on the names in the _second column_ below to find the actual members and their description.
//   - In Visual Studio: CTRL+comma ("Edit.GoToAll") can follow symbols inside comments, whereas CTRL+F12 ("Edit.GoToImplementation") cannot.
//   - In Visual Studio w/ Visual Assist installed: ALT+G ("VAssistX.GoToImplementation") can also follow symbols inside comments.
//   - In VS Code, CLion, etc.: CTRL+click can follow symbols inside comments.
// - When changing this enum, you need to update the associated internal table GStyleVarInfo[] accordingly. This is where we link enum values to members offset/type.
enum UxImGuiStyleVar_
{
    // Enum name -------------------------- // Member in UxImGuiStyle structure (see UxImGuiStyle for descriptions)
    UxImGuiStyleVar_Alpha,                    // float     Alpha
    UxImGuiStyleVar_DisabledAlpha,            // float     DisabledAlpha
    UxImGuiStyleVar_WindowPadding,            // UxImVec2    WindowPadding
    UxImGuiStyleVar_WindowRounding,           // float     WindowRounding
    UxImGuiStyleVar_WindowBorderSize,         // float     WindowBorderSize
    UxImGuiStyleVar_WindowMinSize,            // UxImVec2    WindowMinSize
    UxImGuiStyleVar_WindowTitleAlign,         // UxImVec2    WindowTitleAlign
    UxImGuiStyleVar_ChildRounding,            // float     ChildRounding
    UxImGuiStyleVar_ChildBorderSize,          // float     ChildBorderSize
    UxImGuiStyleVar_PopupRounding,            // float     PopupRounding
    UxImGuiStyleVar_PopupBorderSize,          // float     PopupBorderSize
    UxImGuiStyleVar_FramePadding,             // UxImVec2    FramePadding
    UxImGuiStyleVar_FrameRounding,            // float     FrameRounding
    UxImGuiStyleVar_FrameBorderSize,          // float     FrameBorderSize
    UxImGuiStyleVar_ItemSpacing,              // UxImVec2    ItemSpacing
    UxImGuiStyleVar_ItemInnerSpacing,         // UxImVec2    ItemInnerSpacing
    UxImGuiStyleVar_IndentSpacing,            // float     IndentSpacing
    UxImGuiStyleVar_CellPadding,              // UxImVec2    CellPadding
    UxImGuiStyleVar_ScrollbarSize,            // float     ScrollbarSize
    UxImGuiStyleVar_ScrollbarRounding,        // float     ScrollbarRounding
    UxImGuiStyleVar_GrabMinSize,              // float     GrabMinSize
    UxImGuiStyleVar_GrabRounding,             // float     GrabRounding
    UxImGuiStyleVar_ImageBorderSize,          // float     ImageBorderSize
    UxImGuiStyleVar_TabRounding,              // float     TabRounding
    UxImGuiStyleVar_TabBorderSize,            // float     TabBorderSize
    UxImGuiStyleVar_TabBarBorderSize,         // float     TabBarBorderSize
    UxImGuiStyleVar_TabBarOverlineSize,       // float     TabBarOverlineSize
    UxImGuiStyleVar_TableAngledHeadersAngle,  // float     TableAngledHeadersAngle
    UxImGuiStyleVar_TableAngledHeadersTextAlign,// UxImVec2  TableAngledHeadersTextAlign
    UxImGuiStyleVar_TreeLinesSize,            // float     TreeLinesSize
    UxImGuiStyleVar_TreeLinesRounding,        // float     TreeLinesRounding
    UxImGuiStyleVar_ButtonTextAlign,          // UxImVec2    ButtonTextAlign
    UxImGuiStyleVar_SelectableTextAlign,      // UxImVec2    SelectableTextAlign
    UxImGuiStyleVar_SeparatorTextBorderSize,  // float     SeparatorTextBorderSize
    UxImGuiStyleVar_SeparatorTextAlign,       // UxImVec2    SeparatorTextAlign
    UxImGuiStyleVar_SeparatorTextPadding,     // UxImVec2    SeparatorTextPadding
    UxImGuiStyleVar_DockingSeparatorSize,     // float     DockingSeparatorSize
    UxImGuiStyleVar_COUNT
};

// Flags for InvisibleButton() [extended in imgui_internal.h]
enum UxImGuiButtonFlags_
{
    UxImGuiButtonFlags_None                   = 0,
    UxImGuiButtonFlags_MouseButtonLeft        = 1 << 0,   // React on left mouse button (default)
    UxImGuiButtonFlags_MouseButtonRight       = 1 << 1,   // React on right mouse button
    UxImGuiButtonFlags_MouseButtonMiddle      = 1 << 2,   // React on center mouse button
    UxImGuiButtonFlags_MouseButtonMask_       = UxImGuiButtonFlags_MouseButtonLeft | UxImGuiButtonFlags_MouseButtonRight | UxImGuiButtonFlags_MouseButtonMiddle, // [Internal]
    UxImGuiButtonFlags_EnableNav              = 1 << 3,   // InvisibleButton(): do not disable navigation/tabbing. Otherwise disabled by default.
};

// Flags for ColorEdit3() / ColorEdit4() / ColorPicker3() / ColorPicker4() / ColorButton()
enum UxImGuiColorEditFlags_
{
    UxImGuiColorEditFlags_None            = 0,
    UxImGuiColorEditFlags_NoAlpha         = 1 << 1,   //              // ColorEdit, ColorPicker, ColorButton: ignore Alpha component (will only read 3 components from the input pointer).
    UxImGuiColorEditFlags_NoPicker        = 1 << 2,   //              // ColorEdit: disable picker when clicking on color square.
    UxImGuiColorEditFlags_NoOptions       = 1 << 3,   //              // ColorEdit: disable toggling options menu when right-clicking on inputs/small preview.
    UxImGuiColorEditFlags_NoSmallPreview  = 1 << 4,   //              // ColorEdit, ColorPicker: disable color square preview next to the inputs. (e.g. to show only the inputs)
    UxImGuiColorEditFlags_NoInputs        = 1 << 5,   //              // ColorEdit, ColorPicker: disable inputs sliders/text widgets (e.g. to show only the small preview color square).
    UxImGuiColorEditFlags_NoTooltip       = 1 << 6,   //              // ColorEdit, ColorPicker, ColorButton: disable tooltip when hovering the preview.
    UxImGuiColorEditFlags_NoLabel         = 1 << 7,   //              // ColorEdit, ColorPicker: disable display of inline text label (the label is still forwarded to the tooltip and picker).
    UxImGuiColorEditFlags_NoSidePreview   = 1 << 8,   //              // ColorPicker: disable bigger color preview on right side of the picker, use small color square preview instead.
    UxImGuiColorEditFlags_NoDragDrop      = 1 << 9,   //              // ColorEdit: disable drag and drop target. ColorButton: disable drag and drop source.
    UxImGuiColorEditFlags_NoBorder        = 1 << 10,  //              // ColorButton: disable border (which is enforced by default)

    // Alpha preview
    // - Prior to 1.91.8 (2025/01/21): alpha was made opaque in the preview by default using old name UxImGuiColorEditFlags_AlphaPreview.
    // - We now display the preview as transparent by default. You can use UxImGuiColorEditFlags_AlphaOpaque to use old behavior.
    // - The new flags may be combined better and allow finer controls.
    UxImGuiColorEditFlags_AlphaOpaque     = 1 << 11,  //              // ColorEdit, ColorPicker, ColorButton: disable alpha in the preview,. Contrary to _NoAlpha it may still be edited when calling ColorEdit4()/ColorPicker4(). For ColorButton() this does the same as _NoAlpha.
    UxImGuiColorEditFlags_AlphaNoBg       = 1 << 12,  //              // ColorEdit, ColorPicker, ColorButton: disable rendering a checkerboard background behind transparent color.
    UxImGuiColorEditFlags_AlphaPreviewHalf= 1 << 13,  //              // ColorEdit, ColorPicker, ColorButton: display half opaque / half transparent preview.

    // User Options (right-click on widget to change some of them).
    UxImGuiColorEditFlags_AlphaBar        = 1 << 16,  //              // ColorEdit, ColorPicker: show vertical alpha bar/gradient in picker.
    UxImGuiColorEditFlags_HDR             = 1 << 19,  //              // (WIP) ColorEdit: Currently only disable 0.0f..1.0f limits in RGBA edition (note: you probably want to use UxImGuiColorEditFlags_Float flag as well).
    UxImGuiColorEditFlags_DisplayRGB      = 1 << 20,  // [Display]    // ColorEdit: override _display_ type among RGB/HSV/Hex. ColorPicker: select any combination using one or more of RGB/HSV/Hex.
    UxImGuiColorEditFlags_DisplayHSV      = 1 << 21,  // [Display]    // "
    UxImGuiColorEditFlags_DisplayHex      = 1 << 22,  // [Display]    // "
    UxImGuiColorEditFlags_Uint8           = 1 << 23,  // [DataType]   // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0..255.
    UxImGuiColorEditFlags_Float           = 1 << 24,  // [DataType]   // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0.0f..1.0f floats instead of 0..255 integers. No round-trip of value via integers.
    UxImGuiColorEditFlags_PickerHueBar    = 1 << 25,  // [Picker]     // ColorPicker: bar for Hue, rectangle for Sat/Value.
    UxImGuiColorEditFlags_PickerHueWheel  = 1 << 26,  // [Picker]     // ColorPicker: wheel for Hue, triangle for Sat/Value.
    UxImGuiColorEditFlags_InputRGB        = 1 << 27,  // [Input]      // ColorEdit, ColorPicker: input and output data in RGB format.
    UxImGuiColorEditFlags_InputHSV        = 1 << 28,  // [Input]      // ColorEdit, ColorPicker: input and output data in HSV format.

    // Defaults Options. You can set application defaults using SetColorEditOptions(). The intent is that you probably don't want to
    // override them in most of your calls. Let the user choose via the option menu and/or call SetColorEditOptions() once during startup.
    UxImGuiColorEditFlags_DefaultOptions_ = UxImGuiColorEditFlags_Uint8 | UxImGuiColorEditFlags_DisplayRGB | UxImGuiColorEditFlags_InputRGB | UxImGuiColorEditFlags_PickerHueBar,

    // [Internal] Masks
    UxImGuiColorEditFlags_AlphaMask_      = UxImGuiColorEditFlags_NoAlpha | UxImGuiColorEditFlags_AlphaOpaque | UxImGuiColorEditFlags_AlphaNoBg | UxImGuiColorEditFlags_AlphaPreviewHalf,
    UxImGuiColorEditFlags_DisplayMask_    = UxImGuiColorEditFlags_DisplayRGB | UxImGuiColorEditFlags_DisplayHSV | UxImGuiColorEditFlags_DisplayHex,
    UxImGuiColorEditFlags_DataTypeMask_   = UxImGuiColorEditFlags_Uint8 | UxImGuiColorEditFlags_Float,
    UxImGuiColorEditFlags_PickerMask_     = UxImGuiColorEditFlags_PickerHueWheel | UxImGuiColorEditFlags_PickerHueBar,
    UxImGuiColorEditFlags_InputMask_      = UxImGuiColorEditFlags_InputRGB | UxImGuiColorEditFlags_InputHSV,

    // Obsolete names
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiColorEditFlags_AlphaPreview = 0,         // [Removed in 1.91.8] This is the default now. Will display a checkerboard unless UxImGuiColorEditFlags_AlphaNoBg is set.
#endif
    //UxImGuiColorEditFlags_RGB = UxImGuiColorEditFlags_DisplayRGB, UxImGuiColorEditFlags_HSV = UxImGuiColorEditFlags_DisplayHSV, UxImGuiColorEditFlags_HEX = UxImGuiColorEditFlags_DisplayHex  // [renamed in 1.69]
};

// Flags for DragFloat(), DragInt(), SliderFloat(), SliderInt() etc.
// We use the same sets of flags for DragXXX() and SliderXXX() functions as the features are the same and it makes it easier to swap them.
// (Those are per-item flags. There is shared behavior flag too: UxImGuiIO: io.ConfigDragClickToInputText)
enum UxImGuiSliderFlags_
{
    UxImGuiSliderFlags_None               = 0,
    UxImGuiSliderFlags_Logarithmic        = 1 << 5,       // Make the widget logarithmic (linear otherwise). Consider using UxImGuiSliderFlags_NoRoundToFormat with this if using a format-string with small amount of digits.
    UxImGuiSliderFlags_NoRoundToFormat    = 1 << 6,       // Disable rounding underlying value to match precision of the display format string (e.g. %.3f values are rounded to those 3 digits).
    UxImGuiSliderFlags_NoInput            = 1 << 7,       // Disable CTRL+Click or Enter key allowing to input text directly into the widget.
    UxImGuiSliderFlags_WrapAround         = 1 << 8,       // Enable wrapping around from max to min and from min to max. Only supported by DragXXX() functions for now.
    UxImGuiSliderFlags_ClampOnInput       = 1 << 9,       // Clamp value to min/max bounds when input manually with CTRL+Click. By default CTRL+Click allows going out of bounds.
    UxImGuiSliderFlags_ClampZeroRange     = 1 << 10,      // Clamp even if min==max==0.0f. Otherwise due to legacy reason DragXXX functions don't clamp with those values. When your clamping limits are dynamic you almost always want to use it.
    UxImGuiSliderFlags_NoSpeedTweaks      = 1 << 11,      // Disable keyboard modifiers altering tweak speed. Useful if you want to alter tweak speed yourself based on your own logic.
    UxImGuiSliderFlags_AlwaysClamp        = UxImGuiSliderFlags_ClampOnInput | UxImGuiSliderFlags_ClampZeroRange,
    UxImGuiSliderFlags_InvalidMask_       = 0x7000000F,   // [Internal] We treat using those bits as being potentially a 'float power' argument from the previous API that has got miscast to this enum, and will trigger an assert if needed.
};

// Identify a mouse button.
// Those values are guaranteed to be stable and we frequently use 0/1 directly. Named enums provided for convenience.
enum UxImGuiMouseButton_
{
    UxImGuiMouseButton_Left = 0,
    UxImGuiMouseButton_Right = 1,
    UxImGuiMouseButton_Middle = 2,
    UxImGuiMouseButton_COUNT = 5
};

// Enumeration for GetMouseCursor()
// User code may request backend to display given cursor by calling SetMouseCursor(), which is why we have some cursors that are marked unused here
enum UxImGuiMouseCursor_
{
    UxImGuiMouseCursor_None = -1,
    UxImGuiMouseCursor_Arrow = 0,
    UxImGuiMouseCursor_TextInput,         // When hovering over InputText, etc.
    UxImGuiMouseCursor_ResizeAll,         // (Unused by Dear UxImGui functions)
    UxImGuiMouseCursor_ResizeNS,          // When hovering over a horizontal border
    UxImGuiMouseCursor_ResizeEW,          // When hovering over a vertical border or a column
    UxImGuiMouseCursor_ResizeNESW,        // When hovering over the bottom-left corner of a window
    UxImGuiMouseCursor_ResizeNWSE,        // When hovering over the bottom-right corner of a window
    UxImGuiMouseCursor_Hand,              // (Unused by Dear UxImGui functions. Use for e.g. hyperlinks)
    UxImGuiMouseCursor_Wait,              // When waiting for something to process/load.
    UxImGuiMouseCursor_Progress,          // When waiting for something to process/load, but application is still interactive.
    UxImGuiMouseCursor_NotAllowed,        // When hovering something with disallowed interaction. Usually a crossed circle.
    UxImGuiMouseCursor_COUNT
};

// Enumeration for AddMouseSourceEvent() actual source of Mouse Input data.
// Historically we use "Mouse" terminology everywhere to indicate pointer data, e.g. MousePos, IsMousePressed(), io.AddMousePosEvent()
// But that "Mouse" data can come from different source which occasionally may be useful for application to know about.
// You can submit a change of pointer type using io.AddMouseSourceEvent().
enum UxImGuiMouseSource : int
{
    UxImGuiMouseSource_Mouse = 0,         // Input is coming from an actual mouse.
    UxImGuiMouseSource_TouchScreen,       // Input is coming from a touch screen (no hovering prior to initial press, less precise initial press aiming, dual-axis wheeling possible).
    UxImGuiMouseSource_Pen,               // Input is coming from a pressure/magnetic pen (often used in conjunction with high-sampling rates).
    UxImGuiMouseSource_COUNT
};

// Enumeration for UxImGui::SetNextWindow***(), SetWindow***(), SetNextItem***() functions
// Represent a condition.
// Important: Treat as a regular enum! Do NOT combine multiple values using binary operators! All the functions above treat 0 as a shortcut to UxImGuiCond_Always.
enum UxImGuiCond_
{
    UxImGuiCond_None          = 0,        // No condition (always set the variable), same as _Always
    UxImGuiCond_Always        = 1 << 0,   // No condition (always set the variable), same as _None
    UxImGuiCond_Once          = 1 << 1,   // Set the variable once per runtime session (only the first call will succeed)
    UxImGuiCond_FirstUseEver  = 1 << 2,   // Set the variable if the object/window has no persistently saved data (no entry in .ini file)
    UxImGuiCond_Appearing     = 1 << 3,   // Set the variable if the object/window is appearing after being hidden/inactive (or the first time)
};

//-----------------------------------------------------------------------------
// [SECTION] Tables API flags and structures (UxImGuiTableFlags, UxImGuiTableColumnFlags, UxImGuiTableRowFlags, UxImGuiTableBgTarget, UxImGuiTableSortSpecs, UxImGuiTableColumnSortSpecs)
//-----------------------------------------------------------------------------

// Flags for UxImGui::BeginTable()
// - Important! Sizing policies have complex and subtle side effects, much more so than you would expect.
//   Read comments/demos carefully + experiment with live demos to get acquainted with them.
// - The DEFAULT sizing policies are:
//    - Default to UxImGuiTableFlags_SizingFixedFit    if ScrollX is on, or if host window has UxImGuiWindowFlags_AlwaysAutoResize.
//    - Default to UxImGuiTableFlags_SizingStretchSame if ScrollX is off.
// - When ScrollX is off:
//    - Table defaults to UxImGuiTableFlags_SizingStretchSame -> all Columns defaults to UxImGuiTableColumnFlags_WidthStretch with same weight.
//    - Columns sizing policy allowed: Stretch (default), Fixed/Auto.
//    - Fixed Columns (if any) will generally obtain their requested width (unless the table cannot fit them all).
//    - Stretch Columns will share the remaining width according to their respective weight.
//    - Mixed Fixed/Stretch columns is possible but has various side-effects on resizing behaviors.
//      The typical use of mixing sizing policies is: any number of LEADING Fixed columns, followed by one or two TRAILING Stretch columns.
//      (this is because the visible order of columns have subtle but necessary effects on how they react to manual resizing).
// - When ScrollX is on:
//    - Table defaults to UxImGuiTableFlags_SizingFixedFit -> all Columns defaults to UxImGuiTableColumnFlags_WidthFixed
//    - Columns sizing policy allowed: Fixed/Auto mostly.
//    - Fixed Columns can be enlarged as needed. Table will show a horizontal scrollbar if needed.
//    - When using auto-resizing (non-resizable) fixed columns, querying the content width to use item right-alignment e.g. SetNextItemWidth(-FLT_MIN) doesn't make sense, would create a feedback loop.
//    - Using Stretch columns OFTEN DOES NOT MAKE SENSE if ScrollX is on, UNLESS you have specified a value for 'inner_width' in BeginTable().
//      If you specify a value for 'inner_width' then effectively the scrolling space is known and Stretch or mixed Fixed/Stretch columns become meaningful again.
// - Read on documentation at the top of imgui_tables.cpp for details.
enum UxImGuiTableFlags_
{
    // Features
    UxImGuiTableFlags_None                       = 0,
    UxImGuiTableFlags_Resizable                  = 1 << 0,   // Enable resizing columns.
    UxImGuiTableFlags_Reorderable                = 1 << 1,   // Enable reordering columns in header row (need calling TableSetupColumn() + TableHeadersRow() to display headers)
    UxImGuiTableFlags_Hideable                   = 1 << 2,   // Enable hiding/disabling columns in context menu.
    UxImGuiTableFlags_Sortable                   = 1 << 3,   // Enable sorting. Call TableGetSortSpecs() to obtain sort specs. Also see UxImGuiTableFlags_SortMulti and UxImGuiTableFlags_SortTristate.
    UxImGuiTableFlags_NoSavedSettings            = 1 << 4,   // Disable persisting columns order, width and sort settings in the .ini file.
    UxImGuiTableFlags_ContextMenuInBody          = 1 << 5,   // Right-click on columns body/contents will display table context menu. By default it is available in TableHeadersRow().
    // Decorations
    UxImGuiTableFlags_RowBg                      = 1 << 6,   // Set each RowBg color with UxImGuiCol_TableRowBg or UxImGuiCol_TableRowBgAlt (equivalent of calling TableSetBgColor with UxImGuiTableBgFlags_RowBg0 on each row manually)
    UxImGuiTableFlags_BordersInnerH              = 1 << 7,   // Draw horizontal borders between rows.
    UxImGuiTableFlags_BordersOuterH              = 1 << 8,   // Draw horizontal borders at the top and bottom.
    UxImGuiTableFlags_BordersInnerV              = 1 << 9,   // Draw vertical borders between columns.
    UxImGuiTableFlags_BordersOuterV              = 1 << 10,  // Draw vertical borders on the left and right sides.
    UxImGuiTableFlags_BordersH                   = UxImGuiTableFlags_BordersInnerH | UxImGuiTableFlags_BordersOuterH, // Draw horizontal borders.
    UxImGuiTableFlags_BordersV                   = UxImGuiTableFlags_BordersInnerV | UxImGuiTableFlags_BordersOuterV, // Draw vertical borders.
    UxImGuiTableFlags_BordersInner               = UxImGuiTableFlags_BordersInnerV | UxImGuiTableFlags_BordersInnerH, // Draw inner borders.
    UxImGuiTableFlags_BordersOuter               = UxImGuiTableFlags_BordersOuterV | UxImGuiTableFlags_BordersOuterH, // Draw outer borders.
    UxImGuiTableFlags_Borders                    = UxImGuiTableFlags_BordersInner | UxImGuiTableFlags_BordersOuter,   // Draw all borders.
    UxImGuiTableFlags_NoBordersInBody            = 1 << 11,  // [ALPHA] Disable vertical borders in columns Body (borders will always appear in Headers). -> May move to style
    UxImGuiTableFlags_NoBordersInBodyUntilResize = 1 << 12,  // [ALPHA] Disable vertical borders in columns Body until hovered for resize (borders will always appear in Headers). -> May move to style
    // Sizing Policy (read above for defaults)
    UxImGuiTableFlags_SizingFixedFit             = 1 << 13,  // Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching contents width.
    UxImGuiTableFlags_SizingFixedSame            = 2 << 13,  // Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching the maximum contents width of all columns. Implicitly enable UxImGuiTableFlags_NoKeepColumnsVisible.
    UxImGuiTableFlags_SizingStretchProp          = 3 << 13,  // Columns default to _WidthStretch with default weights proportional to each columns contents widths.
    UxImGuiTableFlags_SizingStretchSame          = 4 << 13,  // Columns default to _WidthStretch with default weights all equal, unless overridden by TableSetupColumn().
    // Sizing Extra Options
    UxImGuiTableFlags_NoHostExtendX              = 1 << 16,  // Make outer width auto-fit to columns, overriding outer_size.x value. Only available when ScrollX/ScrollY are disabled and Stretch columns are not used.
    UxImGuiTableFlags_NoHostExtendY              = 1 << 17,  // Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit). Only available when ScrollX/ScrollY are disabled. Data below the limit will be clipped and not visible.
    UxImGuiTableFlags_NoKeepColumnsVisible       = 1 << 18,  // Disable keeping column always minimally visible when ScrollX is off and table gets too small. Not recommended if columns are resizable.
    UxImGuiTableFlags_PreciseWidths              = 1 << 19,  // Disable distributing remainder width to stretched columns (width allocation on a 100-wide table with 3 columns: Without this flag: 33,33,34. With this flag: 33,33,33). With larger number of columns, resizing will appear to be less smooth.
    // Clipping
    UxImGuiTableFlags_NoClip                     = 1 << 20,  // Disable clipping rectangle for every individual columns (reduce draw command count, items will be able to overflow into other columns). Generally incompatible with TableSetupScrollFreeze().
    // Padding
    UxImGuiTableFlags_PadOuterX                  = 1 << 21,  // Default if BordersOuterV is on. Enable outermost padding. Generally desirable if you have headers.
    UxImGuiTableFlags_NoPadOuterX                = 1 << 22,  // Default if BordersOuterV is off. Disable outermost padding.
    UxImGuiTableFlags_NoPadInnerX                = 1 << 23,  // Disable inner padding between columns (double inner padding if BordersOuterV is on, single inner padding if BordersOuterV is off).
    // Scrolling
    UxImGuiTableFlags_ScrollX                    = 1 << 24,  // Enable horizontal scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size. Changes default sizing policy. Because this creates a child window, ScrollY is currently generally recommended when using ScrollX.
    UxImGuiTableFlags_ScrollY                    = 1 << 25,  // Enable vertical scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size.
    // Sorting
    UxImGuiTableFlags_SortMulti                  = 1 << 26,  // Hold shift when clicking headers to sort on multiple column. TableGetSortSpecs() may return specs where (SpecsCount > 1).
    UxImGuiTableFlags_SortTristate               = 1 << 27,  // Allow no sorting, disable default sorting. TableGetSortSpecs() may return specs where (SpecsCount == 0).
    // Miscellaneous
    UxImGuiTableFlags_HighlightHoveredColumn     = 1 << 28,  // Highlight column headers when hovered (may evolve into a fuller highlight)

    // [Internal] Combinations and masks
    UxImGuiTableFlags_SizingMask_                = UxImGuiTableFlags_SizingFixedFit | UxImGuiTableFlags_SizingFixedSame | UxImGuiTableFlags_SizingStretchProp | UxImGuiTableFlags_SizingStretchSame,
};

// Flags for UxImGui::TableSetupColumn()
enum UxImGuiTableColumnFlags_
{
    // Input configuration flags
    UxImGuiTableColumnFlags_None                  = 0,
    UxImGuiTableColumnFlags_Disabled              = 1 << 0,   // Overriding/master disable flag: hide column, won't show in context menu (unlike calling TableSetColumnEnabled() which manipulates the user accessible state)
    UxImGuiTableColumnFlags_DefaultHide           = 1 << 1,   // Default as a hidden/disabled column.
    UxImGuiTableColumnFlags_DefaultSort           = 1 << 2,   // Default as a sorting column.
    UxImGuiTableColumnFlags_WidthStretch          = 1 << 3,   // Column will stretch. Preferable with horizontal scrolling disabled (default if table sizing policy is _SizingStretchSame or _SizingStretchProp).
    UxImGuiTableColumnFlags_WidthFixed            = 1 << 4,   // Column will not stretch. Preferable with horizontal scrolling enabled (default if table sizing policy is _SizingFixedFit and table is resizable).
    UxImGuiTableColumnFlags_NoResize              = 1 << 5,   // Disable manual resizing.
    UxImGuiTableColumnFlags_NoReorder             = 1 << 6,   // Disable manual reordering this column, this will also prevent other columns from crossing over this column.
    UxImGuiTableColumnFlags_NoHide                = 1 << 7,   // Disable ability to hide/disable this column.
    UxImGuiTableColumnFlags_NoClip                = 1 << 8,   // Disable clipping for this column (all NoClip columns will render in a same draw command).
    UxImGuiTableColumnFlags_NoSort                = 1 << 9,   // Disable ability to sort on this field (even if UxImGuiTableFlags_Sortable is set on the table).
    UxImGuiTableColumnFlags_NoSortAscending       = 1 << 10,  // Disable ability to sort in the ascending direction.
    UxImGuiTableColumnFlags_NoSortDescending      = 1 << 11,  // Disable ability to sort in the descending direction.
    UxImGuiTableColumnFlags_NoHeaderLabel         = 1 << 12,  // TableHeadersRow() will submit an empty label for this column. Convenient for some small columns. Name will still appear in context menu or in angled headers. You may append into this cell by calling TableSetColumnIndex() right after the TableHeadersRow() call.
    UxImGuiTableColumnFlags_NoHeaderWidth         = 1 << 13,  // Disable header text width contribution to automatic column width.
    UxImGuiTableColumnFlags_PreferSortAscending   = 1 << 14,  // Make the initial sort direction Ascending when first sorting on this column (default).
    UxImGuiTableColumnFlags_PreferSortDescending  = 1 << 15,  // Make the initial sort direction Descending when first sorting on this column.
    UxImGuiTableColumnFlags_IndentEnable          = 1 << 16,  // Use current Indent value when entering cell (default for column 0).
    UxImGuiTableColumnFlags_IndentDisable         = 1 << 17,  // Ignore current Indent value when entering cell (default for columns > 0). Indentation changes _within_ the cell will still be honored.
    UxImGuiTableColumnFlags_AngledHeader          = 1 << 18,  // TableHeadersRow() will submit an angled header row for this column. Note this will add an extra row.

    // Output status flags, read-only via TableGetColumnFlags()
    UxImGuiTableColumnFlags_IsEnabled             = 1 << 24,  // Status: is enabled == not hidden by user/api (referred to as "Hide" in _DefaultHide and _NoHide) flags.
    UxImGuiTableColumnFlags_IsVisible             = 1 << 25,  // Status: is visible == is enabled AND not clipped by scrolling.
    UxImGuiTableColumnFlags_IsSorted              = 1 << 26,  // Status: is currently part of the sort specs
    UxImGuiTableColumnFlags_IsHovered             = 1 << 27,  // Status: is hovered by mouse

    // [Internal] Combinations and masks
    UxImGuiTableColumnFlags_WidthMask_            = UxImGuiTableColumnFlags_WidthStretch | UxImGuiTableColumnFlags_WidthFixed,
    UxImGuiTableColumnFlags_IndentMask_           = UxImGuiTableColumnFlags_IndentEnable | UxImGuiTableColumnFlags_IndentDisable,
    UxImGuiTableColumnFlags_StatusMask_           = UxImGuiTableColumnFlags_IsEnabled | UxImGuiTableColumnFlags_IsVisible | UxImGuiTableColumnFlags_IsSorted | UxImGuiTableColumnFlags_IsHovered,
    UxImGuiTableColumnFlags_NoDirectResize_       = 1 << 30,  // [Internal] Disable user resizing this column directly (it may however we resized indirectly from its left edge)
};

// Flags for UxImGui::TableNextRow()
enum UxImGuiTableRowFlags_
{
    UxImGuiTableRowFlags_None                     = 0,
    UxImGuiTableRowFlags_Headers                  = 1 << 0,   // Identify header row (set default background color + width of its contents accounted differently for auto column width)
};

// Enum for UxImGui::TableSetBgColor()
// Background colors are rendering in 3 layers:
//  - Layer 0: draw with RowBg0 color if set, otherwise draw with ColumnBg0 if set.
//  - Layer 1: draw with RowBg1 color if set, otherwise draw with ColumnBg1 if set.
//  - Layer 2: draw with CellBg color if set.
// The purpose of the two row/columns layers is to let you decide if a background color change should override or blend with the existing color.
// When using UxImGuiTableFlags_RowBg on the table, each row has the RowBg0 color automatically set for odd/even rows.
// If you set the color of RowBg0 target, your color will override the existing RowBg0 color.
// If you set the color of RowBg1 or ColumnBg1 target, your color will blend over the RowBg0 color.
enum UxImGuiTableBgTarget_
{
    UxImGuiTableBgTarget_None                     = 0,
    UxImGuiTableBgTarget_RowBg0                   = 1,        // Set row background color 0 (generally used for background, automatically set when UxImGuiTableFlags_RowBg is used)
    UxImGuiTableBgTarget_RowBg1                   = 2,        // Set row background color 1 (generally used for selection marking)
    UxImGuiTableBgTarget_CellBg                   = 3,        // Set cell background color (top-most color)
};

// Sorting specifications for a table (often handling sort specs for a single column, occasionally more)
// Obtained by calling TableGetSortSpecs().
// When 'SpecsDirty == true' you can sort your data. It will be true with sorting specs have changed since last call, or the first time.
// Make sure to set 'SpecsDirty = false' after sorting, else you may wastefully sort your data every frame!
struct UxImGuiTableSortSpecs
{
    const UxImGuiTableColumnSortSpecs* Specs;     // Pointer to sort spec array.
    int                         SpecsCount;     // Sort spec count. Most often 1. May be > 1 when UxImGuiTableFlags_SortMulti is enabled. May be == 0 when UxImGuiTableFlags_SortTristate is enabled.
    bool                        SpecsDirty;     // Set to true when specs have changed since last time! Use this to sort again, then clear the flag.

    UxImGuiTableSortSpecs()       { memset(this, 0, sizeof(*this)); }
};

// Sorting specification for one column of a table (sizeof == 12 bytes)
struct UxImGuiTableColumnSortSpecs
{
    UxImGuiID                     ColumnUserID;       // User id of the column (if specified by a TableSetupColumn() call)
    UxImS16                       ColumnIndex;        // Index of the column
    UxImS16                       SortOrder;          // Index within parent UxImGuiTableSortSpecs (always stored in order starting from 0, tables sorted on a single criteria will always have a 0 here)
    UxImGuiSortDirection          SortDirection;      // UxImGuiSortDirection_Ascending or UxImGuiSortDirection_Descending

    UxImGuiTableColumnSortSpecs() { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// [SECTION] Helpers: Debug log, memory allocations macros, UxImVector<>
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Debug Logging into ShowDebugLogWindow(), tty and more.
//-----------------------------------------------------------------------------

#ifndef IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DEBUG_LOG(...)        UxImGui::DebugLog(__VA_ARGS__)
#else
#define IMGUI_DEBUG_LOG(...)        ((void)0)
#endif

//-----------------------------------------------------------------------------
// IM_MALLOC(), IM_FREE(), IM_NEW(), IM_PLACEMENT_NEW(), IM_DELETE()
// We call C++ constructor on own allocated memory via the placement "new(ptr) Type()" syntax.
// Defining a custom placement new() with a custom parameter allows us to bypass including <new> which on some platforms complains when user has disabled exceptions.
//-----------------------------------------------------------------------------

struct UxImNewWrapper {};
inline void* operator new(size_t, UxImNewWrapper, void* ptr) { return ptr; }
inline void  operator delete(void*, UxImNewWrapper, void*)   {} // This is only required so we can use the symmetrical new()
#define IM_ALLOC(_SIZE)                     UxImGui::MemAlloc(_SIZE)
#define IM_FREE(_PTR)                       UxImGui::MemFree(_PTR)
#define IM_PLACEMENT_NEW(_PTR)              new(UxImNewWrapper(), _PTR)
#define IM_NEW(_TYPE)                       new(UxImNewWrapper(), UxImGui::MemAlloc(sizeof(_TYPE))) _TYPE
template<typename T> void IM_DELETE(T* p)   { if (p) { p->~T(); UxImGui::MemFree(p); } }

//-----------------------------------------------------------------------------
// UxImVector<>
// Lightweight std::vector<>-like class to avoid dragging dependencies (also, some implementations of STL with debug enabled are absurdly slow, we bypass it so our code runs fast in debug).
//-----------------------------------------------------------------------------
// - You generally do NOT need to care or use this ever. But we need to make it available in imgui.h because some of our public structures are relying on it.
// - We use std-like naming convention here, which is a little unusual for this codebase.
// - Important: clear() frees memory, resize(0) keep the allocated buffer. We use resize(0) a lot to intentionally recycle allocated buffers across frames and amortize our costs.
// - Important: our implementation does NOT call C++ constructors/destructors, we treat everything as raw data! This is intentional but be extra mindful of that,
//   Do NOT use this class as a std::vector replacement in your own code! Many of the structures used by dear imgui can be safely initialized by a zero-memset.
//-----------------------------------------------------------------------------

IM_MSVC_RUNTIME_CHECKS_OFF
template<typename T>
struct UxImVector
{
    int                 Size;
    int                 Capacity;
    T*                  Data;

    // Provide standard typedefs but we don't use them ourselves.
    typedef T                   value_type;
    typedef value_type*         iterator;
    typedef const value_type*   const_iterator;

    // Constructors, destructor
    inline UxImVector()                                       { Size = Capacity = 0; Data = NULL; }
    inline UxImVector(const UxImVector<T>& src)                 { Size = Capacity = 0; Data = NULL; operator=(src); }
    inline UxImVector<T>& operator=(const UxImVector<T>& src)   { clear(); resize(src.Size); if (src.Data) memcpy(Data, src.Data, (size_t)Size * sizeof(T)); return *this; }
    inline ~UxImVector()                                      { if (Data) IM_FREE(Data); } // Important: does not destruct anything

    inline void         clear()                             { if (Data) { Size = Capacity = 0; IM_FREE(Data); Data = NULL; } }  // Important: does not destruct anything
    inline void         clear_delete()                      { for (int n = 0; n < Size; n++) IM_DELETE(Data[n]); clear(); }     // Important: never called automatically! always explicit.
    inline void         clear_destruct()                    { for (int n = 0; n < Size; n++) Data[n].~T(); clear(); }           // Important: never called automatically! always explicit.

    inline bool         empty() const                       { return Size == 0; }
    inline int          size() const                        { return Size; }
    inline int          size_in_bytes() const               { return Size * (int)sizeof(T); }
    inline int          max_size() const                    { return 0x7FFFFFFF / (int)sizeof(T); }
    inline int          capacity() const                    { return Capacity; }
    inline T&           operator[](int i)                   { IM_ASSERT(i >= 0 && i < Size); return Data[i]; }
    inline const T&     operator[](int i) const             { IM_ASSERT(i >= 0 && i < Size); return Data[i]; }

    inline T*           begin()                             { return Data; }
    inline const T*     begin() const                       { return Data; }
    inline T*           end()                               { return Data + Size; }
    inline const T*     end() const                         { return Data + Size; }
    inline T&           front()                             { IM_ASSERT(Size > 0); return Data[0]; }
    inline const T&     front() const                       { IM_ASSERT(Size > 0); return Data[0]; }
    inline T&           back()                              { IM_ASSERT(Size > 0); return Data[Size - 1]; }
    inline const T&     back() const                        { IM_ASSERT(Size > 0); return Data[Size - 1]; }
    inline void         swap(UxImVector<T>& rhs)              { int rhs_size = rhs.Size; rhs.Size = Size; Size = rhs_size; int rhs_cap = rhs.Capacity; rhs.Capacity = Capacity; Capacity = rhs_cap; T* rhs_data = rhs.Data; rhs.Data = Data; Data = rhs_data; }

    inline int          _grow_capacity(int sz) const        { int new_capacity = Capacity ? (Capacity + Capacity / 2) : 8; return new_capacity > sz ? new_capacity : sz; }
    inline void         resize(int new_size)                { if (new_size > Capacity) reserve(_grow_capacity(new_size)); Size = new_size; }
    inline void         resize(int new_size, const T& v)    { if (new_size > Capacity) reserve(_grow_capacity(new_size)); if (new_size > Size) for (int n = Size; n < new_size; n++) memcpy(&Data[n], &v, sizeof(v)); Size = new_size; }
    inline void         shrink(int new_size)                { IM_ASSERT(new_size <= Size); Size = new_size; } // Resize a vector to a smaller size, guaranteed not to cause a reallocation
    inline void         reserve(int new_capacity)           { if (new_capacity <= Capacity) return; T* new_data = (T*)IM_ALLOC((size_t)new_capacity * sizeof(T)); if (Data) { memcpy(new_data, Data, (size_t)Size * sizeof(T)); IM_FREE(Data); } Data = new_data; Capacity = new_capacity; }
    inline void         reserve_discard(int new_capacity)   { if (new_capacity <= Capacity) return; if (Data) IM_FREE(Data); Data = (T*)IM_ALLOC((size_t)new_capacity * sizeof(T)); Capacity = new_capacity; }

    // NB: It is illegal to call push_back/push_front/insert with a reference pointing inside the UxImVector data itself! e.g. v.push_back(v[10]) is forbidden.
    inline void         push_back(const T& v)               { if (Size == Capacity) reserve(_grow_capacity(Size + 1)); memcpy(&Data[Size], &v, sizeof(v)); Size++; }
    inline void         pop_back()                          { IM_ASSERT(Size > 0); Size--; }
    inline void         push_front(const T& v)              { if (Size == 0) push_back(v); else insert(Data, v); }
    inline T*           erase(const T* it)                  { IM_ASSERT(it >= Data && it < Data + Size); const ptrdiff_t off = it - Data; memmove(Data + off, Data + off + 1, ((size_t)Size - (size_t)off - 1) * sizeof(T)); Size--; return Data + off; }
    inline T*           erase(const T* it, const T* it_last){ IM_ASSERT(it >= Data && it < Data + Size && it_last >= it && it_last <= Data + Size); const ptrdiff_t count = it_last - it; const ptrdiff_t off = it - Data; memmove(Data + off, Data + off + count, ((size_t)Size - (size_t)off - (size_t)count) * sizeof(T)); Size -= (int)count; return Data + off; }
    inline T*           erase_unsorted(const T* it)         { IM_ASSERT(it >= Data && it < Data + Size);  const ptrdiff_t off = it - Data; if (it < Data + Size - 1) memcpy(Data + off, Data + Size - 1, sizeof(T)); Size--; return Data + off; }
    inline T*           insert(const T* it, const T& v)     { IM_ASSERT(it >= Data && it <= Data + Size); const ptrdiff_t off = it - Data; if (Size == Capacity) reserve(_grow_capacity(Size + 1)); if (off < (int)Size) memmove(Data + off + 1, Data + off, ((size_t)Size - (size_t)off) * sizeof(T)); memcpy(&Data[off], &v, sizeof(v)); Size++; return Data + off; }
    inline bool         contains(const T& v) const          { const T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data++ == v) return true; return false; }
    inline T*           find(const T& v)                    { T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data == v) break; else ++data; return data; }
    inline const T*     find(const T& v) const              { const T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data == v) break; else ++data; return data; }
    inline int          find_index(const T& v) const        { const T* data_end = Data + Size; const T* it = find(v); if (it == data_end) return -1; const ptrdiff_t off = it - Data; return (int)off; }
    inline bool         find_erase(const T& v)              { const T* it = find(v); if (it < Data + Size) { erase(it); return true; } return false; }
    inline bool         find_erase_unsorted(const T& v)     { const T* it = find(v); if (it < Data + Size) { erase_unsorted(it); return true; } return false; }
    inline int          index_from_ptr(const T* it) const   { IM_ASSERT(it >= Data && it < Data + Size); const ptrdiff_t off = it - Data; return (int)off; }
};
IM_MSVC_RUNTIME_CHECKS_RESTORE

//-----------------------------------------------------------------------------
// [SECTION] UxImGuiStyle
//-----------------------------------------------------------------------------
// You may modify the UxImGui::GetStyle() main instance during initialization and before NewFrame().
// During the frame, use UxImGui::PushStyleVar(UxImGuiStyleVar_XXXX)/PopStyleVar() to alter the main style values,
// and UxImGui::PushStyleColor(UxImGuiCol_XXX)/PopStyleColor() for colors.
//-----------------------------------------------------------------------------

struct UxImGuiStyle
{
    float       Alpha;                      // Global alpha applies to everything in Dear UxImGui.
    float       DisabledAlpha;              // Additional alpha multiplier applied by BeginDisabled(). Multiply over current value of Alpha.
    UxImVec2      WindowPadding;              // Padding within a window.
    float       WindowRounding;             // Radius of window corners rounding. Set to 0.0f to have rectangular windows. Large values tend to lead to variety of artifacts and are not recommended.
    float       WindowBorderSize;           // Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    float       WindowBorderHoverPadding;   // Hit-testing extent outside/inside resizing border. Also extend determination of hovered window. Generally meaningfully larger than WindowBorderSize to make it easy to reach borders.
    UxImVec2      WindowMinSize;              // Minimum window size. This is a global setting. If you want to constrain individual windows, use SetNextWindowSizeConstraints().
    UxImVec2      WindowTitleAlign;           // Alignment for title bar text. Defaults to (0.0f,0.5f) for left-aligned,vertically centered.
    UxImGuiDir    WindowMenuButtonPosition;   // Side of the collapsing/docking button in the title bar (None/Left/Right). Defaults to UxImGuiDir_Left.
    float       ChildRounding;              // Radius of child window corners rounding. Set to 0.0f to have rectangular windows.
    float       ChildBorderSize;            // Thickness of border around child windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    float       PopupRounding;              // Radius of popup window corners rounding. (Note that tooltip windows use WindowRounding)
    float       PopupBorderSize;            // Thickness of border around popup/tooltip windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    UxImVec2      FramePadding;               // Padding within a framed rectangle (used by most widgets).
    float       FrameRounding;              // Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
    float       FrameBorderSize;            // Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    UxImVec2      ItemSpacing;                // Horizontal and vertical spacing between widgets/lines.
    UxImVec2      ItemInnerSpacing;           // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label).
    UxImVec2      CellPadding;                // Padding within a table cell. Cellpadding.x is locked for entire table. CellPadding.y may be altered between different rows.
    UxImVec2      TouchExtraPadding;          // Expand reactive bounding box for touch-based system where touch position is not accurate enough. Unfortunately we don't sort widgets so priority on overlap will always be given to the first widget. So don't grow this too much!
    float       IndentSpacing;              // Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
    float       ColumnsMinSpacing;          // Minimum horizontal spacing between two columns. Preferably > (FramePadding.x + 1).
    float       ScrollbarSize;              // Width of the vertical scrollbar, Height of the horizontal scrollbar.
    float       ScrollbarRounding;          // Radius of grab corners for scrollbar.
    float       GrabMinSize;                // Minimum width/height of a grab box for slider/scrollbar.
    float       GrabRounding;               // Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
    float       LogSliderDeadzone;          // The size in pixels of the dead-zone around zero on logarithmic sliders that cross zero.
    float       ImageBorderSize;            // Thickness of border around Image() calls.
    float       TabRounding;                // Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs.
    float       TabBorderSize;              // Thickness of border around tabs.
    float       TabCloseButtonMinWidthSelected;     // -1: always visible. 0.0f: visible when hovered. >0.0f: visible when hovered if minimum width.
    float       TabCloseButtonMinWidthUnselected;   // -1: always visible. 0.0f: visible when hovered. >0.0f: visible when hovered if minimum width. FLT_MAX: never show close button when unselected.
    float       TabBarBorderSize;           // Thickness of tab-bar separator, which takes on the tab active color to denote focus.
    float       TabBarOverlineSize;         // Thickness of tab-bar overline, which highlights the selected tab-bar.
    float       TableAngledHeadersAngle;    // Angle of angled headers (supported values range from -50.0f degrees to +50.0f degrees).
    UxImVec2      TableAngledHeadersTextAlign;// Alignment of angled headers within the cell
    UxImGuiTreeNodeFlags TreeLinesFlags;      // Default way to draw lines connecting TreeNode hierarchy. UxImGuiTreeNodeFlags_DrawLinesNone or UxImGuiTreeNodeFlags_DrawLinesFull or UxImGuiTreeNodeFlags_DrawLinesToNodes.
    float       TreeLinesSize;              // Thickness of outlines when using UxImGuiTreeNodeFlags_DrawLines.
    float       TreeLinesRounding;          // Radius of lines connecting child nodes to the vertical line.
    UxImGuiDir    ColorButtonPosition;        // Side of the color button in the ColorEdit4 widget (left/right). Defaults to UxImGuiDir_Right.
    UxImVec2      ButtonTextAlign;            // Alignment of button text when button is larger than text. Defaults to (0.5f, 0.5f) (centered).
    UxImVec2      SelectableTextAlign;        // Alignment of selectable text. Defaults to (0.0f, 0.0f) (top-left aligned). It's generally important to keep this left-aligned if you want to lay multiple items on a same line.
    float       SeparatorTextBorderSize;    // Thickness of border in SeparatorText()
    UxImVec2      SeparatorTextAlign;         // Alignment of text within the separator. Defaults to (0.0f, 0.5f) (left aligned, center).
    UxImVec2      SeparatorTextPadding;       // Horizontal offset of text from each edge of the separator + spacing on other axis. Generally small values. .y is recommended to be == FramePadding.y.
    UxImVec2      DisplayWindowPadding;       // Apply to regular windows: amount which we enforce to keep visible when moving near edges of your screen.
    UxImVec2      DisplaySafeAreaPadding;     // Apply to every windows, menus, popups, tooltips: amount where we avoid displaying contents. Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).
    float       DockingSeparatorSize;       // Thickness of resizing border between docked windows
    float       MouseCursorScale;           // Scale software rendered mouse cursor (when io.MouseDrawCursor is enabled). We apply per-monitor DPI scaling over this scale. May be removed later.
    bool        AntiAliasedLines;           // Enable anti-aliased lines/borders. Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to UxImDrawList).
    bool        AntiAliasedLinesUseTex;     // Enable anti-aliased lines/borders using textures where possible. Require backend to render with bilinear filtering (NOT point/nearest filtering). Latched at the beginning of the frame (copied to UxImDrawList).
    bool        AntiAliasedFill;            // Enable anti-aliased edges around filled shapes (rounded rectangles, circles, etc.). Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to UxImDrawList).
    float       CurveTessellationTol;       // Tessellation tolerance when using PathBezierCurveTo() without a specific number of segments. Decrease for highly tessellated curves (higher quality, more polygons), increase to reduce quality.
    float       CircleTessellationMaxError; // Maximum error (in pixels) allowed when using AddCircle()/AddCircleFilled() or drawing rounded corner rectangles with no explicit segment count specified. Decrease for higher quality but more geometry.

    // Colors
    UxImVec4      Colors[UxImGuiCol_COUNT];

    // Behaviors
    // (It is possible to modify those fields mid-frame if specific behavior need it, unlike e.g. configuration fields in UxImGuiIO)
    float             HoverStationaryDelay;     // Delay for IsItemHovered(UxImGuiHoveredFlags_Stationary). Time required to consider mouse stationary.
    float             HoverDelayShort;          // Delay for IsItemHovered(UxImGuiHoveredFlags_DelayShort). Usually used along with HoverStationaryDelay.
    float             HoverDelayNormal;         // Delay for IsItemHovered(UxImGuiHoveredFlags_DelayNormal). "
    UxImGuiHoveredFlags HoverFlagsForTooltipMouse;// Default flags when using IsItemHovered(UxImGuiHoveredFlags_ForTooltip) or BeginItemTooltip()/SetItemTooltip() while using mouse.
    UxImGuiHoveredFlags HoverFlagsForTooltipNav;  // Default flags when using IsItemHovered(UxImGuiHoveredFlags_ForTooltip) or BeginItemTooltip()/SetItemTooltip() while using keyboard/gamepad.

    IMGUI_API UxImGuiStyle();
    IMGUI_API void ScaleAllSizes(float scale_factor);

    // Obsolete names
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    // TabMinWidthForCloseButton = TabCloseButtonMinWidthUnselected // Renamed in 1.91.9.
#endif
};

//-----------------------------------------------------------------------------
// [SECTION] UxImGuiIO
//-----------------------------------------------------------------------------
// Communicate most settings and inputs/outputs to Dear UxImGui using this structure.
// Access via UxImGui::GetIO(). Read 'Programmer guide' section in .cpp file for general usage.
// It is generally expected that:
// - initialization: backends and user code writes to UxImGuiIO.
// - main loop: backends writes to UxImGuiIO, user code and imgui code reads from UxImGuiIO.
//-----------------------------------------------------------------------------
// Also see UxImGui::GetPlatformIO() and UxImGuiPlatformIO struct for OS/platform related functions: clipboard, IME etc.
//-----------------------------------------------------------------------------

// [Internal] Storage used by IsKeyDown(), IsKeyPressed() etc functions.
// If prior to 1.87 you used io.KeysDownDuration[] (which was marked as internal), you should use GetKeyData(key)->DownDuration and *NOT* io.KeysData[key]->DownDuration.
struct UxImGuiKeyData
{
    bool        Down;               // True for if key is down
    float       DownDuration;       // Duration the key has been down (<0.0f: not pressed, 0.0f: just pressed, >0.0f: time held)
    float       DownDurationPrev;   // Last frame duration the key has been down
    float       AnalogValue;        // 0.0f..1.0f for gamepad values
};

struct UxImGuiIO
{
    //------------------------------------------------------------------
    // Configuration                            // Default value
    //------------------------------------------------------------------

    UxImGuiConfigFlags   ConfigFlags;             // = 0              // See UxImGuiConfigFlags_ enum. Set by user/application. Keyboard/Gamepad navigation options, etc.
    UxImGuiBackendFlags  BackendFlags;            // = 0              // See UxImGuiBackendFlags_ enum. Set by backend (imgui_impl_xxx files or custom backend) to communicate features supported by the backend.
    UxImVec2      DisplaySize;                    // <unset>          // Main display size, in pixels (== GetMainViewport()->Size). May change every frame.
    UxImVec2      DisplayFramebufferScale;        // = (1, 1)         // Main display density. For retina display where window coordinates are different from framebuffer coordinates. This will affect font density + will end up in UxImDrawData::FramebufferScale.
    float       DeltaTime;                      // = 1.0f/60.0f     // Time elapsed since last frame, in seconds. May change every frame.
    float       IniSavingRate;                  // = 5.0f           // Minimum time between saving positions/sizes to .ini file, in seconds.
    const char* IniFilename;                    // = "imgui.ini"    // Path to .ini file (important: default "imgui.ini" is relative to current working dir!). Set NULL to disable automatic .ini loading/saving or if you want to manually call LoadIniSettingsXXX() / SaveIniSettingsXXX() functions.
    const char* LogFilename;                    // = "imgui_log.txt"// Path to .log file (default parameter to UxImGui::LogToFile when no file is specified).
    void*       UserData;                       // = NULL           // Store your own data.

    // Font system
    UxImFontAtlas*Fonts;                          // <auto>           // Font atlas: load, rasterize and pack one or more fonts into a single texture.
    float       FontGlobalScale;                // = 1.0f           // Global scale all fonts
    bool        FontAllowUserScaling;           // = false          // [OBSOLETE] Allow user scaling text of individual window with CTRL+Wheel.
    UxImFont*     FontDefault;                    // = NULL           // Font to use on NewFrame(). Use NULL to uses Fonts->Fonts[0].

    // Keyboard/Gamepad Navigation options
    bool        ConfigNavSwapGamepadButtons;    // = false          // Swap Activate<>Cancel (A<>B) buttons, matching typical "Nintendo/Japanese style" gamepad layout.
    bool        ConfigNavMoveSetMousePos;       // = false          // Directional/tabbing navigation teleports the mouse cursor. May be useful on TV/console systems where moving a virtual mouse is difficult. Will update io.MousePos and set io.WantSetMousePos=true.
    bool        ConfigNavCaptureKeyboard;       // = true           // Sets io.WantCaptureKeyboard when io.NavActive is set.
    bool        ConfigNavEscapeClearFocusItem;  // = true           // Pressing Escape can clear focused item + navigation id/highlight. Set to false if you want to always keep highlight on.
    bool        ConfigNavEscapeClearFocusWindow;// = false          // Pressing Escape can clear focused window as well (super set of io.ConfigNavEscapeClearFocusItem).
    bool        ConfigNavCursorVisibleAuto;     // = true           // Using directional navigation key makes the cursor visible. Mouse click hides the cursor.
    bool        ConfigNavCursorVisibleAlways;   // = false          // Navigation cursor is always visible.

    // Docking options (when UxImGuiConfigFlags_DockingEnable is set)
    bool        ConfigDockingNoSplit;           // = false          // Simplified docking mode: disable window splitting, so docking is limited to merging multiple windows together into tab-bars.
    bool        ConfigDockingWithShift;         // = false          // Enable docking with holding Shift key (reduce visual noise, allows dropping in wider space)
    bool        ConfigDockingAlwaysTabBar;      // = false          // [BETA] [FIXME: This currently creates regression with auto-sizing and general overhead] Make every single floating window display within a docking node.
    bool        ConfigDockingTransparentPayload;// = false          // [BETA] Make window or viewport transparent when docking and only display docking boxes on the target viewport. Useful if rendering of multiple viewport cannot be synced. Best used with ConfigViewportsNoAutoMerge.

    // Viewport options (when UxImGuiConfigFlags_ViewportsEnable is set)
    bool        ConfigViewportsNoAutoMerge;     // = false;         // Set to make all floating imgui windows always create their own viewport. Otherwise, they are merged into the main host viewports when overlapping it. May also set UxImGuiViewportFlags_NoAutoMerge on individual viewport.
    bool        ConfigViewportsNoTaskBarIcon;   // = false          // Disable default OS task bar icon flag for secondary viewports. When a viewport doesn't want a task bar icon, UxImGuiViewportFlags_NoTaskBarIcon will be set on it.
    bool        ConfigViewportsNoDecoration;    // = true           // Disable default OS window decoration flag for secondary viewports. When a viewport doesn't want window decorations, UxImGuiViewportFlags_NoDecoration will be set on it. Enabling decoration can create subsequent issues at OS levels (e.g. minimum window size).
    bool        ConfigViewportsNoDefaultParent; // = false          // Disable default OS parenting to main viewport for secondary viewports. By default, viewports are marked with ParentViewportId = <main_viewport>, expecting the platform backend to setup a parent/child relationship between the OS windows (some backend may ignore this). Set to true if you want the default to be 0, then all viewports will be top-level OS windows.

    // Miscellaneous options
    // (you can visualize and interact with all options in 'Demo->Configuration')
    bool        MouseDrawCursor;                // = false          // Request UxImGui to draw a mouse cursor for you (if you are on a platform without a mouse cursor). Cannot be easily renamed to 'io.ConfigXXX' because this is frequently used by backend implementations.
    bool        ConfigMacOSXBehaviors;          // = defined(__APPLE__) // Swap Cmd<>Ctrl keys + OS X style text editing cursor movement using Alt instead of Ctrl, Shortcuts using Cmd/Super instead of Ctrl, Line/Text Start and End using Cmd+Arrows instead of Home/End, Double click selects by word instead of selecting whole text, Multi-selection in lists uses Cmd/Super instead of Ctrl.
    bool        ConfigInputTrickleEventQueue;   // = true           // Enable input queue trickling: some types of events submitted during the same frame (e.g. button down + up) will be spread over multiple frames, improving interactions with low framerates.
    bool        ConfigInputTextCursorBlink;     // = true           // Enable blinking cursor (optional as some users consider it to be distracting).
    bool        ConfigInputTextEnterKeepActive; // = false          // [BETA] Pressing Enter will keep item active and select contents (single-line only).
    bool        ConfigDragClickToInputText;     // = false          // [BETA] Enable turning DragXXX widgets into text input with a simple mouse click-release (without moving). Not desirable on devices without a keyboard.
    bool        ConfigWindowsResizeFromEdges;   // = true           // Enable resizing of windows from their edges and from the lower-left corner. This requires UxImGuiBackendFlags_HasMouseCursors for better mouse cursor feedback. (This used to be a per-window UxImGuiWindowFlags_ResizeFromAnySide flag)
    bool        ConfigWindowsMoveFromTitleBarOnly;  // = false      // Enable allowing to move windows only when clicking on their title bar. Does not apply to windows without a title bar.
    bool        ConfigWindowsCopyContentsWithCtrlC; // = false      // [EXPERIMENTAL] CTRL+C copy the contents of focused window into the clipboard. Experimental because: (1) has known issues with nested Begin/End pairs (2) text output quality varies (3) text output is in submission order rather than spatial order.
    bool        ConfigScrollbarScrollByPage;    // = true           // Enable scrolling page by page when clicking outside the scrollbar grab. When disabled, always scroll to clicked location. When enabled, Shift+Click scrolls to clicked location.
    float       ConfigMemoryCompactTimer;       // = 60.0f          // Timer (in seconds) to free transient windows/tables memory buffers when unused. Set to -1.0f to disable.

    // Inputs Behaviors
    // (other variables, ones which are expected to be tweaked within UI code, are exposed in UxImGuiStyle)
    float       MouseDoubleClickTime;           // = 0.30f          // Time for a double-click, in seconds.
    float       MouseDoubleClickMaxDist;        // = 6.0f           // Distance threshold to stay in to validate a double-click, in pixels.
    float       MouseDragThreshold;             // = 6.0f           // Distance threshold before considering we are dragging.
    float       KeyRepeatDelay;                 // = 0.275f         // When holding a key/button, time before it starts repeating, in seconds (for buttons in Repeat mode, etc.).
    float       KeyRepeatRate;                  // = 0.050f         // When holding a key/button, rate at which it repeats, in seconds.

    //------------------------------------------------------------------
    // Debug options
    //------------------------------------------------------------------

    // Options to configure Error Handling and how we handle recoverable errors [EXPERIMENTAL]
    // - Error recovery is provided as a way to facilitate:
    //    - Recovery after a programming error (native code or scripting language - the later tends to facilitate iterating on code while running).
    //    - Recovery after running an exception handler or any error processing which may skip code after an error has been detected.
    // - Error recovery is not perfect nor guaranteed! It is a feature to ease development.
    //   You not are not supposed to rely on it in the course of a normal application run.
    // - Functions that support error recovery are using IM_ASSERT_USER_ERROR() instead of IM_ASSERT().
    // - By design, we do NOT allow error recovery to be 100% silent. One of the three options needs to be checked!
    // - Always ensure that on programmers seats you have at minimum Asserts or Tooltips enabled when making direct imgui API calls!
    //   Otherwise it would severely hinder your ability to catch and correct mistakes!
    // Read https://github.com/ocornut/imgui/wiki/Error-Handling for details.
    // - Programmer seats: keep asserts (default), or disable asserts and keep error tooltips (new and nice!)
    // - Non-programmer seats: maybe disable asserts, but make sure errors are resurfaced (tooltips, visible log entries, use callback etc.)
    // - Recovery after error/exception: record stack sizes with ErrorRecoveryStoreState(), disable assert, set log callback (to e.g. trigger high-level breakpoint), recover with ErrorRecoveryTryToRecoverState(), restore settings.
    bool        ConfigErrorRecovery;                // = true       // Enable error recovery support. Some errors won't be detected and lead to direct crashes if recovery is disabled.
    bool        ConfigErrorRecoveryEnableAssert;    // = true       // Enable asserts on recoverable error. By default call IM_ASSERT() when returning from a failing IM_ASSERT_USER_ERROR()
    bool        ConfigErrorRecoveryEnableDebugLog;  // = true       // Enable debug log output on recoverable errors.
    bool        ConfigErrorRecoveryEnableTooltip;   // = true       // Enable tooltip on recoverable errors. The tooltip include a way to enable asserts if they were disabled.

    // Option to enable various debug tools showing buttons that will call the IM_DEBUG_BREAK() macro.
    // - The Item Picker tool will be available regardless of this being enabled, in order to maximize its discoverability.
    // - Requires a debugger being attached, otherwise IM_DEBUG_BREAK() options will appear to crash your application.
    //   e.g. io.ConfigDebugIsDebuggerPresent = ::IsDebuggerPresent() on Win32, or refer to UxImOsIsDebuggerPresent() imgui_test_engine/imgui_te_utils.cpp for a Unix compatible version).
    bool        ConfigDebugIsDebuggerPresent;   // = false          // Enable various tools calling IM_DEBUG_BREAK().

    // Tools to detect code submitting items with conflicting/duplicate IDs
    // - Code should use PushID()/PopID() in loops, or append "##xx" to same-label identifiers.
    // - Empty label e.g. Button("") == same ID as parent widget/node. Use Button("##xx") instead!
    // - See FAQ https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-about-the-id-stack-system
    bool        ConfigDebugHighlightIdConflicts;// = true           // Highlight and show an error message popup when multiple items have conflicting identifiers.
    bool        ConfigDebugHighlightIdConflictsShowItemPicker;//=true // Show "Item Picker" button in aforementioned popup.

    // Tools to test correct Begin/End and BeginChild/EndChild behaviors.
    // - Presently Begin()/End() and BeginChild()/EndChild() needs to ALWAYS be called in tandem, regardless of return value of BeginXXX()
    // - This is inconsistent with other BeginXXX functions and create confusion for many users.
    // - We expect to update the API eventually. In the meanwhile we provide tools to facilitate checking user-code behavior.
    bool        ConfigDebugBeginReturnValueOnce;// = false          // First-time calls to Begin()/BeginChild() will return false. NEEDS TO BE SET AT APPLICATION BOOT TIME if you don't want to miss windows.
    bool        ConfigDebugBeginReturnValueLoop;// = false          // Some calls to Begin()/BeginChild() will return false. Will cycle through window depths then repeat. Suggested use: add "io.ConfigDebugBeginReturnValue = io.KeyShift" in your main loop then occasionally press SHIFT. Windows should be flickering while running.

    // Option to deactivate io.AddFocusEvent(false) handling.
    // - May facilitate interactions with a debugger when focus loss leads to clearing inputs data.
    // - Backends may have other side-effects on focus loss, so this will reduce side-effects but not necessary remove all of them.
    bool        ConfigDebugIgnoreFocusLoss;     // = false          // Ignore io.AddFocusEvent(false), consequently not calling io.ClearInputKeys()/io.ClearInputMouse() in input processing.

    // Option to audit .ini data
    bool        ConfigDebugIniSettings;         // = false          // Save .ini data with extra comments (particularly helpful for Docking, but makes saving slower)

    //------------------------------------------------------------------
    // Platform Identifiers
    // (the imgui_impl_xxxx backend files are setting those up for you)
    //------------------------------------------------------------------

    // Nowadays those would be stored in UxImGuiPlatformIO but we are leaving them here for legacy reasons.
    // Optional: Platform/Renderer backend name (informational only! will be displayed in About Window) + User data for backend/wrappers to store their own stuff.
    const char* BackendPlatformName;            // = NULL
    const char* BackendRendererName;            // = NULL
    void*       BackendPlatformUserData;        // = NULL           // User data for platform backend
    void*       BackendRendererUserData;        // = NULL           // User data for renderer backend
    void*       BackendLanguageUserData;        // = NULL           // User data for non C++ programming language backend

    //------------------------------------------------------------------
    // Input - Call before calling NewFrame()
    //------------------------------------------------------------------

    // Input Functions
    IMGUI_API void  AddKeyEvent(UxImGuiKey key, bool down);                   // Queue a new key down/up event. Key should be "translated" (as in, generally UxImGuiKey_A matches the key end-user would use to emit an 'A' character)
    IMGUI_API void  AddKeyAnalogEvent(UxImGuiKey key, bool down, float v);    // Queue a new key down/up event for analog values (e.g. UxImGuiKey_Gamepad_ values). Dead-zones should be handled by the backend.
    IMGUI_API void  AddMousePosEvent(float x, float y);                     // Queue a mouse position update. Use -FLT_MAX,-FLT_MAX to signify no mouse (e.g. app not focused and not hovered)
    IMGUI_API void  AddMouseButtonEvent(int button, bool down);             // Queue a mouse button change
    IMGUI_API void  AddMouseWheelEvent(float wheel_x, float wheel_y);       // Queue a mouse wheel update. wheel_y<0: scroll down, wheel_y>0: scroll up, wheel_x<0: scroll right, wheel_x>0: scroll left.
    IMGUI_API void  AddMouseSourceEvent(UxImGuiMouseSource source);           // Queue a mouse source change (Mouse/TouchScreen/Pen)
    IMGUI_API void  AddMouseViewportEvent(UxImGuiID id);                      // Queue a mouse hovered viewport. Requires backend to set UxImGuiBackendFlags_HasMouseHoveredViewport to call this (for multi-viewport support).
    IMGUI_API void  AddFocusEvent(bool focused);                            // Queue a gain/loss of focus for the application (generally based on OS/platform focus of your window)
    IMGUI_API void  AddInputCharacter(unsigned int c);                      // Queue a new character input
    IMGUI_API void  AddInputCharacterUTF16(UxImWchar16 c);                    // Queue a new character input from a UTF-16 character, it can be a surrogate
    IMGUI_API void  AddInputCharactersUTF8(const char* str);                // Queue a new characters input from a UTF-8 string

    IMGUI_API void  SetKeyEventNativeData(UxImGuiKey key, int native_keycode, int native_scancode, int native_legacy_index = -1); // [Optional] Specify index for legacy <1.87 IsKeyXXX() functions with native indices + specify native keycode, scancode.
    IMGUI_API void  SetAppAcceptingEvents(bool accepting_events);           // Set master flag for accepting key/mouse/text events (default to true). Useful if you have native dialog boxes that are interrupting your application loop/refresh, and you want to disable events being queued while your app is frozen.
    IMGUI_API void  ClearEventsQueue();                                     // Clear all incoming events.
    IMGUI_API void  ClearInputKeys();                                       // Clear current keyboard/gamepad state + current frame text input buffer. Equivalent to releasing all keys/buttons.
    IMGUI_API void  ClearInputMouse();                                      // Clear current mouse state.
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_API void  ClearInputCharacters();                                 // [Obsoleted in 1.89.8] Clear the current frame text input buffer. Now included within ClearInputKeys().
#endif

    //------------------------------------------------------------------
    // Output - Updated by NewFrame() or EndFrame()/Render()
    // (when reading from the io.WantCaptureMouse, io.WantCaptureKeyboard flags to dispatch your inputs, it is
    //  generally easier and more correct to use their state BEFORE calling NewFrame(). See FAQ for details!)
    //------------------------------------------------------------------

    bool        WantCaptureMouse;                   // Set when Dear UxImGui will use mouse inputs, in this case do not dispatch them to your main game/application (either way, always pass on mouse inputs to imgui). (e.g. unclicked mouse is hovering over an imgui window, widget is active, mouse was clicked over an imgui window, etc.).
    bool        WantCaptureKeyboard;                // Set when Dear UxImGui will use keyboard inputs, in this case do not dispatch them to your main game/application (either way, always pass keyboard inputs to imgui). (e.g. InputText active, or an imgui window is focused and navigation is enabled, etc.).
    bool        WantTextInput;                      // Mobile/console: when set, you may display an on-screen keyboard. This is set by Dear UxImGui when it wants textual keyboard input to happen (e.g. when a InputText widget is active).
    bool        WantSetMousePos;                    // MousePos has been altered, backend should reposition mouse on next frame. Rarely used! Set only when io.ConfigNavMoveSetMousePos is enabled.
    bool        WantSaveIniSettings;                // When manual .ini load/save is active (io.IniFilename == NULL), this will be set to notify your application that you can call SaveIniSettingsToMemory() and save yourself. Important: clear io.WantSaveIniSettings yourself after saving!
    bool        NavActive;                          // Keyboard/Gamepad navigation is currently allowed (will handle UxImGuiKey_NavXXX events) = a window is focused and it doesn't use the UxImGuiWindowFlags_NoNavInputs flag.
    bool        NavVisible;                         // Keyboard/Gamepad navigation highlight is visible and allowed (will handle UxImGuiKey_NavXXX events).
    float       Framerate;                          // Estimate of application framerate (rolling average over 60 frames, based on io.DeltaTime), in frame per second. Solely for convenience. Slow applications may not want to use a moving average or may want to reset underlying buffers occasionally.
    int         MetricsRenderVertices;              // Vertices output during last call to Render()
    int         MetricsRenderIndices;               // Indices output during last call to Render() = number of triangles * 3
    int         MetricsRenderWindows;               // Number of visible windows
    int         MetricsActiveWindows;               // Number of active windows
    UxImVec2      MouseDelta;                         // Mouse delta. Note that this is zero if either current or previous position are invalid (-FLT_MAX,-FLT_MAX), so a disappearing/reappearing mouse won't have a huge delta.

    //------------------------------------------------------------------
    // [Internal] Dear UxImGui will maintain those fields. Forward compatibility not guaranteed!
    //------------------------------------------------------------------

    UxImGuiContext* Ctx;                              // Parent UI context (needs to be set explicitly by parent).

    // Main Input State
    // (this block used to be written by backend, since 1.87 it is best to NOT write to those directly, call the AddXXX functions above instead)
    // (reading from those variables is fair game, as they are extremely unlikely to be moving anywhere)
    UxImVec2      MousePos;                           // Mouse position, in pixels. Set to UxImVec2(-FLT_MAX, -FLT_MAX) if mouse is unavailable (on another screen, etc.)
    bool        MouseDown[5];                       // Mouse buttons: 0=left, 1=right, 2=middle + extras (UxImGuiMouseButton_COUNT == 5). Dear UxImGui mostly uses left and right buttons. Other buttons allow us to track if the mouse is being used by your application + available to user as a convenience via IsMouse** API.
    float       MouseWheel;                         // Mouse wheel Vertical: 1 unit scrolls about 5 lines text. >0 scrolls Up, <0 scrolls Down. Hold SHIFT to turn vertical scroll into horizontal scroll.
    float       MouseWheelH;                        // Mouse wheel Horizontal. >0 scrolls Left, <0 scrolls Right. Most users don't have a mouse with a horizontal wheel, may not be filled by all backends.
    UxImGuiMouseSource MouseSource;                   // Mouse actual input peripheral (Mouse/TouchScreen/Pen).
    UxImGuiID     MouseHoveredViewport;               // (Optional) Modify using io.AddMouseViewportEvent(). With multi-viewports: viewport the OS mouse is hovering. If possible _IGNORING_ viewports with the UxImGuiViewportFlags_NoInputs flag is much better (few backends can handle that). Set io.BackendFlags |= UxImGuiBackendFlags_HasMouseHoveredViewport if you can provide this info. If you don't imgui will infer the value using the rectangles and last focused time of the viewports it knows about (ignoring other OS windows).
    bool        KeyCtrl;                            // Keyboard modifier down: Control
    bool        KeyShift;                           // Keyboard modifier down: Shift
    bool        KeyAlt;                             // Keyboard modifier down: Alt
    bool        KeySuper;                           // Keyboard modifier down: Cmd/Super/Windows

    // Other state maintained from data above + IO function calls
    UxImGuiKeyChord KeyMods;                          // Key mods flags (any of UxImGuiMod_Ctrl/UxImGuiMod_Shift/UxImGuiMod_Alt/UxImGuiMod_Super flags, same as io.KeyCtrl/KeyShift/KeyAlt/KeySuper but merged into flags. Read-only, updated by NewFrame()
    UxImGuiKeyData  KeysData[UxImGuiKey_NamedKey_COUNT];// Key state for all known keys. Use IsKeyXXX() functions to access this.
    bool        WantCaptureMouseUnlessPopupClose;   // Alternative to WantCaptureMouse: (WantCaptureMouse == true && WantCaptureMouseUnlessPopupClose == false) when a click over void is expected to close a popup.
    UxImVec2      MousePosPrev;                       // Previous mouse position (note that MouseDelta is not necessary == MousePos-MousePosPrev, in case either position is invalid)
    UxImVec2      MouseClickedPos[5];                 // Position at time of clicking
    double      MouseClickedTime[5];                // Time of last click (used to figure out double-click)
    bool        MouseClicked[5];                    // Mouse button went from !Down to Down (same as MouseClickedCount[x] != 0)
    bool        MouseDoubleClicked[5];              // Has mouse button been double-clicked? (same as MouseClickedCount[x] == 2)
    UxImU16       MouseClickedCount[5];               // == 0 (not clicked), == 1 (same as MouseClicked[]), == 2 (double-clicked), == 3 (triple-clicked) etc. when going from !Down to Down
    UxImU16       MouseClickedLastCount[5];           // Count successive number of clicks. Stays valid after mouse release. Reset after another click is done.
    bool        MouseReleased[5];                   // Mouse button went from Down to !Down
    double      MouseReleasedTime[5];               // Time of last released (rarely used! but useful to handle delayed single-click when trying to disambiguate them from double-click).
    bool        MouseDownOwned[5];                  // Track if button was clicked inside a dear imgui window or over void blocked by a popup. We don't request mouse capture from the application if click started outside UxImGui bounds.
    bool        MouseDownOwnedUnlessPopupClose[5];  // Track if button was clicked inside a dear imgui window.
    bool        MouseWheelRequestAxisSwap;          // On a non-Mac system, holding SHIFT requests WheelY to perform the equivalent of a WheelX event. On a Mac system this is already enforced by the system.
    bool        MouseCtrlLeftAsRightClick;          // (OSX) Set to true when the current click was a Ctrl+click that spawned a simulated right click
    float       MouseDownDuration[5];               // Duration the mouse button has been down (0.0f == just clicked)
    float       MouseDownDurationPrev[5];           // Previous time the mouse button has been down
    UxImVec2      MouseDragMaxDistanceAbs[5];         // Maximum distance, absolute, on each axis, of how much mouse has traveled from the clicking point
    float       MouseDragMaxDistanceSqr[5];         // Squared maximum distance of how much mouse has traveled from the clicking point (used for moving thresholds)
    float       PenPressure;                        // Touch/Pen pressure (0.0f to 1.0f, should be >0.0f only when MouseDown[0] == true). Helper storage currently unused by Dear UxImGui.
    bool        AppFocusLost;                       // Only modify via AddFocusEvent()
    bool        AppAcceptingEvents;                 // Only modify via SetAppAcceptingEvents()
    UxImWchar16   InputQueueSurrogate;                // For AddInputCharacterUTF16()
    UxImVector<UxImWchar> InputQueueCharacters;         // Queue of _characters_ input (obtained by platform backend). Fill using AddInputCharacter() helper.

    // Legacy: before 1.87, we required backend to fill io.KeyMap[] (imgui->native map) during initialization and io.KeysDown[] (native indices) every frame.
    // This is still temporarily supported as a legacy feature. However the new preferred scheme is for backend to call io.AddKeyEvent().
    //   Old (<1.87):  UxImGui::IsKeyPressed(UxImGui::GetIO().KeyMap[UxImGuiKey_Space]) --> New (1.87+) UxImGui::IsKeyPressed(UxImGuiKey_Space)
    //   Old (<1.87):  UxImGui::IsKeyPressed(MYPLATFORM_KEY_SPACE)                  --> New (1.87+) UxImGui::IsKeyPressed(UxImGuiKey_Space)
    // Read https://github.com/ocornut/imgui/issues/4921 for details.
    //int       KeyMap[UxImGuiKey_COUNT];             // [LEGACY] Input: map of indices into the KeysDown[512] entries array which represent your "native" keyboard state. The first 512 are now unused and should be kept zero. Legacy backend will write into KeyMap[] using UxImGuiKey_ indices which are always >512.
    //bool      KeysDown[UxImGuiKey_COUNT];           // [LEGACY] Input: Keyboard keys that are pressed (ideally left in the "native" order your engine has access to keyboard keys, so you can use your own defines/enums for keys). This used to be [512] sized. It is now UxImGuiKey_COUNT to allow legacy io.KeysDown[GetKeyIndex(...)] to work without an overflow.
    //float     NavInputs[UxImGuiNavInput_COUNT];     // [LEGACY] Since 1.88, NavInputs[] was removed. Backends from 1.60 to 1.86 won't build. Feed gamepad inputs via io.AddKeyEvent() and UxImGuiKey_GamepadXXX enums.
    //void*     ImeWindowHandle;                    // [Obsoleted in 1.87] Set UxImGuiViewport::PlatformHandleRaw instead. Set this to your HWND to get automatic IME cursor positioning.

    // Legacy: before 1.91.1, clipboard functions were stored in UxImGuiIO instead of UxImGuiPlatformIO.
    // As this is will affect all users of custom engines/backends, we are providing proper legacy redirection (will obsolete).
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    const char* (*GetClipboardTextFn)(void* user_data);
    void        (*SetClipboardTextFn)(void* user_data, const char* text);
    void*       ClipboardUserData;
#endif

    IMGUI_API   UxImGuiIO();
};

//-----------------------------------------------------------------------------
// [SECTION] Misc data structures (UxImGuiInputTextCallbackData, UxImGuiSizeCallbackData, UxImGuiPayload)
//-----------------------------------------------------------------------------

// Shared state of InputText(), passed as an argument to your callback when a UxImGuiInputTextFlags_Callback* flag is used.
// The callback function should return 0 by default.
// Callbacks (follow a flag name and see comments in UxImGuiInputTextFlags_ declarations for more details)
// - UxImGuiInputTextFlags_CallbackEdit:        Callback on buffer edit. Note that InputText() already returns true on edit + you can always use IsItemEdited(). The callback is useful to manipulate the underlying buffer while focus is active.
// - UxImGuiInputTextFlags_CallbackAlways:      Callback on each iteration
// - UxImGuiInputTextFlags_CallbackCompletion:  Callback on pressing TAB
// - UxImGuiInputTextFlags_CallbackHistory:     Callback on pressing Up/Down arrows
// - UxImGuiInputTextFlags_CallbackCharFilter:  Callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
// - UxImGuiInputTextFlags_CallbackResize:      Callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow.
struct UxImGuiInputTextCallbackData
{
    UxImGuiContext*       Ctx;            // Parent UI context
    UxImGuiInputTextFlags EventFlag;      // One UxImGuiInputTextFlags_Callback*    // Read-only
    UxImGuiInputTextFlags Flags;          // What user passed to InputText()      // Read-only
    void*               UserData;       // What user passed to InputText()      // Read-only

    // Arguments for the different callback events
    // - During Resize callback, Buf will be same as your input buffer.
    // - However, during Completion/History/Always callback, Buf always points to our own internal data (it is not the same as your buffer)! Changes to it will be reflected into your own buffer shortly after the callback.
    // - To modify the text buffer in a callback, prefer using the InsertChars() / DeleteChars() function. InsertChars() will take care of calling the resize callback if necessary.
    // - If you know your edits are not going to resize the underlying buffer allocation, you may modify the contents of 'Buf[]' directly. You need to update 'BufTextLen' accordingly (0 <= BufTextLen < BufSize) and set 'BufDirty'' to true so InputText can update its internal state.
    UxImWchar             EventChar;      // Character input                      // Read-write   // [CharFilter] Replace character with another one, or set to zero to drop. return 1 is equivalent to setting EventChar=0;
    UxImGuiKey            EventKey;       // Key pressed (Up/Down/TAB)            // Read-only    // [Completion,History]
    char*               Buf;            // Text buffer                          // Read-write   // [Resize] Can replace pointer / [Completion,History,Always] Only write to pointed data, don't replace the actual pointer!
    int                 BufTextLen;     // Text length (in bytes)               // Read-write   // [Resize,Completion,History,Always] Exclude zero-terminator storage. In C land: == strlen(some_text), in C++ land: string.length()
    int                 BufSize;        // Buffer size (in bytes) = capacity+1  // Read-only    // [Resize,Completion,History,Always] Include zero-terminator storage. In C land == ARRAYSIZE(my_char_array), in C++ land: string.capacity()+1
    bool                BufDirty;       // Set if you modify Buf/BufTextLen!    // Write        // [Completion,History,Always]
    int                 CursorPos;      //                                      // Read-write   // [Completion,History,Always]
    int                 SelectionStart; //                                      // Read-write   // [Completion,History,Always] == to SelectionEnd when no selection)
    int                 SelectionEnd;   //                                      // Read-write   // [Completion,History,Always]

    // Helper functions for text manipulation.
    // Use those function to benefit from the CallbackResize behaviors. Calling those function reset the selection.
    IMGUI_API UxImGuiInputTextCallbackData();
    IMGUI_API void      DeleteChars(int pos, int bytes_count);
    IMGUI_API void      InsertChars(int pos, const char* text, const char* text_end = NULL);
    void                SelectAll()             { SelectionStart = 0; SelectionEnd = BufTextLen; }
    void                ClearSelection()        { SelectionStart = SelectionEnd = BufTextLen; }
    bool                HasSelection() const    { return SelectionStart != SelectionEnd; }
};

// Resizing callback data to apply custom constraint. As enabled by SetNextWindowSizeConstraints(). Callback is called during the next Begin().
// NB: For basic min/max size constraint on each axis you don't need to use the callback! The SetNextWindowSizeConstraints() parameters are enough.
struct UxImGuiSizeCallbackData
{
    void*   UserData;       // Read-only.   What user passed to SetNextWindowSizeConstraints(). Generally store an integer or float in here (need reinterpret_cast<>).
    UxImVec2  Pos;            // Read-only.   Window position, for reference.
    UxImVec2  CurrentSize;    // Read-only.   Current window size.
    UxImVec2  DesiredSize;    // Read-write.  Desired size, based on user's mouse position. Write to this field to restrain resizing.
};

// [ALPHA] Rarely used / very advanced uses only. Use with SetNextWindowClass() and DockSpace() functions.
// Important: the content of this class is still highly WIP and likely to change and be refactored
// before we stabilize Docking features. Please be mindful if using this.
// Provide hints:
// - To the platform backend via altered viewport flags (enable/disable OS decoration, OS task bar icons, etc.)
// - To the platform backend for OS level parent/child relationships of viewport.
// - To the docking system for various options and filtering.
struct UxImGuiWindowClass
{
    UxImGuiID             ClassId;                    // User data. 0 = Default class (unclassed). Windows of different classes cannot be docked with each others.
    UxImGuiID             ParentViewportId;           // Hint for the platform backend. -1: use default. 0: request platform backend to not parent the platform. != 0: request platform backend to create a parent<>child relationship between the platform windows. Not conforming backends are free to e.g. parent every viewport to the main viewport or not.
    UxImGuiID             FocusRouteParentWindowId;   // ID of parent window for shortcut focus route evaluation, e.g. Shortcut() call from Parent Window will succeed when this window is focused.
    UxImGuiViewportFlags  ViewportFlagsOverrideSet;   // Viewport flags to set when a window of this class owns a viewport. This allows you to enforce OS decoration or task bar icon, override the defaults on a per-window basis.
    UxImGuiViewportFlags  ViewportFlagsOverrideClear; // Viewport flags to clear when a window of this class owns a viewport. This allows you to enforce OS decoration or task bar icon, override the defaults on a per-window basis.
    UxImGuiTabItemFlags   TabItemFlagsOverrideSet;    // [EXPERIMENTAL] TabItem flags to set when a window of this class gets submitted into a dock node tab bar. May use with UxImGuiTabItemFlags_Leading or UxImGuiTabItemFlags_Trailing.
    UxImGuiDockNodeFlags  DockNodeFlagsOverrideSet;   // [EXPERIMENTAL] Dock node flags to set when a window of this class is hosted by a dock node (it doesn't have to be selected!)
    bool                DockingAlwaysTabBar;        // Set to true to enforce single floating windows of this class always having their own docking node (equivalent of setting the global io.ConfigDockingAlwaysTabBar)
    bool                DockingAllowUnclassed;      // Set to true to allow windows of this class to be docked/merged with an unclassed window. // FIXME-DOCK: Move to DockNodeFlags override?

    UxImGuiWindowClass() { memset(this, 0, sizeof(*this)); ParentViewportId = (UxImGuiID)-1; DockingAllowUnclassed = true; }
};

// Data payload for Drag and Drop operations: AcceptDragDropPayload(), GetDragDropPayload()
struct UxImGuiPayload
{
    // Members
    void*           Data;               // Data (copied and owned by dear imgui)
    int             DataSize;           // Data size

    // [Internal]
    UxImGuiID         SourceId;           // Source item id
    UxImGuiID         SourceParentId;     // Source parent id (if available)
    int             DataFrameCount;     // Data timestamp
    char            DataType[32 + 1];   // Data type tag (short user-supplied string, 32 characters max)
    bool            Preview;            // Set when AcceptDragDropPayload() was called and mouse has been hovering the target item (nb: handle overlapping drag targets)
    bool            Delivery;           // Set when AcceptDragDropPayload() was called and mouse button is released over the target item.

    UxImGuiPayload()  { Clear(); }
    void Clear()    { SourceId = SourceParentId = 0; Data = NULL; DataSize = 0; memset(DataType, 0, sizeof(DataType)); DataFrameCount = -1; Preview = Delivery = false; }
    bool IsDataType(const char* type) const { return DataFrameCount != -1 && strcmp(type, DataType) == 0; }
    bool IsPreview() const                  { return Preview; }
    bool IsDelivery() const                 { return Delivery; }
};

//-----------------------------------------------------------------------------
// [SECTION] Helpers (UxImGuiOnceUponAFrame, UxImGuiTextFilter, UxImGuiTextBuffer, UxImGuiStorage, UxImGuiListClipper, Math Operators, UxImColor)
//-----------------------------------------------------------------------------

// Helper: Unicode defines
#define IM_UNICODE_CODEPOINT_INVALID 0xFFFD     // Invalid Unicode code point (standard value).
#ifdef IMGUI_USE_WCHAR32
#define IM_UNICODE_CODEPOINT_MAX     0x10FFFF   // Maximum Unicode code point supported by this build.
#else
#define IM_UNICODE_CODEPOINT_MAX     0xFFFF     // Maximum Unicode code point supported by this build.
#endif

// Helper: Execute a block of code at maximum once a frame. Convenient if you want to quickly create a UI within deep-nested code that runs multiple times every frame.
// Usage: static UxImGuiOnceUponAFrame oaf; if (oaf) UxImGui::Text("This will be called only once per frame");
struct UxImGuiOnceUponAFrame
{
    UxImGuiOnceUponAFrame() { RefFrame = -1; }
    mutable int RefFrame;
    operator bool() const { int current_frame = UxImGui::GetFrameCount(); if (RefFrame == current_frame) return false; RefFrame = current_frame; return true; }
};

// Helper: Parse and apply text filters. In format "aaaaa[,bbbb][,ccccc]"
struct UxImGuiTextFilter
{
    IMGUI_API           UxImGuiTextFilter(const char* default_filter = "");
    IMGUI_API bool      Draw(const char* label = "Filter (inc,-exc)", float width = 0.0f);  // Helper calling InputText+Build
    IMGUI_API bool      PassFilter(const char* text, const char* text_end = NULL) const;
    IMGUI_API void      Build();
    void                Clear()          { InputBuf[0] = 0; Build(); }
    bool                IsActive() const { return !Filters.empty(); }

    // [Internal]
    struct UxImGuiTextRange
    {
        const char*     b;
        const char*     e;

        UxImGuiTextRange()                                { b = e = NULL; }
        UxImGuiTextRange(const char* _b, const char* _e)  { b = _b; e = _e; }
        bool            empty() const                   { return b == e; }
        IMGUI_API void  split(char separator, UxImVector<UxImGuiTextRange>* out) const;
    };
    char                    InputBuf[256];
    UxImVector<UxImGuiTextRange>Filters;
    int                     CountGrep;
};

// Helper: Growable text buffer for logging/accumulating text
// (this could be called 'UxImGuiTextBuilder' / 'UxImGuiStringBuilder')
struct UxImGuiTextBuffer
{
    UxImVector<char>      Buf;
    IMGUI_API static char EmptyString[1];

    UxImGuiTextBuffer()   { }
    inline char         operator[](int i) const { IM_ASSERT(Buf.Data != NULL); return Buf.Data[i]; }
    const char*         begin() const           { return Buf.Data ? &Buf.front() : EmptyString; }
    const char*         end() const             { return Buf.Data ? &Buf.back() : EmptyString; } // Buf is zero-terminated, so end() will point on the zero-terminator
    int                 size() const            { return Buf.Size ? Buf.Size - 1 : 0; }
    bool                empty() const           { return Buf.Size <= 1; }
    void                clear()                 { Buf.clear(); }
    void                resize(int size)        { if (Buf.Size > size) Buf.Data[size] = 0; Buf.resize(size ? size + 1 : 0, 0); } // Similar to resize(0) on UxImVector: empty string but don't free buffer.
    void                reserve(int capacity)   { Buf.reserve(capacity); }
    const char*         c_str() const           { return Buf.Data ? Buf.Data : EmptyString; }
    IMGUI_API void      append(const char* str, const char* str_end = NULL);
    IMGUI_API void      appendf(const char* fmt, ...) IM_FMTARGS(2);
    IMGUI_API void      appendfv(const char* fmt, va_list args) IM_FMTLIST(2);
};

// [Internal] Key+Value for UxImGuiStorage
struct UxImGuiStoragePair
{
    UxImGuiID     key;
    union       { int val_i; float val_f; void* val_p; };
    UxImGuiStoragePair(UxImGuiID _key, int _val)    { key = _key; val_i = _val; }
    UxImGuiStoragePair(UxImGuiID _key, float _val)  { key = _key; val_f = _val; }
    UxImGuiStoragePair(UxImGuiID _key, void* _val)  { key = _key; val_p = _val; }
};

// Helper: Key->Value storage
// Typically you don't have to worry about this since a storage is held within each Window.
// We use it to e.g. store collapse state for a tree (Int 0/1)
// This is optimized for efficient lookup (dichotomy into a contiguous buffer) and rare insertion (typically tied to user interactions aka max once a frame)
// You can use it as custom user storage for temporary values. Declare your own storage if, for example:
// - You want to manipulate the open/close state of a particular sub-tree in your interface (tree node uses Int 0/1 to store their state).
// - You want to store custom debug data easily without adding or editing structures in your code (probably not efficient, but convenient)
// Types are NOT stored, so it is up to you to make sure your Key don't collide with different types.
struct UxImGuiStorage
{
    // [Internal]
    UxImVector<UxImGuiStoragePair>      Data;

    // - Get***() functions find pair, never add/allocate. Pairs are sorted so a query is O(log N)
    // - Set***() functions find pair, insertion on demand if missing.
    // - Sorted insertion is costly, paid once. A typical frame shouldn't need to insert any new pair.
    void                Clear() { Data.clear(); }
    IMGUI_API int       GetInt(UxImGuiID key, int default_val = 0) const;
    IMGUI_API void      SetInt(UxImGuiID key, int val);
    IMGUI_API bool      GetBool(UxImGuiID key, bool default_val = false) const;
    IMGUI_API void      SetBool(UxImGuiID key, bool val);
    IMGUI_API float     GetFloat(UxImGuiID key, float default_val = 0.0f) const;
    IMGUI_API void      SetFloat(UxImGuiID key, float val);
    IMGUI_API void*     GetVoidPtr(UxImGuiID key) const; // default_val is NULL
    IMGUI_API void      SetVoidPtr(UxImGuiID key, void* val);

    // - Get***Ref() functions finds pair, insert on demand if missing, return pointer. Useful if you intend to do Get+Set.
    // - References are only valid until a new value is added to the storage. Calling a Set***() function or a Get***Ref() function invalidates the pointer.
    // - A typical use case where this is convenient for quick hacking (e.g. add storage during a live Edit&Continue session if you can't modify existing struct)
    //      float* pvar = UxImGui::GetFloatRef(key); UxImGui::SliderFloat("var", pvar, 0, 100.0f); some_var += *pvar;
    IMGUI_API int*      GetIntRef(UxImGuiID key, int default_val = 0);
    IMGUI_API bool*     GetBoolRef(UxImGuiID key, bool default_val = false);
    IMGUI_API float*    GetFloatRef(UxImGuiID key, float default_val = 0.0f);
    IMGUI_API void**    GetVoidPtrRef(UxImGuiID key, void* default_val = NULL);

    // Advanced: for quicker full rebuild of a storage (instead of an incremental one), you may add all your contents and then sort once.
    IMGUI_API void      BuildSortByKey();
    // Obsolete: use on your own storage if you know only integer are being stored (open/close all tree nodes)
    IMGUI_API void      SetAllInt(int val);

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    //typedef ::UxImGuiStoragePair UxImGuiStoragePair;  // 1.90.8: moved type outside struct
#endif
};

// Helper: Manually clip large list of items.
// If you have lots evenly spaced items and you have random access to the list, you can perform coarse
// clipping based on visibility to only submit items that are in view.
// The clipper calculates the range of visible items and advance the cursor to compensate for the non-visible items we have skipped.
// (Dear UxImGui already clip items based on their bounds but: it needs to first layout the item to do so, and generally
//  fetching/submitting your own data incurs additional cost. Coarse clipping using UxImGuiListClipper allows you to easily
//  scale using lists with tens of thousands of items without a problem)
// Usage:
//   UxImGuiListClipper clipper;
//   clipper.Begin(1000);         // We have 1000 elements, evenly spaced.
//   while (clipper.Step())
//       for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
//           UxImGui::Text("line number %d", i);
// Generally what happens is:
// - Clipper lets you process the first element (DisplayStart = 0, DisplayEnd = 1) regardless of it being visible or not.
// - User code submit that one element.
// - Clipper can measure the height of the first element
// - Clipper calculate the actual range of elements to display based on the current clipping rectangle, position the cursor before the first visible element.
// - User code submit visible elements.
// - The clipper also handles various subtleties related to keyboard/gamepad navigation, wrapping etc.
struct UxImGuiListClipper
{
    UxImGuiContext*   Ctx;                // Parent UI context
    int             DisplayStart;       // First item to display, updated by each call to Step()
    int             DisplayEnd;         // End of items to display (exclusive)
    int             ItemsCount;         // [Internal] Number of items
    float           ItemsHeight;        // [Internal] Height of item after a first step and item submission can calculate it
    float           StartPosY;          // [Internal] Cursor position at the time of Begin() or after table frozen rows are all processed
    double          StartSeekOffsetY;   // [Internal] Account for frozen rows in a table and initial loss of precision in very large windows.
    void*           TempData;           // [Internal] Internal data

    // items_count: Use INT_MAX if you don't know how many items you have (in which case the cursor won't be advanced in the final step, and you can call SeekCursorForItem() manually if you need)
    // items_height: Use -1.0f to be calculated automatically on first step. Otherwise pass in the distance between your items, typically GetTextLineHeightWithSpacing() or GetFrameHeightWithSpacing().
    IMGUI_API UxImGuiListClipper();
    IMGUI_API ~UxImGuiListClipper();
    IMGUI_API void  Begin(int items_count, float items_height = -1.0f);
    IMGUI_API void  End();             // Automatically called on the last call of Step() that returns false.
    IMGUI_API bool  Step();            // Call until it returns false. The DisplayStart/DisplayEnd fields will be set and you can process/draw those items.

    // Call IncludeItemByIndex() or IncludeItemsByIndex() *BEFORE* first call to Step() if you need a range of items to not be clipped, regardless of their visibility.
    // (Due to alignment / padding of certain items it is possible that an extra item may be included on either end of the display range).
    inline void     IncludeItemByIndex(int item_index)                  { IncludeItemsByIndex(item_index, item_index + 1); }
    IMGUI_API void  IncludeItemsByIndex(int item_begin, int item_end);  // item_end is exclusive e.g. use (42, 42+1) to make item 42 never clipped.

    // Seek cursor toward given item. This is automatically called while stepping.
    // - The only reason to call this is: you can use UxImGuiListClipper::Begin(INT_MAX) if you don't know item count ahead of time.
    // - In this case, after all steps are done, you'll want to call SeekCursorForItem(item_count).
    IMGUI_API void  SeekCursorForItem(int item_index);

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    inline void IncludeRangeByIndices(int item_begin, int item_end)      { IncludeItemsByIndex(item_begin, item_end); } // [renamed in 1.89.9]
    //inline void ForceDisplayRangeByIndices(int item_begin, int item_end) { IncludeItemsByIndex(item_begin, item_end); } // [renamed in 1.89.6]
    //inline UxImGuiListClipper(int items_count, float items_height = -1.0f) { memset(this, 0, sizeof(*this)); ItemsCount = -1; Begin(items_count, items_height); } // [removed in 1.79]
#endif
};

// Helpers: UxImVec2/UxImVec4 operators
// - It is important that we are keeping those disabled by default so they don't leak in user space.
// - This is in order to allow user enabling implicit cast operators between UxImVec2/UxImVec4 and their own types (using IM_VEC2_CLASS_EXTRA in imconfig.h)
// - Add '#define IMGUI_DEFINE_MATH_OPERATORS' before including this file (or in imconfig.h) to access courtesy maths operators for UxImVec2 and UxImVec4.
// - We intentionally provide UxImVec2*float but not float*UxImVec2: this is rare enough and we want to reduce the surface for possible user mistake.
#ifdef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED
IM_MSVC_RUNTIME_CHECKS_OFF
// UxImVec2 operators
static inline UxImVec2  operator*(const UxImVec2& lhs, const float rhs)     { return UxImVec2(lhs.x * rhs, lhs.y * rhs); }
static inline UxImVec2  operator/(const UxImVec2& lhs, const float rhs)     { return UxImVec2(lhs.x / rhs, lhs.y / rhs); }
static inline UxImVec2  operator+(const UxImVec2& lhs, const UxImVec2& rhs)   { return UxImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline UxImVec2  operator-(const UxImVec2& lhs, const UxImVec2& rhs)   { return UxImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline UxImVec2  operator*(const UxImVec2& lhs, const UxImVec2& rhs)   { return UxImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline UxImVec2  operator/(const UxImVec2& lhs, const UxImVec2& rhs)   { return UxImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
static inline UxImVec2  operator-(const UxImVec2& lhs)                      { return UxImVec2(-lhs.x, -lhs.y); }
static inline UxImVec2& operator*=(UxImVec2& lhs, const float rhs)          { lhs.x *= rhs; lhs.y *= rhs; return lhs; }
static inline UxImVec2& operator/=(UxImVec2& lhs, const float rhs)          { lhs.x /= rhs; lhs.y /= rhs; return lhs; }
static inline UxImVec2& operator+=(UxImVec2& lhs, const UxImVec2& rhs)        { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
static inline UxImVec2& operator-=(UxImVec2& lhs, const UxImVec2& rhs)        { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }
static inline UxImVec2& operator*=(UxImVec2& lhs, const UxImVec2& rhs)        { lhs.x *= rhs.x; lhs.y *= rhs.y; return lhs; }
static inline UxImVec2& operator/=(UxImVec2& lhs, const UxImVec2& rhs)        { lhs.x /= rhs.x; lhs.y /= rhs.y; return lhs; }
static inline bool    operator==(const UxImVec2& lhs, const UxImVec2& rhs)  { return lhs.x == rhs.x && lhs.y == rhs.y; }
static inline bool    operator!=(const UxImVec2& lhs, const UxImVec2& rhs)  { return lhs.x != rhs.x || lhs.y != rhs.y; }
// UxImVec4 operators
static inline UxImVec4  operator*(const UxImVec4& lhs, const float rhs)     { return UxImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs); }
static inline UxImVec4  operator/(const UxImVec4& lhs, const float rhs)     { return UxImVec4(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs); }
static inline UxImVec4  operator+(const UxImVec4& lhs, const UxImVec4& rhs)   { return UxImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }
static inline UxImVec4  operator-(const UxImVec4& lhs, const UxImVec4& rhs)   { return UxImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }
static inline UxImVec4  operator*(const UxImVec4& lhs, const UxImVec4& rhs)   { return UxImVec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w); }
static inline UxImVec4  operator/(const UxImVec4& lhs, const UxImVec4& rhs)   { return UxImVec4(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w); }
static inline UxImVec4  operator-(const UxImVec4& lhs)                      { return UxImVec4(-lhs.x, -lhs.y, -lhs.z, -lhs.w); }
static inline bool    operator==(const UxImVec4& lhs, const UxImVec4& rhs)  { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w; }
static inline bool    operator!=(const UxImVec4& lhs, const UxImVec4& rhs)  { return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z || lhs.w != rhs.w; }
IM_MSVC_RUNTIME_CHECKS_RESTORE
#endif

// Helpers macros to generate 32-bit encoded colors
// - User can declare their own format by #defining the 5 _SHIFT/_MASK macros in their imconfig file.
// - Any setting other than the default will need custom backend support. The only standard backend that supports anything else than the default is DirectX9.
#ifndef IM_COL32_R_SHIFT
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IM_COL32_R_SHIFT    16
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    0
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#else
#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#endif
#endif
#define IM_COL32(R,G,B,A)    (((UxImU32)(A)<<IM_COL32_A_SHIFT) | ((UxImU32)(B)<<IM_COL32_B_SHIFT) | ((UxImU32)(G)<<IM_COL32_G_SHIFT) | ((UxImU32)(R)<<IM_COL32_R_SHIFT))
#define IM_COL32_WHITE       IM_COL32(255,255,255,255)  // Opaque white = 0xFFFFFFFF
#define IM_COL32_BLACK       IM_COL32(0,0,0,255)        // Opaque black
#define IM_COL32_BLACK_TRANS IM_COL32(0,0,0,0)          // Transparent black = 0x00000000

// Helper: UxImColor() implicitly converts colors to either UxImU32 (packed 4x1 byte) or UxImVec4 (4x1 float)
// Prefer using IM_COL32() macros if you want a guaranteed compile-time UxImU32 for usage with UxImDrawList API.
// **Avoid storing UxImColor! Store either u32 of UxImVec4. This is not a full-featured color class. MAY OBSOLETE.
// **None of the UxImGui API are using UxImColor directly but you can use it as a convenience to pass colors in either UxImU32 or UxImVec4 formats. Explicitly cast to UxImU32 or UxImVec4 if needed.
struct UxImColor
{
    UxImVec4          Value;

    constexpr UxImColor()                                             { }
    constexpr UxImColor(float r, float g, float b, float a = 1.0f)    : Value(r, g, b, a) { }
    constexpr UxImColor(const UxImVec4& col)                            : Value(col) {}
    constexpr UxImColor(int r, int g, int b, int a = 255)             : Value((float)r * (1.0f / 255.0f), (float)g * (1.0f / 255.0f), (float)b * (1.0f / 255.0f), (float)a* (1.0f / 255.0f)) {}
    constexpr UxImColor(UxImU32 rgba)                                   : Value((float)((rgba >> IM_COL32_R_SHIFT) & 0xFF) * (1.0f / 255.0f), (float)((rgba >> IM_COL32_G_SHIFT) & 0xFF) * (1.0f / 255.0f), (float)((rgba >> IM_COL32_B_SHIFT) & 0xFF) * (1.0f / 255.0f), (float)((rgba >> IM_COL32_A_SHIFT) & 0xFF) * (1.0f / 255.0f)) {}
    inline operator UxImU32() const                                   { return UxImGui::ColorConvertFloat4ToU32(Value); }
    inline operator UxImVec4() const                                  { return Value; }

    // FIXME-OBSOLETE: May need to obsolete/cleanup those helpers.
    inline void    SetHSV(float h, float s, float v, float a = 1.0f){ UxImGui::ColorConvertHSVtoRGB(h, s, v, Value.x, Value.y, Value.z); Value.w = a; }
    static UxImColor HSV(float h, float s, float v, float a = 1.0f)   { float r, g, b; UxImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b); return UxImColor(r, g, b, a); }
};

//-----------------------------------------------------------------------------
// [SECTION] Multi-Select API flags and structures (UxImGuiMultiSelectFlags, UxImGuiSelectionRequestType, UxImGuiSelectionRequest, UxImGuiMultiSelectIO, UxImGuiSelectionBasicStorage)
//-----------------------------------------------------------------------------

// Multi-selection system
// Documentation at: https://github.com/ocornut/imgui/wiki/Multi-Select
// - Refer to 'Demo->Widgets->Selection State & Multi-Select' for demos using this.
// - This system implements standard multi-selection idioms (CTRL+Mouse/Keyboard, SHIFT+Mouse/Keyboard, etc)
//   with support for clipper (skipping non-visible items), box-select and many other details.
// - Selectable(), Checkbox() are supported but custom widgets may use it as well.
// - TreeNode() is technically supported but... using this correctly is more complicated: you need some sort of linear/random access to your tree,
//   which is suited to advanced trees setups also implementing filters and clipper. We will work toward simplifying and demoing it.
// - In the spirit of Dear UxImGui design, your code owns actual selection data.
//   This is designed to allow all kinds of selection storage you may use in your application e.g. set/map/hash.
// About UxImGuiSelectionBasicStorage:
// - This is an optional helper to store a selection state and apply selection requests.
// - It is used by our demos and provided as a convenience to quickly implement multi-selection.
// Usage:
// - Identify submitted items with SetNextItemSelectionUserData(), most likely using an index into your current data-set.
// - Store and maintain actual selection data using persistent object identifiers.
// - Usage flow:
//     BEGIN - (1) Call BeginMultiSelect() and retrieve the UxImGuiMultiSelectIO* result.
//           - (2) Honor request list (SetAll/SetRange requests) by updating your selection data. Same code as Step 6.
//           - (3) [If using clipper] You need to make sure RangeSrcItem is always submitted. Calculate its index and pass to clipper.IncludeItemByIndex(). If storing indices in UxImGuiSelectionUserData, a simple clipper.IncludeItemByIndex(ms_io->RangeSrcItem) call will work.
//     LOOP  - (4) Submit your items with SetNextItemSelectionUserData() + Selectable()/TreeNode() calls.
//     END   - (5) Call EndMultiSelect() and retrieve the UxImGuiMultiSelectIO* result.
//           - (6) Honor request list (SetAll/SetRange requests) by updating your selection data. Same code as Step 2.
//     If you submit all items (no clipper), Step 2 and 3 are optional and will be handled by each item themselves. It is fine to always honor those steps.
// About UxImGuiSelectionUserData:
// - This can store an application-defined identifier (e.g. index or pointer) submitted via SetNextItemSelectionUserData().
// - In return we store them into RangeSrcItem/RangeFirstItem/RangeLastItem and other fields in UxImGuiMultiSelectIO.
// - Most applications will store an object INDEX, hence the chosen name and type. Storing an index is natural, because
//   SetRange requests will give you two end-points and you will need to iterate/interpolate between them to update your selection.
// - However it is perfectly possible to store a POINTER or another IDENTIFIER inside UxImGuiSelectionUserData.
//   Our system never assume that you identify items by indices, it never attempts to interpolate between two values.
// - If you enable UxImGuiMultiSelectFlags_NoRangeSelect then it is guaranteed that you will never have to interpolate
//   between two UxImGuiSelectionUserData, which may be a convenient way to use part of the feature with less code work.
// - As most users will want to store an index, for convenience and to reduce confusion we use UxImS64 instead of void*,
//   being syntactically easier to downcast. Feel free to reinterpret_cast and store a pointer inside.

// Flags for BeginMultiSelect()
enum UxImGuiMultiSelectFlags_
{
    UxImGuiMultiSelectFlags_None                  = 0,
    UxImGuiMultiSelectFlags_SingleSelect          = 1 << 0,   // Disable selecting more than one item. This is available to allow single-selection code to share same code/logic if desired. It essentially disables the main purpose of BeginMultiSelect() tho!
    UxImGuiMultiSelectFlags_NoSelectAll           = 1 << 1,   // Disable CTRL+A shortcut to select all.
    UxImGuiMultiSelectFlags_NoRangeSelect         = 1 << 2,   // Disable Shift+selection mouse/keyboard support (useful for unordered 2D selection). With BoxSelect is also ensure contiguous SetRange requests are not combined into one. This allows not handling interpolation in SetRange requests.
    UxImGuiMultiSelectFlags_NoAutoSelect          = 1 << 3,   // Disable selecting items when navigating (useful for e.g. supporting range-select in a list of checkboxes).
    UxImGuiMultiSelectFlags_NoAutoClear           = 1 << 4,   // Disable clearing selection when navigating or selecting another one (generally used with UxImGuiMultiSelectFlags_NoAutoSelect. useful for e.g. supporting range-select in a list of checkboxes).
    UxImGuiMultiSelectFlags_NoAutoClearOnReselect = 1 << 5,   // Disable clearing selection when clicking/selecting an already selected item.
    UxImGuiMultiSelectFlags_BoxSelect1d           = 1 << 6,   // Enable box-selection with same width and same x pos items (e.g. full row Selectable()). Box-selection works better with little bit of spacing between items hit-box in order to be able to aim at empty space.
    UxImGuiMultiSelectFlags_BoxSelect2d           = 1 << 7,   // Enable box-selection with varying width or varying x pos items support (e.g. different width labels, or 2D layout/grid). This is slower: alters clipping logic so that e.g. horizontal movements will update selection of normally clipped items.
    UxImGuiMultiSelectFlags_BoxSelectNoScroll     = 1 << 8,   // Disable scrolling when box-selecting near edges of scope.
    UxImGuiMultiSelectFlags_ClearOnEscape         = 1 << 9,   // Clear selection when pressing Escape while scope is focused.
    UxImGuiMultiSelectFlags_ClearOnClickVoid      = 1 << 10,  // Clear selection when clicking on empty location within scope.
    UxImGuiMultiSelectFlags_ScopeWindow           = 1 << 11,  // Scope for _BoxSelect and _ClearOnClickVoid is whole window (Default). Use if BeginMultiSelect() covers a whole window or used a single time in same window.
    UxImGuiMultiSelectFlags_ScopeRect             = 1 << 12,  // Scope for _BoxSelect and _ClearOnClickVoid is rectangle encompassing BeginMultiSelect()/EndMultiSelect(). Use if BeginMultiSelect() is called multiple times in same window.
    UxImGuiMultiSelectFlags_SelectOnClick         = 1 << 13,  // Apply selection on mouse down when clicking on unselected item. (Default)
    UxImGuiMultiSelectFlags_SelectOnClickRelease  = 1 << 14,  // Apply selection on mouse release when clicking an unselected item. Allow dragging an unselected item without altering selection.
    //UxImGuiMultiSelectFlags_RangeSelect2d       = 1 << 15,  // Shift+Selection uses 2d geometry instead of linear sequence, so possible to use Shift+up/down to select vertically in grid. Analogous to what BoxSelect does.
    UxImGuiMultiSelectFlags_NavWrapX              = 1 << 16,  // [Temporary] Enable navigation wrapping on X axis. Provided as a convenience because we don't have a design for the general Nav API for this yet. When the more general feature be public we may obsolete this flag in favor of new one.
};

// Main IO structure returned by BeginMultiSelect()/EndMultiSelect().
// This mainly contains a list of selection requests.
// - Use 'Demo->Tools->Debug Log->Selection' to see requests as they happen.
// - Some fields are only useful if your list is dynamic and allows deletion (getting post-deletion focus/state right is shown in the demo)
// - Below: who reads/writes each fields? 'r'=read, 'w'=write, 'ms'=multi-select code, 'app'=application/user code.
struct UxImGuiMultiSelectIO
{
    //------------------------------------------// BeginMultiSelect / EndMultiSelect
    UxImVector<UxImGuiSelectionRequest> Requests;   //  ms:w, app:r     /  ms:w  app:r   // Requests to apply to your selection data.
    UxImGuiSelectionUserData      RangeSrcItem;   //  ms:w  app:r     /                // (If using clipper) Begin: Source item (often the first selected item) must never be clipped: use clipper.IncludeItemByIndex() to ensure it is submitted.
    UxImGuiSelectionUserData      NavIdItem;      //  ms:w, app:r     /                // (If using deletion) Last known SetNextItemSelectionUserData() value for NavId (if part of submitted items).
    bool                        NavIdSelected;  //  ms:w, app:r     /        app:r   // (If using deletion) Last known selection state for NavId (if part of submitted items).
    bool                        RangeSrcReset;  //        app:w     /  ms:r          // (If using deletion) Set before EndMultiSelect() to reset ResetSrcItem (e.g. if deleted selection).
    int                         ItemsCount;     //  ms:w, app:r     /        app:r   // 'int items_count' parameter to BeginMultiSelect() is copied here for convenience, allowing simpler calls to your ApplyRequests handler. Not used internally.
};

// Selection request type
enum UxImGuiSelectionRequestType
{
    UxImGuiSelectionRequestType_None = 0,
    UxImGuiSelectionRequestType_SetAll,           // Request app to clear selection (if Selected==false) or select all items (if Selected==true). We cannot set RangeFirstItem/RangeLastItem as its contents is entirely up to user (not necessarily an index)
    UxImGuiSelectionRequestType_SetRange,         // Request app to select/unselect [RangeFirstItem..RangeLastItem] items (inclusive) based on value of Selected. Only EndMultiSelect() request this, app code can read after BeginMultiSelect() and it will always be false.
};

// Selection request item
struct UxImGuiSelectionRequest
{
    //------------------------------------------// BeginMultiSelect / EndMultiSelect
    UxImGuiSelectionRequestType   Type;           //  ms:w, app:r     /  ms:w, app:r   // Request type. You'll most often receive 1 Clear + 1 SetRange with a single-item range.
    bool                        Selected;       //  ms:w, app:r     /  ms:w, app:r   // Parameter for SetAll/SetRange requests (true = select, false = unselect)
    UxImS8                        RangeDirection; //                  /  ms:w  app:r   // Parameter for SetRange request: +1 when RangeFirstItem comes before RangeLastItem, -1 otherwise. Useful if you want to preserve selection order on a backward Shift+Click.
    UxImGuiSelectionUserData      RangeFirstItem; //                  /  ms:w, app:r   // Parameter for SetRange request (this is generally == RangeSrcItem when shift selecting from top to bottom).
    UxImGuiSelectionUserData      RangeLastItem;  //                  /  ms:w, app:r   // Parameter for SetRange request (this is generally == RangeSrcItem when shift selecting from bottom to top). Inclusive!
};

// Optional helper to store multi-selection state + apply multi-selection requests.
// - Used by our demos and provided as a convenience to easily implement basic multi-selection.
// - Iterate selection with 'void* it = NULL; UxImGuiID id; while (selection.GetNextSelectedItem(&it, &id)) { ... }'
//   Or you can check 'if (Contains(id)) { ... }' for each possible object if their number is not too high to iterate.
// - USING THIS IS NOT MANDATORY. This is only a helper and not a required API.
// To store a multi-selection, in your application you could:
// - Use this helper as a convenience. We use our simple key->value UxImGuiStorage as a std::set<UxImGuiID> replacement.
// - Use your own external storage: e.g. std::set<MyObjectId>, std::vector<MyObjectId>, interval trees, intrusively stored selection etc.
// In UxImGuiSelectionBasicStorage we:
// - always use indices in the multi-selection API (passed to SetNextItemSelectionUserData(), retrieved in UxImGuiMultiSelectIO)
// - use the AdapterIndexToStorageId() indirection layer to abstract how persistent selection data is derived from an index.
// - use decently optimized logic to allow queries and insertion of very large selection sets.
// - do not preserve selection order.
// Many combinations are possible depending on how you prefer to store your items and how you prefer to store your selection.
// Large applications are likely to eventually want to get rid of this indirection layer and do their own thing.
// See https://github.com/ocornut/imgui/wiki/Multi-Select for details and pseudo-code using this helper.
struct UxImGuiSelectionBasicStorage
{
    // Members
    int             Size;           //          // Number of selected items, maintained by this helper.
    bool            PreserveOrder;  // = false  // GetNextSelectedItem() will return ordered selection (currently implemented by two additional sorts of selection. Could be improved)
    void*           UserData;       // = NULL   // User data for use by adapter function        // e.g. selection.UserData = (void*)my_items;
    UxImGuiID         (*AdapterIndexToStorageId)(UxImGuiSelectionBasicStorage* self, int idx);      // e.g. selection.AdapterIndexToStorageId = [](UxImGuiSelectionBasicStorage* self, int idx) { return ((MyItems**)self->UserData)[idx]->ID; };
    int             _SelectionOrder;// [Internal] Increasing counter to store selection order
    UxImGuiStorage    _Storage;       // [Internal] Selection set. Think of this as similar to e.g. std::set<UxImGuiID>. Prefer not accessing directly: iterate with GetNextSelectedItem().

    // Methods
    IMGUI_API UxImGuiSelectionBasicStorage();
    IMGUI_API void  ApplyRequests(UxImGuiMultiSelectIO* ms_io);   // Apply selection requests coming from BeginMultiSelect() and EndMultiSelect() functions. It uses 'items_count' passed to BeginMultiSelect()
    IMGUI_API bool  Contains(UxImGuiID id) const;                 // Query if an item id is in selection.
    IMGUI_API void  Clear();                                    // Clear selection
    IMGUI_API void  Swap(UxImGuiSelectionBasicStorage& r);        // Swap two selections
    IMGUI_API void  SetItemSelected(UxImGuiID id, bool selected); // Add/remove an item from selection (generally done by ApplyRequests() function)
    IMGUI_API bool  GetNextSelectedItem(void** opaque_it, UxImGuiID* out_id); // Iterate selection with 'void* it = NULL; UxImGuiID id; while (selection.GetNextSelectedItem(&it, &id)) { ... }'
    inline UxImGuiID  GetStorageIdFromIndex(int idx)              { return AdapterIndexToStorageId(this, idx); }  // Convert index to item id based on provided adapter.
};

// Optional helper to apply multi-selection requests to existing randomly accessible storage.
// Convenient if you want to quickly wire multi-select API on e.g. an array of bool or items storing their own selection state.
struct UxImGuiSelectionExternalStorage
{
    // Members
    void*           UserData;       // User data for use by adapter function                                // e.g. selection.UserData = (void*)my_items;
    void            (*AdapterSetItemSelected)(UxImGuiSelectionExternalStorage* self, int idx, bool selected); // e.g. AdapterSetItemSelected = [](UxImGuiSelectionExternalStorage* self, int idx, bool selected) { ((MyItems**)self->UserData)[idx]->Selected = selected; }

    // Methods
    IMGUI_API UxImGuiSelectionExternalStorage();
    IMGUI_API void  ApplyRequests(UxImGuiMultiSelectIO* ms_io);   // Apply selection requests by using AdapterSetItemSelected() calls
};

//-----------------------------------------------------------------------------
// [SECTION] Drawing API (UxImDrawCmd, UxImDrawIdx, UxImDrawVert, UxImDrawChannel, UxImDrawListSplitter, UxImDrawListFlags, UxImDrawList, UxImDrawData)
// Hold a series of drawing commands. The user provides a renderer for UxImDrawData which essentially contains an array of UxImDrawList.
//-----------------------------------------------------------------------------

// The maximum line width to bake anti-aliased textures for. Build atlas with UxImFontAtlasFlags_NoBakedLines to disable baking.
#ifndef IM_DRAWLIST_TEX_LINES_WIDTH_MAX
#define IM_DRAWLIST_TEX_LINES_WIDTH_MAX     (32)
#endif

// UxImDrawIdx: vertex index. [Compile-time configurable type]
// - To use 16-bit indices + allow large meshes: backend need to set 'io.BackendFlags |= UxImGuiBackendFlags_RendererHasVtxOffset' and handle UxImDrawCmd::VtxOffset (recommended).
// - To use 32-bit indices: override with '#define UxImDrawIdx unsigned int' in your imconfig.h file.
#ifndef UxImDrawIdx
typedef unsigned short UxImDrawIdx;   // Default: 16-bit (for maximum compatibility with renderer backends)
#endif

// UxImDrawCallback: Draw callbacks for advanced uses [configurable type: override in imconfig.h]
// NB: You most likely do NOT need to use draw callbacks just to create your own widget or customized UI rendering,
// you can poke into the draw list for that! Draw callback may be useful for example to:
//  A) Change your GPU render state,
//  B) render a complex 3D scene inside a UI element without an intermediate texture/render target, etc.
// The expected behavior from your rendering function is 'if (cmd.UserCallback != NULL) { cmd.UserCallback(parent_list, cmd); } else { RenderTriangles() }'
// If you want to override the signature of UxImDrawCallback, you can simply use e.g. '#define UxImDrawCallback MyDrawCallback' (in imconfig.h) + update rendering backend accordingly.
#ifndef UxImDrawCallback
typedef void (*UxImDrawCallback)(const UxImDrawList* parent_list, const UxImDrawCmd* cmd);
#endif

// Special Draw callback value to request renderer backend to reset the graphics/render state.
// The renderer backend needs to handle this special value, otherwise it will crash trying to call a function at this address.
// This is useful, for example, if you submitted callbacks which you know have altered the render state and you want it to be restored.
// Render state is not reset by default because they are many perfectly useful way of altering render state (e.g. changing shader/blending settings before an Image call).
#define UxImDrawCallback_ResetRenderState     (UxImDrawCallback)(-8)

// Typically, 1 command = 1 GPU draw call (unless command is a callback)
// - VtxOffset: When 'io.BackendFlags & UxImGuiBackendFlags_RendererHasVtxOffset' is enabled,
//   this fields allow us to render meshes larger than 64K vertices while keeping 16-bit indices.
//   Backends made for <1.71. will typically ignore the VtxOffset fields.
// - The ClipRect/TexRef/VtxOffset fields must be contiguous as we memcmp() them together (this is asserted for).
struct UxImDrawCmd
{
    UxImVec4          ClipRect;           // 4*4  // Clipping rectangle (x1, y1, x2, y2). Subtract UxImDrawData->DisplayPos to get clipping rectangle in "viewport" coordinates
    UxImTextureRef    TexRef;             // 16   // Reference to a font/texture atlas (where backend called UxImTextureData::SetTexID()) or to a user-provided texture ID (via e.g. UxImGui::Image() calls). Both will lead to a UxImTextureID value.
    unsigned int    VtxOffset;          // 4    // Start offset in vertex buffer. UxImGuiBackendFlags_RendererHasVtxOffset: always 0, otherwise may be >0 to support meshes larger than 64K vertices with 16-bit indices.
    unsigned int    IdxOffset;          // 4    // Start offset in index buffer.
    unsigned int    ElemCount;          // 4    // Number of indices (multiple of 3) to be rendered as triangles. Vertices are stored in the callee UxImDrawList's vtx_buffer[] array, indices in idx_buffer[].
    UxImDrawCallback  UserCallback;       // 4-8  // If != NULL, call the function instead of rendering the vertices. clip_rect and texture_id will be set normally.
    void*           UserCallbackData;   // 4-8  // Callback user data (when UserCallback != NULL). If called AddCallback() with size == 0, this is a copy of the AddCallback() argument. If called AddCallback() with size > 0, this is pointing to a buffer where data is stored.
    int             UserCallbackDataSize;  // 4 // Size of callback user data when using storage, otherwise 0.
    int             UserCallbackDataOffset;// 4 // [Internal] Offset of callback user data when using storage, otherwise -1.

    UxImDrawCmd()     { memset(this, 0, sizeof(*this)); } // Also ensure our padding fields are zeroed

    // Since 1.83: returns UxImTextureID associated with this draw call. Warning: DO NOT assume this is always same as 'TextureId' (we will change this function for an upcoming feature)
    // Since 1.92: removed UxImDrawCmd::TextureId field, the getter function must be used!
    inline UxImTextureID GetTexID() const;    // == (TexRef._TexData ? TexRef._TexData->TexID : TexRef._TexID
};

// Vertex layout
#ifndef IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT
struct UxImDrawVert
{
    UxImVec2  pos;
    UxImVec2  uv;
    UxImU32   col;
};
#else
// You can override the vertex format layout by defining IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT in imconfig.h
// The code expect UxImVec2 pos (8 bytes), UxImVec2 uv (8 bytes), UxImU32 col (4 bytes), but you can re-order them or add other fields as needed to simplify integration in your engine.
// The type has to be described within the macro (you can either declare the struct or use a typedef). This is because UxImVec2/UxImU32 are likely not declared at the time you'd want to set your type up.
// NOTE: IMGUI DOESN'T CLEAR THE STRUCTURE AND DOESN'T CALL A CONSTRUCTOR SO ANY CUSTOM FIELD WILL BE UNINITIALIZED. IF YOU ADD EXTRA FIELDS (SUCH AS A 'Z' COORDINATES) YOU WILL NEED TO CLEAR THEM DURING RENDER OR TO IGNORE THEM.
IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT;
#endif

// [Internal] For use by UxImDrawList
struct UxImDrawCmdHeader
{
    UxImVec4          ClipRect;
    UxImTextureRef    TexRef;
    unsigned int    VtxOffset;
};

// [Internal] For use by UxImDrawListSplitter
struct UxImDrawChannel
{
    UxImVector<UxImDrawCmd>         _CmdBuffer;
    UxImVector<UxImDrawIdx>         _IdxBuffer;
};

// Split/Merge functions are used to split the draw list into different layers which can be drawn into out of order.
// This is used by the Columns/Tables API, so items of each column can be batched together in a same draw call.
struct UxImDrawListSplitter
{
    int                         _Current;    // Current channel number (0)
    int                         _Count;      // Number of active channels (1+)
    UxImVector<UxImDrawChannel>     _Channels;   // Draw channels (not resized down so _Count might be < Channels.Size)

    inline UxImDrawListSplitter()  { memset(this, 0, sizeof(*this)); }
    inline ~UxImDrawListSplitter() { ClearFreeMemory(); }
    inline void                 Clear() { _Current = 0; _Count = 1; } // Do not clear Channels[] so our allocations are reused next frame
    IMGUI_API void              ClearFreeMemory();
    IMGUI_API void              Split(UxImDrawList* draw_list, int count);
    IMGUI_API void              Merge(UxImDrawList* draw_list);
    IMGUI_API void              SetCurrentChannel(UxImDrawList* draw_list, int channel_idx);
};

// Flags for UxImDrawList functions
// (Legacy: bit 0 must always correspond to UxImDrawFlags_Closed to be backward compatible with old API using a bool. Bits 1..3 must be unused)
enum UxImDrawFlags_
{
    UxImDrawFlags_None                        = 0,
    UxImDrawFlags_Closed                      = 1 << 0, // PathStroke(), AddPolyline(): specify that shape should be closed (Important: this is always == 1 for legacy reason)
    UxImDrawFlags_RoundCornersTopLeft         = 1 << 4, // AddRect(), AddRectFilled(), PathRect(): enable rounding top-left corner only (when rounding > 0.0f, we default to all corners). Was 0x01.
    UxImDrawFlags_RoundCornersTopRight        = 1 << 5, // AddRect(), AddRectFilled(), PathRect(): enable rounding top-right corner only (when rounding > 0.0f, we default to all corners). Was 0x02.
    UxImDrawFlags_RoundCornersBottomLeft      = 1 << 6, // AddRect(), AddRectFilled(), PathRect(): enable rounding bottom-left corner only (when rounding > 0.0f, we default to all corners). Was 0x04.
    UxImDrawFlags_RoundCornersBottomRight     = 1 << 7, // AddRect(), AddRectFilled(), PathRect(): enable rounding bottom-right corner only (when rounding > 0.0f, we default to all corners). Wax 0x08.
    UxImDrawFlags_RoundCornersNone            = 1 << 8, // AddRect(), AddRectFilled(), PathRect(): disable rounding on all corners (when rounding > 0.0f). This is NOT zero, NOT an implicit flag!
    UxImDrawFlags_RoundCornersTop             = UxImDrawFlags_RoundCornersTopLeft | UxImDrawFlags_RoundCornersTopRight,
    UxImDrawFlags_RoundCornersBottom          = UxImDrawFlags_RoundCornersBottomLeft | UxImDrawFlags_RoundCornersBottomRight,
    UxImDrawFlags_RoundCornersLeft            = UxImDrawFlags_RoundCornersBottomLeft | UxImDrawFlags_RoundCornersTopLeft,
    UxImDrawFlags_RoundCornersRight           = UxImDrawFlags_RoundCornersBottomRight | UxImDrawFlags_RoundCornersTopRight,
    UxImDrawFlags_RoundCornersAll             = UxImDrawFlags_RoundCornersTopLeft | UxImDrawFlags_RoundCornersTopRight | UxImDrawFlags_RoundCornersBottomLeft | UxImDrawFlags_RoundCornersBottomRight,
    UxImDrawFlags_RoundCornersDefault_        = UxImDrawFlags_RoundCornersAll, // Default to ALL corners if none of the _RoundCornersXX flags are specified.
    UxImDrawFlags_RoundCornersMask_           = UxImDrawFlags_RoundCornersAll | UxImDrawFlags_RoundCornersNone,
};

// Flags for UxImDrawList instance. Those are set automatically by UxImGui:: functions from UxImGuiIO settings, and generally not manipulated directly.
// It is however possible to temporarily alter flags between calls to UxImDrawList:: functions.
enum UxImDrawListFlags_
{
    UxImDrawListFlags_None                    = 0,
    UxImDrawListFlags_AntiAliasedLines        = 1 << 0,  // Enable anti-aliased lines/borders (*2 the number of triangles for 1.0f wide line or lines thin enough to be drawn using textures, otherwise *3 the number of triangles)
    UxImDrawListFlags_AntiAliasedLinesUseTex  = 1 << 1,  // Enable anti-aliased lines/borders using textures when possible. Require backend to render with bilinear filtering (NOT point/nearest filtering).
    UxImDrawListFlags_AntiAliasedFill         = 1 << 2,  // Enable anti-aliased edge around filled shapes (rounded rectangles, circles).
    UxImDrawListFlags_AllowVtxOffset          = 1 << 3,  // Can emit 'VtxOffset > 0' to allow large meshes. Set when 'UxImGuiBackendFlags_RendererHasVtxOffset' is enabled.
};

// Draw command list
// This is the low-level list of polygons that UxImGui:: functions are filling. At the end of the frame,
// all command lists are passed to your UxImGuiIO::RenderDrawListFn function for rendering.
// Each dear imgui window contains its own UxImDrawList. You can use UxImGui::GetWindowDrawList() to
// access the current window draw list and draw custom primitives.
// You can interleave normal UxImGui:: calls and adding primitives to the current draw list.
// In single viewport mode, top-left is == GetMainViewport()->Pos (generally 0,0), bottom-right is == GetMainViewport()->Pos+Size (generally io.DisplaySize).
// You are totally free to apply whatever transformation matrix you want to the data (depending on the use of the transformation you may want to apply it to ClipRect as well!)
// Important: Primitives are always added to the list and not culled (culling is done at higher-level by UxImGui:: functions), if you use this API a lot consider coarse culling your drawn objects.
struct UxImDrawList
{
    // This is what you have to render
    UxImVector<UxImDrawCmd>     CmdBuffer;          // Draw commands. Typically 1 command = 1 GPU draw call, unless the command is a callback.
    UxImVector<UxImDrawIdx>     IdxBuffer;          // Index buffer. Each command consume UxImDrawCmd::ElemCount of those
    UxImVector<UxImDrawVert>    VtxBuffer;          // Vertex buffer.
    UxImDrawListFlags         Flags;              // Flags, you may poke into these to adjust anti-aliasing settings per-primitive.

    // [Internal, used while building lists]
    unsigned int            _VtxCurrentIdx;     // [Internal] generally == VtxBuffer.Size unless we are past 64K vertices, in which case this gets reset to 0.
    UxImDrawListSharedData*   _Data;              // Pointer to shared draw data (you can use UxImGui::GetDrawListSharedData() to get the one from current UxImGui context)
    UxImDrawVert*             _VtxWritePtr;       // [Internal] point within VtxBuffer.Data after each add command (to avoid using the UxImVector<> operators too much)
    UxImDrawIdx*              _IdxWritePtr;       // [Internal] point within IdxBuffer.Data after each add command (to avoid using the UxImVector<> operators too much)
    UxImVector<UxImVec2>        _Path;              // [Internal] current path building
    UxImDrawCmdHeader         _CmdHeader;         // [Internal] template of active commands. Fields should match those of CmdBuffer.back().
    UxImDrawListSplitter      _Splitter;          // [Internal] for channels api (note: prefer using your own persistent instance of UxImDrawListSplitter!)
    UxImVector<UxImVec4>        _ClipRectStack;     // [Internal]
    UxImVector<UxImTextureRef>  _TextureStack;      // [Internal]
    UxImVector<UxImU8>          _CallbacksDataBuf;  // [Internal]
    float                   _FringeScale;       // [Internal] anti-alias fringe is scaled by this value, this helps to keep things sharp while zooming at vertex buffer content
    const char*             _OwnerName;         // Pointer to owner window's name for debugging

    // If you want to create UxImDrawList instances, pass them UxImGui::GetDrawListSharedData().
    // (advanced: you may create and use your own UxImDrawListSharedData so you can use UxImDrawList without UxImGui, but that's more involved)
    IMGUI_API UxImDrawList(UxImDrawListSharedData* shared_data);
    IMGUI_API ~UxImDrawList();

    IMGUI_API void  PushClipRect(const UxImVec2& clip_rect_min, const UxImVec2& clip_rect_max, bool intersect_with_current_clip_rect = false);  // Render-level scissoring. This is passed down to your render function but not used for CPU-side coarse clipping. Prefer using higher-level UxImGui::PushClipRect() to affect logic (hit-testing and widget culling)
    IMGUI_API void  PushClipRectFullScreen();
    IMGUI_API void  PopClipRect();
    IMGUI_API void  PushTexture(UxImTextureRef tex_ref);
    IMGUI_API void  PopTexture();
    inline UxImVec2   GetClipRectMin() const { const UxImVec4& cr = _ClipRectStack.back(); return UxImVec2(cr.x, cr.y); }
    inline UxImVec2   GetClipRectMax() const { const UxImVec4& cr = _ClipRectStack.back(); return UxImVec2(cr.z, cr.w); }

    // Primitives
    // - Filled shapes must always use clockwise winding order. The anti-aliasing fringe depends on it. Counter-clockwise shapes will have "inward" anti-aliasing.
    // - For rectangular primitives, "p_min" and "p_max" represent the upper-left and lower-right corners.
    // - For circle primitives, use "num_segments == 0" to automatically calculate tessellation (preferred).
    //   In older versions (until Dear UxImGui 1.77) the AddCircle functions defaulted to num_segments == 12.
    //   In future versions we will use textures to provide cheaper and higher-quality circles.
    //   Use AddNgon() and AddNgonFilled() functions if you need to guarantee a specific number of sides.
    IMGUI_API void  AddLine(const UxImVec2& p1, const UxImVec2& p2, UxImU32 col, float thickness = 1.0f);
    IMGUI_API void  AddRect(const UxImVec2& p_min, const UxImVec2& p_max, UxImU32 col, float rounding = 0.0f, UxImDrawFlags flags = 0, float thickness = 1.0f);   // a: upper-left, b: lower-right (== upper-left + size)
    IMGUI_API void  AddRectFilled(const UxImVec2& p_min, const UxImVec2& p_max, UxImU32 col, float rounding = 0.0f, UxImDrawFlags flags = 0);                     // a: upper-left, b: lower-right (== upper-left + size)
    IMGUI_API void  AddRectFilledMultiColor(const UxImVec2& p_min, const UxImVec2& p_max, UxImU32 col_upr_left, UxImU32 col_upr_right, UxImU32 col_bot_right, UxImU32 col_bot_left);
    IMGUI_API void  AddQuad(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, UxImU32 col, float thickness = 1.0f);
    IMGUI_API void  AddQuadFilled(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, UxImU32 col);
    IMGUI_API void  AddTriangle(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, UxImU32 col, float thickness = 1.0f);
    IMGUI_API void  AddTriangleFilled(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, UxImU32 col);
    IMGUI_API void  AddCircle(const UxImVec2& center, float radius, UxImU32 col, int num_segments = 0, float thickness = 1.0f);
    IMGUI_API void  AddCircleFilled(const UxImVec2& center, float radius, UxImU32 col, int num_segments = 0);
    IMGUI_API void  AddNgon(const UxImVec2& center, float radius, UxImU32 col, int num_segments, float thickness = 1.0f);
    IMGUI_API void  AddNgonFilled(const UxImVec2& center, float radius, UxImU32 col, int num_segments);
    IMGUI_API void  AddEllipse(const UxImVec2& center, const UxImVec2& radius, UxImU32 col, float rot = 0.0f, int num_segments = 0, float thickness = 1.0f);
    IMGUI_API void  AddEllipseFilled(const UxImVec2& center, const UxImVec2& radius, UxImU32 col, float rot = 0.0f, int num_segments = 0);
    IMGUI_API void  AddText(const UxImVec2& pos, UxImU32 col, const char* text_begin, const char* text_end = NULL);
    IMGUI_API void  AddText(UxImFont* font, float font_size, const UxImVec2& pos, UxImU32 col, const char* text_begin, const char* text_end = NULL, float wrap_width = 0.0f, const UxImVec4* cpu_fine_clip_rect = NULL);
    IMGUI_API void  AddBezierCubic(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, UxImU32 col, float thickness, int num_segments = 0); // Cubic Bezier (4 control points)
    IMGUI_API void  AddBezierQuadratic(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, UxImU32 col, float thickness, int num_segments = 0);               // Quadratic Bezier (3 control points)

    // General polygon
    // - Only simple polygons are supported by filling functions (no self-intersections, no holes).
    // - Concave polygon fill is more expensive than convex one: it has O(N^2) complexity. Provided as a convenience for the user but not used by the main library.
    IMGUI_API void  AddPolyline(const UxImVec2* points, int num_points, UxImU32 col, UxImDrawFlags flags, float thickness);
    IMGUI_API void  AddConvexPolyFilled(const UxImVec2* points, int num_points, UxImU32 col);
    IMGUI_API void  AddConcavePolyFilled(const UxImVec2* points, int num_points, UxImU32 col);

    // Image primitives
    // - Read FAQ to understand what UxImTextureID/UxImTextureRef are.
    // - "p_min" and "p_max" represent the upper-left and lower-right corners of the rectangle.
    // - "uv_min" and "uv_max" represent the normalized texture coordinates to use for those corners. Using (0,0)->(1,1) texture coordinates will generally display the entire texture.
    IMGUI_API void  AddImage(UxImTextureRef tex_ref, const UxImVec2& p_min, const UxImVec2& p_max, const UxImVec2& uv_min = UxImVec2(0, 0), const UxImVec2& uv_max = UxImVec2(1, 1), UxImU32 col = IM_COL32_WHITE);
    IMGUI_API void  AddImageQuad(UxImTextureRef tex_ref, const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, const UxImVec2& uv1 = UxImVec2(0, 0), const UxImVec2& uv2 = UxImVec2(1, 0), const UxImVec2& uv3 = UxImVec2(1, 1), const UxImVec2& uv4 = UxImVec2(0, 1), UxImU32 col = IM_COL32_WHITE);
    IMGUI_API void  AddImageRounded(UxImTextureRef tex_ref, const UxImVec2& p_min, const UxImVec2& p_max, const UxImVec2& uv_min, const UxImVec2& uv_max, UxImU32 col, float rounding, UxImDrawFlags flags = 0);

    // Stateful path API, add points then finish with PathFillConvex() or PathStroke()
    // - Important: filled shapes must always use clockwise winding order! The anti-aliasing fringe depends on it. Counter-clockwise shapes will have "inward" anti-aliasing.
    //   so e.g. 'PathArcTo(center, radius, PI * -0.5f, PI)' is ok, whereas 'PathArcTo(center, radius, PI, PI * -0.5f)' won't have correct anti-aliasing when followed by PathFillConvex().
    inline    void  PathClear()                                                 { _Path.Size = 0; }
    inline    void  PathLineTo(const UxImVec2& pos)                               { _Path.push_back(pos); }
    inline    void  PathLineToMergeDuplicate(const UxImVec2& pos)                 { if (_Path.Size == 0 || memcmp(&_Path.Data[_Path.Size - 1], &pos, 8) != 0) _Path.push_back(pos); }
    inline    void  PathFillConvex(UxImU32 col)                                   { AddConvexPolyFilled(_Path.Data, _Path.Size, col); _Path.Size = 0; }
    inline    void  PathFillConcave(UxImU32 col)                                  { AddConcavePolyFilled(_Path.Data, _Path.Size, col); _Path.Size = 0; }
    inline    void  PathStroke(UxImU32 col, UxImDrawFlags flags = 0, float thickness = 1.0f) { AddPolyline(_Path.Data, _Path.Size, col, flags, thickness); _Path.Size = 0; }
    IMGUI_API void  PathArcTo(const UxImVec2& center, float radius, float a_min, float a_max, int num_segments = 0);
    IMGUI_API void  PathArcToFast(const UxImVec2& center, float radius, int a_min_of_12, int a_max_of_12);                // Use precomputed angles for a 12 steps circle
    IMGUI_API void  PathEllipticalArcTo(const UxImVec2& center, const UxImVec2& radius, float rot, float a_min, float a_max, int num_segments = 0); // Ellipse
    IMGUI_API void  PathBezierCubicCurveTo(const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, int num_segments = 0); // Cubic Bezier (4 control points)
    IMGUI_API void  PathBezierQuadraticCurveTo(const UxImVec2& p2, const UxImVec2& p3, int num_segments = 0);               // Quadratic Bezier (3 control points)
    IMGUI_API void  PathRect(const UxImVec2& rect_min, const UxImVec2& rect_max, float rounding = 0.0f, UxImDrawFlags flags = 0);

    // Advanced: Draw Callbacks
    // - May be used to alter render state (change sampler, blending, current shader). May be used to emit custom rendering commands (difficult to do correctly, but possible).
    // - Use special UxImDrawCallback_ResetRenderState callback to instruct backend to reset its render state to the default.
    // - Your rendering loop must check for 'UserCallback' in UxImDrawCmd and call the function instead of rendering triangles. All standard backends are honoring this.
    // - For some backends, the callback may access selected render-states exposed by the backend in a UxImGui_ImplXXXX_RenderState structure pointed to by platform_io.Renderer_RenderState.
    // - IMPORTANT: please be mindful of the different level of indirection between using size==0 (copying argument) and using size>0 (copying pointed data into a buffer).
    //   - If userdata_size == 0: we copy/store the 'userdata' argument as-is. It will be available unmodified in UxImDrawCmd::UserCallbackData during render.
    //   - If userdata_size > 0,  we copy/store 'userdata_size' bytes pointed to by 'userdata'. We store them in a buffer stored inside the drawlist. UxImDrawCmd::UserCallbackData will point inside that buffer so you have to retrieve data from there. Your callback may need to use UxImDrawCmd::UserCallbackDataSize if you expect dynamically-sized data.
    //   - Support for userdata_size > 0 was added in v1.91.4, October 2024. So earlier code always only allowed to copy/store a simple void*.
    IMGUI_API void  AddCallback(UxImDrawCallback callback, void* userdata, size_t userdata_size = 0);

    // Advanced: Miscellaneous
    IMGUI_API void  AddDrawCmd();                                               // This is useful if you need to forcefully create a new draw call (to allow for dependent rendering / blending). Otherwise primitives are merged into the same draw-call as much as possible
    IMGUI_API UxImDrawList* CloneOutput() const;                                  // Create a clone of the CmdBuffer/IdxBuffer/VtxBuffer.

    // Advanced: Channels
    // - Use to split render into layers. By switching channels to can render out-of-order (e.g. submit FG primitives before BG primitives)
    // - Use to minimize draw calls (e.g. if going back-and-forth between multiple clipping rectangles, prefer to append into separate channels then merge at the end)
    // - This API shouldn't have been in UxImDrawList in the first place!
    //   Prefer using your own persistent instance of UxImDrawListSplitter as you can stack them.
    //   Using the UxImDrawList::ChannelsXXXX you cannot stack a split over another.
    inline void     ChannelsSplit(int count)    { _Splitter.Split(this, count); }
    inline void     ChannelsMerge()             { _Splitter.Merge(this); }
    inline void     ChannelsSetCurrent(int n)   { _Splitter.SetCurrentChannel(this, n); }

    // Advanced: Primitives allocations
    // - We render triangles (three vertices)
    // - All primitives needs to be reserved via PrimReserve() beforehand.
    IMGUI_API void  PrimReserve(int idx_count, int vtx_count);
    IMGUI_API void  PrimUnreserve(int idx_count, int vtx_count);
    IMGUI_API void  PrimRect(const UxImVec2& a, const UxImVec2& b, UxImU32 col);      // Axis aligned rectangle (composed of two triangles)
    IMGUI_API void  PrimRectUV(const UxImVec2& a, const UxImVec2& b, const UxImVec2& uv_a, const UxImVec2& uv_b, UxImU32 col);
    IMGUI_API void  PrimQuadUV(const UxImVec2& a, const UxImVec2& b, const UxImVec2& c, const UxImVec2& d, const UxImVec2& uv_a, const UxImVec2& uv_b, const UxImVec2& uv_c, const UxImVec2& uv_d, UxImU32 col);
    inline    void  PrimWriteVtx(const UxImVec2& pos, const UxImVec2& uv, UxImU32 col)    { _VtxWritePtr->pos = pos; _VtxWritePtr->uv = uv; _VtxWritePtr->col = col; _VtxWritePtr++; _VtxCurrentIdx++; }
    inline    void  PrimWriteIdx(UxImDrawIdx idx)                                     { *_IdxWritePtr = idx; _IdxWritePtr++; }
    inline    void  PrimVtx(const UxImVec2& pos, const UxImVec2& uv, UxImU32 col)         { PrimWriteIdx((UxImDrawIdx)_VtxCurrentIdx); PrimWriteVtx(pos, uv, col); } // Write vertex with unique index

    // Obsolete names
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_API void  PushTextureID(UxImTextureRef tex_ref) { PushTexture(tex_ref); }   // RENAMED in 1.92.x
    IMGUI_API void  PopTextureID()                      { PopTexture(); }           // RENAMED in 1.92.x
#endif
    //inline  void  AddEllipse(const UxImVec2& center, float radius_x, float radius_y, UxImU32 col, float rot = 0.0f, int num_segments = 0, float thickness = 1.0f) { AddEllipse(center, UxImVec2(radius_x, radius_y), col, rot, num_segments, thickness); } // OBSOLETED in 1.90.5 (Mar 2024)
    //inline  void  AddEllipseFilled(const UxImVec2& center, float radius_x, float radius_y, UxImU32 col, float rot = 0.0f, int num_segments = 0) { AddEllipseFilled(center, UxImVec2(radius_x, radius_y), col, rot, num_segments); }                        // OBSOLETED in 1.90.5 (Mar 2024)
    //inline  void  PathEllipticalArcTo(const UxImVec2& center, float radius_x, float radius_y, float rot, float a_min, float a_max, int num_segments = 0) { PathEllipticalArcTo(center, UxImVec2(radius_x, radius_y), rot, a_min, a_max, num_segments); } // OBSOLETED in 1.90.5 (Mar 2024)
    //inline  void  AddBezierCurve(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, UxImU32 col, float thickness, int num_segments = 0) { AddBezierCubic(p1, p2, p3, p4, col, thickness, num_segments); }                         // OBSOLETED in 1.80 (Jan 2021)
    //inline  void  PathBezierCurveTo(const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, int num_segments = 0) { PathBezierCubicCurveTo(p2, p3, p4, num_segments); }                                                                                // OBSOLETED in 1.80 (Jan 2021)

    // [Internal helpers]
    IMGUI_API void  _SetDrawListSharedData(UxImDrawListSharedData* data);
    IMGUI_API void  _ResetForNewFrame();
    IMGUI_API void  _ClearFreeMemory();
    IMGUI_API void  _PopUnusedDrawCmd();
    IMGUI_API void  _TryMergeDrawCmds();
    IMGUI_API void  _OnChangedClipRect();
    IMGUI_API void  _OnChangedTexture();
    IMGUI_API void  _OnChangedVtxOffset();
    IMGUI_API void  _SetTexture(UxImTextureRef tex_ref);
    IMGUI_API int   _CalcCircleAutoSegmentCount(float radius) const;
    IMGUI_API void  _PathArcToFastEx(const UxImVec2& center, float radius, int a_min_sample, int a_max_sample, int a_step);
    IMGUI_API void  _PathArcToN(const UxImVec2& center, float radius, float a_min, float a_max, int num_segments);
};

// All draw data to render a Dear UxImGui frame
// (NB: the style and the naming convention here is a little inconsistent, we currently preserve them for backward compatibility purpose,
// as this is one of the oldest structure exposed by the library! Basically, UxImDrawList == CmdList)
struct UxImDrawData
{
    bool                Valid;              // Only valid after Render() is called and before the next NewFrame() is called.
    int                 CmdListsCount;      // Number of UxImDrawList* to render. (== CmdLists.Size). Exists for legacy reason.
    int                 TotalIdxCount;      // For convenience, sum of all UxImDrawList's IdxBuffer.Size
    int                 TotalVtxCount;      // For convenience, sum of all UxImDrawList's VtxBuffer.Size
    UxImVector<UxImDrawList*> CmdLists;         // Array of UxImDrawList* to render. The UxImDrawLists are owned by UxImGuiContext and only pointed to from here.
    UxImVec2              DisplayPos;         // Top-left position of the viewport to render (== top-left of the orthogonal projection matrix to use) (== GetMainViewport()->Pos for the main viewport, == (0.0) in most single-viewport applications)
    UxImVec2              DisplaySize;        // Size of the viewport to render (== GetMainViewport()->Size for the main viewport, == io.DisplaySize in most single-viewport applications)
    UxImVec2              FramebufferScale;   // Amount of pixels for each unit of DisplaySize. Copied from viewport->FramebufferScale (== io.DisplayFramebufferScale for main viewport). Generally (1,1) on normal display, (2,2) on OSX with Retina display.
    UxImGuiViewport*      OwnerViewport;      // Viewport carrying the UxImDrawData instance, might be of use to the renderer (generally not).
    UxImVector<UxImTextureData*>* Textures;     // List of textures to update. Most of the times the list is shared by all UxImDrawData, has only 1 texture and it doesn't need any update. This almost always points to UxImGui::GetPlatformIO().Textures[]. May be overriden or set to NULL if you want to manually update textures.

    // Functions
    UxImDrawData()    { Clear(); }
    IMGUI_API void  Clear();
    IMGUI_API void  AddDrawList(UxImDrawList* draw_list);     // Helper to add an external draw list into an existing UxImDrawData.
    IMGUI_API void  DeIndexAllBuffers();                    // Helper to convert all buffers from indexed to non-indexed, in case you cannot render indexed. Note: this is slow and most likely a waste of resources. Always prefer indexed rendering!
    IMGUI_API void  ScaleClipRects(const UxImVec2& fb_scale); // Helper to scale the ClipRect field of each UxImDrawCmd. Use if your final output buffer is at a different scale than Dear UxImGui expects, or if there is a difference between your window resolution and framebuffer resolution.
};

//-----------------------------------------------------------------------------
// [SECTION] Texture API (UxImTextureFormat, UxImTextureStatus, UxImTextureRect, UxImTextureData)
//-----------------------------------------------------------------------------
// In principle, the only data types that user/application code should care about are 'UxImTextureRef' and 'UxImTextureID'.
// They are defined above in this header file. Read their description to the difference between UxImTextureRef and UxImTextureID.
// FOR ALL OTHER UxImTextureXXXX TYPES: ONLY CORE LIBRARY AND RENDERER BACKENDS NEED TO KNOW AND CARE ABOUT THEM.
//-----------------------------------------------------------------------------

// We intentionally support a limited amount of texture formats to limit burden on CPU-side code and extension.
// Most standard backends only support RGBA32 but we provide a single channel option for low-resource/embedded systems.
enum UxImTextureFormat
{
    UxImTextureFormat_RGBA32,         // 4 components per pixel, each is unsigned 8-bit. Total size = TexWidth * TexHeight * 4
    UxImTextureFormat_Alpha8,         // 1 component per pixel, each is unsigned 8-bit. Total size = TexWidth * TexHeight
};

// Status of a texture to communicate with Renderer Backend.
enum UxImTextureStatus
{
    UxImTextureStatus_OK,
    UxImTextureStatus_Destroyed,      // Backend destroyed the texture.
    UxImTextureStatus_WantCreate,     // Requesting backend to create the texture. Set status OK when done.
    UxImTextureStatus_WantUpdates,    // Requesting backend to update specific blocks of pixels (write to texture portions which have never been used before). Set status OK when done.
    UxImTextureStatus_WantDestroy,    // Requesting backend to destroy the texture. Set status to Destroyed when done.
};

// Coordinates of a rectangle within a texture.
// When a texture is in UxImTextureStatus_WantUpdates state, we provide a list of individual rectangles to copy to the graphics system.
// You may use UxImTextureData::Updates[] for the list, or UxImTextureData::UpdateBox for a single bounding box.
struct UxImTextureRect
{
    unsigned short      x, y;       // Upper-left coordinates of rectangle to update
    unsigned short      w, h;       // Size of rectangle to update (in pixels)
};

// Specs and pixel storage for a texture used by Dear UxImGui.
// This is only useful for (1) core library and (2) backends. End-user/applications do not need to care about this.
// Renderer Backends will create a GPU-side version of this.
// Why does we store two identifiers: TexID and BackendUserData?
// - UxImTextureID    TexID           = lower-level identifier stored in UxImDrawCmd. UxImDrawCmd can refer to textures not created by the backend, and for which there's no UxImTextureData.
// - void*          BackendUserData = higher-level opaque storage for backend own book-keeping. Some backends may have enough with TexID and not need both.
 // In columns below: who reads/writes each fields? 'r'=read, 'w'=write, 'core'=main library, 'backend'=renderer backend
struct UxImTextureData
{
    //------------------------------------------ core / backend ---------------------------------------
    int                 UniqueID;               // w    -   // Sequential index to facilitate identifying a texture when debugging/printing. Unique per atlas.
    UxImTextureStatus     Status;                 // rw   rw  // UxImTextureStatus_OK/_WantCreate/_WantUpdates/_WantDestroy. Always use SetStatus() to modify!
    void*               BackendUserData;        // -    rw  // Convenience storage for backend. Some backends may have enough with TexID.
    UxImTextureID         TexID;                  // r    w   // Backend-specific texture identifier. Always use SetTexID() to modify! The identifier will stored in UxImDrawCmd::GetTexID() and passed to backend's RenderDrawData function.
    UxImTextureFormat     Format;                 // w    r   // UxImTextureFormat_RGBA32 (default) or UxImTextureFormat_Alpha8
    int                 Width;                  // w    r   // Texture width
    int                 Height;                 // w    r   // Texture height
    int                 BytesPerPixel;          // w    r   // 4 or 1
    unsigned char*      Pixels;                 // w    r   // Pointer to buffer holding 'Width*Height' pixels and 'Width*Height*BytesPerPixels' bytes.
    UxImTextureRect       UsedRect;               // w    r   // Bounding box encompassing all past and queued Updates[].
    UxImTextureRect       UpdateRect;             // w    r   // Bounding box encompassing all queued Updates[].
    UxImVector<UxImTextureRect> Updates;            // w    r   // Array of individual updates.
    int                 UnusedFrames;           // w    r   // In order to facilitate handling Status==WantDestroy in some backend: this is a count successive frames where the texture was not used. Always >0 when Status==WantDestroy.
    unsigned short      RefCount;               // w    r   // Number of contexts using this texture. Used during backend shutdown.
    bool                UseColors;              // w    r   // Tell whether our texture data is known to use colors (rather than just white + alpha).
    bool                WantDestroyNextFrame;   // rw   -   // [Internal] Queued to set UxImTextureStatus_WantDestroy next frame. May still be used in the current frame.

    // Functions
    UxImTextureData()     { memset(this, 0, sizeof(*this)); }
    ~UxImTextureData()    { DestroyPixels(); }
    IMGUI_API void      Create(UxImTextureFormat format, int w, int h);
    IMGUI_API void      DestroyPixels();
    unsigned char*      GetPixels()                 { IM_ASSERT(Pixels != NULL); return Pixels; }
    unsigned char*      GetPixelsAt(int x, int y)   { IM_ASSERT(Pixels != NULL); return Pixels + (x + y * Width) * BytesPerPixel; }
    int                 GetSizeInBytes() const      { return Width * Height * BytesPerPixel; }
    int                 GetPitch() const            { return Width * BytesPerPixel; }
    UxImTextureRef        GetTexRef()                 { UxImTextureRef tex_ref; tex_ref._TexData = this; tex_ref._TexID = UxImTextureID_Invalid; return tex_ref; }
    UxImTextureID         GetTexID() const            { return TexID; }

    // Called by Renderer backend
    void                SetTexID(UxImTextureID tex_id)      { TexID = tex_id; }   // Call after creating or destroying the texture. Never modify TexID directly!
    void                SetStatus(UxImTextureStatus status) { Status = status; }  // Call after honoring a request. Never modify Status directly!
};

//-----------------------------------------------------------------------------
// [SECTION] Font API (UxImFontConfig, UxImFontGlyph, UxImFontAtlasFlags, UxImFontAtlas, UxImFontGlyphRangesBuilder, UxImFont)
//-----------------------------------------------------------------------------

// A font input/source (we may rename this to UxImFontSource in the future)
struct UxImFontConfig
{
    void*           FontData;               //          // TTF/OTF data
    int             FontDataSize;           //          // TTF/OTF data size
    bool            FontDataOwnedByAtlas;   // true     // TTF/OTF data ownership taken by the container UxImFontAtlas (will delete memory itself).
    bool            MergeMode;              // false    // Merge into previous UxImFont, so you can combine multiple inputs font into one UxImFont (e.g. ASCII font + icons + Japanese glyphs). You may want to use GlyphOffset.y when merge font of different heights.
    bool            PixelSnapH;             // false    // Align every glyph AdvanceX to pixel boundaries. Useful e.g. if you are merging a non-pixel aligned font with the default font. If enabled, you can set OversampleH/V to 1.
    bool            PixelSnapV;             // true     // Align Scaled GlyphOffset.y to pixel boundaries.
    int             FontNo;                 // 0        // Index of font within TTF/OTF file
    int             OversampleH;            // 0 (2)    // Rasterize at higher quality for sub-pixel positioning. 0 == auto == 1 or 2 depending on size. Note the difference between 2 and 3 is minimal. You can reduce this to 1 for large glyphs save memory. Read https://github.com/nothings/stb/blob/master/tests/oversample/README.md for details.
    int             OversampleV;            // 0 (1)    // Rasterize at higher quality for sub-pixel positioning. 0 == auto == 1. This is not really useful as we don't use sub-pixel positions on the Y axis.
    float           SizePixels;             //          // Size in pixels for rasterizer (more or less maps to the resulting font height).
    //UxImVec2        GlyphExtraSpacing;      // 0, 0     // (REMOVED AT IT SEEMS LARGELY OBSOLETE. PLEASE REPORT IF YOU WERE USING THIS). Extra spacing (in pixels) between glyphs when rendered: essentially add to glyph->AdvanceX. Only X axis is supported for now.
    UxImVec2          GlyphOffset;            // 0, 0     // Offset (in pixels) all glyphs from this font input. Absolute value for default size, other sizes will scale this value.
    const UxImWchar*  GlyphRanges;            // NULL     // THE ARRAY DATA NEEDS TO PERSIST AS LONG AS THE FONT IS ALIVE. Pointer to a user-provided list of Unicode range (2 value per range, values are inclusive, zero-terminated list).
    const UxImWchar*  GlyphExcludeRanges;     // NULL     // Pointer to a VERY SHORT user-provided list of Unicode range (2 value per range, values are inclusive, zero-terminated list). This is very close to GlyphRanges[] but designed to exclude ranges from a font source, when merging fonts with overlapping glyphs.
    float           GlyphMinAdvanceX;       // 0        // Minimum AdvanceX for glyphs, set Min to align font icons, set both Min/Max to enforce mono-space font. Absolute value for default size, other sizes will scale this value.
    float           GlyphMaxAdvanceX;       // FLT_MAX  // Maximum AdvanceX for glyphs
    float           GlyphExtraAdvanceX;     // 0        // Extra spacing (in pixels) between glyphs. Please contact us if you are using this.
    unsigned int    FontBuilderFlags;       // 0        // Settings for custom font builder. THIS IS BUILDER IMPLEMENTATION DEPENDENT. Leave as zero if unsure.
    float           RasterizerMultiply;     // 1.0f     // Linearly brighten (>1.0f) or darken (<1.0f) font output. Brightening small fonts may be a good workaround to make them more readable. This is a silly thing we may remove in the future.
    float           RasterizerDensity;      // 1.0f     // (Legacy: this only makes sense when UxImGuiBackendFlags_RendererHasTextures is not supported). DPI scale multiplier for rasterization. Not altering other font metrics: makes it easy to swap between e.g. a 100% and a 400% fonts for a zooming display, or handle Retina screen. IMPORTANT: If you change this it is expected that you increase/decrease font scale roughly to the inverse of this, otherwise quality may look lowered.
    UxImWchar         EllipsisChar;           // 0        // Explicitly specify Unicode codepoint of ellipsis character. When fonts are being merged first specified ellipsis will be used.

    // [Internal]
    char            Name[40];               // Name (strictly to ease debugging)
    UxImFontFlags     Flags;                  // Font flags (don't use just yet)
    UxImFont*         DstFont;                // Target font (as we merging fonts, multiple UxImFontConfig may target the same font)
    const UxImFontLoader* FontLoader;         // Custom font backend for this source (other use one stored in UxImFontAtlas)
    void*           FontLoaderData;         // Font loader opaque storage (per font config)

    IMGUI_API UxImFontConfig();
};

// Hold rendering data for one glyph.
// (Note: some language parsers may fail to convert the bitfield members, in this case maybe drop store a single u32 or we can rework this)
struct UxImFontGlyph
{
    unsigned int    Colored : 1;        // Flag to indicate glyph is colored and should generally ignore tinting (make it usable with no shift on little-endian as this is used in loops)
    unsigned int    Visible : 1;        // Flag to indicate glyph has no visible pixels (e.g. space). Allow early out when rendering.
    unsigned int    SourceIdx : 4;      // Index of source in parent font
    unsigned int    Codepoint : 26;     // 0x0000..0x10FFFF
    float           AdvanceX;           // Horizontal distance to advance cursor/layout position.
    float           X0, Y0, X1, Y1;     // Glyph corners. Offsets from current cursor/layout position.
    float           U0, V0, U1, V1;     // Texture coordinates for the current value of UxImFontAtlas->TexRef. Cached equivalent of calling GetCustomRect() with PackId.
    int             PackId;             // [Internal] UxImFontAtlasRectId value (FIXME: Cold data, could be moved elsewhere?)

    UxImFontGlyph()   { memset(this, 0, sizeof(*this)); PackId = -1; }
};

// Helper to build glyph ranges from text/string data. Feed your application strings/characters to it then call BuildRanges().
// This is essentially a tightly packed of vector of 64k booleans = 8KB storage.
struct UxImFontGlyphRangesBuilder
{
    UxImVector<UxImU32> UsedChars;            // Store 1-bit per Unicode code point (0=unused, 1=used)

    UxImFontGlyphRangesBuilder()              { Clear(); }
    inline void     Clear()                 { int size_in_bytes = (IM_UNICODE_CODEPOINT_MAX + 1) / 8; UsedChars.resize(size_in_bytes / (int)sizeof(UxImU32)); memset(UsedChars.Data, 0, (size_t)size_in_bytes); }
    inline bool     GetBit(size_t n) const  { int off = (int)(n >> 5); UxImU32 mask = 1u << (n & 31); return (UsedChars[off] & mask) != 0; }  // Get bit n in the array
    inline void     SetBit(size_t n)        { int off = (int)(n >> 5); UxImU32 mask = 1u << (n & 31); UsedChars[off] |= mask; }               // Set bit n in the array
    inline void     AddChar(UxImWchar c)      { SetBit(c); }                      // Add character
    IMGUI_API void  AddText(const char* text, const char* text_end = NULL);     // Add string (each character of the UTF-8 string are added)
    IMGUI_API void  AddRanges(const UxImWchar* ranges);                           // Add ranges, e.g. builder.AddRanges(UxImFontAtlas::GetGlyphRangesDefault()) to force add all of ASCII/Latin+Ext
    IMGUI_API void  BuildRanges(UxImVector<UxImWchar>* out_ranges);                 // Output new ranges
};

// An opaque identifier to a rectangle in the atlas. -1 when invalid.
// The rectangle may move and UV may be invalidated, use GetCustomRect() to retrieve it.
typedef int UxImFontAtlasRectId;
#define UxImFontAtlasRectId_Invalid -1

// Output of UxImFontAtlas::GetCustomRect() when using custom rectangles.
// Those values may not be cached/stored as they are only valid for the current value of atlas->TexRef
// (this is in theory derived from UxImTextureRect but we use separate structures for reasons)
struct UxImFontAtlasRect
{
    unsigned short  x, y;               // Position (in current texture)
    unsigned short  w, h;               // Size
    UxImVec2          uv0, uv1;           // UV coordinates (in current texture)

    UxImFontAtlasRect() { memset(this, 0, sizeof(*this)); }
};

// Flags for UxImFontAtlas build
enum UxImFontAtlasFlags_
{
    UxImFontAtlasFlags_None               = 0,
    UxImFontAtlasFlags_NoPowerOfTwoHeight = 1 << 0,   // Don't round the height to next power of two
    UxImFontAtlasFlags_NoMouseCursors     = 1 << 1,   // Don't build software mouse cursors into the atlas (save a little texture memory)
    UxImFontAtlasFlags_NoBakedLines       = 1 << 2,   // Don't build thick line textures into the atlas (save a little texture memory, allow support for point/nearest filtering). The AntiAliasedLinesUseTex features uses them, otherwise they will be rendered using polygons (more expensive for CPU/GPU).
};

// Load and rasterize multiple TTF/OTF fonts into a same texture. The font atlas will build a single texture holding:
//  - One or more fonts.
//  - Custom graphics data needed to render the shapes needed by Dear UxImGui.
//  - Mouse cursor shapes for software cursor rendering (unless setting 'Flags |= UxImFontAtlasFlags_NoMouseCursors' in the font atlas).
//  - If you don't call any AddFont*** functions, the default font embedded in the code will be loaded for you.
// It is the rendering backend responsibility to upload texture into your graphics API:
//  - UxImGui_ImplXXXX_RenderDrawData() functions generally iterate platform_io->Textures[] to create/update/destroy each UxImTextureData instance.
//  - Backend then set UxImTextureData's TexID and BackendUserData.
//  - Texture id are passed back to you during rendering to identify the texture. Read FAQ entry about UxImTextureID/UxImTextureRef for more details.
// Legacy path:
//  - Call Build() + GetTexDataAsAlpha8() or GetTexDataAsRGBA32() to build and retrieve pixels data.
//  - Call SetTexID(my_tex_id); and pass the pointer/identifier to your texture in a format natural to your graphics API.
// Common pitfalls:
// - If you pass a 'glyph_ranges' array to AddFont*** functions, you need to make sure that your array persist up until the
//   atlas is build (when calling GetTexData*** or Build()). We only copy the pointer, not the data.
// - Important: By default, AddFontFromMemoryTTF() takes ownership of the data. Even though we are not writing to it, we will free the pointer on destruction.
//   You can set font_cfg->FontDataOwnedByAtlas=false to keep ownership of your data and it won't be freed,
// - Even though many functions are suffixed with "TTF", OTF data is supported just as well.
// - This is an old API and it is currently awkward for those and various other reasons! We will address them in the future!
struct UxImFontAtlas
{
    IMGUI_API UxImFontAtlas();
    IMGUI_API ~UxImFontAtlas();
    IMGUI_API UxImFont*           AddFont(const UxImFontConfig* font_cfg);
    IMGUI_API UxImFont*           AddFontDefault(const UxImFontConfig* font_cfg = NULL);
    IMGUI_API UxImFont*           AddFontFromFileTTF(const char* filename, float size_pixels, const UxImFontConfig* font_cfg = NULL, const UxImWchar* glyph_ranges = NULL);
    IMGUI_API UxImFont*           AddFontFromMemoryTTF(void* font_data, int font_data_size, float size_pixels, const UxImFontConfig* font_cfg = NULL, const UxImWchar* glyph_ranges = NULL); // Note: Transfer ownership of 'ttf_data' to UxImFontAtlas! Will be deleted after destruction of the atlas. Set font_cfg->FontDataOwnedByAtlas=false to keep ownership of your data and it won't be freed.
    IMGUI_API UxImFont*           AddFontFromMemoryCompressedTTF(const void* compressed_font_data, int compressed_font_data_size, float size_pixels, const UxImFontConfig* font_cfg = NULL, const UxImWchar* glyph_ranges = NULL); // 'compressed_font_data' still owned by caller. Compress with binary_to_compressed_c.cpp.
    IMGUI_API UxImFont*           AddFontFromMemoryCompressedBase85TTF(const char* compressed_font_data_base85, float size_pixels, const UxImFontConfig* font_cfg = NULL, const UxImWchar* glyph_ranges = NULL);              // 'compressed_font_data_base85' still owned by caller. Compress with binary_to_compressed_c.cpp with -base85 parameter.
    IMGUI_API void              RemoveFont(UxImFont* font);

    IMGUI_API void              Clear();                    // Clear everything (input fonts, output glyphs/textures)
    IMGUI_API void              CompactCache();             // Compact cached glyphs and texture.

    // As we are transitioning toward a new font system, we expect to obsolete those soon:
    IMGUI_API void              ClearInputData();           // [OBSOLETE] Clear input data (all UxImFontConfig structures including sizes, TTF data, glyph ranges, etc.) = all the data used to build the texture and fonts.
    IMGUI_API void              ClearFonts();               // [OBSOLETE] Clear input+output font data (same as ClearInputData() + glyphs storage, UV coordinates).
    IMGUI_API void              ClearTexData();             // [OBSOLETE] Clear CPU-side copy of the texture data. Saves RAM once the texture has been copied to graphics memory.

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    // Legacy path for build atlas + retrieving pixel data.
    // - User is in charge of copying the pixels into graphics memory (e.g. create a texture with your engine). Then store your texture handle with SetTexID().
    // - The pitch is always = Width * BytesPerPixels (1 or 4)
    // - Building in RGBA32 format is provided for convenience and compatibility, but note that unless you manually manipulate or copy color data into
    //   the texture (e.g. when using the AddCustomRect*** api), then the RGB pixels emitted will always be white (~75% of memory/bandwidth waste.
    // - From 1.92 with backends supporting UxImGuiBackendFlags_RendererHasTextures:
    //   - Calling Build(), GetTexDataAsAlpha8(), GetTexDataAsRGBA32() is not needed.
    //   - In backend: replace calls to UxImFontAtlas::SetTexID() with calls to UxImTextureData::SetTexID() after honoring texture creation.
    IMGUI_API bool  Build();                    // Build pixels data. This is called automatically for you by the GetTexData*** functions.
    IMGUI_API void  GetTexDataAsAlpha8(unsigned char** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel = NULL); // 1 byte per-pixel
    IMGUI_API void  GetTexDataAsRGBA32(unsigned char** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel = NULL); // 4 bytes-per-pixel
    void            SetTexID(UxImTextureID id)    { IM_ASSERT(TexRef._TexID == UxImTextureID_Invalid); TexRef._TexData->TexID = id; }                               // Called by legacy backends. May be called before texture creation.
    void            SetTexID(UxImTextureRef id)   { IM_ASSERT(TexRef._TexID == UxImTextureID_Invalid && id._TexData == NULL); TexRef._TexData->TexID = id._TexID; } // Called by legacy backends.
    bool            IsBuilt() const { return Fonts.Size > 0 && TexIsBuilt; } // Bit ambiguous: used to detect when user didn't build texture but effectively we should check TexID != 0 except that would be backend dependent..
#endif

    //-------------------------------------------
    // Glyph Ranges
    //-------------------------------------------

    // Since 1.92: specifying glyph ranges is only useful/necessary if your backend doesn't support UxImGuiBackendFlags_RendererHasTextures!
    IMGUI_API const UxImWchar*    GetGlyphRangesDefault();                // Basic Latin, Extended Latin
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    // Helpers to retrieve list of common Unicode ranges (2 value per range, values are inclusive, zero-terminated list)
    // NB: Make sure that your string are UTF-8 and NOT in your local code page.
    // Read https://github.com/ocornut/imgui/blob/master/docs/FONTS.md/#about-utf-8-encoding for details.
    // NB: Consider using UxImFontGlyphRangesBuilder to build glyph ranges from textual data.
    IMGUI_API const UxImWchar*    GetGlyphRangesGreek();                  // Default + Greek and Coptic
    IMGUI_API const UxImWchar*    GetGlyphRangesKorean();                 // Default + Korean characters
    IMGUI_API const UxImWchar*    GetGlyphRangesJapanese();               // Default + Hiragana, Katakana, Half-Width, Selection of 2999 Ideographs
    IMGUI_API const UxImWchar*    GetGlyphRangesChineseFull();            // Default + Half-Width + Japanese Hiragana/Katakana + full set of about 21000 CJK Unified Ideographs
    IMGUI_API const UxImWchar*    GetGlyphRangesChineseSimplifiedCommon();// Default + Half-Width + Japanese Hiragana/Katakana + set of 2500 CJK Unified Ideographs for common simplified Chinese
    IMGUI_API const UxImWchar*    GetGlyphRangesCyrillic();               // Default + about 400 Cyrillic characters
    IMGUI_API const UxImWchar*    GetGlyphRangesThai();                   // Default + Thai characters
    IMGUI_API const UxImWchar*    GetGlyphRangesVietnamese();             // Default + Vietnamese characters
#endif

    //-------------------------------------------
    // [ALPHA] Custom Rectangles/Glyphs API
    //-------------------------------------------

    // Register and retrieve custom rectangles
    // - You can request arbitrary rectangles to be packed into the atlas, for your own purpose.
    // - Since 1.92.X, packing is done immediately in the function call (previously packing was done during the Build call)
    // - You can render your pixels into the texture right after calling the AddCustomRect() functions.
    // - VERY IMPORTANT:
    //   - Texture may be created/resized at any time when calling UxImGui or UxImFontAtlas functions.
    //   - IT WILL INVALIDATE RECTANGLE DATA SUCH AS UV COORDINATES. Always use latest values from GetCustomRect().
    //   - UV coordinates are associated to the current texture identifier aka 'atlas->TexRef'. Both TexRef and UV coordinates are typically changed at the same time.
    // - If you render colored output into your custom rectangles: set 'atlas->TexPixelsUseColors = true' as this may help some backends decide of preferred texture format.
    // - Read docs/FONTS.md for more details about using colorful icons.
    // - Note: this API may be reworked further in order to facilitate supporting e.g. multi-monitor, varying DPI settings.
    // - (Pre-1.92 names) ------------> (1.92 names)
    //   - GetCustomRectByIndex()   --> Use GetCustomRect()
    //   - CalcCustomRectUV()       --> Use GetCustomRect() and read uv0, uv1 fields.
    //   - AddCustomRectRegular()   --> Renamed to AddCustomRect()
    //   - AddCustomRectFontGlyph() --> Prefer using custom UxImFontLoader inside UxImFontConfig
    //   - UxImFontAtlasCustomRect    --> Renamed to UxImFontAtlasRect
    IMGUI_API UxImFontAtlasRectId AddCustomRect(int width, int height, UxImFontAtlasRect* out_r = NULL);// Register a rectangle. Return -1 (UxImFontAtlasRectId_Invalid) on error.
    IMGUI_API void              RemoveCustomRect(UxImFontAtlasRectId id);                             // Unregister a rectangle. Existing pixels will stay in texture until resized / garbage collected.
    IMGUI_API bool              GetCustomRect(UxImFontAtlasRectId id, UxImFontAtlasRect* out_r) const;  // Get rectangle coordinates for current texture. Valid immediately, never store this (read above)!

    //-------------------------------------------
    // Members
    //-------------------------------------------

    // Input
    UxImFontAtlasFlags            Flags;              // Build flags (see UxImFontAtlasFlags_)
    UxImTextureFormat             TexDesiredFormat;   // Desired texture format (default to UxImTextureFormat_RGBA32 but may be changed to UxImTextureFormat_Alpha8).
    int                         TexGlyphPadding;    // FIXME: Should be called "TexPackPadding". Padding between glyphs within texture in pixels. Defaults to 1. If your rendering method doesn't rely on bilinear filtering you may set this to 0 (will also need to set AntiAliasedLinesUseTex = false).
    int                         TexMinWidth;        // Minimum desired texture width. Must be a power of two. Default to 512.
    int                         TexMinHeight;       // Minimum desired texture height. Must be a power of two. Default to 128.
    int                         TexMaxWidth;        // Maximum desired texture width. Must be a power of two. Default to 8192.
    int                         TexMaxHeight;       // Maximum desired texture height. Must be a power of two. Default to 8192.
    void*                       UserData;           // Store your own atlas related user-data (if e.g. you have multiple font atlas).

    // Output
    // - Because textures are dynamically created/resized, the current texture identifier may changed at *ANY TIME* during the frame.
    // - This should not affect you as you can always use the latest value. But note that any precomputed UV coordinates are only valid for the current TexRef.
#ifdef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImTextureRef                TexRef;             // Latest texture identifier == TexData->GetTexRef().
#else
    union { UxImTextureRef TexRef; UxImTextureRef TexID; }; // Latest texture identifier == TexData->GetTexRef(). // RENAMED TexID to TexRef in 1.92.x
#endif
    UxImTextureData*              TexData;            // Latest texture.

    // [Internal]
    UxImVector<UxImTextureData*>    TexList;            // Texture list (most often TexList.Size == 1). TexData is always == TexList.back(). DO NOT USE DIRECTLY, USE GetDrawData().Textures[]/GetPlatformIO().Textures[] instead!
    bool                        Locked;             // Marked as locked during UxImGui::NewFrame()..EndFrame() scope if TexUpdates are not supported. Any attempt to modify the atlas will assert.
    bool                        RendererHasTextures;// Copy of (BackendFlags & UxImGuiBackendFlags_RendererHasTextures) from supporting context.
    bool                        TexIsBuilt;         // Set when texture was built matching current font input. Mostly useful for legacy IsBuilt() call.
    bool                        TexPixelsUseColors; // Tell whether our texture data is known to use colors (rather than just alpha channel), in order to help backend select a format or conversion process.
    UxImVec2                      TexUvScale;         // = (1.0f/TexData->TexWidth, 1.0f/TexData->TexHeight). May change as new texture gets created.
    UxImVec2                      TexUvWhitePixel;    // Texture coordinates to a white pixel. May change as new texture gets created.
    UxImVector<UxImFont*>           Fonts;              // Hold all the fonts returned by AddFont*. Fonts[0] is the default font upon calling UxImGui::NewFrame(), use UxImGui::PushFont()/PopFont() to change the current font.
    UxImVector<UxImFontConfig>      Sources;            // Source/configuration data
    UxImVec4                      TexUvLines[IM_DRAWLIST_TEX_LINES_WIDTH_MAX + 1];  // UVs for baked anti-aliased lines
    int                         TexNextUniqueID;    // Next value to be stored in TexData->UniqueID
    int                         FontNextUniqueID;   // Next value to be stored in UxImFont->FontID
    UxImVector<UxImDrawListSharedData*> DrawListSharedDatas; // List of users for this atlas. Typically one per Dear UxImGui context.
    UxImFontAtlasBuilder*         Builder;            // Opaque interface to our data that doesn't need to be public and may be discarded when rebuilding.
    const UxImFontLoader*         FontLoader;         // Font loader opaque interface (default to stb_truetype, can be changed to use FreeType by defining IMGUI_ENABLE_FREETYPE). Don't set directly!
    const char*                 FontLoaderName;     // Font loader name (for display e.g. in About box) == FontLoader->Name
    void*                       FontLoaderData;     // Font backend opaque storage
    unsigned int                FontBuilderFlags;   // [FIXME: Should be called FontLoaderFlags] Shared flags (for all fonts) for font loader. THIS IS BUILD IMPLEMENTATION DEPENDENT (e.g. . Per-font override is also available in UxImFontConfig.
    int                         RefCount;           // Number of contexts using this atlas
    UxImGuiContext*               OwnerContext;       // Context which own the atlas will be in charge of updating and destroying it.

    // [Obsolete]
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    // Legacy: You can request your rectangles to be mapped as font glyph (given a font + Unicode point), so you can render e.g. custom colorful icons and use them as regular glyphs. --> Prefer using a custom UxImFontLoader.
    UxImFontAtlasRect             TempRect;           // For old GetCustomRectByIndex() API
    inline UxImFontAtlasRectId    AddCustomRectRegular(int w, int h)                                                          { return AddCustomRect(w, h); }                             // RENAMED in 1.92.X
    inline const UxImFontAtlasRect* GetCustomRectByIndex(UxImFontAtlasRectId id)                                                { return GetCustomRect(id, &TempRect) ? &TempRect : NULL; } // OBSOLETED in 1.92.X
    inline void                 CalcCustomRectUV(const UxImFontAtlasRect* r, UxImVec2* out_uv_min, UxImVec2* out_uv_max) const    { *out_uv_min = r->uv0; *out_uv_max = r->uv1; }             // OBSOLETED in 1.92.X
    IMGUI_API UxImFontAtlasRectId AddCustomRectFontGlyph(UxImFont* font, UxImWchar codepoint, int w, int h, float advance_x, const UxImVec2& offset = UxImVec2(0, 0));                            // OBSOLETED in 1.92.X: Use custom UxImFontLoader in UxImFontConfig
    IMGUI_API UxImFontAtlasRectId AddCustomRectFontGlyphForSize(UxImFont* font, float font_size, UxImWchar codepoint, int w, int h, float advance_x, const UxImVec2& offset = UxImVec2(0, 0));    // ADDED AND OBSOLETED in 1.92.X
#endif
    //int                               TexDesiredWidth;         // OBSOLETED in 1.92.X (force texture width before calling Build(). Must be a power-of-two. If have many glyphs your graphics API have texture size restrictions you may want to increase texture width to decrease height)
    //typedef UxImFontAtlasRect           UxImFontAtlasCustomRect;   // OBSOLETED in 1.92.X
    //typedef UxImFontAtlasCustomRect     CustomRect;              // OBSOLETED in 1.72+
    //typedef UxImFontGlyphRangesBuilder  GlyphRangesBuilder;      // OBSOLETED in 1.67+
};

// Font runtime data for a given size
// Important: pointers to UxImFontBaked are only valid for the current frame.
struct UxImFontBaked
{
    // [Internal] Members: Hot ~20/24 bytes (for CalcTextSize)
    UxImVector<float>             IndexAdvanceX;      // 12-16 // out // Sparse. Glyphs->AdvanceX in a directly indexable way (cache-friendly for CalcTextSize functions which only this info, and are often bottleneck in large UI).
    float                       FallbackAdvanceX;   // 4     // out // FindGlyph(FallbackChar)->AdvanceX
    float                       Size;               // 4     // in  // Height of characters/line, set during loading (doesn't change after loading)
    float                       RasterizerDensity;  // 4     // in  // Density this is baked at

    // [Internal] Members: Hot ~28/36 bytes (for RenderText loop)
    UxImVector<UxImU16>             IndexLookup;        // 12-16 // out // Sparse. Index glyphs by Unicode code-point.
    UxImVector<UxImFontGlyph>       Glyphs;             // 12-16 // out // All glyphs.
    int                         FallbackGlyphIndex; // 4     // out // Index of FontFallbackChar

    // [Internal] Members: Cold
    float                       Ascent, Descent;    // 4+4   // out // Ascent: distance from top to bottom of e.g. 'A' [0..FontSize] (unscaled)
    unsigned int                MetricsTotalSurface:26;// 3  // out // Total surface in pixels to get an idea of the font rasterization/texture cost (not exact, we approximate the cost of padding between glyphs)
    unsigned int                WantDestroy:1;         // 0  //     // Queued for destroy
    unsigned int                LockLoadingFallback:1; // 0  //     //
    int                         LastUsedFrame;      // 4     //     // Record of that time this was bounds
    UxImGuiID                     BakedId;            // 4     //
    UxImFont*                     ContainerFont;      // 4-8   // in  // Parent font
    void*                       FontLoaderDatas;    // 4-8   //     // Font loader opaque storage (per baked font * sources): single contiguous buffer allocated by imgui, passed to loader.

    // Functions
    IMGUI_API UxImFontBaked();
    IMGUI_API void              ClearOutputData();
    IMGUI_API UxImFontGlyph*      FindGlyph(UxImWchar c);               // Return U+FFFD glyph if requested glyph doesn't exists.
    IMGUI_API UxImFontGlyph*      FindGlyphNoFallback(UxImWchar c);     // Return NULL if glyph doesn't exist
    IMGUI_API float             GetCharAdvance(UxImWchar c);
    IMGUI_API bool              IsGlyphLoaded(UxImWchar c);
};

// Font flags
// (in future versions as we redesign font loading API, this will become more important and better documented. for now please consider this as internal/advanced use)
enum UxImFontFlags_
{
    UxImFontFlags_None                    = 0,
    UxImFontFlags_UseDefaultSize          = 1 << 0,   // Legacy compatibility: make PushFont() calls without explicit size use font->DefaultSize instead of current font size.
    UxImFontFlags_NoLoadError             = 1 << 1,   // Disable throwing an error/assert when calling AddFontXXX() with missing file/data. Calling code is expected to check AddFontXXX() return value.
    UxImFontFlags_NoLoadGlyphs            = 1 << 2,   // [Internal] Disable loading new glyphs.
    UxImFontFlags_LockBakedSizes          = 1 << 3,   // [Internal] Disable loading new baked sizes, disable garbage collecting current ones. e.g. if you want to lock a font to a single size. Important: if you use this to preload given sizes, consider the possibility of multiple font density used on Retina display.
};

// Font runtime data and rendering
// - UxImFontAtlas automatically loads a default embedded font for you if you didn't load one manually.
// - Since 1.92.X a font may be rendered as any size! Therefore a font doesn't have one specific size.
// - Use 'font->GetFontBaked(size)' to retrieve the UxImFontBaked* corresponding to a given size.
// - If you used g.Font + g.FontSize (which is frequent from the UxImGui layer), you can use g.FontBaked as a shortcut, as g.FontBaked == g.Font->GetFontBaked(g.FontSize).
struct UxImFont
{
    // [Internal] Members: Hot ~12-20 bytes
    UxImFontBaked*                LastBaked;          // 4-8   // Cache last bound baked. NEVER USE DIRECTLY. Use GetFontBaked().
    UxImFontAtlas*                ContainerAtlas;     // 4-8   // What we have been loaded into.
    UxImFontFlags                 Flags;              // 4     // Font flags.
    float                       CurrentRasterizerDensity;    // Current rasterizer density. This is a varying state of the font.

    // [Internal] Members: Cold ~24-52 bytes
    // Conceptually Sources[] is the list of font sources merged to create this font.
    UxImGuiID                     FontId;             // Unique identifier for the font
    float                       DefaultSize;        // 4     // in  // Default font size passed to AddFont(). It's unlikely you should use this (use UxImGui::GetFontBaked() to get font baked at current bound size).
    UxImVector<UxImFontConfig*>     Sources;            // 16    // in  // List of sources. Pointers within ContainerAtlas->Sources[]
    UxImWchar                     EllipsisChar;       // 2-4   // out // Character used for ellipsis rendering ('...').
    UxImWchar                     FallbackChar;       // 2-4   // out // Character used if a glyph isn't found (U+FFFD, '?')
    float                       Scale;              // 4     // in  // Base font scale (~1.0f), multiplied by the per-window font scale which you can adjust with SetWindowFontScale()
    UxImU8                        Used8kPagesMap[(IM_UNICODE_CODEPOINT_MAX+1)/8192/8]; // 1 bytes if UxImWchar=UxImWchar16, 16 bytes if UxImWchar==UxImWchar32. Store 1-bit for each block of 4K codepoints that has one active glyph. This is mainly used to facilitate iterations across all used codepoints.
    bool                        EllipsisAutoBake;   // 1     //     // Mark when the "..." glyph needs to be generated.
    UxImGuiStorage                RemapPairs;         // 16    //     // Remapping pairs when using AddRemapChar(), otherwise empty.

    // Methods
    IMGUI_API UxImFont();
    IMGUI_API ~UxImFont();
    IMGUI_API UxImFontBaked*      GetFontBaked(float font_size, float density = -1.0f);  // Get or create baked data for given size
    IMGUI_API bool              IsGlyphInFont(UxImWchar c);
    bool                        IsLoaded() const                { return ContainerAtlas != NULL; }
    const char*                 GetDebugName() const            { return Sources.Size ? Sources[0]->Name : "<unknown>"; } // Fill UxImFontConfig::Name.

    // [Internal] Don't use!
    // 'max_width' stops rendering after a certain width (could be turned into a 2d size). FLT_MAX to disable.
    // 'wrap_width' enable automatic word-wrapping across multiple lines to fit into given width. 0.0f to disable.
    IMGUI_API UxImVec2            CalcTextSizeA(float size, float max_width, float wrap_width, const char* text_begin, const char* text_end = NULL, const char** remaining = NULL); // utf8
    IMGUI_API const char*       CalcWordWrapPosition(float size, const char* text, const char* text_end, float wrap_width);
    IMGUI_API void              RenderChar(UxImDrawList* draw_list, float size, const UxImVec2& pos, UxImU32 col, UxImWchar c, const UxImVec4* cpu_fine_clip = NULL);
    IMGUI_API void              RenderText(UxImDrawList* draw_list, float size, const UxImVec2& pos, UxImU32 col, const UxImVec4& clip_rect, const char* text_begin, const char* text_end, float wrap_width = 0.0f, bool cpu_fine_clip = false);
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    inline const char*          CalcWordWrapPositionA(float scale, const char* text, const char* text_end, float wrap_width) { return CalcWordWrapPosition(DefaultSize * scale, text, text_end, wrap_width); }
#endif

    // [Internal] Don't use!
    IMGUI_API void              ClearOutputData();
    IMGUI_API void              AddRemapChar(UxImWchar from_codepoint, UxImWchar to_codepoint); // Makes 'from_codepoint' character points to 'to_codepoint' glyph.
    IMGUI_API bool              IsGlyphRangeUnused(unsigned int c_begin, unsigned int c_last);
};

// This is provided for consistency (but we don't actually use this)
inline UxImTextureID UxImTextureRef::GetTexID() const
{
    IM_ASSERT(!(_TexData != NULL && _TexID != UxImTextureID_Invalid));
    return _TexData ? _TexData->TexID : _TexID;
}

// Using an indirection to avoid patching UxImDrawCmd after a SetTexID() call (but this could be an alternative solution too)
inline UxImTextureID UxImDrawCmd::GetTexID() const
{
    // If you are getting this assert: A renderer backend with support for UxImGuiBackendFlags_RendererHasTextures (1.92)
    // must iterate and handle UxImTextureData requests stored in UxImDrawData::Textures[].
    UxImTextureID tex_id = TexRef._TexData ? TexRef._TexData->TexID : TexRef._TexID; // == TexRef.GetTexID() above.
    if (TexRef._TexData != NULL)
        IM_ASSERT(tex_id != UxImTextureID_Invalid && "UxImDrawCmd is referring to UxImTextureData that wasn't uploaded to graphics system. Backend must call UxImTextureData::SetTexID() after handling UxImTextureStatus_WantCreate request!");
    return tex_id;
}

//-----------------------------------------------------------------------------
// [SECTION] Viewports
//-----------------------------------------------------------------------------

// Flags stored in UxImGuiViewport::Flags, giving indications to the platform backends.
enum UxImGuiViewportFlags_
{
    UxImGuiViewportFlags_None                     = 0,
    UxImGuiViewportFlags_IsPlatformWindow         = 1 << 0,   // Represent a Platform Window
    UxImGuiViewportFlags_IsPlatformMonitor        = 1 << 1,   // Represent a Platform Monitor (unused yet)
    UxImGuiViewportFlags_OwnedByApp               = 1 << 2,   // Platform Window: Is created/managed by the user application? (rather than our backend)
    UxImGuiViewportFlags_NoDecoration             = 1 << 3,   // Platform Window: Disable platform decorations: title bar, borders, etc. (generally set all windows, but if UxImGuiConfigFlags_ViewportsDecoration is set we only set this on popups/tooltips)
    UxImGuiViewportFlags_NoTaskBarIcon            = 1 << 4,   // Platform Window: Disable platform task bar icon (generally set on popups/tooltips, or all windows if UxImGuiConfigFlags_ViewportsNoTaskBarIcon is set)
    UxImGuiViewportFlags_NoFocusOnAppearing       = 1 << 5,   // Platform Window: Don't take focus when created.
    UxImGuiViewportFlags_NoFocusOnClick           = 1 << 6,   // Platform Window: Don't take focus when clicked on.
    UxImGuiViewportFlags_NoInputs                 = 1 << 7,   // Platform Window: Make mouse pass through so we can drag this window while peaking behind it.
    UxImGuiViewportFlags_NoRendererClear          = 1 << 8,   // Platform Window: Renderer doesn't need to clear the framebuffer ahead (because we will fill it entirely).
    UxImGuiViewportFlags_NoAutoMerge              = 1 << 9,   // Platform Window: Avoid merging this window into another host window. This can only be set via UxImGuiWindowClass viewport flags override (because we need to now ahead if we are going to create a viewport in the first place!).
    UxImGuiViewportFlags_TopMost                  = 1 << 10,  // Platform Window: Display on top (for tooltips only).
    UxImGuiViewportFlags_CanHostOtherWindows      = 1 << 11,  // Viewport can host multiple imgui windows (secondary viewports are associated to a single window). // FIXME: In practice there's still probably code making the assumption that this is always and only on the MainViewport. Will fix once we add support for "no main viewport".

    // Output status flags (from Platform)
    UxImGuiViewportFlags_IsMinimized              = 1 << 12,  // Platform Window: Window is minimized, can skip render. When minimized we tend to avoid using the viewport pos/size for clipping window or testing if they are contained in the viewport.
    UxImGuiViewportFlags_IsFocused                = 1 << 13,  // Platform Window: Window is focused (last call to Platform_GetWindowFocus() returned true)
};

// - Currently represents the Platform Window created by the application which is hosting our Dear UxImGui windows.
// - With multi-viewport enabled, we extend this concept to have multiple active viewports.
// - In the future we will extend this concept further to also represent Platform Monitor and support a "no main platform window" operation mode.
// - About Main Area vs Work Area:
//   - Main Area = entire viewport.
//   - Work Area = entire viewport minus sections used by main menu bars (for platform windows), or by task bar (for platform monitor).
//   - Windows are generally trying to stay within the Work Area of their host viewport.
struct UxImGuiViewport
{
    UxImGuiID             ID;                     // Unique identifier for the viewport
    UxImGuiViewportFlags  Flags;                  // See UxImGuiViewportFlags_
    UxImVec2              Pos;                    // Main Area: Position of the viewport (Dear UxImGui coordinates are the same as OS desktop/native coordinates)
    UxImVec2              Size;                   // Main Area: Size of the viewport.
    UxImVec2              FramebufferScale;       // Density of the viewport for Retina display (always 1,1 on Windows, may be 2,2 etc on macOS/iOS). This will affect font rasterizer density.
    UxImVec2              WorkPos;                // Work Area: Position of the viewport minus task bars, menus bars, status bars (>= Pos)
    UxImVec2              WorkSize;               // Work Area: Size of the viewport minus task bars, menu bars, status bars (<= Size)
    float               DpiScale;               // 1.0f = 96 DPI = No extra scale.
    UxImGuiID             ParentViewportId;       // (Advanced) 0: no parent. Instruct the platform backend to setup a parent/child relationship between platform windows.
    UxImDrawData*         DrawData;               // The UxImDrawData corresponding to this viewport. Valid after Render() and until the next call to NewFrame().

    // Platform/Backend Dependent Data
    // Our design separate the Renderer and Platform backends to facilitate combining default backends with each others.
    // When our create your own backend for a custom engine, it is possible that both Renderer and Platform will be handled
    // by the same system and you may not need to use all the UserData/Handle fields.
    // The library never uses those fields, they are merely storage to facilitate backend implementation.
    void*               RendererUserData;       // void* to hold custom data structure for the renderer (e.g. swap chain, framebuffers etc.). generally set by your Renderer_CreateWindow function.
    void*               PlatformUserData;       // void* to hold custom data structure for the OS / platform (e.g. windowing info, render context). generally set by your Platform_CreateWindow function.
    void*               PlatformHandle;         // void* to hold higher-level, platform window handle (e.g. HWND for Win32 backend, Uint32 WindowID for SDL, GLFWWindow* for GLFW), for FindViewportByPlatformHandle().
    void*               PlatformHandleRaw;      // void* to hold lower-level, platform-native window handle (always HWND on Win32 platform, unused for other platforms).
    bool                PlatformWindowCreated;  // Platform window has been created (Platform_CreateWindow() has been called). This is false during the first frame where a viewport is being created.
    bool                PlatformRequestMove;    // Platform window requested move (e.g. window was moved by the OS / host window manager, authoritative position will be OS window position)
    bool                PlatformRequestResize;  // Platform window requested resize (e.g. window was resized by the OS / host window manager, authoritative size will be OS window size)
    bool                PlatformRequestClose;   // Platform window requested closure (e.g. window was moved by the OS / host window manager, e.g. pressing ALT-F4)

    UxImGuiViewport()     { memset(this, 0, sizeof(*this)); }
    ~UxImGuiViewport()    { IM_ASSERT(PlatformUserData == NULL && RendererUserData == NULL); }

    // Helpers
    UxImVec2              GetCenter() const       { return UxImVec2(Pos.x + Size.x * 0.5f, Pos.y + Size.y * 0.5f); }
    UxImVec2              GetWorkCenter() const   { return UxImVec2(WorkPos.x + WorkSize.x * 0.5f, WorkPos.y + WorkSize.y * 0.5f); }
};

//-----------------------------------------------------------------------------
// [SECTION] UxImGuiPlatformIO + other Platform Dependent Interfaces (UxImGuiPlatformMonitor, UxImGuiPlatformImeData)
//-----------------------------------------------------------------------------

// [BETA] (Optional) Multi-Viewport Support!
// If you are new to Dear UxImGui and trying to integrate it into your engine, you can probably ignore this for now.
//
// This feature allows you to seamlessly drag Dear UxImGui windows outside of your application viewport.
// This is achieved by creating new Platform/OS windows on the fly, and rendering into them.
// Dear UxImGui manages the viewport structures, and the backend create and maintain one Platform/OS window for each of those viewports.
//
// See Recap:   https://github.com/ocornut/imgui/wiki/Multi-Viewports
// See Glossary https://github.com/ocornut/imgui/wiki/Glossary for details about some of the terminology.
//
// About the coordinates system:
// - When multi-viewports are enabled, all Dear UxImGui coordinates become absolute coordinates (same as OS coordinates!)
// - So e.g. UxImGui::SetNextWindowPos(UxImVec2(0,0)) will position a window relative to your primary monitor!
// - If you want to position windows relative to your main application viewport, use UxImGui::GetMainViewport()->Pos as a base position.
//
// Steps to use multi-viewports in your application, when using a default backend from the examples/ folder:
// - Application:  Enable feature with 'io.ConfigFlags |= UxImGuiConfigFlags_ViewportsEnable'.
// - Backend:      The backend initialization will setup all necessary UxImGuiPlatformIO's functions and update monitors info every frame.
// - Application:  In your main loop, call UxImGui::UpdatePlatformWindows(), UxImGui::RenderPlatformWindowsDefault() after EndFrame() or Render().
// - Application:  Fix absolute coordinates used in UxImGui::SetWindowPos() or UxImGui::SetNextWindowPos() calls.
//
// Steps to use multi-viewports in your application, when using a custom backend:
// - Important:    THIS IS NOT EASY TO DO and comes with many subtleties not described here!
//                 It's also an experimental feature, so some of the requirements may evolve.
//                 Consider using default backends if you can. Either way, carefully follow and refer to examples/ backends for details.
// - Application:  Enable feature with 'io.ConfigFlags |= UxImGuiConfigFlags_ViewportsEnable'.
// - Backend:      Hook UxImGuiPlatformIO's Platform_* and Renderer_* callbacks (see below).
//                 Set 'io.BackendFlags |= UxImGuiBackendFlags_PlatformHasViewports' and 'io.BackendFlags |= UxImGuiBackendFlags_PlatformHasViewports'.
//                 Update UxImGuiPlatformIO's Monitors list every frame.
//                 Update MousePos every frame, in absolute coordinates.
// - Application:  In your main loop, call UxImGui::UpdatePlatformWindows(), UxImGui::RenderPlatformWindowsDefault() after EndFrame() or Render().
//                 You may skip calling RenderPlatformWindowsDefault() if its API is not convenient for your needs. Read comments below.
// - Application:  Fix absolute coordinates used in UxImGui::SetWindowPos() or UxImGui::SetNextWindowPos() calls.
//
// About UxImGui::RenderPlatformWindowsDefault():
// - This function is a mostly a _helper_ for the common-most cases, and to facilitate using default backends.
// - You can check its simple source code to understand what it does.
//   It basically iterates secondary viewports and call 4 functions that are setup in UxImGuiPlatformIO, if available:
//     Platform_RenderWindow(), Renderer_RenderWindow(), Platform_SwapBuffers(), Renderer_SwapBuffers()
//   Those functions pointers exists only for the benefit of RenderPlatformWindowsDefault().
// - If you have very specific rendering needs (e.g. flipping multiple swap-chain simultaneously, unusual sync/threading issues, etc.),
//   you may be tempted to ignore RenderPlatformWindowsDefault() and write customized code to perform your renderingg.
//   You may decide to setup the platform_io's *RenderWindow and *SwapBuffers pointers and call your functions through those pointers,
//   or you may decide to never setup those pointers and call your code directly. They are a convenience, not an obligatory interface.
//-----------------------------------------------------------------------------

// Access via UxImGui::GetPlatformIO()
struct UxImGuiPlatformIO
{
    IMGUI_API UxImGuiPlatformIO();

    //------------------------------------------------------------------
    // Input - Interface with OS and Platform backend (most common stuff)
    //------------------------------------------------------------------

    // Optional: Access OS clipboard
    // (default to use native Win32 clipboard on Windows, otherwise uses a private clipboard. Override to access OS clipboard on other architectures)
    const char* (*Platform_GetClipboardTextFn)(UxImGuiContext* ctx);
    void        (*Platform_SetClipboardTextFn)(UxImGuiContext* ctx, const char* text);
    void*       Platform_ClipboardUserData;

    // Optional: Open link/folder/file in OS Shell
    // (default to use ShellExecuteW() on Windows, system() on Linux/Mac)
    bool        (*Platform_OpenInShellFn)(UxImGuiContext* ctx, const char* path);
    void*       Platform_OpenInShellUserData;

    // Optional: Notify OS Input Method Editor of the screen position of your cursor for text input position (e.g. when using Japanese/Chinese IME on Windows)
    // (default to use native imm32 api on Windows)
    void        (*Platform_SetImeDataFn)(UxImGuiContext* ctx, UxImGuiViewport* viewport, UxImGuiPlatformImeData* data);
    void*       Platform_ImeUserData;
    //void      (*SetPlatformImeDataFn)(UxImGuiViewport* viewport, UxImGuiPlatformImeData* data); // [Renamed to platform_io.PlatformSetImeDataFn in 1.91.1]

    // Optional: Platform locale
    // [Experimental] Configure decimal point e.g. '.' or ',' useful for some languages (e.g. German), generally pulled from *localeconv()->decimal_point
    UxImWchar     Platform_LocaleDecimalPoint;     // '.'

    //------------------------------------------------------------------
    // Input - Interface with Renderer Backend
    //------------------------------------------------------------------

    // Optional: Maximum texture size supported by renderer (used to adjust how we size textures). 0 if not known.
    int         Renderer_TextureMaxWidth;
    int         Renderer_TextureMaxHeight;

    // Written by some backends during UxImGui_ImplXXXX_RenderDrawData() call to point backend_specific UxImGui_ImplXXXX_RenderState* structure.
    void*       Renderer_RenderState;

    //------------------------------------------------------------------
    // Input - Interface with Platform & Renderer backends for Multi-Viewport support
    //------------------------------------------------------------------

    // For reference, the second column shows which function are generally calling the Platform Functions:
    //   N = UxImGui::NewFrame()                        ~ beginning of the dear imgui frame: read info from platform/OS windows (latest size/position)
    //   F = UxImGui::Begin(), UxImGui::EndFrame()        ~ during the dear imgui frame
    //   U = UxImGui::UpdatePlatformWindows()           ~ after the dear imgui frame: create and update all platform/OS windows
    //   R = UxImGui::RenderPlatformWindowsDefault()    ~ render
    //   D = UxImGui::DestroyPlatformWindows()          ~ shutdown
    // The general idea is that NewFrame() we will read the current Platform/OS state, and UpdatePlatformWindows() will write to it.

    // The handlers are designed so we can mix and match two imgui_impl_xxxx files, one Platform backend and one Renderer backend.
    // Custom engine backends will often provide both Platform and Renderer interfaces together and so may not need to use all functions.
    // Platform functions are typically called _before_ their Renderer counterpart, apart from Destroy which are called the other way.

    // Platform Backend functions (e.g. Win32, GLFW, SDL) ------------------- Called by -----
    void    (*Platform_CreateWindow)(UxImGuiViewport* vp);                    // . . U . .  // Create a new platform window for the given viewport
    void    (*Platform_DestroyWindow)(UxImGuiViewport* vp);                   // N . U . D  //
    void    (*Platform_ShowWindow)(UxImGuiViewport* vp);                      // . . U . .  // Newly created windows are initially hidden so SetWindowPos/Size/Title can be called on them before showing the window
    void    (*Platform_SetWindowPos)(UxImGuiViewport* vp, UxImVec2 pos);        // . . U . .  // Set platform window position (given the upper-left corner of client area)
    UxImVec2  (*Platform_GetWindowPos)(UxImGuiViewport* vp);                    // N . . . .  //
    void    (*Platform_SetWindowSize)(UxImGuiViewport* vp, UxImVec2 size);      // . . U . .  // Set platform window client area size (ignoring OS decorations such as OS title bar etc.)
    UxImVec2  (*Platform_GetWindowSize)(UxImGuiViewport* vp);                   // N . . . .  // Get platform window client area size
    UxImVec2  (*Platform_GetWindowFramebufferScale)(UxImGuiViewport* vp);       // N . . . .  // Return viewport density. Always 1,1 on Windows, often 2,2 on Retina display on macOS/iOS. MUST BE INTEGER VALUES.
    void    (*Platform_SetWindowFocus)(UxImGuiViewport* vp);                  // N . . . .  // Move window to front and set input focus
    bool    (*Platform_GetWindowFocus)(UxImGuiViewport* vp);                  // . . U . .  //
    bool    (*Platform_GetWindowMinimized)(UxImGuiViewport* vp);              // N . . . .  // Get platform window minimized state. When minimized, we generally won't attempt to get/set size and contents will be culled more easily
    void    (*Platform_SetWindowTitle)(UxImGuiViewport* vp, const char* str); // . . U . .  // Set platform window title (given an UTF-8 string)
    void    (*Platform_SetWindowAlpha)(UxImGuiViewport* vp, float alpha);     // . . U . .  // (Optional) Setup global transparency (not per-pixel transparency)
    void    (*Platform_UpdateWindow)(UxImGuiViewport* vp);                    // . . U . .  // (Optional) Called by UpdatePlatformWindows(). Optional hook to allow the platform backend from doing general book-keeping every frame.
    void    (*Platform_RenderWindow)(UxImGuiViewport* vp, void* render_arg);  // . . . R .  // (Optional) Main rendering (platform side! This is often unused, or just setting a "current" context for OpenGL bindings). 'render_arg' is the value passed to RenderPlatformWindowsDefault().
    void    (*Platform_SwapBuffers)(UxImGuiViewport* vp, void* render_arg);   // . . . R .  // (Optional) Call Present/SwapBuffers (platform side! This is often unused!). 'render_arg' is the value passed to RenderPlatformWindowsDefault().
    float   (*Platform_GetWindowDpiScale)(UxImGuiViewport* vp);               // N . . . .  // (Optional) [BETA] FIXME-DPI: DPI handling: Return DPI scale for this viewport. 1.0f = 96 DPI.
    void    (*Platform_OnChangedViewport)(UxImGuiViewport* vp);               // . F . . .  // (Optional) [BETA] FIXME-DPI: DPI handling: Called during Begin() every time the viewport we are outputting into changes, so backend has a chance to swap fonts to adjust style.
    UxImVec4  (*Platform_GetWindowWorkAreaInsets)(UxImGuiViewport* vp);         // N . . . .  // (Optional) [BETA] Get initial work area inset for the viewport (won't be covered by main menu bar, dockspace over viewport etc.). Default to (0,0),(0,0). 'safeAreaInsets' in iOS land, 'DisplayCutout' in Android land.
    int     (*Platform_CreateVkSurface)(UxImGuiViewport* vp, UxImU64 vk_inst, const void* vk_allocators, UxImU64* out_vk_surface); // (Optional) For a Vulkan Renderer to call into Platform code (since the surface creation needs to tie them both).

    // Renderer Backend functions (e.g. DirectX, OpenGL, Vulkan) ------------ Called by -----
    void    (*Renderer_CreateWindow)(UxImGuiViewport* vp);                    // . . U . .  // Create swap chain, frame buffers etc. (called after Platform_CreateWindow)
    void    (*Renderer_DestroyWindow)(UxImGuiViewport* vp);                   // N . U . D  // Destroy swap chain, frame buffers etc. (called before Platform_DestroyWindow)
    void    (*Renderer_SetWindowSize)(UxImGuiViewport* vp, UxImVec2 size);      // . . U . .  // Resize swap chain, frame buffers etc. (called after Platform_SetWindowSize)
    void    (*Renderer_RenderWindow)(UxImGuiViewport* vp, void* render_arg);  // . . . R .  // (Optional) Clear framebuffer, setup render target, then render the viewport->DrawData. 'render_arg' is the value passed to RenderPlatformWindowsDefault().
    void    (*Renderer_SwapBuffers)(UxImGuiViewport* vp, void* render_arg);   // . . . R .  // (Optional) Call Present/SwapBuffers. 'render_arg' is the value passed to RenderPlatformWindowsDefault().

    // (Optional) Monitor list
    // - Updated by: app/backend. Update every frame to dynamically support changing monitor or DPI configuration.
    // - Used by: dear imgui to query DPI info, clamp popups/tooltips within same monitor and not have them straddle monitors.
    UxImVector<UxImGuiPlatformMonitor>  Monitors;

    //------------------------------------------------------------------
    // Output
    //------------------------------------------------------------------

    // Textures list (the list is updated by calling UxImGui::EndFrame or UxImGui::Render)
    // The UxImGui_ImplXXXX_RenderDrawData() function of each backend generally access this via UxImDrawData::Textures which points to this. The array is available here mostly because backends will want to destroy textures on shutdown.
    UxImVector<UxImTextureData*>        Textures;           // List of textures used by Dear UxImGui (most often 1) + contents of external texture list is automatically appended into this.

    // Viewports list (the list is updated by calling UxImGui::EndFrame or UxImGui::Render)
    // (in the future we will attempt to organize this feature to remove the need for a "main viewport")
    UxImVector<UxImGuiViewport*>        Viewports;          // Main viewports, followed by all secondary viewports.
};

// (Optional) This is required when enabling multi-viewport. Represent the bounds of each connected monitor/display and their DPI.
// We use this information for multiple DPI support + clamping the position of popups and tooltips so they don't straddle multiple monitors.
struct UxImGuiPlatformMonitor
{
    UxImVec2  MainPos, MainSize;      // Coordinates of the area displayed on this monitor (Min = upper left, Max = bottom right)
    UxImVec2  WorkPos, WorkSize;      // Coordinates without task bars / side bars / menu bars. Used to avoid positioning popups/tooltips inside this region. If you don't have this info, please copy the value for MainPos/MainSize.
    float   DpiScale;               // 1.0f = 96 DPI
    void*   PlatformHandle;         // Backend dependant data (e.g. HMONITOR, GLFWmonitor*, SDL Display Index, NSScreen*)
    UxImGuiPlatformMonitor()          { MainPos = MainSize = WorkPos = WorkSize = UxImVec2(0, 0); DpiScale = 1.0f; PlatformHandle = NULL; }
};

// (Optional) Support for IME (Input Method Editor) via the platform_io.Platform_SetImeDataFn() function. Handler is called during EndFrame().
struct UxImGuiPlatformImeData
{
    bool    WantVisible;            // A widget wants the IME to be visible.
    bool    WantTextInput;          // A widget wants text input, not necessarily IME to be visible. This is automatically set to the upcoming value of io.WantTextInput.
    UxImVec2  InputPos;               // Position of input cursor (for IME).
    float   InputLineHeight;        // Line height (for IME).
    UxImGuiID ViewportId;             // ID of platform window/viewport.

    UxImGuiPlatformImeData()          { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// [SECTION] Obsolete functions and types
// (Will be removed! Read 'API BREAKING CHANGES' section in imgui.cpp for details)
// Please keep your copy of dear imgui up to date! Occasionally set '#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS' in imconfig.h to stay ahead.
//-----------------------------------------------------------------------------

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
namespace UxImGui
{
    // OBSOLETED in 1.91.9 (from February 2025)
    IMGUI_API void      Image(UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1, const UxImVec4& tint_col, const UxImVec4& border_col); // <-- border_col was removed in favor of UxImGuiCol_ImageBorder.
    // OBSOLETED in 1.91.0 (from July 2024)
    static inline void  PushButtonRepeat(bool repeat)                           { PushItemFlag(UxImGuiItemFlags_ButtonRepeat, repeat); }
    static inline void  PopButtonRepeat()                                       { PopItemFlag(); }
    static inline void  PushTabStop(bool tab_stop)                              { PushItemFlag(UxImGuiItemFlags_NoTabStop, !tab_stop); }
    static inline void  PopTabStop()                                            { PopItemFlag(); }
    IMGUI_API UxImVec2    GetContentRegionMax();                                  // Content boundaries max (e.g. window boundaries including scrolling, or current column boundaries). You should never need this. Always use GetCursorScreenPos() and GetContentRegionAvail()!
    IMGUI_API UxImVec2    GetWindowContentRegionMin();                            // Content boundaries min for the window (roughly (0,0)-Scroll), in window-local coordinates. You should never need this. Always use GetCursorScreenPos() and GetContentRegionAvail()!
    IMGUI_API UxImVec2    GetWindowContentRegionMax();                            // Content boundaries max for the window (roughly (0,0)+Size-Scroll), in window-local coordinates. You should never need this. Always use GetCursorScreenPos() and GetContentRegionAvail()!
    // OBSOLETED in 1.90.0 (from September 2023)
    static inline bool  BeginChildFrame(UxImGuiID id, const UxImVec2& size, UxImGuiWindowFlags window_flags = 0)  { return BeginChild(id, size, UxImGuiChildFlags_FrameStyle, window_flags); }
    static inline void  EndChildFrame()                                                                     { EndChild(); }
    //static inline bool BeginChild(const char* str_id, const UxImVec2& size_arg, bool borders, UxImGuiWindowFlags window_flags){ return BeginChild(str_id, size_arg, borders ? UxImGuiChildFlags_Borders : UxImGuiChildFlags_None, window_flags); } // Unnecessary as true == UxImGuiChildFlags_Borders
    //static inline bool BeginChild(UxImGuiID id, const UxImVec2& size_arg, bool borders, UxImGuiWindowFlags window_flags)        { return BeginChild(id, size_arg, borders ? UxImGuiChildFlags_Borders : UxImGuiChildFlags_None, window_flags);     } // Unnecessary as true == UxImGuiChildFlags_Borders
    static inline void  ShowStackToolWindow(bool* p_open = NULL)                { ShowIDStackToolWindow(p_open); }
    IMGUI_API bool      Combo(const char* label, int* current_item, bool (*old_callback)(void* user_data, int idx, const char** out_text), void* user_data, int items_count, int popup_max_height_in_items = -1);
    IMGUI_API bool      ListBox(const char* label, int* current_item, bool (*old_callback)(void* user_data, int idx, const char** out_text), void* user_data, int items_count, int height_in_items = -1);
    // OBSOLETED in 1.89.7 (from June 2023)
    IMGUI_API void      SetItemAllowOverlap();                                  // Use SetNextItemAllowOverlap() before item.

    // Some of the older obsolete names along with their replacement (commented out so they are not reported in IDE)
    //-- OBSOLETED in 1.89.4 (from March 2023)
    //static inline void  PushAllowKeyboardFocus(bool tab_stop)                                       { PushItemFlag(UxImGuiItemFlags_NoTabStop, !tab_stop); }
    //static inline void  PopAllowKeyboardFocus()                                                     { PopItemFlag(); }
    //-- OBSOLETED in 1.89 (from August 2022)
    //IMGUI_API bool      ImageButton(UxImTextureID user_texture_id, const UxImVec2& size, const UxImVec2& uv0 = UxImVec2(0, 0), const UxImVec2& uv1 = UxImVec2(1, 1), int frame_padding = -1, const UxImVec4& bg_col = UxImVec4(0, 0, 0, 0), const UxImVec4& tint_col = UxImVec4(1, 1, 1, 1)); // --> Use new ImageButton() signature (explicit item id, regular FramePadding). Refer to code in 1.91 if you want to grab a copy of this version.
    //-- OBSOLETED in 1.88 (from May 2022)
    //static inline void  CaptureKeyboardFromApp(bool want_capture_keyboard = true)                   { SetNextFrameWantCaptureKeyboard(want_capture_keyboard); } // Renamed as name was misleading + removed default value.
    //static inline void  CaptureMouseFromApp(bool want_capture_mouse = true)                         { SetNextFrameWantCaptureMouse(want_capture_mouse); }       // Renamed as name was misleading + removed default value.
    //-- OBSOLETED in 1.87 (from February 2022, more formally obsoleted April 2024)
    //IMGUI_API UxImGuiKey  GetKeyIndex(UxImGuiKey key);                                                  { IM_ASSERT(key >= UxImGuiKey_NamedKey_BEGIN && key < UxImGuiKey_NamedKey_END); const UxImGuiKeyData* key_data = GetKeyData(key); return (UxImGuiKey)(key_data - g.IO.KeysData); } // Map UxImGuiKey_* values into legacy native key index. == io.KeyMap[key]. When using a 1.87+ backend using io.AddKeyEvent(), calling GetKeyIndex() with ANY UxImGuiKey_XXXX values will return the same value!
    //static inline UxImGuiKey GetKeyIndex(UxImGuiKey key)                                                { IM_ASSERT(key >= UxImGuiKey_NamedKey_BEGIN && key < UxImGuiKey_NamedKey_END); return key; }
    //-- OBSOLETED in 1.86 (from November 2021)
    //IMGUI_API void      CalcListClipping(int items_count, float items_height, int* out_items_display_start, int* out_items_display_end); // Code removed, see 1.90 for last version of the code. Calculate range of visible items for large list of evenly sized items. Prefer using UxImGuiListClipper.
    //-- OBSOLETED in 1.85 (from August 2021)
    //static inline float GetWindowContentRegionWidth()                                               { return GetWindowContentRegionMax().x - GetWindowContentRegionMin().x; }
    //-- OBSOLETED in 1.81 (from February 2021)
    //static inline bool  ListBoxHeader(const char* label, const UxImVec2& size = UxImVec2(0, 0))         { return BeginListBox(label, size); }
    //static inline bool  ListBoxHeader(const char* label, int items_count, int height_in_items = -1) { float height = GetTextLineHeightWithSpacing() * ((height_in_items < 0 ? UxImMin(items_count, 7) : height_in_items) + 0.25f) + GetStyle().FramePadding.y * 2.0f; return BeginListBox(label, UxImVec2(0.0f, height)); } // Helper to calculate size from items_count and height_in_items
    //static inline void  ListBoxFooter()                                                             { EndListBox(); }
    //-- OBSOLETED in 1.79 (from August 2020)
    //static inline void  OpenPopupContextItem(const char* str_id = NULL, UxImGuiMouseButton mb = 1)    { OpenPopupOnItemClick(str_id, mb); } // Bool return value removed. Use IsWindowAppearing() in BeginPopup() instead. Renamed in 1.77, renamed back in 1.79. Sorry!
    //-- OBSOLETED in 1.78 (from June 2020): Old drag/sliders functions that took a 'float power > 1.0f' argument instead of UxImGuiSliderFlags_Logarithmic. See github.com/ocornut/imgui/issues/3361 for details.
    //IMGUI_API bool      DragScalar(const char* label, UxImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, float power = 1.0f)                                                            // OBSOLETED in 1.78 (from June 2020)
    //IMGUI_API bool      DragScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, float power = 1.0f);                                          // OBSOLETED in 1.78 (from June 2020)
    //IMGUI_API bool      SliderScalar(const char* label, UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, float power = 1.0f);                                                                        // OBSOLETED in 1.78 (from June 2020)
    //IMGUI_API bool      SliderScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, const char* format, float power = 1.0f);                                                       // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, float power = 1.0f)    { return DragScalar(label, UxImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, power); }     // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, float power = 1.0f) { return DragScalarN(label, UxImGuiDataType_Float, v, 2, v_speed, &v_min, &v_max, format, power); } // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, float power = 1.0f) { return DragScalarN(label, UxImGuiDataType_Float, v, 3, v_speed, &v_min, &v_max, format, power); } // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, float power = 1.0f) { return DragScalarN(label, UxImGuiDataType_Float, v, 4, v_speed, &v_min, &v_max, format, power); } // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, float power = 1.0f)                 { return SliderScalar(label, UxImGuiDataType_Float, v, &v_min, &v_max, format, power); }            // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, float power = 1.0f)              { return SliderScalarN(label, UxImGuiDataType_Float, v, 2, &v_min, &v_max, format, power); }        // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, float power = 1.0f)              { return SliderScalarN(label, UxImGuiDataType_Float, v, 3, &v_min, &v_max, format, power); }        // OBSOLETED in 1.78 (from June 2020)
    //static inline bool  SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, float power = 1.0f)              { return SliderScalarN(label, UxImGuiDataType_Float, v, 4, &v_min, &v_max, format, power); }        // OBSOLETED in 1.78 (from June 2020)
    //-- OBSOLETED in 1.77 and before
    //static inline bool  BeginPopupContextWindow(const char* str_id, UxImGuiMouseButton mb, bool over_items) { return BeginPopupContextWindow(str_id, mb | (over_items ? 0 : UxImGuiPopupFlags_NoOpenOverItems)); } // OBSOLETED in 1.77 (from June 2020)
    //static inline void  TreeAdvanceToLabelPos()               { SetCursorPosX(GetCursorPosX() + GetTreeNodeToLabelSpacing()); }   // OBSOLETED in 1.72 (from July 2019)
    //static inline void  SetNextTreeNodeOpen(bool open, UxImGuiCond cond = 0) { SetNextItemOpen(open, cond); }                       // OBSOLETED in 1.71 (from June 2019)
    //static inline float GetContentRegionAvailWidth()          { return GetContentRegionAvail().x; }                               // OBSOLETED in 1.70 (from May 2019)
    //static inline UxImDrawList* GetOverlayDrawList()            { return GetForegroundDrawList(); }                                 // OBSOLETED in 1.69 (from Mar 2019)
    //static inline void  SetScrollHere(float ratio = 0.5f)     { SetScrollHereY(ratio); }                                          // OBSOLETED in 1.66 (from Nov 2018)
    //static inline bool  IsItemDeactivatedAfterChange()        { return IsItemDeactivatedAfterEdit(); }                            // OBSOLETED in 1.63 (from Aug 2018)
    //-- OBSOLETED in 1.60 and before
    //static inline bool  IsAnyWindowFocused()                  { return IsWindowFocused(UxImGuiFocusedFlags_AnyWindow); }            // OBSOLETED in 1.60 (from Apr 2018)
    //static inline bool  IsAnyWindowHovered()                  { return IsWindowHovered(UxImGuiHoveredFlags_AnyWindow); }            // OBSOLETED in 1.60 (between Dec 2017 and Apr 2018)
    //static inline void  ShowTestWindow()                      { return ShowDemoWindow(); }                                        // OBSOLETED in 1.53 (between Oct 2017 and Dec 2017)
    //static inline bool  IsRootWindowFocused()                 { return IsWindowFocused(UxImGuiFocusedFlags_RootWindow); }           // OBSOLETED in 1.53 (between Oct 2017 and Dec 2017)
    //static inline bool  IsRootWindowOrAnyChildFocused()       { return IsWindowFocused(UxImGuiFocusedFlags_RootAndChildWindows); }  // OBSOLETED in 1.53 (between Oct 2017 and Dec 2017)
    //static inline void  SetNextWindowContentWidth(float w)    { SetNextWindowContentSize(UxImVec2(w, 0.0f)); }                      // OBSOLETED in 1.53 (between Oct 2017 and Dec 2017)
    //static inline float GetItemsLineHeightWithSpacing()       { return GetFrameHeightWithSpacing(); }                             // OBSOLETED in 1.53 (between Oct 2017 and Dec 2017)
    //IMGUI_API bool      Begin(char* name, bool* p_open, UxImVec2 size_first_use, float bg_alpha = -1.0f, UxImGuiWindowFlags flags=0); // OBSOLETED in 1.52 (between Aug 2017 and Oct 2017): Equivalent of using SetNextWindowSize(size, UxImGuiCond_FirstUseEver) and SetNextWindowBgAlpha().
    //static inline bool  IsRootWindowOrAnyChildHovered()       { return IsWindowHovered(UxImGuiHoveredFlags_RootAndChildWindows); }  // OBSOLETED in 1.52 (between Aug 2017 and Oct 2017)
    //static inline void  AlignFirstTextHeightToWidgets()       { AlignTextToFramePadding(); }                                      // OBSOLETED in 1.52 (between Aug 2017 and Oct 2017)
    //static inline void  SetNextWindowPosCenter(UxImGuiCond c=0) { SetNextWindowPos(GetMainViewport()->GetCenter(), c, UxImVec2(0.5f,0.5f)); } // OBSOLETED in 1.52 (between Aug 2017 and Oct 2017)
    //static inline bool  IsItemHoveredRect()                   { return IsItemHovered(UxImGuiHoveredFlags_RectOnly); }               // OBSOLETED in 1.51 (between Jun 2017 and Aug 2017)
    //static inline bool  IsPosHoveringAnyWindow(const UxImVec2&) { IM_ASSERT(0); return false; }                                     // OBSOLETED in 1.51 (between Jun 2017 and Aug 2017): This was misleading and partly broken. You probably want to use the io.WantCaptureMouse flag instead.
    //static inline bool  IsMouseHoveringAnyWindow()            { return IsWindowHovered(UxImGuiHoveredFlags_AnyWindow); }            // OBSOLETED in 1.51 (between Jun 2017 and Aug 2017)
    //static inline bool  IsMouseHoveringWindow()               { return IsWindowHovered(UxImGuiHoveredFlags_AllowWhenBlockedByPopup | UxImGuiHoveredFlags_AllowWhenBlockedByActiveItem); }       // OBSOLETED in 1.51 (between Jun 2017 and Aug 2017)
    //-- OBSOLETED in 1.50 and before
    //static inline bool  CollapsingHeader(char* label, const char* str_id, bool framed = true, bool default_open = false) { return CollapsingHeader(label, (default_open ? (1 << 5) : 0)); } // OBSOLETED in 1.49
    //static inline UxImFont*GetWindowFont()                      { return GetFont(); }                                               // OBSOLETED in 1.48
    //static inline float GetWindowFontSize()                   { return GetFontSize(); }                                           // OBSOLETED in 1.48
    //static inline void  SetScrollPosHere()                    { SetScrollHere(); }                                                // OBSOLETED in 1.42
}

//-- OBSOLETED in 1.92.x: UxImFontAtlasCustomRect becomes UxImTextureRect
// - UxImFontAtlasCustomRect::X,Y          --> UxImTextureRect::x,y
// - UxImFontAtlasCustomRect::Width,Height --> UxImTextureRect::w,h
// - UxImFontAtlasCustomRect::GlyphColored --> if you need to write to this, instead you can write to 'font->Glyphs.back()->Colored' after calling AddCustomRectFontGlyph()
// We could make UxImTextureRect an union to use old names, but 1) this would be confusing 2) the fix is easy 3) UxImFontAtlasCustomRect was always a rather esoteric api.
typedef UxImFontAtlasRect UxImFontAtlasCustomRect;
/*struct UxImFontAtlasCustomRect
{
    unsigned short  X, Y;           // Output   // Packed position in Atlas
    unsigned short  Width, Height;  // Input    // [Internal] Desired rectangle dimension
    unsigned int    GlyphID:31;     // Input    // [Internal] For custom font glyphs only (ID < 0x110000)
    unsigned int    GlyphColored:1; // Input    // [Internal] For custom font glyphs only: glyph is colored, removed tinting.
    float           GlyphAdvanceX;  // Input    // [Internal] For custom font glyphs only: glyph xadvance
    UxImVec2          GlyphOffset;    // Input    // [Internal] For custom font glyphs only: glyph display offset
    UxImFont*         Font;           // Input    // [Internal] For custom font glyphs only: target font
    UxImFontAtlasCustomRect()         { X = Y = 0xFFFF; Width = Height = 0; GlyphID = 0; GlyphColored = 0; GlyphAdvanceX = 0.0f; GlyphOffset = UxImVec2(0, 0); Font = NULL; }
    bool IsPacked() const           { return X != 0xFFFF; }
};*/

//-- OBSOLETED in 1.82 (from Mars 2021): flags for AddRect(), AddRectFilled(), AddImageRounded(), PathRect()
//typedef UxImDrawFlags UxImDrawCornerFlags;
//enum UxImDrawCornerFlags_
//{
//    UxImDrawCornerFlags_None      = UxImDrawFlags_RoundCornersNone,         // Was == 0 prior to 1.82, this is now == UxImDrawFlags_RoundCornersNone which is != 0 and not implicit
//    UxImDrawCornerFlags_TopLeft   = UxImDrawFlags_RoundCornersTopLeft,      // Was == 0x01 (1 << 0) prior to 1.82. Order matches UxImDrawFlags_NoRoundCorner* flag (we exploit this internally).
//    UxImDrawCornerFlags_TopRight  = UxImDrawFlags_RoundCornersTopRight,     // Was == 0x02 (1 << 1) prior to 1.82.
//    UxImDrawCornerFlags_BotLeft   = UxImDrawFlags_RoundCornersBottomLeft,   // Was == 0x04 (1 << 2) prior to 1.82.
//    UxImDrawCornerFlags_BotRight  = UxImDrawFlags_RoundCornersBottomRight,  // Was == 0x08 (1 << 3) prior to 1.82.
//    UxImDrawCornerFlags_All       = UxImDrawFlags_RoundCornersAll,          // Was == 0x0F prior to 1.82
//    UxImDrawCornerFlags_Top       = UxImDrawCornerFlags_TopLeft | UxImDrawCornerFlags_TopRight,
//    UxImDrawCornerFlags_Bot       = UxImDrawCornerFlags_BotLeft | UxImDrawCornerFlags_BotRight,
//    UxImDrawCornerFlags_Left      = UxImDrawCornerFlags_TopLeft | UxImDrawCornerFlags_BotLeft,
//    UxImDrawCornerFlags_Right     = UxImDrawCornerFlags_TopRight | UxImDrawCornerFlags_BotRight,
//};

// RENAMED and MERGED both UxImGuiKey_ModXXX and UxImGuiModFlags_XXX into UxImGuiMod_XXX (from September 2022)
// RENAMED UxImGuiKeyModFlags -> UxImGuiModFlags in 1.88 (from April 2022). Exceptionally commented out ahead of obscolescence schedule to reduce confusion and because they were not meant to be used in the first place.
//typedef UxImGuiKeyChord UxImGuiModFlags;      // == int. We generally use UxImGuiKeyChord to mean "a UxImGuiKey or-ed with any number of UxImGuiMod_XXX value", so you may store mods in there.
//enum UxImGuiModFlags_ { UxImGuiModFlags_None = 0, UxImGuiModFlags_Ctrl = UxImGuiMod_Ctrl, UxImGuiModFlags_Shift = UxImGuiMod_Shift, UxImGuiModFlags_Alt = UxImGuiMod_Alt, UxImGuiModFlags_Super = UxImGuiMod_Super };
//typedef UxImGuiKeyChord UxImGuiKeyModFlags; // == int
//enum UxImGuiKeyModFlags_ { UxImGuiKeyModFlags_None = 0, UxImGuiKeyModFlags_Ctrl = UxImGuiMod_Ctrl, UxImGuiKeyModFlags_Shift = UxImGuiMod_Shift, UxImGuiKeyModFlags_Alt = UxImGuiMod_Alt, UxImGuiKeyModFlags_Super = UxImGuiMod_Super };

#define IM_OFFSETOF(_TYPE,_MEMBER)  offsetof(_TYPE, _MEMBER)    // OBSOLETED IN 1.90 (now using C++11 standard version)

#endif // #ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS

// RENAMED IMGUI_DISABLE_METRICS_WINDOW > IMGUI_DISABLE_DEBUG_TOOLS in 1.88 (from June 2022)
#ifdef IMGUI_DISABLE_METRICS_WINDOW
#error IMGUI_DISABLE_METRICS_WINDOW was renamed to IMGUI_DISABLE_DEBUG_TOOLS, please use new name.
#endif

//-----------------------------------------------------------------------------

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning (pop)
#endif

// Include imgui_user.h at the end of imgui.h
// May be convenient for some users to only explicitly include vanilla imgui.h and have extra stuff included.
#ifdef IMGUI_INCLUDE_IMGUI_USER_H
#ifdef IMGUI_USER_H_FILENAME
#include IMGUI_USER_H_FILENAME
#else
#include "imgui_user.h"
#endif
#endif

#endif // #ifndef IMGUI_DISABLE


