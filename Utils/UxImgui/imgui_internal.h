// dear imgui, v1.92.0 WIP
// (internal structures/api)

// You may use this file to debug, understand or extend Dear UxImGui features but we don't provide any guarantee of forward compatibility.

/*

Index of this file:

// [SECTION] Header mess
// [SECTION] Forward declarations
// [SECTION] Context pointer
// [SECTION] STB libraries includes
// [SECTION] Macros
// [SECTION] Generic helpers
// [SECTION] UxImDrawList support
// [SECTION] Style support
// [SECTION] Data types support
// [SECTION] Widgets support: flags, enums, data structures
// [SECTION] Popup support
// [SECTION] Inputs support
// [SECTION] Clipper support
// [SECTION] Navigation support
// [SECTION] Typing-select support
// [SECTION] Columns support
// [SECTION] Box-select support
// [SECTION] Multi-select support
// [SECTION] Docking support
// [SECTION] Viewport support
// [SECTION] Settings support
// [SECTION] Localization support
// [SECTION] Error handling, State recovery support
// [SECTION] Metrics, Debug tools
// [SECTION] Generic context hooks
// [SECTION] UxImGuiContext (main imgui context)
// [SECTION] UxImGuiWindowTempData, UxImGuiWindow
// [SECTION] Tab bar, Tab item support
// [SECTION] Table support
// [SECTION] UxImGui internal API
// [SECTION] UxImFontLoader
// [SECTION] UxImFontAtlas internal API
// [SECTION] Test Engine specific hooks (imgui_test_engine)

*/

#pragma once
#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Header mess
//-----------------------------------------------------------------------------

#ifndef IMGUI_VERSION
#include "imgui.h"
#endif

#include <stdio.h>      // FILE*, sscanf
#include <stdlib.h>     // NULL, malloc, free, qsort, atoi, atof
#include <math.h>       // sqrtf, fabsf, fmodf, powf, floorf, ceilf, cosf, sinf
#include <limits.h>     // INT_MIN, INT_MAX

// Enable SSE intrinsics if available
#if (defined __SSE__ || defined __x86_64__ || defined _M_X64 || (defined(_M_IX86_FP) && (_M_IX86_FP >= 1))) && !defined(IMGUI_DISABLE_SSE)
#define IMGUI_ENABLE_SSE
#include <immintrin.h>
#if (defined __AVX__ || defined __SSE4_2__)
#define IMGUI_ENABLE_SSE4_2
#include <nmmintrin.h>
#endif
#endif
// Emscripten has partial SSE 4.2 support where _mm_crc32_u32 is not available. See https://emscripten.org/docs/porting/simd.html#id11 and #8213
#if defined(IMGUI_ENABLE_SSE4_2) && !defined(IMGUI_USE_LEGACY_CRC32_ADLER) && !defined(__EMSCRIPTEN__)
#define IMGUI_ENABLE_SSE4_2_CRC
#endif

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4251)     // class 'xxx' needs to have dll-interface to be used by clients of struct 'xxx' // when IMGUI_API is set to__declspec(dllexport)
#pragma warning (disable: 26812)    // The enum type 'xxx' is unscoped. Prefer 'enum class' over 'enum' (Enum.3). [MSVC Static Analyzer)
#pragma warning (disable: 26495)    // [Static Analyzer] Variable 'XXX' is uninitialized. Always initialize a member variable (type.6).
#if defined(_MSC_VER) && _MSC_VER >= 1922 // MSVC 2019 16.2 or later
#pragma warning (disable: 5054)     // operator '|': deprecated between enumerations of different types
#endif
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wfloat-equal"                    // warning: comparing floating point with == or != is unsafe // storing and comparing against same constants ok, for UxImFloor()
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#pragma clang diagnostic ignored "-Wmissing-noreturn"               // warning: function 'xxx' could be declared with attribute 'noreturn'
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"// warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"            // warning: 'xxx' is an unsafe pointer used for buffer access
#pragma clang diagnostic ignored "-Wnontrivial-memaccess"           // warning: first argument in call to 'memset' is a pointer to non-trivially copyable type
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"                          // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wfloat-equal"                      // warning: comparing floating-point with '==' or '!=' is unsafe
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"  // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#endif

// In 1.89.4, we moved the implementation of "courtesy maths operators" from imgui_internal.h in imgui.h
// As they are frequently requested, we do not want to encourage to many people using imgui_internal.h
#if defined(IMGUI_DEFINE_MATH_OPERATORS) && !defined(IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED)
#error Please '#define IMGUI_DEFINE_MATH_OPERATORS' _BEFORE_ including imgui.h!
#endif

// Legacy defines
#ifdef IMGUI_DISABLE_FORMAT_STRING_FUNCTIONS            // Renamed in 1.74
#error Use IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
#endif
#ifdef IMGUI_DISABLE_MATH_FUNCTIONS                     // Renamed in 1.74
#error Use IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
#endif

// Enable stb_truetype by default unless FreeType is enabled.
// You can compile with both by defining both IMGUI_ENABLE_FREETYPE and IMGUI_ENABLE_STB_TRUETYPE together.
#ifndef IMGUI_ENABLE_FREETYPE
#define IMGUI_ENABLE_STB_TRUETYPE
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward declarations
//-----------------------------------------------------------------------------

// Utilities
// (other types which are not forwarded declared are: UxImBitArray<>, UxImSpan<>, UxImSpanAllocator<>, UxImStableVector<>, UxImPool<>, UxImChunkStream<>)
struct UxImBitVector;                 // Store 1-bit per value
struct UxImRect;                      // An axis-aligned rectangle (2 points)
struct UxImGuiTextIndex;              // Maintain a line index for a text buffer.

// UxImDrawList/UxImFontAtlas
struct UxImDrawDataBuilder;           // Helper to build a UxImDrawData instance
struct UxImDrawListSharedData;        // Data shared between all UxImDrawList instances
struct UxImFontAtlasBuilder;          // Internal storage for incrementally packing and building a UxImFontAtlas
struct UxImFontAtlasPostProcessData;  // Data available to potential texture post-processing functions
struct UxImFontAtlasRectEntry;        // Packed rectangle lookup entry

// UxImGui
struct UxImGuiBoxSelectState;         // Box-selection state (currently used by multi-selection, could potentially be used by others)
struct UxImGuiColorMod;               // Stacked color modifier, backup of modified data so we can restore it
struct UxImGuiContext;                // Main Dear UxImGui context
struct UxImGuiContextHook;            // Hook for extensions like UxImGuiTestEngine
struct UxImGuiDataTypeInfo;           // Type information associated to a UxImGuiDataType enum
struct UxImGuiDeactivatedItemData;    // Data for IsItemDeactivated()/IsItemDeactivatedAfterEdit() function.
struct UxImGuiDockContext;            // Docking system context
struct UxImGuiDockRequest;            // Docking system dock/undock queued request
struct UxImGuiDockNode;               // Docking system node (hold a list of Windows OR two child dock nodes)
struct UxImGuiDockNodeSettings;       // Storage for a dock node in .ini file (we preserve those even if the associated dock node isn't active during the session)
struct UxImGuiErrorRecoveryState;     // Storage of stack sizes for error handling and recovery
struct UxImGuiGroupData;              // Stacked storage data for BeginGroup()/EndGroup()
struct UxImGuiInputTextState;         // Internal state of the currently focused/edited text input box
struct UxImGuiInputTextDeactivateData;// Short term storage to backup text of a deactivating InputText() while another is stealing active id
struct UxImGuiLastItemData;           // Status storage for last submitted items
struct UxImGuiLocEntry;               // A localization entry.
struct UxImGuiMenuColumns;            // Simple column measurement, currently used for MenuItem() only
struct UxImGuiMultiSelectState;       // Multi-selection persistent state (for focused selection).
struct UxImGuiMultiSelectTempData;    // Multi-selection temporary state (while traversing).
struct UxImGuiNavItemData;            // Result of a keyboard/gamepad directional navigation move query result
struct UxImGuiMetricsConfig;          // Storage for ShowMetricsWindow() and DebugNodeXXX() functions
struct UxImGuiNextWindowData;         // Storage for SetNextWindow** functions
struct UxImGuiNextItemData;           // Storage for SetNextItem** functions
struct UxImGuiOldColumnData;          // Storage data for a single column for legacy Columns() api
struct UxImGuiOldColumns;             // Storage data for a columns set for legacy Columns() api
struct UxImGuiPopupData;              // Storage for current popup stack
struct UxImGuiSettingsHandler;        // Storage for one type registered in the .ini file
struct UxImGuiStyleMod;               // Stacked style modifier, backup of modified data so we can restore it
struct UxImGuiStyleVarInfo;           // Style variable information (e.g. to access style variables from an enum)
struct UxImGuiTabBar;                 // Storage for a tab bar
struct UxImGuiTabItem;                // Storage for a tab item (within a tab bar)
struct UxImGuiTable;                  // Storage for a table
struct UxImGuiTableHeaderData;        // Storage for TableAngledHeadersRow()
struct UxImGuiTableColumn;            // Storage for one column of a table
struct UxImGuiTableInstanceData;      // Storage for one instance of a same table
struct UxImGuiTableTempData;          // Temporary storage for one table (one per table in the stack), shared between tables.
struct UxImGuiTableSettings;          // Storage for a table .ini settings
struct UxImGuiTableColumnsSettings;   // Storage for a column .ini settings
struct UxImGuiTreeNodeStackData;      // Temporary storage for TreeNode().
struct UxImGuiTypingSelectState;      // Storage for GetTypingSelectRequest()
struct UxImGuiTypingSelectRequest;    // Storage for GetTypingSelectRequest() (aimed to be public)
struct UxImGuiWindow;                 // Storage for one window
struct UxImGuiWindowDockStyle;        // Storage for window-style data which needs to be stored for docking purpose
struct UxImGuiWindowTempData;         // Temporary storage for one window (that's the data which in theory we could ditch at the end of the frame, in practice we currently keep it for each window)
struct UxImGuiWindowSettings;         // Storage for a window .ini settings (we keep one of those even if the actual window wasn't instanced during this session)

// Enumerations
// Use your programming IDE "Go to definition" facility on the names of the center columns to find the actual flags/enum lists.
enum UxImGuiLocKey : int;                 // -> enum UxImGuiLocKey              // Enum: a localization entry for translation.
typedef int UxImGuiDataAuthority;         // -> enum UxImGuiDataAuthority_      // Enum: for storing the source authority (dock node vs window) of a field
typedef int UxImGuiLayoutType;            // -> enum UxImGuiLayoutType_         // Enum: Horizontal or vertical

// Flags
typedef int UxImGuiActivateFlags;         // -> enum UxImGuiActivateFlags_      // Flags: for navigation/focus function (will be for ActivateItem() later)
typedef int UxImGuiDebugLogFlags;         // -> enum UxImGuiDebugLogFlags_      // Flags: for ShowDebugLogWindow(), g.DebugLogFlags
typedef int UxImGuiFocusRequestFlags;     // -> enum UxImGuiFocusRequestFlags_  // Flags: for FocusWindow()
typedef int UxImGuiItemStatusFlags;       // -> enum UxImGuiItemStatusFlags_    // Flags: for g.LastItemData.StatusFlags
typedef int UxImGuiOldColumnFlags;        // -> enum UxImGuiOldColumnFlags_     // Flags: for BeginColumns()
typedef int UxImGuiLogFlags;              // -> enum UxImGuiLogFlags_           // Flags: for LogBegin() text capturing function
typedef int UxImGuiNavRenderCursorFlags;  // -> enum UxImGuiNavRenderCursorFlags_//Flags: for RenderNavCursor()
typedef int UxImGuiNavMoveFlags;          // -> enum UxImGuiNavMoveFlags_       // Flags: for navigation requests
typedef int UxImGuiNextItemDataFlags;     // -> enum UxImGuiNextItemDataFlags_  // Flags: for SetNextItemXXX() functions
typedef int UxImGuiNextWindowDataFlags;   // -> enum UxImGuiNextWindowDataFlags_// Flags: for SetNextWindowXXX() functions
typedef int UxImGuiScrollFlags;           // -> enum UxImGuiScrollFlags_        // Flags: for ScrollToItem() and navigation requests
typedef int UxImGuiSeparatorFlags;        // -> enum UxImGuiSeparatorFlags_     // Flags: for SeparatorEx()
typedef int UxImGuiTextFlags;             // -> enum UxImGuiTextFlags_          // Flags: for TextEx()
typedef int UxImGuiTooltipFlags;          // -> enum UxImGuiTooltipFlags_       // Flags: for BeginTooltipEx()
typedef int UxImGuiTypingSelectFlags;     // -> enum UxImGuiTypingSelectFlags_  // Flags: for GetTypingSelectRequest()
typedef int UxImGuiWindowRefreshFlags;    // -> enum UxImGuiWindowRefreshFlags_ // Flags: for SetNextWindowRefreshPolicy()

// Table column indexing
typedef UxImS16 UxImGuiTableColumnIdx;
typedef UxImU16 UxImGuiTableDrawChannelIdx;

//-----------------------------------------------------------------------------
// [SECTION] Context pointer
// See implementation of this variable in imgui.cpp for comments and details.
//-----------------------------------------------------------------------------

#ifndef GUxImGui
extern IMGUI_API UxImGuiContext* GUxImGui;  // Current implicit context pointer
#endif

//-----------------------------------------------------------------------------
// [SECTION] Macros
//-----------------------------------------------------------------------------

// Internal Drag and Drop payload types. String starting with '_' are reserved for Dear UxImGui.
#define IMGUI_PAYLOAD_TYPE_WINDOW       "_IMWINDOW"     // Payload == UxImGuiWindow*

// Debug Printing Into TTY
// (since IMGUI_VERSION_NUM >= 18729: IMGUI_DEBUG_LOG was reworked into IMGUI_DEBUG_PRINTF (and removed framecount from it). If you were using a #define IMGUI_DEBUG_LOG please rename)
#ifndef IMGUI_DEBUG_PRINTF
#ifndef IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
#define IMGUI_DEBUG_PRINTF(_FMT,...)    printf(_FMT, __VA_ARGS__)
#else
#define IMGUI_DEBUG_PRINTF(_FMT,...)    ((void)0)
#endif
#endif

// Debug Logging for ShowDebugLogWindow(). This is designed for relatively rare events so please don't spam.
#define IMGUI_DEBUG_LOG_ERROR(...)      do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventError)       IMGUI_DEBUG_LOG(__VA_ARGS__); else g.DebugLogSkippedErrors++; } while (0)
#define IMGUI_DEBUG_LOG_ACTIVEID(...)   do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventActiveId)    IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_FOCUS(...)      do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventFocus)       IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_POPUP(...)      do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventPopup)       IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_NAV(...)        do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventNav)         IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_SELECTION(...)  do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventSelection)   IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_CLIPPER(...)    do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventClipper)     IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_IO(...)         do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventIO)          IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_FONT(...)       do { UxImGuiContext* g2 = GUxImGui; if (g2 && g2->DebugLogFlags & UxImGuiDebugLogFlags_EventFont) IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0) // Called from UxImFontAtlas function which may operate without a context.
#define IMGUI_DEBUG_LOG_INPUTROUTING(...) do{if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventInputRouting)IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_DOCKING(...)    do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventDocking)     IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)
#define IMGUI_DEBUG_LOG_VIEWPORT(...)   do { if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventViewport)    IMGUI_DEBUG_LOG(__VA_ARGS__); } while (0)

// Static Asserts
#define IM_STATIC_ASSERT(_COND)         static_assert(_COND, "")

// "Paranoid" Debug Asserts are meant to only be enabled during specific debugging/work, otherwise would slow down the code too much.
// We currently don't have many of those so the effect is currently negligible, but onward intent to add more aggressive ones in the code.
//#define IMGUI_DEBUG_PARANOID
#ifdef IMGUI_DEBUG_PARANOID
#define IM_ASSERT_PARANOID(_EXPR)       IM_ASSERT(_EXPR)
#else
#define IM_ASSERT_PARANOID(_EXPR)
#endif

// Misc Macros
#define IM_PI                           3.14159265358979323846f
#ifdef _WIN32
#define IM_NEWLINE                      "\r\n"   // Play it nice with Windows users (Update: since 2018-05, Notepad finally appears to support Unix-style carriage returns!)
#else
#define IM_NEWLINE                      "\n"
#endif
#ifndef IM_TABSIZE                      // Until we move this to runtime and/or add proper tab support, at least allow users to compile-time override
#define IM_TABSIZE                      (4)
#endif
#define IM_MEMALIGN(_OFF,_ALIGN)        (((_OFF) + ((_ALIGN) - 1)) & ~((_ALIGN) - 1))           // Memory align e.g. IM_ALIGN(0,4)=0, IM_ALIGN(1,4)=4, IM_ALIGN(4,4)=4, IM_ALIGN(5,4)=8
#define IM_F32_TO_INT8_UNBOUND(_VAL)    ((int)((_VAL) * 255.0f + ((_VAL)>=0 ? 0.5f : -0.5f)))   // Unsaturated, for display purpose
#define IM_F32_TO_INT8_SAT(_VAL)        ((int)(UxImSaturate(_VAL) * 255.0f + 0.5f))               // Saturated, always output 0..255
#define IM_TRUNC(_VAL)                  ((float)(int)(_VAL))                                    // UxImTrunc() is not inlined in MSVC debug builds
#define IM_ROUND(_VAL)                  ((float)(int)((_VAL) + 0.5f))                           //
#define IM_STRINGIFY_HELPER(_X)         #_X
#define IM_STRINGIFY(_X)                IM_STRINGIFY_HELPER(_X)                                 // Preprocessor idiom to stringify e.g. an integer.
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IM_FLOOR IM_TRUNC
#endif

// Hint for branch prediction
#if (defined(__cplusplus) && (__cplusplus >= 202002L)) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#define IM_LIKELY   [[likely]]
#define IM_UNLIKELY [[unlikely]]
#else
#define IM_LIKELY
#define IM_UNLIKELY
#endif

// Enforce cdecl calling convention for functions called by the standard library, in case compilation settings changed the default to e.g. __vectorcall
#ifdef _MSC_VER
#define IMGUI_CDECL __cdecl
#else
#define IMGUI_CDECL
#endif

// Warnings
#if defined(_MSC_VER) && !defined(__clang__)
#define IM_MSVC_WARNING_SUPPRESS(XXXX)  __pragma(warning(suppress: XXXX))
#else
#define IM_MSVC_WARNING_SUPPRESS(XXXX)
#endif

// Debug Tools
// Use 'Metrics/Debugger->Tools->Item Picker' to break into the call-stack of a specific item.
// This will call IM_DEBUG_BREAK() which you may redefine yourself. See https://github.com/scottt/debugbreak for more reference.
#ifndef IM_DEBUG_BREAK
#if defined (_MSC_VER)
#define IM_DEBUG_BREAK()    __debugbreak()
#elif defined(__clang__)
#define IM_DEBUG_BREAK()    __builtin_debugtrap()
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define IM_DEBUG_BREAK()    __asm__ volatile("int3;nop")
#elif defined(__GNUC__) && defined(__thumb__)
#define IM_DEBUG_BREAK()    __asm__ volatile(".inst 0xde01")
#elif defined(__GNUC__) && defined(__arm__) && !defined(__thumb__)
#define IM_DEBUG_BREAK()    __asm__ volatile(".inst 0xe7f001f0")
#else
#define IM_DEBUG_BREAK()    IM_ASSERT(0)    // It is expected that you define IM_DEBUG_BREAK() into something that will break nicely in a debugger!
#endif
#endif // #ifndef IM_DEBUG_BREAK

// Format specifiers, printing 64-bit hasn't been decently standardized...
// In a real application you should be using PRId64 and PRIu64 from <inttypes.h> (non-windows) and on Windows define them yourself.
#if defined(_MSC_VER) && !defined(__clang__)
#define IM_PRId64   "I64d"
#define IM_PRIu64   "I64u"
#define IM_PRIX64   "I64X"
#else
#define IM_PRId64   "lld"
#define IM_PRIu64   "llu"
#define IM_PRIX64   "llX"
#endif

//-----------------------------------------------------------------------------
// [SECTION] Generic helpers
// Note that the UxImXXX helpers functions are lower-level than UxImGui functions.
// UxImGui functions or the UxImGui context are never called/used from other UxImXXX functions.
//-----------------------------------------------------------------------------
// - Helpers: Hashing
// - Helpers: Sorting
// - Helpers: Bit manipulation
// - Helpers: String
// - Helpers: Formatting
// - Helpers: UTF-8 <> wchar conversions
// - Helpers: UxImVec2/UxImVec4 operators
// - Helpers: Maths
// - Helpers: Geometry
// - Helper: UxImVec1
// - Helper: UxImVec2ih
// - Helper: UxImRect
// - Helper: UxImBitArray
// - Helper: UxImBitVector
// - Helper: UxImSpan<>, UxImSpanAllocator<>
// - Helper: UxImStableVector<>
// - Helper: UxImPool<>
// - Helper: UxImChunkStream<>
// - Helper: UxImGuiTextIndex
// - Helper: UxImGuiStorage
//-----------------------------------------------------------------------------

// Helpers: Hashing
IMGUI_API UxImGuiID       UxImHashData(const void* data, size_t data_size, UxImGuiID seed = 0);
IMGUI_API UxImGuiID       UxImHashStr(const char* data, size_t data_size = 0, UxImGuiID seed = 0);

// Helpers: Sorting
#ifndef UxImQsort
static inline void      UxImQsort(void* base, size_t count, size_t size_of_element, int(IMGUI_CDECL *compare_func)(void const*, void const*)) { if (count > 1) qsort(base, count, size_of_element, compare_func); }
#endif

// Helpers: Color Blending
IMGUI_API UxImU32         UxImAlphaBlendColors(UxImU32 col_a, UxImU32 col_b);

// Helpers: Bit manipulation
static inline bool      UxImIsPowerOfTwo(int v)               { return v != 0 && (v & (v - 1)) == 0; }
static inline bool      UxImIsPowerOfTwo(UxImU64 v)             { return v != 0 && (v & (v - 1)) == 0; }
static inline int       UxImUpperPowerOfTwo(int v)            { v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v++; return v; }
static inline unsigned int UxImCountSetBits(unsigned int v)   { unsigned int count = 0; while (v > 0) { v = v & (v - 1); count++; } return count; }

// Helpers: String
#define UxImStrlen strlen
#define UxImMemchr memchr
IMGUI_API int           UxImStricmp(const char* str1, const char* str2);                      // Case insensitive compare.
IMGUI_API int           UxImStrnicmp(const char* str1, const char* str2, size_t count);       // Case insensitive compare to a certain count.
IMGUI_API void          UxImStrncpy(char* dst, const char* src, size_t count);                // Copy to a certain count and always zero terminate (strncpy doesn't).
IMGUI_API char*         UxImStrdup(const char* str);                                          // Duplicate a string.
IMGUI_API void*         UxImMemdup(const void* src, size_t size);                             // Duplicate a chunk of memory.
IMGUI_API char*         UxImStrdupcpy(char* dst, size_t* p_dst_size, const char* str);        // Copy in provided buffer, recreate buffer if needed.
IMGUI_API const char*   UxImStrchrRange(const char* str_begin, const char* str_end, char c);  // Find first occurrence of 'c' in string range.
IMGUI_API const char*   UxImStreolRange(const char* str, const char* str_end);                // End end-of-line
IMGUI_API const char*   UxImStristr(const char* haystack, const char* haystack_end, const char* needle, const char* needle_end);  // Find a substring in a string range.
IMGUI_API void          UxImStrTrimBlanks(char* str);                                         // Remove leading and trailing blanks from a buffer.
IMGUI_API const char*   UxImStrSkipBlank(const char* str);                                    // Find first non-blank character.
IMGUI_API int           UxImStrlenW(const UxImWchar* str);                                      // Computer string length (UxImWchar string)
IMGUI_API const char*   UxImStrbol(const char* buf_mid_line, const char* buf_begin);          // Find beginning-of-line
IM_MSVC_RUNTIME_CHECKS_OFF
static inline char      UxImToUpper(char c)               { return (c >= 'a' && c <= 'z') ? c &= ~32 : c; }
static inline bool      UxImCharIsBlankA(char c)          { return c == ' ' || c == '\t'; }
static inline bool      UxImCharIsBlankW(unsigned int c)  { return c == ' ' || c == '\t' || c == 0x3000; }
static inline bool      UxImCharIsXdigitA(char c)         { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
IM_MSVC_RUNTIME_CHECKS_RESTORE

// Helpers: Formatting
IMGUI_API int           UxImFormatString(char* buf, size_t buf_size, const char* fmt, ...) IM_FMTARGS(3);
IMGUI_API int           UxImFormatStringV(char* buf, size_t buf_size, const char* fmt, va_list args) IM_FMTLIST(3);
IMGUI_API void          UxImFormatStringToTempBuffer(const char** out_buf, const char** out_buf_end, const char* fmt, ...) IM_FMTARGS(3);
IMGUI_API void          UxImFormatStringToTempBufferV(const char** out_buf, const char** out_buf_end, const char* fmt, va_list args) IM_FMTLIST(3);
IMGUI_API const char*   UxImParseFormatFindStart(const char* format);
IMGUI_API const char*   UxImParseFormatFindEnd(const char* format);
IMGUI_API const char*   UxImParseFormatTrimDecorations(const char* format, char* buf, size_t buf_size);
IMGUI_API void          UxImParseFormatSanitizeForPrinting(const char* fmt_in, char* fmt_out, size_t fmt_out_size);
IMGUI_API const char*   UxImParseFormatSanitizeForScanning(const char* fmt_in, char* fmt_out, size_t fmt_out_size);
IMGUI_API int           UxImParseFormatPrecision(const char* format, int default_value);

// Helpers: UTF-8 <> wchar conversions
IMGUI_API const char*   UxImTextCharToUtf8(char out_buf[5], unsigned int c);                                                      // return out_buf
IMGUI_API int           UxImTextStrToUtf8(char* out_buf, int out_buf_size, const UxImWchar* in_text, const UxImWchar* in_text_end);   // return output UTF-8 bytes count
IMGUI_API int           UxImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end);               // read one character. return input UTF-8 bytes count
IMGUI_API int           UxImTextStrFromUtf8(UxImWchar* out_buf, int out_buf_size, const char* in_text, const char* in_text_end, const char** in_remaining = NULL);   // return input UTF-8 bytes count
IMGUI_API int           UxImTextCountCharsFromUtf8(const char* in_text, const char* in_text_end);                                 // return number of UTF-8 code-points (NOT bytes count)
IMGUI_API int           UxImTextCountUtf8BytesFromChar(const char* in_text, const char* in_text_end);                             // return number of bytes to express one char in UTF-8
IMGUI_API int           UxImTextCountUtf8BytesFromStr(const UxImWchar* in_text, const UxImWchar* in_text_end);                        // return number of bytes to express string in UTF-8
IMGUI_API const char*   UxImTextFindPreviousUtf8Codepoint(const char* in_text_start, const char* in_text_curr);                   // return previous UTF-8 code-point.
IMGUI_API int           UxImTextCountLines(const char* in_text, const char* in_text_end);                                         // return number of lines taken by text. trailing carriage return doesn't count as an extra line.

// Helpers: File System
#ifdef IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
typedef void* UxImFileHandle;
static inline UxImFileHandle  UxImFileOpen(const char*, const char*)                    { return NULL; }
static inline bool          UxImFileClose(UxImFileHandle)                               { return false; }
static inline UxImU64         UxImFileGetSize(UxImFileHandle)                             { return (UxImU64)-1; }
static inline UxImU64         UxImFileRead(void*, UxImU64, UxImU64, UxImFileHandle)           { return 0; }
static inline UxImU64         UxImFileWrite(const void*, UxImU64, UxImU64, UxImFileHandle)    { return 0; }
#endif
#ifndef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
typedef FILE* UxImFileHandle;
IMGUI_API UxImFileHandle      UxImFileOpen(const char* filename, const char* mode);
IMGUI_API bool              UxImFileClose(UxImFileHandle file);
IMGUI_API UxImU64             UxImFileGetSize(UxImFileHandle file);
IMGUI_API UxImU64             UxImFileRead(void* data, UxImU64 size, UxImU64 count, UxImFileHandle file);
IMGUI_API UxImU64             UxImFileWrite(const void* data, UxImU64 size, UxImU64 count, UxImFileHandle file);
#else
#define IMGUI_DISABLE_TTY_FUNCTIONS // Can't use stdout, fflush if we are not using default file functions
#endif
IMGUI_API void*             UxImFileLoadToMemory(const char* filename, const char* mode, size_t* out_file_size = NULL, int padding_bytes = 0);

// Helpers: Maths
IM_MSVC_RUNTIME_CHECKS_OFF
// - Wrapper for standard libs functions. (Note that imgui_demo.cpp does _not_ use them to keep the code easy to copy)
#ifndef IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
#define UxImFabs(X)           fabsf(X)
#define UxImSqrt(X)           sqrtf(X)
#define UxImFmod(X, Y)        fmodf((X), (Y))
#define UxImCos(X)            cosf(X)
#define UxImSin(X)            sinf(X)
#define UxImAcos(X)           acosf(X)
#define UxImAtan2(Y, X)       atan2f((Y), (X))
#define UxImAtof(STR)         atof(STR)
#define UxImCeil(X)           ceilf(X)
static inline float  UxImPow(float x, float y)    { return powf(x, y); }          // DragBehaviorT/SliderBehaviorT uses UxImPow with either float/double and need the precision
static inline double UxImPow(double x, double y)  { return pow(x, y); }
static inline float  UxImLog(float x)             { return logf(x); }             // DragBehaviorT/SliderBehaviorT uses UxImLog with either float/double and need the precision
static inline double UxImLog(double x)            { return log(x); }
static inline int    UxImAbs(int x)               { return x < 0 ? -x : x; }
static inline float  UxImAbs(float x)             { return fabsf(x); }
static inline double UxImAbs(double x)            { return fabs(x); }
static inline float  UxImSign(float x)            { return (x < 0.0f) ? -1.0f : (x > 0.0f) ? 1.0f : 0.0f; } // Sign operator - returns -1, 0 or 1 based on sign of argument
static inline double UxImSign(double x)           { return (x < 0.0) ? -1.0 : (x > 0.0) ? 1.0 : 0.0; }
#ifdef IMGUI_ENABLE_SSE
static inline float  UxImRsqrt(float x)           { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static inline float  UxImRsqrt(float x)           { return 1.0f / sqrtf(x); }
#endif
static inline double UxImRsqrt(double x)          { return 1.0 / sqrt(x); }
#endif
// - UxImMin/UxImMax/UxImClamp/UxImLerp/UxImSwap are used by widgets which support variety of types: signed/unsigned int/long long float/double
// (Exceptionally using templates here but we could also redefine them for those types)
template<typename T> static inline T UxImMin(T lhs, T rhs)                        { return lhs < rhs ? lhs : rhs; }
template<typename T> static inline T UxImMax(T lhs, T rhs)                        { return lhs >= rhs ? lhs : rhs; }
template<typename T> static inline T UxImClamp(T v, T mn, T mx)                   { return (v < mn) ? mn : (v > mx) ? mx : v; }
template<typename T> static inline T UxImLerp(T a, T b, float t)                  { return (T)(a + (b - a) * t); }
template<typename T> static inline void UxImSwap(T& a, T& b)                      { T tmp = a; a = b; b = tmp; }
template<typename T> static inline T UxImAddClampOverflow(T a, T b, T mn, T mx)   { if (b < 0 && (a < mn - b)) return mn; if (b > 0 && (a > mx - b)) return mx; return a + b; }
template<typename T> static inline T UxImSubClampOverflow(T a, T b, T mn, T mx)   { if (b > 0 && (a < mn + b)) return mn; if (b < 0 && (a > mx + b)) return mx; return a - b; }
// - Misc maths helpers
static inline UxImVec2 UxImMin(const UxImVec2& lhs, const UxImVec2& rhs)                { return UxImVec2(lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y); }
static inline UxImVec2 UxImMax(const UxImVec2& lhs, const UxImVec2& rhs)                { return UxImVec2(lhs.x >= rhs.x ? lhs.x : rhs.x, lhs.y >= rhs.y ? lhs.y : rhs.y); }
static inline UxImVec2 UxImClamp(const UxImVec2& v, const UxImVec2&mn, const UxImVec2&mx) { return UxImVec2((v.x < mn.x) ? mn.x : (v.x > mx.x) ? mx.x : v.x, (v.y < mn.y) ? mn.y : (v.y > mx.y) ? mx.y : v.y); }
static inline UxImVec2 UxImLerp(const UxImVec2& a, const UxImVec2& b, float t)          { return UxImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }
static inline UxImVec2 UxImLerp(const UxImVec2& a, const UxImVec2& b, const UxImVec2& t)  { return UxImVec2(a.x + (b.x - a.x) * t.x, a.y + (b.y - a.y) * t.y); }
static inline UxImVec4 UxImLerp(const UxImVec4& a, const UxImVec4& b, float t)          { return UxImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t); }
static inline float  UxImSaturate(float f)                                        { return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f; }
static inline float  UxImLengthSqr(const UxImVec2& lhs)                             { return (lhs.x * lhs.x) + (lhs.y * lhs.y); }
static inline float  UxImLengthSqr(const UxImVec4& lhs)                             { return (lhs.x * lhs.x) + (lhs.y * lhs.y) + (lhs.z * lhs.z) + (lhs.w * lhs.w); }
static inline float  UxImInvLength(const UxImVec2& lhs, float fail_value)           { float d = (lhs.x * lhs.x) + (lhs.y * lhs.y); if (d > 0.0f) return UxImRsqrt(d); return fail_value; }
static inline float  UxImTrunc(float f)                                           { return (float)(int)(f); }
static inline UxImVec2 UxImTrunc(const UxImVec2& v)                                   { return UxImVec2((float)(int)(v.x), (float)(int)(v.y)); }
static inline float  UxImFloor(float f)                                           { return (float)((f >= 0 || (float)(int)f == f) ? (int)f : (int)f - 1); } // Decent replacement for floorf()
static inline UxImVec2 UxImFloor(const UxImVec2& v)                                   { return UxImVec2(UxImFloor(v.x), UxImFloor(v.y)); }
static inline int    UxImModPositive(int a, int b)                                { return (a + b) % b; }
static inline float  UxImDot(const UxImVec2& a, const UxImVec2& b)                    { return a.x * b.x + a.y * b.y; }
static inline UxImVec2 UxImRotate(const UxImVec2& v, float cos_a, float sin_a)        { return UxImVec2(v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a); }
static inline float  UxImLinearSweep(float current, float target, float speed)    { if (current < target) return UxImMin(current + speed, target); if (current > target) return UxImMax(current - speed, target); return current; }
static inline float  UxImLinearRemapClamp(float s0, float s1, float d0, float d1, float x) { return UxImSaturate((x - s0) / (s1 - s0)) * (d1 - d0) + d0; }
static inline UxImVec2 UxImMul(const UxImVec2& lhs, const UxImVec2& rhs)                { return UxImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline bool   UxImIsFloatAboveGuaranteedIntegerPrecision(float f)          { return f <= -16777216 || f >= 16777216; }
static inline float  UxImExponentialMovingAverage(float avg, float sample, int n) { avg -= avg / n; avg += sample / n; return avg; }
IM_MSVC_RUNTIME_CHECKS_RESTORE

// Helpers: Geometry
IMGUI_API UxImVec2     UxImBezierCubicCalc(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, float t);
IMGUI_API UxImVec2     UxImBezierCubicClosestPoint(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, const UxImVec2& p, int num_segments);       // For curves with explicit number of segments
IMGUI_API UxImVec2     UxImBezierCubicClosestPointCasteljau(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, const UxImVec2& p4, const UxImVec2& p, float tess_tol);// For auto-tessellated curves you can use tess_tol = style.CurveTessellationTol
IMGUI_API UxImVec2     UxImBezierQuadraticCalc(const UxImVec2& p1, const UxImVec2& p2, const UxImVec2& p3, float t);
IMGUI_API UxImVec2     UxImLineClosestPoint(const UxImVec2& a, const UxImVec2& b, const UxImVec2& p);
IMGUI_API bool       UxImTriangleContainsPoint(const UxImVec2& a, const UxImVec2& b, const UxImVec2& c, const UxImVec2& p);
IMGUI_API UxImVec2     UxImTriangleClosestPoint(const UxImVec2& a, const UxImVec2& b, const UxImVec2& c, const UxImVec2& p);
IMGUI_API void       UxImTriangleBarycentricCoords(const UxImVec2& a, const UxImVec2& b, const UxImVec2& c, const UxImVec2& p, float& out_u, float& out_v, float& out_w);
inline float         UxImTriangleArea(const UxImVec2& a, const UxImVec2& b, const UxImVec2& c)          { return UxImFabs((a.x * (b.y - c.y)) + (b.x * (c.y - a.y)) + (c.x * (a.y - b.y))) * 0.5f; }
inline bool          UxImTriangleIsClockwise(const UxImVec2& a, const UxImVec2& b, const UxImVec2& c)   { return ((b.x - a.x) * (c.y - b.y)) - ((c.x - b.x) * (b.y - a.y)) > 0.0f; }

// Helper: UxImVec1 (1D vector)
// (this odd construct is used to facilitate the transition between 1D and 2D, and the maintenance of some branches/patches)
IM_MSVC_RUNTIME_CHECKS_OFF
struct UxImVec1
{
    float   x;
    constexpr UxImVec1()         : x(0.0f) { }
    constexpr UxImVec1(float _x) : x(_x) { }
};

// Helper: UxImVec2i (2D vector, integer)
struct UxImVec2i
{
    int         x, y;
    constexpr UxImVec2i()                             : x(0), y(0) {}
    constexpr UxImVec2i(int _x, int _y)               : x(_x), y(_y) {}
};

// Helper: UxImVec2ih (2D vector, half-size integer, for long-term packed storage)
struct UxImVec2ih
{
    short   x, y;
    constexpr UxImVec2ih()                           : x(0), y(0) {}
    constexpr UxImVec2ih(short _x, short _y)         : x(_x), y(_y) {}
    constexpr explicit UxImVec2ih(const UxImVec2& rhs) : x((short)rhs.x), y((short)rhs.y) {}
};

// Helper: UxImRect (2D axis aligned bounding-box)
// NB: we can't rely on UxImVec2 math operators being available here!
struct IMGUI_API UxImRect
{
    UxImVec2      Min;    // Upper-left
    UxImVec2      Max;    // Lower-right

    constexpr UxImRect()                                        : Min(0.0f, 0.0f), Max(0.0f, 0.0f)  {}
    constexpr UxImRect(const UxImVec2& min, const UxImVec2& max)    : Min(min), Max(max)                {}
    constexpr UxImRect(const UxImVec4& v)                         : Min(v.x, v.y), Max(v.z, v.w)      {}
    constexpr UxImRect(float x1, float y1, float x2, float y2)  : Min(x1, y1), Max(x2, y2)          {}

    UxImVec2      GetCenter() const                   { return UxImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f); }
    UxImVec2      GetSize() const                     { return UxImVec2(Max.x - Min.x, Max.y - Min.y); }
    float       GetWidth() const                    { return Max.x - Min.x; }
    float       GetHeight() const                   { return Max.y - Min.y; }
    float       GetArea() const                     { return (Max.x - Min.x) * (Max.y - Min.y); }
    UxImVec2      GetTL() const                       { return Min; }                   // Top-left
    UxImVec2      GetTR() const                       { return UxImVec2(Max.x, Min.y); }  // Top-right
    UxImVec2      GetBL() const                       { return UxImVec2(Min.x, Max.y); }  // Bottom-left
    UxImVec2      GetBR() const                       { return Max; }                   // Bottom-right
    bool        Contains(const UxImVec2& p) const     { return p.x     >= Min.x && p.y     >= Min.y && p.x     <  Max.x && p.y     <  Max.y; }
    bool        Contains(const UxImRect& r) const     { return r.Min.x >= Min.x && r.Min.y >= Min.y && r.Max.x <= Max.x && r.Max.y <= Max.y; }
    bool        ContainsWithPad(const UxImVec2& p, const UxImVec2& pad) const { return p.x >= Min.x - pad.x && p.y >= Min.y - pad.y && p.x < Max.x + pad.x && p.y < Max.y + pad.y; }
    bool        Overlaps(const UxImRect& r) const     { return r.Min.y <  Max.y && r.Max.y >  Min.y && r.Min.x <  Max.x && r.Max.x >  Min.x; }
    void        Add(const UxImVec2& p)                { if (Min.x > p.x)     Min.x = p.x;     if (Min.y > p.y)     Min.y = p.y;     if (Max.x < p.x)     Max.x = p.x;     if (Max.y < p.y)     Max.y = p.y; }
    void        Add(const UxImRect& r)                { if (Min.x > r.Min.x) Min.x = r.Min.x; if (Min.y > r.Min.y) Min.y = r.Min.y; if (Max.x < r.Max.x) Max.x = r.Max.x; if (Max.y < r.Max.y) Max.y = r.Max.y; }
    void        Expand(const float amount)          { Min.x -= amount;   Min.y -= amount;   Max.x += amount;   Max.y += amount; }
    void        Expand(const UxImVec2& amount)        { Min.x -= amount.x; Min.y -= amount.y; Max.x += amount.x; Max.y += amount.y; }
    void        Translate(const UxImVec2& d)          { Min.x += d.x; Min.y += d.y; Max.x += d.x; Max.y += d.y; }
    void        TranslateX(float dx)                { Min.x += dx; Max.x += dx; }
    void        TranslateY(float dy)                { Min.y += dy; Max.y += dy; }
    void        ClipWith(const UxImRect& r)           { Min = UxImMax(Min, r.Min); Max = UxImMin(Max, r.Max); }                   // Simple version, may lead to an inverted rectangle, which is fine for Contains/Overlaps test but not for display.
    void        ClipWithFull(const UxImRect& r)       { Min = UxImClamp(Min, r.Min, r.Max); Max = UxImClamp(Max, r.Min, r.Max); } // Full version, ensure both points are fully clipped.
    void        Floor()                             { Min.x = IM_TRUNC(Min.x); Min.y = IM_TRUNC(Min.y); Max.x = IM_TRUNC(Max.x); Max.y = IM_TRUNC(Max.y); }
    bool        IsInverted() const                  { return Min.x > Max.x || Min.y > Max.y; }
    UxImVec4      ToVec4() const                      { return UxImVec4(Min.x, Min.y, Max.x, Max.y); }
};

// Helper: UxImBitArray
#define         IM_BITARRAY_TESTBIT(_ARRAY, _N)                 ((_ARRAY[(_N) >> 5] & ((UxImU32)1 << ((_N) & 31))) != 0) // Macro version of UxImBitArrayTestBit(): ensure args have side-effect or are costly!
#define         IM_BITARRAY_CLEARBIT(_ARRAY, _N)                ((_ARRAY[(_N) >> 5] &= ~((UxImU32)1 << ((_N) & 31))))    // Macro version of UxImBitArrayClearBit(): ensure args have side-effect or are costly!
inline size_t   UxImBitArrayGetStorageSizeInBytes(int bitcount)   { return (size_t)((bitcount + 31) >> 5) << 2; }
inline void     UxImBitArrayClearAllBits(UxImU32* arr, int bitcount){ memset(arr, 0, UxImBitArrayGetStorageSizeInBytes(bitcount)); }
inline bool     UxImBitArrayTestBit(const UxImU32* arr, int n)      { UxImU32 mask = (UxImU32)1 << (n & 31); return (arr[n >> 5] & mask) != 0; }
inline void     UxImBitArrayClearBit(UxImU32* arr, int n)           { UxImU32 mask = (UxImU32)1 << (n & 31); arr[n >> 5] &= ~mask; }
inline void     UxImBitArraySetBit(UxImU32* arr, int n)             { UxImU32 mask = (UxImU32)1 << (n & 31); arr[n >> 5] |= mask; }
inline void     UxImBitArraySetBitRange(UxImU32* arr, int n, int n2) // Works on range [n..n2)
{
    n2--;
    while (n <= n2)
    {
        int a_mod = (n & 31);
        int b_mod = (n2 > (n | 31) ? 31 : (n2 & 31)) + 1;
        UxImU32 mask = (UxImU32)(((UxImU64)1 << b_mod) - 1) & ~(UxImU32)(((UxImU64)1 << a_mod) - 1);
        arr[n >> 5] |= mask;
        n = (n + 32) & ~31;
    }
}

typedef UxImU32* UxImBitArrayPtr; // Name for use in structs

// Helper: UxImBitArray class (wrapper over UxImBitArray functions)
// Store 1-bit per value.
template<int BITCOUNT, int OFFSET = 0>
struct UxImBitArray
{
    UxImU32           Storage[(BITCOUNT + 31) >> 5];
    UxImBitArray()                                { ClearAllBits(); }
    void            ClearAllBits()              { memset(Storage, 0, sizeof(Storage)); }
    void            SetAllBits()                { memset(Storage, 255, sizeof(Storage)); }
    bool            TestBit(int n) const        { n += OFFSET; IM_ASSERT(n >= 0 && n < BITCOUNT); return IM_BITARRAY_TESTBIT(Storage, n); }
    void            SetBit(int n)               { n += OFFSET; IM_ASSERT(n >= 0 && n < BITCOUNT); UxImBitArraySetBit(Storage, n); }
    void            ClearBit(int n)             { n += OFFSET; IM_ASSERT(n >= 0 && n < BITCOUNT); UxImBitArrayClearBit(Storage, n); }
    void            SetBitRange(int n, int n2)  { n += OFFSET; n2 += OFFSET; IM_ASSERT(n >= 0 && n < BITCOUNT && n2 > n && n2 <= BITCOUNT); UxImBitArraySetBitRange(Storage, n, n2); } // Works on range [n..n2)
    bool            operator[](int n) const     { n += OFFSET; IM_ASSERT(n >= 0 && n < BITCOUNT); return IM_BITARRAY_TESTBIT(Storage, n); }
};

// Helper: UxImBitVector
// Store 1-bit per value.
struct IMGUI_API UxImBitVector
{
    UxImVector<UxImU32> Storage;
    void            Create(int sz)              { Storage.resize((sz + 31) >> 5); memset(Storage.Data, 0, (size_t)Storage.Size * sizeof(Storage.Data[0])); }
    void            Clear()                     { Storage.clear(); }
    bool            TestBit(int n) const        { IM_ASSERT(n < (Storage.Size << 5)); return IM_BITARRAY_TESTBIT(Storage.Data, n); }
    void            SetBit(int n)               { IM_ASSERT(n < (Storage.Size << 5)); UxImBitArraySetBit(Storage.Data, n); }
    void            ClearBit(int n)             { IM_ASSERT(n < (Storage.Size << 5)); UxImBitArrayClearBit(Storage.Data, n); }
};
IM_MSVC_RUNTIME_CHECKS_RESTORE

// Helper: UxImSpan<>
// Pointing to a span of data we don't own.
template<typename T>
struct UxImSpan
{
    T*                  Data;
    T*                  DataEnd;

    // Constructors, destructor
    inline UxImSpan()                                 { Data = DataEnd = NULL; }
    inline UxImSpan(T* data, int size)                { Data = data; DataEnd = data + size; }
    inline UxImSpan(T* data, T* data_end)             { Data = data; DataEnd = data_end; }

    inline void         set(T* data, int size)      { Data = data; DataEnd = data + size; }
    inline void         set(T* data, T* data_end)   { Data = data; DataEnd = data_end; }
    inline int          size() const                { return (int)(ptrdiff_t)(DataEnd - Data); }
    inline int          size_in_bytes() const       { return (int)(ptrdiff_t)(DataEnd - Data) * (int)sizeof(T); }
    inline T&           operator[](int i)           { T* p = Data + i; IM_ASSERT(p >= Data && p < DataEnd); return *p; }
    inline const T&     operator[](int i) const     { const T* p = Data + i; IM_ASSERT(p >= Data && p < DataEnd); return *p; }

    inline T*           begin()                     { return Data; }
    inline const T*     begin() const               { return Data; }
    inline T*           end()                       { return DataEnd; }
    inline const T*     end() const                 { return DataEnd; }

    // Utilities
    inline int  index_from_ptr(const T* it) const   { IM_ASSERT(it >= Data && it < DataEnd); const ptrdiff_t off = it - Data; return (int)off; }
};

// Helper: UxImSpanAllocator<>
// Facilitate storing multiple chunks into a single large block (the "arena")
// - Usage: call Reserve() N times, allocate GetArenaSizeInBytes() worth, pass it to SetArenaBasePtr(), call GetSpan() N times to retrieve the aligned ranges.
template<int CHUNKS>
struct UxImSpanAllocator
{
    char*   BasePtr;
    int     CurrOff;
    int     CurrIdx;
    int     Offsets[CHUNKS];
    int     Sizes[CHUNKS];

    UxImSpanAllocator()                               { memset(this, 0, sizeof(*this)); }
    inline void  Reserve(int n, size_t sz, int a=4) { IM_ASSERT(n == CurrIdx && n < CHUNKS); CurrOff = IM_MEMALIGN(CurrOff, a); Offsets[n] = CurrOff; Sizes[n] = (int)sz; CurrIdx++; CurrOff += (int)sz; }
    inline int   GetArenaSizeInBytes()              { return CurrOff; }
    inline void  SetArenaBasePtr(void* base_ptr)    { BasePtr = (char*)base_ptr; }
    inline void* GetSpanPtrBegin(int n)             { IM_ASSERT(n >= 0 && n < CHUNKS && CurrIdx == CHUNKS); return (void*)(BasePtr + Offsets[n]); }
    inline void* GetSpanPtrEnd(int n)               { IM_ASSERT(n >= 0 && n < CHUNKS && CurrIdx == CHUNKS); return (void*)(BasePtr + Offsets[n] + Sizes[n]); }
    template<typename T>
    inline void  GetSpan(int n, UxImSpan<T>* span)    { span->set((T*)GetSpanPtrBegin(n), (T*)GetSpanPtrEnd(n)); }
};

// Helper: UxImStableVector<>
// Allocating chunks of BLOCK_SIZE items. Objects pointers are never invalidated when growing, only by clear().
// Important: does not destruct anything!
// Implemented only the minimum set of functions we need for it.
template<typename T, int BLOCK_SIZE>
struct UxImStableVector
{
    int                 Size = 0;
    int                 Capacity = 0;
    UxImVector<T*>        Blocks;

    // Functions
    inline ~UxImStableVector()                        { for (T* block : Blocks) IM_FREE(block); }

    inline void         clear()                     { Size = Capacity = 0; Blocks.clear_delete(); }
    inline void         resize(int new_size)        { if (new_size > Capacity) reserve(new_size); Size = new_size; }
    inline void         reserve(int new_cap)
    {
        new_cap = IM_MEMALIGN(new_cap, BLOCK_SIZE);
        int old_count = Capacity / BLOCK_SIZE;
        int new_count = new_cap / BLOCK_SIZE;
        if (new_count <= old_count)
            return;
        Blocks.resize(new_count);
        for (int n = old_count; n < new_count; n++)
            Blocks[n] = (T*)IM_ALLOC(sizeof(T) * BLOCK_SIZE);
        Capacity = new_cap;
    }
    inline T&           operator[](int i)           { IM_ASSERT(i >= 0 && i < Size); return Blocks[i / BLOCK_SIZE][i % BLOCK_SIZE]; }
    inline const T&     operator[](int i) const     { IM_ASSERT(i >= 0 && i < Size); return Blocks[i / BLOCK_SIZE][i % BLOCK_SIZE]; }
    inline T*           push_back(const T& v)       { int i = Size; IM_ASSERT(i >= 0); if (Size == Capacity) reserve(Capacity + BLOCK_SIZE); void* ptr = &Blocks[i / BLOCK_SIZE][i % BLOCK_SIZE]; memcpy(ptr, &v, sizeof(v)); Size++; return (T*)ptr; }
};

// Helper: UxImPool<>
// Basic keyed storage for contiguous instances, slow/amortized insertion, O(1) indexable, O(Log N) queries by ID over a dense/hot buffer,
// Honor constructor/destructor. Add/remove invalidate all pointers. Indexes have the same lifetime as the associated object.
typedef int UxImPoolIdx;
template<typename T>
struct UxImPool
{
    UxImVector<T>     Buf;        // Contiguous data
    UxImGuiStorage    Map;        // ID->Index
    UxImPoolIdx       FreeIdx;    // Next free idx to use
    UxImPoolIdx       AliveCount; // Number of active/alive items (for display purpose)

    UxImPool()    { FreeIdx = AliveCount = 0; }
    ~UxImPool()   { Clear(); }
    T*          GetByKey(UxImGuiID key)               { int idx = Map.GetInt(key, -1); return (idx != -1) ? &Buf[idx] : NULL; }
    T*          GetByIndex(UxImPoolIdx n)             { return &Buf[n]; }
    UxImPoolIdx   GetIndex(const T* p) const          { IM_ASSERT(p >= Buf.Data && p < Buf.Data + Buf.Size); return (UxImPoolIdx)(p - Buf.Data); }
    T*          GetOrAddByKey(UxImGuiID key)          { int* p_idx = Map.GetIntRef(key, -1); if (*p_idx != -1) return &Buf[*p_idx]; *p_idx = FreeIdx; return Add(); }
    bool        Contains(const T* p) const          { return (p >= Buf.Data && p < Buf.Data + Buf.Size); }
    void        Clear()                             { for (int n = 0; n < Map.Data.Size; n++) { int idx = Map.Data[n].val_i; if (idx != -1) Buf[idx].~T(); } Map.Clear(); Buf.clear(); FreeIdx = AliveCount = 0; }
    T*          Add()                               { int idx = FreeIdx; if (idx == Buf.Size) { Buf.resize(Buf.Size + 1); FreeIdx++; } else { FreeIdx = *(int*)&Buf[idx]; } IM_PLACEMENT_NEW(&Buf[idx]) T(); AliveCount++; return &Buf[idx]; }
    void        Remove(UxImGuiID key, const T* p)     { Remove(key, GetIndex(p)); }
    void        Remove(UxImGuiID key, UxImPoolIdx idx)  { Buf[idx].~T(); *(int*)&Buf[idx] = FreeIdx; FreeIdx = idx; Map.SetInt(key, -1); AliveCount--; }
    void        Reserve(int capacity)               { Buf.reserve(capacity); Map.Data.reserve(capacity); }

    // To iterate a UxImPool: for (int n = 0; n < pool.GetMapSize(); n++) if (T* t = pool.TryGetMapData(n)) { ... }
    // Can be avoided if you know .Remove() has never been called on the pool, or AliveCount == GetMapSize()
    int         GetAliveCount() const               { return AliveCount; }      // Number of active/alive items in the pool (for display purpose)
    int         GetBufSize() const                  { return Buf.Size; }
    int         GetMapSize() const                  { return Map.Data.Size; }   // It is the map we need iterate to find valid items, since we don't have "alive" storage anywhere
    T*          TryGetMapData(UxImPoolIdx n)          { int idx = Map.Data[n].val_i; if (idx == -1) return NULL; return GetByIndex(idx); }
};

// Helper: UxImChunkStream<>
// Build and iterate a contiguous stream of variable-sized structures.
// This is used by Settings to store persistent data while reducing allocation count.
// We store the chunk size first, and align the final size on 4 bytes boundaries.
// The tedious/zealous amount of casting is to avoid -Wcast-align warnings.
template<typename T>
struct UxImChunkStream
{
    UxImVector<char>  Buf;

    void    clear()                     { Buf.clear(); }
    bool    empty() const               { return Buf.Size == 0; }
    int     size() const                { return Buf.Size; }
    T*      alloc_chunk(size_t sz)      { size_t HDR_SZ = 4; sz = IM_MEMALIGN(HDR_SZ + sz, 4u); int off = Buf.Size; Buf.resize(off + (int)sz); ((int*)(void*)(Buf.Data + off))[0] = (int)sz; return (T*)(void*)(Buf.Data + off + (int)HDR_SZ); }
    T*      begin()                     { size_t HDR_SZ = 4; if (!Buf.Data) return NULL; return (T*)(void*)(Buf.Data + HDR_SZ); }
    T*      next_chunk(T* p)            { size_t HDR_SZ = 4; IM_ASSERT(p >= begin() && p < end()); p = (T*)(void*)((char*)(void*)p + chunk_size(p)); if (p == (T*)(void*)((char*)end() + HDR_SZ)) return (T*)0; IM_ASSERT(p < end()); return p; }
    int     chunk_size(const T* p)      { return ((const int*)p)[-1]; }
    T*      end()                       { return (T*)(void*)(Buf.Data + Buf.Size); }
    int     offset_from_ptr(const T* p) { IM_ASSERT(p >= begin() && p < end()); const ptrdiff_t off = (const char*)p - Buf.Data; return (int)off; }
    T*      ptr_from_offset(int off)    { IM_ASSERT(off >= 4 && off < Buf.Size); return (T*)(void*)(Buf.Data + off); }
    void    swap(UxImChunkStream<T>& rhs) { rhs.Buf.swap(Buf); }
};

// Helper: UxImGuiTextIndex
// Maintain a line index for a text buffer. This is a strong candidate to be moved into the public API.
struct UxImGuiTextIndex
{
    UxImVector<int>   LineOffsets;
    int             EndOffset = 0;                          // Because we don't own text buffer we need to maintain EndOffset (may bake in LineOffsets?)

    void            clear()                                 { LineOffsets.clear(); EndOffset = 0; }
    int             size()                                  { return LineOffsets.Size; }
    const char*     get_line_begin(const char* base, int n) { return base + LineOffsets[n]; }
    const char*     get_line_end(const char* base, int n)   { return base + (n + 1 < LineOffsets.Size ? (LineOffsets[n + 1] - 1) : EndOffset); }
    void            append(const char* base, int old_size, int new_size);
};

// Helper: UxImGuiStorage
IMGUI_API UxImGuiStoragePair* UxImLowerBound(UxImGuiStoragePair* in_begin, UxImGuiStoragePair* in_end, UxImGuiID key);

//-----------------------------------------------------------------------------
// [SECTION] UxImDrawList support
//-----------------------------------------------------------------------------

// UxImDrawList: Helper function to calculate a circle's segment count given its radius and a "maximum error" value.
// Estimation of number of circle segment based on error is derived using method described in https://stackoverflow.com/a/2244088/15194693
// Number of segments (N) is calculated using equation:
//   N = ceil ( pi / acos(1 - error / r) )     where r > 0, error <= r
// Our equation is significantly simpler that one in the post thanks for choosing segment that is
// perpendicular to X axis. Follow steps in the article from this starting condition and you will
// will get this result.
//
// Rendering circles with an odd number of segments, while mathematically correct will produce
// asymmetrical results on the raster grid. Therefore we're rounding N to next even number (7->8, 8->8, 9->10 etc.)
#define IM_ROUNDUP_TO_EVEN(_V)                                  ((((_V) + 1) / 2) * 2)
#define IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_MIN                     4
#define IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_MAX                     512
#define IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC(_RAD,_MAXERROR)    UxImClamp(IM_ROUNDUP_TO_EVEN((int)UxImCeil(IM_PI / UxImAcos(1 - UxImMin((_MAXERROR), (_RAD)) / (_RAD)))), IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_MIN, IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_MAX)

// Raw equation from IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC rewritten for 'r' and 'error'.
#define IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC_R(_N,_MAXERROR)    ((_MAXERROR) / (1 - UxImCos(IM_PI / UxImMax((float)(_N), IM_PI))))
#define IM_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC_ERROR(_N,_RAD)     ((1 - UxImCos(IM_PI / UxImMax((float)(_N), IM_PI))) / (_RAD))

// UxImDrawList: Lookup table size for adaptive arc drawing, cover full circle.
#ifndef IM_DRAWLIST_ARCFAST_TABLE_SIZE
#define IM_DRAWLIST_ARCFAST_TABLE_SIZE                          48 // Number of samples in lookup table.
#endif
#define IM_DRAWLIST_ARCFAST_SAMPLE_MAX                          IM_DRAWLIST_ARCFAST_TABLE_SIZE // Sample index _PathArcToFastEx() for 360 angle.

// Data shared between all UxImDrawList instances
// Conceptually this could have been called e.g. UxImDrawListSharedContext
// Typically one UxImGui context would create and maintain one of this.
// You may want to create your own instance of you try to UxImDrawList completely without UxImGui. In that case, watch out for future changes to this structure.
struct IMGUI_API UxImDrawListSharedData
{
    UxImVec2          TexUvWhitePixel;            // UV of white pixel in the atlas (== FontAtlas->TexUvWhitePixel)
    const UxImVec4*   TexUvLines;                 // UV of anti-aliased lines in the atlas (== FontAtlas->TexUvLines)
    UxImFont*         Font;                       // Current/default font (optional, for simplified AddText overload)
    float           FontSize;                   // Current/default font size (optional, for simplified AddText overload)
    float           FontScale;                  // Current/default font scale (== FontSize / Font->FontSize)
    float           CurveTessellationTol;       // Tessellation tolerance when using PathBezierCurveTo()
    float           CircleSegmentMaxError;      // Number of circle segments to use per pixel of radius for AddCircle() etc
    float           InitialFringeScale;         // Initial scale to apply to AA fringe
    UxImDrawListFlags InitialFlags;               // Initial flags at the beginning of the frame (it is possible to alter flags on a per-drawlist basis afterwards)
    UxImVec4          ClipRectFullscreen;         // Value for PushClipRectFullscreen()
    UxImVector<UxImVec2> TempBuffer;                // Temporary write buffer
    UxImVector<UxImDrawList*> DrawLists;            // All draw lists associated to this UxImDrawListSharedData
    UxImGuiContext*   Context;                    // [OPTIONAL] Link to Dear UxImGui context. 99% of UxImDrawList/UxImFontAtlas can function without an UxImGui context, but this facilitate handling one legacy edge case.

    // Lookup tables
    UxImVec2          ArcFastVtx[IM_DRAWLIST_ARCFAST_TABLE_SIZE]; // Sample points on the quarter of the circle.
    float           ArcFastRadiusCutoff;                        // Cutoff radius after which arc drawing will fallback to slower PathArcTo()
    UxImU8            CircleSegmentCounts[64];    // Precomputed segment count for given radius before we calculate it dynamically (to avoid calculation overhead)

    UxImDrawListSharedData();
    ~UxImDrawListSharedData();
    void SetCircleTessellationMaxError(float max_error);
};

struct UxImDrawDataBuilder
{
    UxImVector<UxImDrawList*>*  Layers[2];      // Pointers to global layers for: regular, tooltip. LayersP[0] is owned by DrawData.
    UxImVector<UxImDrawList*>   LayerData1;

    UxImDrawDataBuilder()                     { memset(this, 0, sizeof(*this)); }
};

struct UxImFontStackData
{
    UxImFont*     Font;
    float       FontSize;
};

//-----------------------------------------------------------------------------
// [SECTION] Style support
//-----------------------------------------------------------------------------

struct UxImGuiStyleVarInfo
{
    UxImU32           Count : 8;      // 1+
    UxImGuiDataType   DataType : 8;
    UxImU32           Offset : 16;    // Offset in parent structure
    void* GetVarPtr(void* parent) const { return (void*)((unsigned char*)parent + Offset); }
};

// Stacked color modifier, backup of modified data so we can restore it
struct UxImGuiColorMod
{
    UxImGuiCol        Col;
    UxImVec4          BackupValue;
};

// Stacked style modifier, backup of modified data so we can restore it. Data type inferred from the variable.
struct UxImGuiStyleMod
{
    UxImGuiStyleVar   VarIdx;
    union           { int BackupInt[2]; float BackupFloat[2]; };
    UxImGuiStyleMod(UxImGuiStyleVar idx, int v)     { VarIdx = idx; BackupInt[0] = v; }
    UxImGuiStyleMod(UxImGuiStyleVar idx, float v)   { VarIdx = idx; BackupFloat[0] = v; }
    UxImGuiStyleMod(UxImGuiStyleVar idx, UxImVec2 v)  { VarIdx = idx; BackupFloat[0] = v.x; BackupFloat[1] = v.y; }
};

//-----------------------------------------------------------------------------
// [SECTION] Data types support
//-----------------------------------------------------------------------------

struct UxImGuiDataTypeStorage
{
    UxImU8        Data[8];        // Opaque storage to fit any data up to UxImGuiDataType_COUNT
};

// Type information associated to one UxImGuiDataType. Retrieve with DataTypeGetInfo().
struct UxImGuiDataTypeInfo
{
    size_t      Size;           // Size in bytes
    const char* Name;           // Short descriptive name for the type, for debugging
    const char* PrintFmt;       // Default printf format for the type
    const char* ScanFmt;        // Default scanf format for the type
};

// Extend UxImGuiDataType_
enum UxImGuiDataTypePrivate_
{
    UxImGuiDataType_Pointer = UxImGuiDataType_COUNT,
    UxImGuiDataType_ID,
};

//-----------------------------------------------------------------------------
// [SECTION] Widgets support: flags, enums, data structures
//-----------------------------------------------------------------------------

// Extend UxImGuiItemFlags
// - input: PushItemFlag() manipulates g.CurrentItemFlags, g.NextItemData.ItemFlags, ItemAdd() calls may add extra flags too.
// - output: stored in g.LastItemData.ItemFlags
enum UxImGuiItemFlagsPrivate_
{
    // Controlled by user
    UxImGuiItemFlags_Disabled                 = 1 << 10, // false     // Disable interactions (DOES NOT affect visuals. DO NOT mix direct use of this with BeginDisabled(). See BeginDisabled()/EndDisabled() for full disable feature, and github #211).
    UxImGuiItemFlags_ReadOnly                 = 1 << 11, // false     // [ALPHA] Allow hovering interactions but underlying value is not changed.
    UxImGuiItemFlags_MixedValue               = 1 << 12, // false     // [BETA] Represent a mixed/indeterminate value, generally multi-selection where values differ. Currently only supported by Checkbox() (later should support all sorts of widgets)
    UxImGuiItemFlags_NoWindowHoverableCheck   = 1 << 13, // false     // Disable hoverable check in ItemHoverable()
    UxImGuiItemFlags_AllowOverlap             = 1 << 14, // false     // Allow being overlapped by another widget. Not-hovered to Hovered transition deferred by a frame.
    UxImGuiItemFlags_NoNavDisableMouseHover   = 1 << 15, // false     // Nav keyboard/gamepad mode doesn't disable hover highlight (behave as if NavHighlightItemUnderNav==false).
    UxImGuiItemFlags_NoMarkEdited             = 1 << 16, // false     // Skip calling MarkItemEdited()

    // Controlled by widget code
    UxImGuiItemFlags_Inputable                = 1 << 20, // false     // [WIP] Auto-activate input mode when tab focused. Currently only used and supported by a few items before it becomes a generic feature.
    UxImGuiItemFlags_HasSelectionUserData     = 1 << 21, // false     // Set by SetNextItemSelectionUserData()
    UxImGuiItemFlags_IsMultiSelect            = 1 << 22, // false     // Set by SetNextItemSelectionUserData()

    UxImGuiItemFlags_Default_                 = UxImGuiItemFlags_AutoClosePopups,    // Please don't change, use PushItemFlag() instead.

    // Obsolete
    //UxImGuiItemFlags_SelectableDontClosePopup = !UxImGuiItemFlags_AutoClosePopups, // Can't have a redirect as we inverted the behavior
};

// Status flags for an already submitted item
// - output: stored in g.LastItemData.StatusFlags
enum UxImGuiItemStatusFlags_
{
    UxImGuiItemStatusFlags_None               = 0,
    UxImGuiItemStatusFlags_HoveredRect        = 1 << 0,   // Mouse position is within item rectangle (does NOT mean that the window is in correct z-order and can be hovered!, this is only one part of the most-common IsItemHovered test)
    UxImGuiItemStatusFlags_HasDisplayRect     = 1 << 1,   // g.LastItemData.DisplayRect is valid
    UxImGuiItemStatusFlags_Edited             = 1 << 2,   // Value exposed by item was edited in the current frame (should match the bool return value of most widgets)
    UxImGuiItemStatusFlags_ToggledSelection   = 1 << 3,   // Set when Selectable(), TreeNode() reports toggling a selection. We can't report "Selected", only state changes, in order to easily handle clipping with less issues.
    UxImGuiItemStatusFlags_ToggledOpen        = 1 << 4,   // Set when TreeNode() reports toggling their open state.
    UxImGuiItemStatusFlags_HasDeactivated     = 1 << 5,   // Set if the widget/group is able to provide data for the UxImGuiItemStatusFlags_Deactivated flag.
    UxImGuiItemStatusFlags_Deactivated        = 1 << 6,   // Only valid if UxImGuiItemStatusFlags_HasDeactivated is set.
    UxImGuiItemStatusFlags_HoveredWindow      = 1 << 7,   // Override the HoveredWindow test to allow cross-window hover testing.
    UxImGuiItemStatusFlags_Visible            = 1 << 8,   // [WIP] Set when item is overlapping the current clipping rectangle (Used internally. Please don't use yet: API/system will change as we refactor Itemadd()).
    UxImGuiItemStatusFlags_HasClipRect        = 1 << 9,   // g.LastItemData.ClipRect is valid.
    UxImGuiItemStatusFlags_HasShortcut        = 1 << 10,  // g.LastItemData.Shortcut valid. Set by SetNextItemShortcut() -> ItemAdd().

    // Additional status + semantic for UxImGuiTestEngine
#ifdef IMGUI_ENABLE_TEST_ENGINE
    UxImGuiItemStatusFlags_Openable           = 1 << 20,  // Item is an openable (e.g. TreeNode)
    UxImGuiItemStatusFlags_Opened             = 1 << 21,  // Opened status
    UxImGuiItemStatusFlags_Checkable          = 1 << 22,  // Item is a checkable (e.g. CheckBox, MenuItem)
    UxImGuiItemStatusFlags_Checked            = 1 << 23,  // Checked status
    UxImGuiItemStatusFlags_Inputable          = 1 << 24,  // Item is a text-inputable (e.g. InputText, SliderXXX, DragXXX)
#endif
};

// Extend UxImGuiHoveredFlags_
enum UxImGuiHoveredFlagsPrivate_
{
    UxImGuiHoveredFlags_DelayMask_                    = UxImGuiHoveredFlags_DelayNone | UxImGuiHoveredFlags_DelayShort | UxImGuiHoveredFlags_DelayNormal | UxImGuiHoveredFlags_NoSharedDelay,
    UxImGuiHoveredFlags_AllowedMaskForIsWindowHovered = UxImGuiHoveredFlags_ChildWindows | UxImGuiHoveredFlags_RootWindow | UxImGuiHoveredFlags_AnyWindow | UxImGuiHoveredFlags_NoPopupHierarchy | UxImGuiHoveredFlags_DockHierarchy | UxImGuiHoveredFlags_AllowWhenBlockedByPopup | UxImGuiHoveredFlags_AllowWhenBlockedByActiveItem | UxImGuiHoveredFlags_ForTooltip | UxImGuiHoveredFlags_Stationary,
    UxImGuiHoveredFlags_AllowedMaskForIsItemHovered   = UxImGuiHoveredFlags_AllowWhenBlockedByPopup | UxImGuiHoveredFlags_AllowWhenBlockedByActiveItem | UxImGuiHoveredFlags_AllowWhenOverlapped | UxImGuiHoveredFlags_AllowWhenDisabled | UxImGuiHoveredFlags_NoNavOverride | UxImGuiHoveredFlags_ForTooltip | UxImGuiHoveredFlags_Stationary | UxImGuiHoveredFlags_DelayMask_,
};

// Extend UxImGuiInputTextFlags_
enum UxImGuiInputTextFlagsPrivate_
{
    // [Internal]
    UxImGuiInputTextFlags_Multiline           = 1 << 26,  // For internal use by InputTextMultiline()
    UxImGuiInputTextFlags_MergedItem          = 1 << 27,  // For internal use by TempInputText(), will skip calling ItemAdd(). Require bounding-box to strictly match.
    UxImGuiInputTextFlags_LocalizeDecimalPoint= 1 << 28,  // For internal use by InputScalar() and TempInputScalar()
};

// Extend UxImGuiButtonFlags_
enum UxImGuiButtonFlagsPrivate_
{
    UxImGuiButtonFlags_PressedOnClick         = 1 << 4,   // return true on click (mouse down event)
    UxImGuiButtonFlags_PressedOnClickRelease  = 1 << 5,   // [Default] return true on click + release on same item <-- this is what the majority of Button are using
    UxImGuiButtonFlags_PressedOnClickReleaseAnywhere = 1 << 6, // return true on click + release even if the release event is not done while hovering the item
    UxImGuiButtonFlags_PressedOnRelease       = 1 << 7,   // return true on release (default requires click+release)
    UxImGuiButtonFlags_PressedOnDoubleClick   = 1 << 8,   // return true on double-click (default requires click+release)
    UxImGuiButtonFlags_PressedOnDragDropHold  = 1 << 9,   // return true when held into while we are drag and dropping another item (used by e.g. tree nodes, collapsing headers)
    //UxImGuiButtonFlags_Repeat               = 1 << 10,  // hold to repeat -> use UxImGuiItemFlags_ButtonRepeat instead.
    UxImGuiButtonFlags_FlattenChildren        = 1 << 11,  // allow interactions even if a child window is overlapping
    UxImGuiButtonFlags_AllowOverlap           = 1 << 12,  // require previous frame HoveredId to either match id or be null before being usable.
    //UxImGuiButtonFlags_DontClosePopups      = 1 << 13,  // disable automatically closing parent popup on press
    //UxImGuiButtonFlags_Disabled             = 1 << 14,  // disable interactions -> use BeginDisabled() or UxImGuiItemFlags_Disabled
    UxImGuiButtonFlags_AlignTextBaseLine      = 1 << 15,  // vertically align button to match text baseline - ButtonEx() only // FIXME: Should be removed and handled by SmallButton(), not possible currently because of DC.CursorPosPrevLine
    UxImGuiButtonFlags_NoKeyModsAllowed       = 1 << 16,  // disable mouse interaction if a key modifier is held
    UxImGuiButtonFlags_NoHoldingActiveId      = 1 << 17,  // don't set ActiveId while holding the mouse (UxImGuiButtonFlags_PressedOnClick only)
    UxImGuiButtonFlags_NoNavFocus             = 1 << 18,  // don't override navigation focus when activated (FIXME: this is essentially used every time an item uses UxImGuiItemFlags_NoNav, but because legacy specs don't requires LastItemData to be set ButtonBehavior(), we can't poll g.LastItemData.ItemFlags)
    UxImGuiButtonFlags_NoHoveredOnFocus       = 1 << 19,  // don't report as hovered when nav focus is on this item
    UxImGuiButtonFlags_NoSetKeyOwner          = 1 << 20,  // don't set key/input owner on the initial click (note: mouse buttons are keys! often, the key in question will be UxImGuiKey_MouseLeft!)
    UxImGuiButtonFlags_NoTestKeyOwner         = 1 << 21,  // don't test key/input owner when polling the key (note: mouse buttons are keys! often, the key in question will be UxImGuiKey_MouseLeft!)
    UxImGuiButtonFlags_PressedOnMask_         = UxImGuiButtonFlags_PressedOnClick | UxImGuiButtonFlags_PressedOnClickRelease | UxImGuiButtonFlags_PressedOnClickReleaseAnywhere | UxImGuiButtonFlags_PressedOnRelease | UxImGuiButtonFlags_PressedOnDoubleClick | UxImGuiButtonFlags_PressedOnDragDropHold,
    UxImGuiButtonFlags_PressedOnDefault_      = UxImGuiButtonFlags_PressedOnClickRelease,
};

// Extend UxImGuiComboFlags_
enum UxImGuiComboFlagsPrivate_
{
    UxImGuiComboFlags_CustomPreview           = 1 << 20,  // enable BeginComboPreview()
};

// Extend UxImGuiSliderFlags_
enum UxImGuiSliderFlagsPrivate_
{
    UxImGuiSliderFlags_Vertical               = 1 << 20,  // Should this slider be orientated vertically?
    UxImGuiSliderFlags_ReadOnly               = 1 << 21,  // Consider using g.NextItemData.ItemFlags |= UxImGuiItemFlags_ReadOnly instead.
};

// Extend UxImGuiSelectableFlags_
enum UxImGuiSelectableFlagsPrivate_
{
    // NB: need to be in sync with last value of UxImGuiSelectableFlags_
    UxImGuiSelectableFlags_NoHoldingActiveID      = 1 << 20,
    UxImGuiSelectableFlags_SelectOnNav            = 1 << 21,  // (WIP) Auto-select when moved into. This is not exposed in public API as to handle multi-select and modifiers we will need user to explicitly control focus scope. May be replaced with a BeginSelection() API.
    UxImGuiSelectableFlags_SelectOnClick          = 1 << 22,  // Override button behavior to react on Click (default is Click+Release)
    UxImGuiSelectableFlags_SelectOnRelease        = 1 << 23,  // Override button behavior to react on Release (default is Click+Release)
    UxImGuiSelectableFlags_SpanAvailWidth         = 1 << 24,  // Span all avail width even if we declared less for layout purpose. FIXME: We may be able to remove this (added in 6251d379, 2bcafc86 for menus)
    UxImGuiSelectableFlags_SetNavIdOnHover        = 1 << 25,  // Set Nav/Focus ID on mouse hover (used by MenuItem)
    UxImGuiSelectableFlags_NoPadWithHalfSpacing   = 1 << 26,  // Disable padding each side with ItemSpacing * 0.5f
    UxImGuiSelectableFlags_NoSetKeyOwner          = 1 << 27,  // Don't set key/input owner on the initial click (note: mouse buttons are keys! often, the key in question will be UxImGuiKey_MouseLeft!)
};

// Extend UxImGuiTreeNodeFlags_
enum UxImGuiTreeNodeFlagsPrivate_
{
    UxImGuiTreeNodeFlags_NoNavFocus                 = 1 << 27,// Don't claim nav focus when interacting with this item (#8551)
    UxImGuiTreeNodeFlags_ClipLabelForTrailingButton = 1 << 28,// FIXME-WIP: Hard-coded for CollapsingHeader()
    UxImGuiTreeNodeFlags_UpsideDownArrow            = 1 << 29,// FIXME-WIP: Turn Down arrow into an Up arrow, for reversed trees (#6517)
    UxImGuiTreeNodeFlags_OpenOnMask_                = UxImGuiTreeNodeFlags_OpenOnDoubleClick | UxImGuiTreeNodeFlags_OpenOnArrow,
    UxImGuiTreeNodeFlags_DrawLinesMask_             = UxImGuiTreeNodeFlags_DrawLinesNone | UxImGuiTreeNodeFlags_DrawLinesFull | UxImGuiTreeNodeFlags_DrawLinesToNodes,
};

enum UxImGuiSeparatorFlags_
{
    UxImGuiSeparatorFlags_None                    = 0,
    UxImGuiSeparatorFlags_Horizontal              = 1 << 0,   // Axis default to current layout type, so generally Horizontal unless e.g. in a menu bar
    UxImGuiSeparatorFlags_Vertical                = 1 << 1,
    UxImGuiSeparatorFlags_SpanAllColumns          = 1 << 2,   // Make separator cover all columns of a legacy Columns() set.
};

// Flags for FocusWindow(). This is not called UxImGuiFocusFlags to avoid confusion with public-facing UxImGuiFocusedFlags.
// FIXME: Once we finishing replacing more uses of GetTopMostPopupModal()+IsWindowWithinBeginStackOf()
// and FindBlockingModal() with this, we may want to change the flag to be opt-out instead of opt-in.
enum UxImGuiFocusRequestFlags_
{
    UxImGuiFocusRequestFlags_None                 = 0,
    UxImGuiFocusRequestFlags_RestoreFocusedChild  = 1 << 0,   // Find last focused child (if any) and focus it instead.
    UxImGuiFocusRequestFlags_UnlessBelowModal     = 1 << 1,   // Do not set focus if the window is below a modal.
};

enum UxImGuiTextFlags_
{
    UxImGuiTextFlags_None                         = 0,
    UxImGuiTextFlags_NoWidthForLargeClippedText   = 1 << 0,
};

enum UxImGuiTooltipFlags_
{
    UxImGuiTooltipFlags_None                      = 0,
    UxImGuiTooltipFlags_OverridePrevious          = 1 << 1,   // Clear/ignore previously submitted tooltip (defaults to append)
};

// FIXME: this is in development, not exposed/functional as a generic feature yet.
// Horizontal/Vertical enums are fixed to 0/1 so they may be used to index UxImVec2
enum UxImGuiLayoutType_
{
    UxImGuiLayoutType_Horizontal = 0,
    UxImGuiLayoutType_Vertical = 1
};

// Flags for LogBegin() text capturing function
enum UxImGuiLogFlags_
{
    UxImGuiLogFlags_None = 0,

    UxImGuiLogFlags_OutputTTY         = 1 << 0,
    UxImGuiLogFlags_OutputFile        = 1 << 1,
    UxImGuiLogFlags_OutputBuffer      = 1 << 2,
    UxImGuiLogFlags_OutputClipboard   = 1 << 3,
    UxImGuiLogFlags_OutputMask_       = UxImGuiLogFlags_OutputTTY | UxImGuiLogFlags_OutputFile | UxImGuiLogFlags_OutputBuffer | UxImGuiLogFlags_OutputClipboard,
};

// X/Y enums are fixed to 0/1 so they may be used to index UxImVec2
enum UxImGuiAxis
{
    UxImGuiAxis_None = -1,
    UxImGuiAxis_X = 0,
    UxImGuiAxis_Y = 1
};

enum UxImGuiPlotType
{
    UxImGuiPlotType_Lines,
    UxImGuiPlotType_Histogram,
};

// Storage data for BeginComboPreview()/EndComboPreview()
struct IMGUI_API UxImGuiComboPreviewData
{
    UxImRect          PreviewRect;
    UxImVec2          BackupCursorPos;
    UxImVec2          BackupCursorMaxPos;
    UxImVec2          BackupCursorPosPrevLine;
    float           BackupPrevLineTextBaseOffset;
    UxImGuiLayoutType BackupLayout;

    UxImGuiComboPreviewData() { memset(this, 0, sizeof(*this)); }
};

// Stacked storage data for BeginGroup()/EndGroup()
struct IMGUI_API UxImGuiGroupData
{
    UxImGuiID     WindowID;
    UxImVec2      BackupCursorPos;
    UxImVec2      BackupCursorMaxPos;
    UxImVec2      BackupCursorPosPrevLine;
    UxImVec1      BackupIndent;
    UxImVec1      BackupGroupOffset;
    UxImVec2      BackupCurrLineSize;
    float       BackupCurrLineTextBaseOffset;
    UxImGuiID     BackupActiveIdIsAlive;
    bool        BackupDeactivatedIdIsAlive;
    bool        BackupHoveredIdIsAlive;
    bool        BackupIsSameLine;
    bool        EmitItem;
};

// Simple column measurement, currently used for MenuItem() only.. This is very short-sighted/throw-away code and NOT a generic helper.
struct IMGUI_API UxImGuiMenuColumns
{
    UxImU32       TotalWidth;
    UxImU32       NextTotalWidth;
    UxImU16       Spacing;
    UxImU16       OffsetIcon;         // Always zero for now
    UxImU16       OffsetLabel;        // Offsets are locked in Update()
    UxImU16       OffsetShortcut;
    UxImU16       OffsetMark;
    UxImU16       Widths[4];          // Width of:   Icon, Label, Shortcut, Mark  (accumulators for current frame)

    UxImGuiMenuColumns() { memset(this, 0, sizeof(*this)); }
    void        Update(float spacing, bool window_reappearing);
    float       DeclColumns(float w_icon, float w_label, float w_shortcut, float w_mark);
    void        CalcNextTotalWidth(bool update_offsets);
};

// Internal temporary state for deactivating InputText() instances.
struct IMGUI_API UxImGuiInputTextDeactivatedState
{
    UxImGuiID            ID;              // widget id owning the text state (which just got deactivated)
    UxImVector<char>     TextA;           // text buffer

    UxImGuiInputTextDeactivatedState()    { memset(this, 0, sizeof(*this)); }
    void    ClearFreeMemory()           { ID = 0; TextA.clear(); }
};

// Forward declare imstb_textedit.h structure + make its main configuration define accessible
#undef IMSTB_TEXTEDIT_STRING
#undef IMSTB_TEXTEDIT_CHARTYPE
#define IMSTB_TEXTEDIT_STRING             UxImGuiInputTextState
#define IMSTB_TEXTEDIT_CHARTYPE           char
#define IMSTB_TEXTEDIT_GETWIDTH_NEWLINE   (-1.0f)
#define IMSTB_TEXTEDIT_UNDOSTATECOUNT     99
#define IMSTB_TEXTEDIT_UNDOCHARCOUNT      999
namespace UxImStb { struct STB_TexteditState; }
typedef UxImStb::STB_TexteditState UxImStbTexteditState;

// Internal state of the currently focused/edited text input box
// For a given item ID, access with UxImGui::GetInputTextState()
struct IMGUI_API UxImGuiInputTextState
{
    UxImGuiContext*           Ctx;                    // parent UI context (needs to be set explicitly by parent).
    UxImStbTexteditState*     Stb;                    // State for stb_textedit.h
    UxImGuiInputTextFlags     Flags;                  // copy of InputText() flags. may be used to check if e.g. UxImGuiInputTextFlags_Password is set.
    UxImGuiID                 ID;                     // widget id owning the text state
    int                     TextLen;                // UTF-8 length of the string in TextA (in bytes)
    const char*             TextSrc;                // == TextA.Data unless read-only, in which case == buf passed to InputText(). Field only set and valid _inside_ the call InputText() call.
    UxImVector<char>          TextA;                  // main UTF8 buffer. TextA.Size is a buffer size! Should always be >= buf_size passed by user (and of course >= CurLenA + 1).
    UxImVector<char>          TextToRevertTo;         // value to revert to when pressing Escape = backup of end-user buffer at the time of focus (in UTF-8, unaltered)
    UxImVector<char>          CallbackTextBackup;     // temporary storage for callback to support automatic reconcile of undo-stack
    int                     BufCapacity;            // end-user buffer capacity (include zero terminator)
    UxImVec2                  Scroll;                 // horizontal offset (managed manually) + vertical scrolling (pulled from child window's own Scroll.y)
    float                   CursorAnim;             // timer for cursor blink, reset on every user action so the cursor reappears immediately
    bool                    CursorFollow;           // set when we want scrolling to follow the current cursor position (not always!)
    bool                    SelectedAllMouseLock;   // after a double-click to select all, we ignore further mouse drags to update selection
    bool                    Edited;                 // edited this frame
    bool                    WantReloadUserBuf;      // force a reload of user buf so it may be modified externally. may be automatic in future version.
    int                     ReloadSelectionStart;
    int                     ReloadSelectionEnd;

    UxImGuiInputTextState();
    ~UxImGuiInputTextState();
    void        ClearText()                 { TextLen = 0; TextA[0] = 0; CursorClamp(); }
    void        ClearFreeMemory()           { TextA.clear(); TextToRevertTo.clear(); }
    void        OnKeyPressed(int key);      // Cannot be inline because we call in code in stb_textedit.h implementation
    void        OnCharPressed(unsigned int c);

    // Cursor & Selection
    void        CursorAnimReset();
    void        CursorClamp();
    bool        HasSelection() const;
    void        ClearSelection();
    int         GetCursorPos() const;
    int         GetSelectionStart() const;
    int         GetSelectionEnd() const;
    void        SelectAll();

    // Reload user buf (WIP #2890)
    // If you modify underlying user-passed const char* while active you need to call this (InputText V2 may lift this)
    //   strcpy(my_buf, "hello");
    //   if (UxImGuiInputTextState* state = UxImGui::GetInputTextState(id)) // id may be UxImGui::GetItemID() is last item
    //       state->ReloadUserBufAndSelectAll();
    void        ReloadUserBufAndSelectAll();
    void        ReloadUserBufAndKeepSelection();
    void        ReloadUserBufAndMoveToEnd();
};

enum UxImGuiWindowRefreshFlags_
{
    UxImGuiWindowRefreshFlags_None                = 0,
    UxImGuiWindowRefreshFlags_TryToAvoidRefresh   = 1 << 0,   // [EXPERIMENTAL] Try to keep existing contents, USER MUST NOT HONOR BEGIN() RETURNING FALSE AND NOT APPEND.
    UxImGuiWindowRefreshFlags_RefreshOnHover      = 1 << 1,   // [EXPERIMENTAL] Always refresh on hover
    UxImGuiWindowRefreshFlags_RefreshOnFocus      = 1 << 2,   // [EXPERIMENTAL] Always refresh on focus
    // Refresh policy/frequency, Load Balancing etc.
};

enum UxImGuiNextWindowDataFlags_
{
    UxImGuiNextWindowDataFlags_None               = 0,
    UxImGuiNextWindowDataFlags_HasPos             = 1 << 0,
    UxImGuiNextWindowDataFlags_HasSize            = 1 << 1,
    UxImGuiNextWindowDataFlags_HasContentSize     = 1 << 2,
    UxImGuiNextWindowDataFlags_HasCollapsed       = 1 << 3,
    UxImGuiNextWindowDataFlags_HasSizeConstraint  = 1 << 4,
    UxImGuiNextWindowDataFlags_HasFocus           = 1 << 5,
    UxImGuiNextWindowDataFlags_HasBgAlpha         = 1 << 6,
    UxImGuiNextWindowDataFlags_HasScroll          = 1 << 7,
    UxImGuiNextWindowDataFlags_HasWindowFlags     = 1 << 8,
    UxImGuiNextWindowDataFlags_HasChildFlags      = 1 << 9,
    UxImGuiNextWindowDataFlags_HasRefreshPolicy   = 1 << 10,
    UxImGuiNextWindowDataFlags_HasViewport        = 1 << 11,
    UxImGuiNextWindowDataFlags_HasDock            = 1 << 12,
    UxImGuiNextWindowDataFlags_HasWindowClass     = 1 << 13,
};

// Storage for SetNexWindow** functions
struct UxImGuiNextWindowData
{
    UxImGuiNextWindowDataFlags    HasFlags;

    // Members below are NOT cleared. Always rely on HasFlags.
    UxImGuiCond                   PosCond;
    UxImGuiCond                   SizeCond;
    UxImGuiCond                   CollapsedCond;
    UxImGuiCond                   DockCond;
    UxImVec2                      PosVal;
    UxImVec2                      PosPivotVal;
    UxImVec2                      SizeVal;
    UxImVec2                      ContentSizeVal;
    UxImVec2                      ScrollVal;
    UxImGuiWindowFlags            WindowFlags;            // Only honored by BeginTable()
    UxImGuiChildFlags             ChildFlags;
    bool                        PosUndock;
    bool                        CollapsedVal;
    UxImRect                      SizeConstraintRect;
    UxImGuiSizeCallback           SizeCallback;
    void*                       SizeCallbackUserData;
    float                       BgAlphaVal;             // Override background alpha
    UxImGuiID                     ViewportId;
    UxImGuiID                     DockId;
    UxImGuiWindowClass            WindowClass;
    UxImVec2                      MenuBarOffsetMinVal;    // (Always on) This is not exposed publicly, so we don't clear it and it doesn't have a corresponding flag (could we? for consistency?)
    UxImGuiWindowRefreshFlags     RefreshFlagsVal;

    UxImGuiNextWindowData()       { memset(this, 0, sizeof(*this)); }
    inline void ClearFlags()    { HasFlags = UxImGuiNextWindowDataFlags_None; }
};

enum UxImGuiNextItemDataFlags_
{
    UxImGuiNextItemDataFlags_None         = 0,
    UxImGuiNextItemDataFlags_HasWidth     = 1 << 0,
    UxImGuiNextItemDataFlags_HasOpen      = 1 << 1,
    UxImGuiNextItemDataFlags_HasShortcut  = 1 << 2,
    UxImGuiNextItemDataFlags_HasRefVal    = 1 << 3,
    UxImGuiNextItemDataFlags_HasStorageID = 1 << 4,
};

struct UxImGuiNextItemData
{
    UxImGuiNextItemDataFlags      HasFlags;           // Called HasFlags instead of Flags to avoid mistaking this
    UxImGuiItemFlags              ItemFlags;          // Currently only tested/used for UxImGuiItemFlags_AllowOverlap and UxImGuiItemFlags_HasSelectionUserData.

    // Members below are NOT cleared by ItemAdd() meaning they are still valid during e.g. NavProcessItem(). Always rely on HasFlags.
    UxImGuiID                     FocusScopeId;       // Set by SetNextItemSelectionUserData()
    UxImGuiSelectionUserData      SelectionUserData;  // Set by SetNextItemSelectionUserData() (note that NULL/0 is a valid value, we use -1 == UxImGuiSelectionUserData_Invalid to mark invalid values)
    float                       Width;              // Set by SetNextItemWidth()
    UxImGuiKeyChord               Shortcut;           // Set by SetNextItemShortcut()
    UxImGuiInputFlags             ShortcutFlags;      // Set by SetNextItemShortcut()
    bool                        OpenVal;            // Set by SetNextItemOpen()
    UxImU8                        OpenCond;           // Set by SetNextItemOpen()
    UxImGuiDataTypeStorage        RefVal;             // Not exposed yet, for UxImGuiInputTextFlags_ParseEmptyAsRefVal
    UxImGuiID                     StorageId;          // Set by SetNextItemStorageID()

    UxImGuiNextItemData()         { memset(this, 0, sizeof(*this)); SelectionUserData = -1; }
    inline void ClearFlags()    { HasFlags = UxImGuiNextItemDataFlags_None; ItemFlags = UxImGuiItemFlags_None; } // Also cleared manually by ItemAdd()!
};

// Status storage for the last submitted item
struct UxImGuiLastItemData
{
    UxImGuiID                 ID;
    UxImGuiItemFlags          ItemFlags;          // See UxImGuiItemFlags_ (called 'InFlags' before v1.91.4).
    UxImGuiItemStatusFlags    StatusFlags;        // See UxImGuiItemStatusFlags_
    UxImRect                  Rect;               // Full rectangle
    UxImRect                  NavRect;            // Navigation scoring rectangle (not displayed)
    // Rarely used fields are not explicitly cleared, only valid when the corresponding UxImGuiItemStatusFlags are set.
    UxImRect                  DisplayRect;        // Display rectangle. ONLY VALID IF (StatusFlags & UxImGuiItemStatusFlags_HasDisplayRect) is set.
    UxImRect                  ClipRect;           // Clip rectangle at the time of submitting item. ONLY VALID IF (StatusFlags & UxImGuiItemStatusFlags_HasClipRect) is set..
    UxImGuiKeyChord           Shortcut;           // Shortcut at the time of submitting item. ONLY VALID IF (StatusFlags & UxImGuiItemStatusFlags_HasShortcut) is set..

    UxImGuiLastItemData()     { memset(this, 0, sizeof(*this)); }
};

// Store data emitted by TreeNode() for usage by TreePop()
// - To implement UxImGuiTreeNodeFlags_NavLeftJumpsToParent: store the minimum amount of data
//   which we can't infer in TreePop(), to perform the equivalent of NavApplyItemToResult().
//   Only stored when the node is a potential candidate for landing on a Left arrow jump.
struct UxImGuiTreeNodeStackData
{
    UxImGuiID                 ID;
    UxImGuiTreeNodeFlags      TreeFlags;
    UxImGuiItemFlags          ItemFlags;      // Used for nav landing
    UxImRect                  NavRect;        // Used for nav landing
    float                   DrawLinesX1;
    float                   DrawLinesToNodesY2;
    UxImGuiTableColumnIdx     DrawLinesTableColumn;
};

// sizeof() = 20
struct IMGUI_API UxImGuiErrorRecoveryState
{
    short   SizeOfWindowStack;
    short   SizeOfIDStack;
    short   SizeOfTreeStack;
    short   SizeOfColorStack;
    short   SizeOfStyleVarStack;
    short   SizeOfFontStack;
    short   SizeOfFocusScopeStack;
    short   SizeOfGroupStack;
    short   SizeOfItemFlagsStack;
    short   SizeOfBeginPopupStack;
    short   SizeOfDisabledStack;

    UxImGuiErrorRecoveryState() { memset(this, 0, sizeof(*this)); }
};

// Data saved for each window pushed into the stack
struct UxImGuiWindowStackData
{
    UxImGuiWindow*            Window;
    UxImGuiLastItemData       ParentLastItemDataBackup;
    UxImGuiErrorRecoveryState StackSizesInBegin;          // Store size of various stacks for asserting
    bool                    DisabledOverrideReenable;   // Non-child window override disabled flag
    float                   DisabledOverrideReenableAlphaBackup;
};

struct UxImGuiShrinkWidthItem
{
    int         Index;
    float       Width;
    float       InitialWidth;
};

struct UxImGuiPtrOrIndex
{
    void*       Ptr;            // Either field can be set, not both. e.g. Dock node tab bars are loose while BeginTabBar() ones are in a pool.
    int         Index;          // Usually index in a main pool.

    UxImGuiPtrOrIndex(void* ptr)  { Ptr = ptr; Index = -1; }
    UxImGuiPtrOrIndex(int index)  { Ptr = NULL; Index = index; }
};

// Data used by IsItemDeactivated()/IsItemDeactivatedAfterEdit() functions
struct UxImGuiDeactivatedItemData
{
    UxImGuiID     ID;
    int         ElapseFrame;
    bool        HasBeenEditedBefore;
    bool        IsAlive;
};

//-----------------------------------------------------------------------------
// [SECTION] Popup support
//-----------------------------------------------------------------------------

enum UxImGuiPopupPositionPolicy
{
    UxImGuiPopupPositionPolicy_Default,
    UxImGuiPopupPositionPolicy_ComboBox,
    UxImGuiPopupPositionPolicy_Tooltip,
};

// Storage for popup stacks (g.OpenPopupStack and g.BeginPopupStack)
struct UxImGuiPopupData
{
    UxImGuiID             PopupId;        // Set on OpenPopup()
    UxImGuiWindow*        Window;         // Resolved on BeginPopup() - may stay unresolved if user never calls OpenPopup()
    UxImGuiWindow*        RestoreNavWindow;// Set on OpenPopup(), a NavWindow that will be restored on popup close
    int                 ParentNavLayer; // Resolved on BeginPopup(). Actually a UxImGuiNavLayer type (declared down below), initialized to -1 which is not part of an enum, but serves well-enough as "not any of layers" value
    int                 OpenFrameCount; // Set on OpenPopup()
    UxImGuiID             OpenParentId;   // Set on OpenPopup(), we need this to differentiate multiple menu sets from each others (e.g. inside menu bar vs loose menu items)
    UxImVec2              OpenPopupPos;   // Set on OpenPopup(), preferred popup position (typically == OpenMousePos when using mouse)
    UxImVec2              OpenMousePos;   // Set on OpenPopup(), copy of mouse position at the time of opening popup

    UxImGuiPopupData()    { memset(this, 0, sizeof(*this)); ParentNavLayer = OpenFrameCount = -1; }
};

//-----------------------------------------------------------------------------
// [SECTION] Inputs support
//-----------------------------------------------------------------------------

// Bit array for named keys
typedef UxImBitArray<UxImGuiKey_NamedKey_COUNT, -UxImGuiKey_NamedKey_BEGIN>    UxImBitArrayForNamedKeys;

// [Internal] Key ranges
#define UxImGuiKey_LegacyNativeKey_BEGIN  0
#define UxImGuiKey_LegacyNativeKey_END    512
#define UxImGuiKey_Keyboard_BEGIN         (UxImGuiKey_NamedKey_BEGIN)
#define UxImGuiKey_Keyboard_END           (UxImGuiKey_GamepadStart)
#define UxImGuiKey_Gamepad_BEGIN          (UxImGuiKey_GamepadStart)
#define UxImGuiKey_Gamepad_END            (UxImGuiKey_GamepadRStickDown + 1)
#define UxImGuiKey_Mouse_BEGIN            (UxImGuiKey_MouseLeft)
#define UxImGuiKey_Mouse_END              (UxImGuiKey_MouseWheelY + 1)
#define UxImGuiKey_Aliases_BEGIN          (UxImGuiKey_Mouse_BEGIN)
#define UxImGuiKey_Aliases_END            (UxImGuiKey_Mouse_END)

// [Internal] Named shortcuts for Navigation
#define UxImGuiKey_NavKeyboardTweakSlow   UxImGuiMod_Ctrl
#define UxImGuiKey_NavKeyboardTweakFast   UxImGuiMod_Shift
#define UxImGuiKey_NavGamepadTweakSlow    UxImGuiKey_GamepadL1
#define UxImGuiKey_NavGamepadTweakFast    UxImGuiKey_GamepadR1
#define UxImGuiKey_NavGamepadActivate     (g.IO.ConfigNavSwapGamepadButtons ? UxImGuiKey_GamepadFaceRight : UxImGuiKey_GamepadFaceDown)
#define UxImGuiKey_NavGamepadCancel       (g.IO.ConfigNavSwapGamepadButtons ? UxImGuiKey_GamepadFaceDown : UxImGuiKey_GamepadFaceRight)
#define UxImGuiKey_NavGamepadMenu         UxImGuiKey_GamepadFaceLeft
#define UxImGuiKey_NavGamepadInput        UxImGuiKey_GamepadFaceUp

enum UxImGuiInputEventType
{
    UxImGuiInputEventType_None = 0,
    UxImGuiInputEventType_MousePos,
    UxImGuiInputEventType_MouseWheel,
    UxImGuiInputEventType_MouseButton,
    UxImGuiInputEventType_MouseViewport,
    UxImGuiInputEventType_Key,
    UxImGuiInputEventType_Text,
    UxImGuiInputEventType_Focus,
    UxImGuiInputEventType_COUNT
};

enum UxImGuiInputSource
{
    UxImGuiInputSource_None = 0,
    UxImGuiInputSource_Mouse,         // Note: may be Mouse or TouchScreen or Pen. See io.MouseSource to distinguish them.
    UxImGuiInputSource_Keyboard,
    UxImGuiInputSource_Gamepad,
    UxImGuiInputSource_COUNT
};

// FIXME: Structures in the union below need to be declared as anonymous unions appears to be an extension?
// Using UxImVec2() would fail on Clang 'union member 'MousePos' has a non-trivial default constructor'
struct UxImGuiInputEventMousePos      { float PosX, PosY; UxImGuiMouseSource MouseSource; };
struct UxImGuiInputEventMouseWheel    { float WheelX, WheelY; UxImGuiMouseSource MouseSource; };
struct UxImGuiInputEventMouseButton   { int Button; bool Down; UxImGuiMouseSource MouseSource; };
struct UxImGuiInputEventMouseViewport { UxImGuiID HoveredViewportID; };
struct UxImGuiInputEventKey           { UxImGuiKey Key; bool Down; float AnalogValue; };
struct UxImGuiInputEventText          { unsigned int Char; };
struct UxImGuiInputEventAppFocused    { bool Focused; };

struct UxImGuiInputEvent
{
    UxImGuiInputEventType             Type;
    UxImGuiInputSource                Source;
    UxImU32                           EventId;        // Unique, sequential increasing integer to identify an event (if you need to correlate them to other data).
    union
    {
        UxImGuiInputEventMousePos     MousePos;       // if Type == UxImGuiInputEventType_MousePos
        UxImGuiInputEventMouseWheel   MouseWheel;     // if Type == UxImGuiInputEventType_MouseWheel
        UxImGuiInputEventMouseButton  MouseButton;    // if Type == UxImGuiInputEventType_MouseButton
        UxImGuiInputEventMouseViewport MouseViewport; // if Type == UxImGuiInputEventType_MouseViewport
        UxImGuiInputEventKey          Key;            // if Type == UxImGuiInputEventType_Key
        UxImGuiInputEventText         Text;           // if Type == UxImGuiInputEventType_Text
        UxImGuiInputEventAppFocused   AppFocused;     // if Type == UxImGuiInputEventType_Focus
    };
    bool                            AddedByTestEngine;

    UxImGuiInputEvent() { memset(this, 0, sizeof(*this)); }
};

// Input function taking an 'UxImGuiID owner_id' argument defaults to (UxImGuiKeyOwner_Any == 0) aka don't test ownership, which matches legacy behavior.
#define UxImGuiKeyOwner_Any           ((UxImGuiID)0)    // Accept key that have an owner, UNLESS a call to SetKeyOwner() explicitly used UxImGuiInputFlags_LockThisFrame or UxImGuiInputFlags_LockUntilRelease.
#define UxImGuiKeyOwner_NoOwner       ((UxImGuiID)-1)   // Require key to have no owner.
//#define UxImGuiKeyOwner_None UxImGuiKeyOwner_NoOwner  // We previously called this 'UxImGuiKeyOwner_None' but it was inconsistent with our pattern that _None values == 0 and quite dangerous. Also using _NoOwner makes the IsKeyPressed() calls more explicit.

typedef UxImS16 UxImGuiKeyRoutingIndex;

// Routing table entry (sizeof() == 16 bytes)
struct UxImGuiKeyRoutingData
{
    UxImGuiKeyRoutingIndex            NextEntryIndex;
    UxImU16                           Mods;               // Technically we'd only need 4-bits but for simplify we store UxImGuiMod_ values which need 16-bits.
    UxImU8                            RoutingCurrScore;   // [DEBUG] For debug display
    UxImU8                            RoutingNextScore;   // Lower is better (0: perfect score)
    UxImGuiID                         RoutingCurr;
    UxImGuiID                         RoutingNext;

    UxImGuiKeyRoutingData()           { NextEntryIndex = -1; Mods = 0; RoutingCurrScore = RoutingNextScore = 255; RoutingCurr = RoutingNext = UxImGuiKeyOwner_NoOwner; }
};

// Routing table: maintain a desired owner for each possible key-chord (key + mods), and setup owner in NewFrame() when mods are matching.
// Stored in main context (1 instance)
struct UxImGuiKeyRoutingTable
{
    UxImGuiKeyRoutingIndex            Index[UxImGuiKey_NamedKey_COUNT]; // Index of first entry in Entries[]
    UxImVector<UxImGuiKeyRoutingData>   Entries;
    UxImVector<UxImGuiKeyRoutingData>   EntriesNext;                    // Double-buffer to avoid reallocation (could use a shared buffer)

    UxImGuiKeyRoutingTable()          { Clear(); }
    void Clear()                    { for (int n = 0; n < IM_ARRAYSIZE(Index); n++) Index[n] = -1; Entries.clear(); EntriesNext.clear(); }
};

// This extends UxImGuiKeyData but only for named keys (legacy keys don't support the new features)
// Stored in main context (1 per named key). In the future it might be merged into UxImGuiKeyData.
struct UxImGuiKeyOwnerData
{
    UxImGuiID     OwnerCurr;
    UxImGuiID     OwnerNext;
    bool        LockThisFrame;      // Reading this key requires explicit owner id (until end of frame). Set by UxImGuiInputFlags_LockThisFrame.
    bool        LockUntilRelease;   // Reading this key requires explicit owner id (until key is released). Set by UxImGuiInputFlags_LockUntilRelease. When this is true LockThisFrame is always true as well.

    UxImGuiKeyOwnerData()             { OwnerCurr = OwnerNext = UxImGuiKeyOwner_NoOwner; LockThisFrame = LockUntilRelease = false; }
};

// Extend UxImGuiInputFlags_
// Flags for extended versions of IsKeyPressed(), IsMouseClicked(), Shortcut(), SetKeyOwner(), SetItemKeyOwner()
// Don't mistake with UxImGuiInputTextFlags! (which is for UxImGui::InputText() function)
enum UxImGuiInputFlagsPrivate_
{
    // Flags for IsKeyPressed(), IsKeyChordPressed(), IsMouseClicked(), Shortcut()
    // - Repeat mode: Repeat rate selection
    UxImGuiInputFlags_RepeatRateDefault           = 1 << 1,   // Repeat rate: Regular (default)
    UxImGuiInputFlags_RepeatRateNavMove           = 1 << 2,   // Repeat rate: Fast
    UxImGuiInputFlags_RepeatRateNavTweak          = 1 << 3,   // Repeat rate: Faster
    // - Repeat mode: Specify when repeating key pressed can be interrupted.
    // - In theory UxImGuiInputFlags_RepeatUntilOtherKeyPress may be a desirable default, but it would break too many behavior so everything is opt-in.
    UxImGuiInputFlags_RepeatUntilRelease          = 1 << 4,   // Stop repeating when released (default for all functions except Shortcut). This only exists to allow overriding Shortcut() default behavior.
    UxImGuiInputFlags_RepeatUntilKeyModsChange    = 1 << 5,   // Stop repeating when released OR if keyboard mods are changed (default for Shortcut)
    UxImGuiInputFlags_RepeatUntilKeyModsChangeFromNone = 1 << 6,  // Stop repeating when released OR if keyboard mods are leaving the None state. Allows going from Mod+Key to Key by releasing Mod.
    UxImGuiInputFlags_RepeatUntilOtherKeyPress    = 1 << 7,   // Stop repeating when released OR if any other keyboard key is pressed during the repeat

    // Flags for SetKeyOwner(), SetItemKeyOwner()
    // - Locking key away from non-input aware code. Locking is useful to make input-owner-aware code steal keys from non-input-owner-aware code. If all code is input-owner-aware locking would never be necessary.
    UxImGuiInputFlags_LockThisFrame               = 1 << 20,  // Further accesses to key data will require EXPLICIT owner ID (UxImGuiKeyOwner_Any/0 will NOT accepted for polling). Cleared at end of frame.
    UxImGuiInputFlags_LockUntilRelease            = 1 << 21,  // Further accesses to key data will require EXPLICIT owner ID (UxImGuiKeyOwner_Any/0 will NOT accepted for polling). Cleared when the key is released or at end of each frame if key is released.

    // - Condition for SetItemKeyOwner()
    UxImGuiInputFlags_CondHovered                 = 1 << 22,  // Only set if item is hovered (default to both)
    UxImGuiInputFlags_CondActive                  = 1 << 23,  // Only set if item is active (default to both)
    UxImGuiInputFlags_CondDefault_                = UxImGuiInputFlags_CondHovered | UxImGuiInputFlags_CondActive,

    // [Internal] Mask of which function support which flags
    UxImGuiInputFlags_RepeatRateMask_             = UxImGuiInputFlags_RepeatRateDefault | UxImGuiInputFlags_RepeatRateNavMove | UxImGuiInputFlags_RepeatRateNavTweak,
    UxImGuiInputFlags_RepeatUntilMask_            = UxImGuiInputFlags_RepeatUntilRelease | UxImGuiInputFlags_RepeatUntilKeyModsChange | UxImGuiInputFlags_RepeatUntilKeyModsChangeFromNone | UxImGuiInputFlags_RepeatUntilOtherKeyPress,
    UxImGuiInputFlags_RepeatMask_                 = UxImGuiInputFlags_Repeat | UxImGuiInputFlags_RepeatRateMask_ | UxImGuiInputFlags_RepeatUntilMask_,
    UxImGuiInputFlags_CondMask_                   = UxImGuiInputFlags_CondHovered | UxImGuiInputFlags_CondActive,
    UxImGuiInputFlags_RouteTypeMask_              = UxImGuiInputFlags_RouteActive | UxImGuiInputFlags_RouteFocused | UxImGuiInputFlags_RouteGlobal | UxImGuiInputFlags_RouteAlways,
    UxImGuiInputFlags_RouteOptionsMask_           = UxImGuiInputFlags_RouteOverFocused | UxImGuiInputFlags_RouteOverActive | UxImGuiInputFlags_RouteUnlessBgFocused | UxImGuiInputFlags_RouteFromRootWindow,
    UxImGuiInputFlags_SupportedByIsKeyPressed     = UxImGuiInputFlags_RepeatMask_,
    UxImGuiInputFlags_SupportedByIsMouseClicked   = UxImGuiInputFlags_Repeat,
    UxImGuiInputFlags_SupportedByShortcut         = UxImGuiInputFlags_RepeatMask_ | UxImGuiInputFlags_RouteTypeMask_ | UxImGuiInputFlags_RouteOptionsMask_,
    UxImGuiInputFlags_SupportedBySetNextItemShortcut = UxImGuiInputFlags_RepeatMask_ | UxImGuiInputFlags_RouteTypeMask_ | UxImGuiInputFlags_RouteOptionsMask_ | UxImGuiInputFlags_Tooltip,
    UxImGuiInputFlags_SupportedBySetKeyOwner      = UxImGuiInputFlags_LockThisFrame | UxImGuiInputFlags_LockUntilRelease,
    UxImGuiInputFlags_SupportedBySetItemKeyOwner  = UxImGuiInputFlags_SupportedBySetKeyOwner | UxImGuiInputFlags_CondMask_,
};

//-----------------------------------------------------------------------------
// [SECTION] Clipper support
//-----------------------------------------------------------------------------

// Note that Max is exclusive, so perhaps should be using a Begin/End convention.
struct UxImGuiListClipperRange
{
    int     Min;
    int     Max;
    bool    PosToIndexConvert;      // Begin/End are absolute position (will be converted to indices later)
    UxImS8    PosToIndexOffsetMin;    // Add to Min after converting to indices
    UxImS8    PosToIndexOffsetMax;    // Add to Min after converting to indices

    static UxImGuiListClipperRange    FromIndices(int min, int max)                               { UxImGuiListClipperRange r = { min, max, false, 0, 0 }; return r; }
    static UxImGuiListClipperRange    FromPositions(float y1, float y2, int off_min, int off_max) { UxImGuiListClipperRange r = { (int)y1, (int)y2, true, (UxImS8)off_min, (UxImS8)off_max }; return r; }
};

// Temporary clipper data, buffers shared/reused between instances
struct UxImGuiListClipperData
{
    UxImGuiListClipper*               ListClipper;
    float                           LossynessOffset;
    int                             StepNo;
    int                             ItemsFrozen;
    UxImVector<UxImGuiListClipperRange> Ranges;

    UxImGuiListClipperData()          { memset(this, 0, sizeof(*this)); }
    void                            Reset(UxImGuiListClipper* clipper) { ListClipper = clipper; StepNo = ItemsFrozen = 0; Ranges.resize(0); }
};

//-----------------------------------------------------------------------------
// [SECTION] Navigation support
//-----------------------------------------------------------------------------

enum UxImGuiActivateFlags_
{
    UxImGuiActivateFlags_None                 = 0,
    UxImGuiActivateFlags_PreferInput          = 1 << 0,       // Favor activation that requires keyboard text input (e.g. for Slider/Drag). Default for Enter key.
    UxImGuiActivateFlags_PreferTweak          = 1 << 1,       // Favor activation for tweaking with arrows or gamepad (e.g. for Slider/Drag). Default for Space key and if keyboard is not used.
    UxImGuiActivateFlags_TryToPreserveState   = 1 << 2,       // Request widget to preserve state if it can (e.g. InputText will try to preserve cursor/selection)
    UxImGuiActivateFlags_FromTabbing          = 1 << 3,       // Activation requested by a tabbing request
    UxImGuiActivateFlags_FromShortcut         = 1 << 4,       // Activation requested by an item shortcut via SetNextItemShortcut() function.
};

// Early work-in-progress API for ScrollToItem()
enum UxImGuiScrollFlags_
{
    UxImGuiScrollFlags_None                   = 0,
    UxImGuiScrollFlags_KeepVisibleEdgeX       = 1 << 0,       // If item is not visible: scroll as little as possible on X axis to bring item back into view [default for X axis]
    UxImGuiScrollFlags_KeepVisibleEdgeY       = 1 << 1,       // If item is not visible: scroll as little as possible on Y axis to bring item back into view [default for Y axis for windows that are already visible]
    UxImGuiScrollFlags_KeepVisibleCenterX     = 1 << 2,       // If item is not visible: scroll to make the item centered on X axis [rarely used]
    UxImGuiScrollFlags_KeepVisibleCenterY     = 1 << 3,       // If item is not visible: scroll to make the item centered on Y axis
    UxImGuiScrollFlags_AlwaysCenterX          = 1 << 4,       // Always center the result item on X axis [rarely used]
    UxImGuiScrollFlags_AlwaysCenterY          = 1 << 5,       // Always center the result item on Y axis [default for Y axis for appearing window)
    UxImGuiScrollFlags_NoScrollParent         = 1 << 6,       // Disable forwarding scrolling to parent window if required to keep item/rect visible (only scroll window the function was applied to).
    UxImGuiScrollFlags_MaskX_                 = UxImGuiScrollFlags_KeepVisibleEdgeX | UxImGuiScrollFlags_KeepVisibleCenterX | UxImGuiScrollFlags_AlwaysCenterX,
    UxImGuiScrollFlags_MaskY_                 = UxImGuiScrollFlags_KeepVisibleEdgeY | UxImGuiScrollFlags_KeepVisibleCenterY | UxImGuiScrollFlags_AlwaysCenterY,
};

enum UxImGuiNavRenderCursorFlags_
{
    UxImGuiNavRenderCursorFlags_None          = 0,
    UxImGuiNavRenderCursorFlags_Compact       = 1 << 1,       // Compact highlight, no padding/distance from focused item
    UxImGuiNavRenderCursorFlags_AlwaysDraw    = 1 << 2,       // Draw rectangular highlight if (g.NavId == id) even when g.NavCursorVisible == false, aka even when using the mouse.
    UxImGuiNavRenderCursorFlags_NoRounding    = 1 << 3,
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UxImGuiNavHighlightFlags_None             = UxImGuiNavRenderCursorFlags_None,       // Renamed in 1.91.4
    UxImGuiNavHighlightFlags_Compact          = UxImGuiNavRenderCursorFlags_Compact,    // Renamed in 1.91.4
    UxImGuiNavHighlightFlags_AlwaysDraw       = UxImGuiNavRenderCursorFlags_AlwaysDraw, // Renamed in 1.91.4
    UxImGuiNavHighlightFlags_NoRounding       = UxImGuiNavRenderCursorFlags_NoRounding, // Renamed in 1.91.4
#endif
};

enum UxImGuiNavMoveFlags_
{
    UxImGuiNavMoveFlags_None                  = 0,
    UxImGuiNavMoveFlags_LoopX                 = 1 << 0,   // On failed request, restart from opposite side
    UxImGuiNavMoveFlags_LoopY                 = 1 << 1,
    UxImGuiNavMoveFlags_WrapX                 = 1 << 2,   // On failed request, request from opposite side one line down (when NavDir==right) or one line up (when NavDir==left)
    UxImGuiNavMoveFlags_WrapY                 = 1 << 3,   // This is not super useful but provided for completeness
    UxImGuiNavMoveFlags_WrapMask_             = UxImGuiNavMoveFlags_LoopX | UxImGuiNavMoveFlags_LoopY | UxImGuiNavMoveFlags_WrapX | UxImGuiNavMoveFlags_WrapY,
    UxImGuiNavMoveFlags_AllowCurrentNavId     = 1 << 4,   // Allow scoring and considering the current NavId as a move target candidate. This is used when the move source is offset (e.g. pressing PageDown actually needs to send a Up move request, if we are pressing PageDown from the bottom-most item we need to stay in place)
    UxImGuiNavMoveFlags_AlsoScoreVisibleSet   = 1 << 5,   // Store alternate result in NavMoveResultLocalVisible that only comprise elements that are already fully visible (used by PageUp/PageDown)
    UxImGuiNavMoveFlags_ScrollToEdgeY         = 1 << 6,   // Force scrolling to min/max (used by Home/End) // FIXME-NAV: Aim to remove or reword, probably unnecessary
    UxImGuiNavMoveFlags_Forwarded             = 1 << 7,
    UxImGuiNavMoveFlags_DebugNoResult         = 1 << 8,   // Dummy scoring for debug purpose, don't apply result
    UxImGuiNavMoveFlags_FocusApi              = 1 << 9,   // Requests from focus API can land/focus/activate items even if they are marked with _NoTabStop (see NavProcessItemForTabbingRequest() for details)
    UxImGuiNavMoveFlags_IsTabbing             = 1 << 10,  // == Focus + Activate if item is Inputable + DontChangeNavHighlight
    UxImGuiNavMoveFlags_IsPageMove            = 1 << 11,  // Identify a PageDown/PageUp request.
    UxImGuiNavMoveFlags_Activate              = 1 << 12,  // Activate/select target item.
    UxImGuiNavMoveFlags_NoSelect              = 1 << 13,  // Don't trigger selection by not setting g.NavJustMovedTo
    UxImGuiNavMoveFlags_NoSetNavCursorVisible = 1 << 14,  // Do not alter the nav cursor visible state
    UxImGuiNavMoveFlags_NoClearActiveId       = 1 << 15,  // (Experimental) Do not clear active id when applying move result
};

enum UxImGuiNavLayer
{
    UxImGuiNavLayer_Main  = 0,    // Main scrolling layer
    UxImGuiNavLayer_Menu  = 1,    // Menu layer (access with Alt)
    UxImGuiNavLayer_COUNT
};

// Storage for navigation query/results
struct UxImGuiNavItemData
{
    UxImGuiWindow*        Window;         // Init,Move    // Best candidate window (result->ItemWindow->RootWindowForNav == request->Window)
    UxImGuiID             ID;             // Init,Move    // Best candidate item ID
    UxImGuiID             FocusScopeId;   // Init,Move    // Best candidate focus scope ID
    UxImRect              RectRel;        // Init,Move    // Best candidate bounding box in window relative space
    UxImGuiItemFlags      ItemFlags;      // ????,Move    // Best candidate item flags
    float               DistBox;        //      Move    // Best candidate box distance to current NavId
    float               DistCenter;     //      Move    // Best candidate center distance to current NavId
    float               DistAxial;      //      Move    // Best candidate axial distance to current NavId
    UxImGuiSelectionUserData SelectionUserData;//I+Mov    // Best candidate SetNextItemSelectionUserData() value. Valid if (ItemFlags & UxImGuiItemFlags_HasSelectionUserData)

    UxImGuiNavItemData()  { Clear(); }
    void Clear()        { Window = NULL; ID = FocusScopeId = 0; ItemFlags = 0; SelectionUserData = -1; DistBox = DistCenter = DistAxial = FLT_MAX; }
};

// Storage for PushFocusScope(), g.FocusScopeStack[], g.NavFocusRoute[]
struct UxImGuiFocusScopeData
{
    UxImGuiID             ID;
    UxImGuiID             WindowID;
};

//-----------------------------------------------------------------------------
// [SECTION] Typing-select support
//-----------------------------------------------------------------------------

// Flags for GetTypingSelectRequest()
enum UxImGuiTypingSelectFlags_
{
    UxImGuiTypingSelectFlags_None                 = 0,
    UxImGuiTypingSelectFlags_AllowBackspace       = 1 << 0,   // Backspace to delete character inputs. If using: ensure GetTypingSelectRequest() is not called more than once per frame (filter by e.g. focus state)
    UxImGuiTypingSelectFlags_AllowSingleCharMode  = 1 << 1,   // Allow "single char" search mode which is activated when pressing the same character multiple times.
};

// Returned by GetTypingSelectRequest(), designed to eventually be public.
struct IMGUI_API UxImGuiTypingSelectRequest
{
    UxImGuiTypingSelectFlags  Flags;              // Flags passed to GetTypingSelectRequest()
    int                     SearchBufferLen;
    const char*             SearchBuffer;       // Search buffer contents (use full string. unless SingleCharMode is set, in which case use SingleCharSize).
    bool                    SelectRequest;      // Set when buffer was modified this frame, requesting a selection.
    bool                    SingleCharMode;     // Notify when buffer contains same character repeated, to implement special mode. In this situation it preferred to not display any on-screen search indication.
    UxImS8                    SingleCharSize;     // Length in bytes of first letter codepoint (1 for ascii, 2-4 for UTF-8). If (SearchBufferLen==RepeatCharSize) only 1 letter has been input.
};

// Storage for GetTypingSelectRequest()
struct IMGUI_API UxImGuiTypingSelectState
{
    UxImGuiTypingSelectRequest Request;           // User-facing data
    char            SearchBuffer[64];           // Search buffer: no need to make dynamic as this search is very transient.
    UxImGuiID         FocusScope;
    int             LastRequestFrame = 0;
    float           LastRequestTime = 0.0f;
    bool            SingleCharModeLock = false; // After a certain single char repeat count we lock into SingleCharMode. Two benefits: 1) buffer never fill, 2) we can provide an immediate SingleChar mode without timer elapsing.

    UxImGuiTypingSelectState() { memset(this, 0, sizeof(*this)); }
    void            Clear()  { SearchBuffer[0] = 0; SingleCharModeLock = false; } // We preserve remaining data for easier debugging
};

//-----------------------------------------------------------------------------
// [SECTION] Columns support
//-----------------------------------------------------------------------------

// Flags for internal's BeginColumns(). This is an obsolete API. Prefer using BeginTable() nowadays!
enum UxImGuiOldColumnFlags_
{
    UxImGuiOldColumnFlags_None                    = 0,
    UxImGuiOldColumnFlags_NoBorder                = 1 << 0,   // Disable column dividers
    UxImGuiOldColumnFlags_NoResize                = 1 << 1,   // Disable resizing columns when clicking on the dividers
    UxImGuiOldColumnFlags_NoPreserveWidths        = 1 << 2,   // Disable column width preservation when adjusting columns
    UxImGuiOldColumnFlags_NoForceWithinWindow     = 1 << 3,   // Disable forcing columns to fit within window
    UxImGuiOldColumnFlags_GrowParentContentsSize  = 1 << 4,   // Restore pre-1.51 behavior of extending the parent window contents size but _without affecting the columns width at all_. Will eventually remove.

    // Obsolete names (will be removed)
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    //UxImGuiColumnsFlags_None                    = UxImGuiOldColumnFlags_None,
    //UxImGuiColumnsFlags_NoBorder                = UxImGuiOldColumnFlags_NoBorder,
    //UxImGuiColumnsFlags_NoResize                = UxImGuiOldColumnFlags_NoResize,
    //UxImGuiColumnsFlags_NoPreserveWidths        = UxImGuiOldColumnFlags_NoPreserveWidths,
    //UxImGuiColumnsFlags_NoForceWithinWindow     = UxImGuiOldColumnFlags_NoForceWithinWindow,
    //UxImGuiColumnsFlags_GrowParentContentsSize  = UxImGuiOldColumnFlags_GrowParentContentsSize,
#endif
};

struct UxImGuiOldColumnData
{
    float               OffsetNorm;             // Column start offset, normalized 0.0 (far left) -> 1.0 (far right)
    float               OffsetNormBeforeResize;
    UxImGuiOldColumnFlags Flags;                  // Not exposed
    UxImRect              ClipRect;

    UxImGuiOldColumnData() { memset(this, 0, sizeof(*this)); }
};

struct UxImGuiOldColumns
{
    UxImGuiID             ID;
    UxImGuiOldColumnFlags Flags;
    bool                IsFirstFrame;
    bool                IsBeingResized;
    int                 Current;
    int                 Count;
    float               OffMinX, OffMaxX;       // Offsets from HostWorkRect.Min.x
    float               LineMinY, LineMaxY;
    float               HostCursorPosY;         // Backup of CursorPos at the time of BeginColumns()
    float               HostCursorMaxPosX;      // Backup of CursorMaxPos at the time of BeginColumns()
    UxImRect              HostInitialClipRect;    // Backup of ClipRect at the time of BeginColumns()
    UxImRect              HostBackupClipRect;     // Backup of ClipRect during PushColumnsBackground()/PopColumnsBackground()
    UxImRect              HostBackupParentWorkRect;//Backup of WorkRect at the time of BeginColumns()
    UxImVector<UxImGuiOldColumnData> Columns;
    UxImDrawListSplitter  Splitter;

    UxImGuiOldColumns()   { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// [SECTION] Box-select support
//-----------------------------------------------------------------------------

struct UxImGuiBoxSelectState
{
    // Active box-selection data (persistent, 1 active at a time)
    UxImGuiID                 ID;
    bool                    IsActive;
    bool                    IsStarting;
    bool                    IsStartedFromVoid;  // Starting click was not from an item.
    bool                    IsStartedSetNavIdOnce;
    bool                    RequestClear;
    UxImGuiKeyChord           KeyMods : 16;       // Latched key-mods for box-select logic.
    UxImVec2                  StartPosRel;        // Start position in window-contents relative space (to support scrolling)
    UxImVec2                  EndPosRel;          // End position in window-contents relative space
    UxImVec2                  ScrollAccum;        // Scrolling accumulator (to behave at high-frame spaces)
    UxImGuiWindow*            Window;

    // Temporary/Transient data
    bool                    UnclipMode;         // (Temp/Transient, here in hot area). Set/cleared by the BeginMultiSelect()/EndMultiSelect() owning active box-select.
    UxImRect                  UnclipRect;         // Rectangle where ItemAdd() clipping may be temporarily disabled. Need support by multi-select supporting widgets.
    UxImRect                  BoxSelectRectPrev;  // Selection rectangle in absolute coordinates (derived every frame from BoxSelectStartPosRel and MousePos)
    UxImRect                  BoxSelectRectCurr;

    UxImGuiBoxSelectState()   { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// [SECTION] Multi-select support
//-----------------------------------------------------------------------------

// We always assume that -1 is an invalid value (which works for indices and pointers)
#define UxImGuiSelectionUserData_Invalid        ((UxImGuiSelectionUserData)-1)

// Temporary storage for multi-select
struct IMGUI_API UxImGuiMultiSelectTempData
{
    UxImGuiMultiSelectIO      IO;                 // MUST BE FIRST FIELD. Requests are set and returned by BeginMultiSelect()/EndMultiSelect() + written to by user during the loop.
    UxImGuiMultiSelectState*  Storage;
    UxImGuiID                 FocusScopeId;       // Copied from g.CurrentFocusScopeId (unless another selection scope was pushed manually)
    UxImGuiMultiSelectFlags   Flags;
    UxImVec2                  ScopeRectMin;
    UxImVec2                  BackupCursorMaxPos;
    UxImGuiSelectionUserData  LastSubmittedItem;  // Copy of last submitted item data, used to merge output ranges.
    UxImGuiID                 BoxSelectId;
    UxImGuiKeyChord           KeyMods;
    UxImS8                    LoopRequestSetAll;  // -1: no operation, 0: clear all, 1: select all.
    bool                    IsEndIO;            // Set when switching IO from BeginMultiSelect() to EndMultiSelect() state.
    bool                    IsFocused;          // Set if currently focusing the selection scope (any item of the selection). May be used if you have custom shortcut associated to selection.
    bool                    IsKeyboardSetRange; // Set by BeginMultiSelect() when using Shift+Navigation. Because scrolling may be affected we can't afford a frame of lag with Shift+Navigation.
    bool                    NavIdPassedBy;
    bool                    RangeSrcPassedBy;   // Set by the item that matches RangeSrcItem.
    bool                    RangeDstPassedBy;   // Set by the item that matches NavJustMovedToId when IsSetRange is set.

    UxImGuiMultiSelectTempData()  { Clear(); }
    void Clear()            { size_t io_sz = sizeof(IO); ClearIO(); memset((void*)(&IO + 1), 0, sizeof(*this) - io_sz); } // Zero-clear except IO as we preserve IO.Requests[] buffer allocation.
    void ClearIO()          { IO.Requests.resize(0); IO.RangeSrcItem = IO.NavIdItem = UxImGuiSelectionUserData_Invalid; IO.NavIdSelected = IO.RangeSrcReset = false; }
};

// Persistent storage for multi-select (as long as selection is alive)
struct IMGUI_API UxImGuiMultiSelectState
{
    UxImGuiWindow*            Window;
    UxImGuiID                 ID;
    int                     LastFrameActive;    // Last used frame-count, for GC.
    int                     LastSelectionSize;  // Set by BeginMultiSelect() based on optional info provided by user. May be -1 if unknown.
    UxImS8                    RangeSelected;      // -1 (don't have) or true/false
    UxImS8                    NavIdSelected;      // -1 (don't have) or true/false
    UxImGuiSelectionUserData  RangeSrcItem;       //
    UxImGuiSelectionUserData  NavIdItem;          // SetNextItemSelectionUserData() value for NavId (if part of submitted items)

    UxImGuiMultiSelectState() { Window = NULL; ID = 0; LastFrameActive = LastSelectionSize = 0; RangeSelected = NavIdSelected = -1; RangeSrcItem = NavIdItem = UxImGuiSelectionUserData_Invalid; }
};

//-----------------------------------------------------------------------------
// [SECTION] Docking support
//-----------------------------------------------------------------------------

#define DOCKING_HOST_DRAW_CHANNEL_BG 0  // Dock host: background fill
#define DOCKING_HOST_DRAW_CHANNEL_FG 1  // Dock host: decorations and contents

#ifdef IMGUI_HAS_DOCK

// Extend UxImGuiDockNodeFlags_
enum UxImGuiDockNodeFlagsPrivate_
{
    // [Internal]
    UxImGuiDockNodeFlags_DockSpace                = 1 << 10,  // Saved // A dockspace is a node that occupy space within an existing user window. Otherwise the node is floating and create its own window.
    UxImGuiDockNodeFlags_CentralNode              = 1 << 11,  // Saved // The central node has 2 main properties: stay visible when empty, only use "remaining" spaces from its neighbor.
    UxImGuiDockNodeFlags_NoTabBar                 = 1 << 12,  // Saved // Tab bar is completely unavailable. No triangle in the corner to enable it back.
    UxImGuiDockNodeFlags_HiddenTabBar             = 1 << 13,  // Saved // Tab bar is hidden, with a triangle in the corner to show it again (NB: actual tab-bar instance may be destroyed as this is only used for single-window tab bar)
    UxImGuiDockNodeFlags_NoWindowMenuButton       = 1 << 14,  // Saved // Disable window/docking menu (that one that appears instead of the collapse button)
    UxImGuiDockNodeFlags_NoCloseButton            = 1 << 15,  // Saved // Disable close button
    UxImGuiDockNodeFlags_NoResizeX                = 1 << 16,  //       //
    UxImGuiDockNodeFlags_NoResizeY                = 1 << 17,  //       //
    UxImGuiDockNodeFlags_DockedWindowsInFocusRoute= 1 << 18,  //       // Any docked window will be automatically be focus-route chained (window->ParentWindowForFocusRoute set to this) so Shortcut() in this window can run when any docked window is focused.

    // Disable docking/undocking actions in this dockspace or individual node (existing docked nodes will be preserved)
    // Those are not exposed in public because the desirable sharing/inheriting/copy-flag-on-split behaviors are quite difficult to design and understand.
    // The two public flags UxImGuiDockNodeFlags_NoDockingOverCentralNode/UxImGuiDockNodeFlags_NoDockingSplit don't have those issues.
    UxImGuiDockNodeFlags_NoDockingSplitOther      = 1 << 19,  //       // Disable this node from splitting other windows/nodes.
    UxImGuiDockNodeFlags_NoDockingOverMe          = 1 << 20,  //       // Disable other windows/nodes from being docked over this node.
    UxImGuiDockNodeFlags_NoDockingOverOther       = 1 << 21,  //       // Disable this node from being docked over another window or non-empty node.
    UxImGuiDockNodeFlags_NoDockingOverEmpty       = 1 << 22,  //       // Disable this node from being docked over an empty node (e.g. DockSpace with no other windows)
    UxImGuiDockNodeFlags_NoDocking                = UxImGuiDockNodeFlags_NoDockingOverMe | UxImGuiDockNodeFlags_NoDockingOverOther | UxImGuiDockNodeFlags_NoDockingOverEmpty | UxImGuiDockNodeFlags_NoDockingSplit | UxImGuiDockNodeFlags_NoDockingSplitOther,

    // Masks
    UxImGuiDockNodeFlags_SharedFlagsInheritMask_  = ~0,
    UxImGuiDockNodeFlags_NoResizeFlagsMask_       = (int)UxImGuiDockNodeFlags_NoResize | UxImGuiDockNodeFlags_NoResizeX | UxImGuiDockNodeFlags_NoResizeY,

    // When splitting, those local flags are moved to the inheriting child, never duplicated
    UxImGuiDockNodeFlags_LocalFlagsTransferMask_  = (int)UxImGuiDockNodeFlags_NoDockingSplit | UxImGuiDockNodeFlags_NoResizeFlagsMask_ | (int)UxImGuiDockNodeFlags_AutoHideTabBar | UxImGuiDockNodeFlags_CentralNode | UxImGuiDockNodeFlags_NoTabBar | UxImGuiDockNodeFlags_HiddenTabBar | UxImGuiDockNodeFlags_NoWindowMenuButton | UxImGuiDockNodeFlags_NoCloseButton,
    UxImGuiDockNodeFlags_SavedFlagsMask_          = UxImGuiDockNodeFlags_NoResizeFlagsMask_ | UxImGuiDockNodeFlags_DockSpace | UxImGuiDockNodeFlags_CentralNode | UxImGuiDockNodeFlags_NoTabBar | UxImGuiDockNodeFlags_HiddenTabBar | UxImGuiDockNodeFlags_NoWindowMenuButton | UxImGuiDockNodeFlags_NoCloseButton,
};

// Store the source authority (dock node vs window) of a field
enum UxImGuiDataAuthority_
{
    UxImGuiDataAuthority_Auto,
    UxImGuiDataAuthority_DockNode,
    UxImGuiDataAuthority_Window,
};

enum UxImGuiDockNodeState
{
    UxImGuiDockNodeState_Unknown,
    UxImGuiDockNodeState_HostWindowHiddenBecauseSingleWindow,
    UxImGuiDockNodeState_HostWindowHiddenBecauseWindowsAreResizing,
    UxImGuiDockNodeState_HostWindowVisible,
};

// sizeof() 156~192
struct IMGUI_API UxImGuiDockNode
{
    UxImGuiID                 ID;
    UxImGuiDockNodeFlags      SharedFlags;                // (Write) Flags shared by all nodes of a same dockspace hierarchy (inherited from the root node)
    UxImGuiDockNodeFlags      LocalFlags;                 // (Write) Flags specific to this node
    UxImGuiDockNodeFlags      LocalFlagsInWindows;        // (Write) Flags specific to this node, applied from windows
    UxImGuiDockNodeFlags      MergedFlags;                // (Read)  Effective flags (== SharedFlags | LocalFlagsInNode | LocalFlagsInWindows)
    UxImGuiDockNodeState      State;
    UxImGuiDockNode*          ParentNode;
    UxImGuiDockNode*          ChildNodes[2];              // [Split node only] Child nodes (left/right or top/bottom). Consider switching to an array.
    UxImVector<UxImGuiWindow*>  Windows;                    // Note: unordered list! Iterate TabBar->Tabs for user-order.
    UxImGuiTabBar*            TabBar;
    UxImVec2                  Pos;                        // Current position
    UxImVec2                  Size;                       // Current size
    UxImVec2                  SizeRef;                    // [Split node only] Last explicitly written-to size (overridden when using a splitter affecting the node), used to calculate Size.
    UxImGuiAxis               SplitAxis;                  // [Split node only] Split axis (X or Y)
    UxImGuiWindowClass        WindowClass;                // [Root node only]
    UxImU32                   LastBgColor;

    UxImGuiWindow*            HostWindow;
    UxImGuiWindow*            VisibleWindow;              // Generally point to window which is ID is == SelectedTabID, but when CTRL+Tabbing this can be a different window.
    UxImGuiDockNode*          CentralNode;                // [Root node only] Pointer to central node.
    UxImGuiDockNode*          OnlyNodeWithWindows;        // [Root node only] Set when there is a single visible node within the hierarchy.
    int                     CountNodeWithWindows;       // [Root node only]
    int                     LastFrameAlive;             // Last frame number the node was updated or kept alive explicitly with DockSpace() + UxImGuiDockNodeFlags_KeepAliveOnly
    int                     LastFrameActive;            // Last frame number the node was updated.
    int                     LastFrameFocused;           // Last frame number the node was focused.
    UxImGuiID                 LastFocusedNodeId;          // [Root node only] Which of our child docking node (any ancestor in the hierarchy) was last focused.
    UxImGuiID                 SelectedTabId;              // [Leaf node only] Which of our tab/window is selected.
    UxImGuiID                 WantCloseTabId;             // [Leaf node only] Set when closing a specific tab/window.
    UxImGuiID                 RefViewportId;              // Reference viewport ID from visible window when HostWindow == NULL.
    UxImGuiDataAuthority      AuthorityForPos         :3;
    UxImGuiDataAuthority      AuthorityForSize        :3;
    UxImGuiDataAuthority      AuthorityForViewport    :3;
    bool                    IsVisible               :1; // Set to false when the node is hidden (usually disabled as it has no active window)
    bool                    IsFocused               :1;
    bool                    IsBgDrawnThisFrame      :1;
    bool                    HasCloseButton          :1; // Provide space for a close button (if any of the docked window has one). Note that button may be hidden on window without one.
    bool                    HasWindowMenuButton     :1;
    bool                    HasCentralNodeChild     :1;
    bool                    WantCloseAll            :1; // Set when closing all tabs at once.
    bool                    WantLockSizeOnce        :1;
    bool                    WantMouseMove           :1; // After a node extraction we need to transition toward moving the newly created host window
    bool                    WantHiddenTabBarUpdate  :1;
    bool                    WantHiddenTabBarToggle  :1;

    UxImGuiDockNode(UxImGuiID id);
    ~UxImGuiDockNode();
    bool                    IsRootNode() const      { return ParentNode == NULL; }
    bool                    IsDockSpace() const     { return (MergedFlags & UxImGuiDockNodeFlags_DockSpace) != 0; }
    bool                    IsFloatingNode() const  { return ParentNode == NULL && (MergedFlags & UxImGuiDockNodeFlags_DockSpace) == 0; }
    bool                    IsCentralNode() const   { return (MergedFlags & UxImGuiDockNodeFlags_CentralNode) != 0; }
    bool                    IsHiddenTabBar() const  { return (MergedFlags & UxImGuiDockNodeFlags_HiddenTabBar) != 0; } // Hidden tab bar can be shown back by clicking the small triangle
    bool                    IsNoTabBar() const      { return (MergedFlags & UxImGuiDockNodeFlags_NoTabBar) != 0; }     // Never show a tab bar
    bool                    IsSplitNode() const     { return ChildNodes[0] != NULL; }
    bool                    IsLeafNode() const      { return ChildNodes[0] == NULL; }
    bool                    IsEmpty() const         { return ChildNodes[0] == NULL && Windows.Size == 0; }
    UxImRect                  Rect() const            { return UxImRect(Pos.x, Pos.y, Pos.x + Size.x, Pos.y + Size.y); }

    void                    SetLocalFlags(UxImGuiDockNodeFlags flags) { LocalFlags = flags; UpdateMergedFlags(); }
    void                    UpdateMergedFlags()     { MergedFlags = SharedFlags | LocalFlags | LocalFlagsInWindows; }
};

// List of colors that are stored at the time of Begin() into Docked Windows.
// We currently store the packed colors in a simple array window->DockStyle.Colors[].
// A better solution may involve appending into a log of colors in UxImGuiContext + store offsets into those arrays in UxImGuiWindow,
// but it would be more complex as we'd need to double-buffer both as e.g. drop target may refer to window from last frame.
enum UxImGuiWindowDockStyleCol
{
    UxImGuiWindowDockStyleCol_Text,
    UxImGuiWindowDockStyleCol_TabHovered,
    UxImGuiWindowDockStyleCol_TabFocused,
    UxImGuiWindowDockStyleCol_TabSelected,
    UxImGuiWindowDockStyleCol_TabSelectedOverline,
    UxImGuiWindowDockStyleCol_TabDimmed,
    UxImGuiWindowDockStyleCol_TabDimmedSelected,
    UxImGuiWindowDockStyleCol_TabDimmedSelectedOverline,
    UxImGuiWindowDockStyleCol_COUNT
};

// We don't store style.Alpha: dock_node->LastBgColor embeds it and otherwise it would only affect the docking tab, which intuitively I would say we don't want to.
struct UxImGuiWindowDockStyle
{
    UxImU32 Colors[UxImGuiWindowDockStyleCol_COUNT];
};

struct UxImGuiDockContext
{
    UxImGuiStorage                    Nodes;          // Map ID -> UxImGuiDockNode*: Active nodes
    UxImVector<UxImGuiDockRequest>      Requests;
    UxImVector<UxImGuiDockNodeSettings> NodesSettings;
    bool                            WantFullRebuild;
    UxImGuiDockContext()              { memset(this, 0, sizeof(*this)); }
};

#endif // #ifdef IMGUI_HAS_DOCK

//-----------------------------------------------------------------------------
// [SECTION] Viewport support
//-----------------------------------------------------------------------------

// UxImGuiViewport Private/Internals fields (cardinal sin: we are using inheritance!)
// Every instance of UxImGuiViewport is in fact a UxImGuiViewportP.
struct UxImGuiViewportP : public UxImGuiViewport
{
    UxImGuiWindow*        Window;                 // Set when the viewport is owned by a window (and UxImGuiViewportFlags_CanHostOtherWindows is NOT set)
    int                 Idx;
    int                 LastFrameActive;        // Last frame number this viewport was activated by a window
    int                 LastFocusedStampCount;  // Last stamp number from when a window hosted by this viewport was focused (by comparing this value between two viewport we have an implicit viewport z-order we use as fallback)
    UxImGuiID             LastNameHash;
    UxImVec2              LastPos;
    UxImVec2              LastSize;
    float               Alpha;                  // Window opacity (when dragging dockable windows/viewports we make them transparent)
    float               LastAlpha;
    bool                LastFocusedHadNavWindow;// Instead of maintaining a LastFocusedWindow (which may harder to correctly maintain), we merely store weither NavWindow != NULL last time the viewport was focused.
    short               PlatformMonitor;
    int                 BgFgDrawListsLastFrame[2]; // Last frame number the background (0) and foreground (1) draw lists were used
    UxImDrawList*         BgFgDrawLists[2];       // Convenience background (0) and foreground (1) draw lists. We use them to draw software mouser cursor when io.MouseDrawCursor is set and to draw most debug overlays.
    UxImDrawData          DrawDataP;
    UxImDrawDataBuilder   DrawDataBuilder;        // Temporary data while building final UxImDrawData
    UxImVec2              LastPlatformPos;
    UxImVec2              LastPlatformSize;
    UxImVec2              LastRendererSize;

    // Per-viewport work area
    // - Insets are >= 0.0f values, distance from viewport corners to work area.
    // - BeginMainMenuBar() and DockspaceOverViewport() tend to use work area to avoid stepping over existing contents.
    // - Generally 'safeAreaInsets' in iOS land, 'DisplayCutout' in Android land.
    UxImVec2              WorkInsetMin;           // Work Area inset locked for the frame. GetWorkRect() always fits within GetMainRect().
    UxImVec2              WorkInsetMax;           // "
    UxImVec2              BuildWorkInsetMin;      // Work Area inset accumulator for current frame, to become next frame's WorkInset
    UxImVec2              BuildWorkInsetMax;      // "

    UxImGuiViewportP()                    { Window = NULL; Idx = -1; LastFrameActive = BgFgDrawListsLastFrame[0] = BgFgDrawListsLastFrame[1] = LastFocusedStampCount = -1; LastNameHash = 0; Alpha = LastAlpha = 1.0f; LastFocusedHadNavWindow = false; PlatformMonitor = -1; BgFgDrawLists[0] = BgFgDrawLists[1] = NULL; LastPlatformPos = LastPlatformSize = LastRendererSize = UxImVec2(FLT_MAX, FLT_MAX); }
    ~UxImGuiViewportP()                   { if (BgFgDrawLists[0]) IM_DELETE(BgFgDrawLists[0]); if (BgFgDrawLists[1]) IM_DELETE(BgFgDrawLists[1]); }
    void    ClearRequestFlags()         { PlatformRequestClose = PlatformRequestMove = PlatformRequestResize = false; }

    // Calculate work rect pos/size given a set of offset (we have 1 pair of offset for rect locked from last frame data, and 1 pair for currently building rect)
    UxImVec2  CalcWorkRectPos(const UxImVec2& inset_min) const                           { return UxImVec2(Pos.x + inset_min.x, Pos.y + inset_min.y); }
    UxImVec2  CalcWorkRectSize(const UxImVec2& inset_min, const UxImVec2& inset_max) const { return UxImVec2(UxImMax(0.0f, Size.x - inset_min.x - inset_max.x), UxImMax(0.0f, Size.y - inset_min.y - inset_max.y)); }
    void    UpdateWorkRect()            { WorkPos = CalcWorkRectPos(WorkInsetMin); WorkSize = CalcWorkRectSize(WorkInsetMin, WorkInsetMax); } // Update public fields

    // Helpers to retrieve UxImRect (we don't need to store BuildWorkRect as every access tend to change it, hence the code asymmetry)
    UxImRect  GetMainRect() const         { return UxImRect(Pos.x, Pos.y, Pos.x + Size.x, Pos.y + Size.y); }
    UxImRect  GetWorkRect() const         { return UxImRect(WorkPos.x, WorkPos.y, WorkPos.x + WorkSize.x, WorkPos.y + WorkSize.y); }
    UxImRect  GetBuildWorkRect() const    { UxImVec2 pos = CalcWorkRectPos(BuildWorkInsetMin); UxImVec2 size = CalcWorkRectSize(BuildWorkInsetMin, BuildWorkInsetMax); return UxImRect(pos.x, pos.y, pos.x + size.x, pos.y + size.y); }
};

//-----------------------------------------------------------------------------
// [SECTION] Settings support
//-----------------------------------------------------------------------------

// Windows data saved in imgui.ini file
// Because we never destroy or rename UxImGuiWindowSettings, we can store the names in a separate buffer easily.
// (this is designed to be stored in a UxImChunkStream buffer, with the variable-length Name following our structure)
struct UxImGuiWindowSettings
{
    UxImGuiID     ID;
    UxImVec2ih    Pos;            // NB: Settings position are stored RELATIVE to the viewport! Whereas runtime ones are absolute positions.
    UxImVec2ih    Size;
    UxImVec2ih    ViewportPos;
    UxImGuiID     ViewportId;
    UxImGuiID     DockId;         // ID of last known DockNode (even if the DockNode is invisible because it has only 1 active window), or 0 if none.
    UxImGuiID     ClassId;        // ID of window class if specified
    short       DockOrder;      // Order of the last time the window was visible within its DockNode. This is used to reorder windows that are reappearing on the same frame. Same value between windows that were active and windows that were none are possible.
    bool        Collapsed;
    bool        IsChild;
    bool        WantApply;      // Set when loaded from .ini data (to enable merging/loading .ini data into an already running context)
    bool        WantDelete;     // Set to invalidate/delete the settings entry

    UxImGuiWindowSettings()       { memset(this, 0, sizeof(*this)); DockOrder = -1; }
    char* GetName()             { return (char*)(this + 1); }
};

struct UxImGuiSettingsHandler
{
    const char* TypeName;       // Short description stored in .ini file. Disallowed characters: '[' ']'
    UxImGuiID     TypeHash;       // == UxImHashStr(TypeName)
    void        (*ClearAllFn)(UxImGuiContext* ctx, UxImGuiSettingsHandler* handler);                                // Clear all settings data
    void        (*ReadInitFn)(UxImGuiContext* ctx, UxImGuiSettingsHandler* handler);                                // Read: Called before reading (in registration order)
    void*       (*ReadOpenFn)(UxImGuiContext* ctx, UxImGuiSettingsHandler* handler, const char* name);              // Read: Called when entering into a new ini entry e.g. "[Window][Name]"
    void        (*ReadLineFn)(UxImGuiContext* ctx, UxImGuiSettingsHandler* handler, void* entry, const char* line); // Read: Called for every line of text within an ini entry
    void        (*ApplyAllFn)(UxImGuiContext* ctx, UxImGuiSettingsHandler* handler);                                // Read: Called after reading (in registration order)
    void        (*WriteAllFn)(UxImGuiContext* ctx, UxImGuiSettingsHandler* handler, UxImGuiTextBuffer* out_buf);      // Write: Output every entries into 'out_buf'
    void*       UserData;

    UxImGuiSettingsHandler() { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// [SECTION] Localization support
//-----------------------------------------------------------------------------

// This is experimental and not officially supported, it'll probably fall short of features, if/when it does we may backtrack.
enum UxImGuiLocKey : int
{
    UxImGuiLocKey_VersionStr,
    UxImGuiLocKey_TableSizeOne,
    UxImGuiLocKey_TableSizeAllFit,
    UxImGuiLocKey_TableSizeAllDefault,
    UxImGuiLocKey_TableResetOrder,
    UxImGuiLocKey_WindowingMainMenuBar,
    UxImGuiLocKey_WindowingPopup,
    UxImGuiLocKey_WindowingUntitled,
    UxImGuiLocKey_OpenLink_s,
    UxImGuiLocKey_CopyLink,
    UxImGuiLocKey_DockingHideTabBar,
    UxImGuiLocKey_DockingHoldShiftToDock,
    UxImGuiLocKey_DockingDragToUndockOrMoveNode,
    UxImGuiLocKey_COUNT
};

struct UxImGuiLocEntry
{
    UxImGuiLocKey     Key;
    const char*     Text;
};

//-----------------------------------------------------------------------------
// [SECTION] Error handling, State recovery support
//-----------------------------------------------------------------------------

// Macros used by Recoverable Error handling
// - Only dispatch error if _EXPR: evaluate as assert (similar to an assert macro).
// - The message will always be a string literal, in order to increase likelihood of being display by an assert handler.
// - See 'Demo->Configuration->Error Handling' and UxImGuiIO definitions for details on error handling.
// - Read https://github.com/ocornut/imgui/wiki/Error-Handling for details on error handling.
#ifndef IM_ASSERT_USER_ERROR
#define IM_ASSERT_USER_ERROR(_EXPR,_MSG)    do { if (!(_EXPR) && UxImGui::ErrorLog(_MSG)) { IM_ASSERT((_EXPR) && _MSG); } } while (0)    // Recoverable User Error
#endif

// The error callback is currently not public, as it is expected that only advanced users will rely on it.
typedef void (*UxImGuiErrorCallback)(UxImGuiContext* ctx, void* user_data, const char* msg); // Function signature for g.ErrorCallback

//-----------------------------------------------------------------------------
// [SECTION] Metrics, Debug Tools
//-----------------------------------------------------------------------------

// See IMGUI_DEBUG_LOG() and IMGUI_DEBUG_LOG_XXX() macros.
enum UxImGuiDebugLogFlags_
{
    // Event types
    UxImGuiDebugLogFlags_None                 = 0,
    UxImGuiDebugLogFlags_EventError           = 1 << 0,   // Error submitted by IM_ASSERT_USER_ERROR()
    UxImGuiDebugLogFlags_EventActiveId        = 1 << 1,
    UxImGuiDebugLogFlags_EventFocus           = 1 << 2,
    UxImGuiDebugLogFlags_EventPopup           = 1 << 3,
    UxImGuiDebugLogFlags_EventNav             = 1 << 4,
    UxImGuiDebugLogFlags_EventClipper         = 1 << 5,
    UxImGuiDebugLogFlags_EventSelection       = 1 << 6,
    UxImGuiDebugLogFlags_EventIO              = 1 << 7,
    UxImGuiDebugLogFlags_EventFont            = 1 << 8,
    UxImGuiDebugLogFlags_EventInputRouting    = 1 << 9,
    UxImGuiDebugLogFlags_EventDocking         = 1 << 10,
    UxImGuiDebugLogFlags_EventViewport        = 1 << 11,

    UxImGuiDebugLogFlags_EventMask_           = UxImGuiDebugLogFlags_EventError | UxImGuiDebugLogFlags_EventActiveId | UxImGuiDebugLogFlags_EventFocus | UxImGuiDebugLogFlags_EventPopup | UxImGuiDebugLogFlags_EventNav | UxImGuiDebugLogFlags_EventClipper | UxImGuiDebugLogFlags_EventSelection | UxImGuiDebugLogFlags_EventIO | UxImGuiDebugLogFlags_EventFont | UxImGuiDebugLogFlags_EventInputRouting | UxImGuiDebugLogFlags_EventDocking | UxImGuiDebugLogFlags_EventViewport,
    UxImGuiDebugLogFlags_OutputToTTY          = 1 << 20,  // Also send output to TTY
    UxImGuiDebugLogFlags_OutputToTestEngine   = 1 << 21,  // Also send output to Test Engine
};

struct UxImGuiDebugAllocEntry
{
    int         FrameCount;
    UxImS16       AllocCount;
    UxImS16       FreeCount;
};

struct UxImGuiDebugAllocInfo
{
    int         TotalAllocCount;            // Number of call to MemAlloc().
    int         TotalFreeCount;
    UxImS16       LastEntriesIdx;             // Current index in buffer
    UxImGuiDebugAllocEntry LastEntriesBuf[6]; // Track last 6 frames that had allocations

    UxImGuiDebugAllocInfo() { memset(this, 0, sizeof(*this)); }
};

struct UxImGuiMetricsConfig
{
    bool        ShowDebugLog = false;
    bool        ShowIDStackTool = false;
    bool        ShowWindowsRects = false;
    bool        ShowWindowsBeginOrder = false;
    bool        ShowTablesRects = false;
    bool        ShowDrawCmdMesh = true;
    bool        ShowDrawCmdBoundingBoxes = true;
    bool        ShowTextEncodingViewer = false;
    bool        ShowTextureUsedRect = false;
    bool        ShowDockingNodes = false;
    int         ShowWindowsRectsType = -1;
    int         ShowTablesRectsType = -1;
    int         HighlightMonitorIdx = -1;
    UxImGuiID     HighlightViewportID = 0;
    bool        ShowFontPreview = true;
};

struct UxImGuiStackLevelInfo
{
    UxImGuiID                 ID;
    UxImS8                    QueryFrameCount;            // >= 1: Query in progress
    bool                    QuerySuccess;               // Obtained result from DebugHookIdInfo()
    UxImGuiDataType           DataType : 8;
    char                    Desc[57];                   // Arbitrarily sized buffer to hold a result (FIXME: could replace Results[] with a chunk stream?) FIXME: Now that we added CTRL+C this should be fixed.

    UxImGuiStackLevelInfo()   { memset(this, 0, sizeof(*this)); }
};

// State for ID Stack tool queries
struct UxImGuiIDStackTool
{
    int                     LastActiveFrame;
    int                     StackLevel;                 // -1: query stack and resize Results, >= 0: individual stack level
    UxImGuiID                 QueryId;                    // ID to query details for
    UxImVector<UxImGuiStackLevelInfo> Results;
    bool                    CopyToClipboardOnCtrlC;
    float                   CopyToClipboardLastTime;
    UxImGuiTextBuffer         ResultPathBuf;

    UxImGuiIDStackTool()      { memset(this, 0, sizeof(*this)); CopyToClipboardLastTime = -FLT_MAX; }
};

//-----------------------------------------------------------------------------
// [SECTION] Generic context hooks
//-----------------------------------------------------------------------------

typedef void (*UxImGuiContextHookCallback)(UxImGuiContext* ctx, UxImGuiContextHook* hook);
enum UxImGuiContextHookType { UxImGuiContextHookType_NewFramePre, UxImGuiContextHookType_NewFramePost, UxImGuiContextHookType_EndFramePre, UxImGuiContextHookType_EndFramePost, UxImGuiContextHookType_RenderPre, UxImGuiContextHookType_RenderPost, UxImGuiContextHookType_Shutdown, UxImGuiContextHookType_PendingRemoval_ };

struct UxImGuiContextHook
{
    UxImGuiID                     HookId;     // A unique ID assigned by AddContextHook()
    UxImGuiContextHookType        Type;
    UxImGuiID                     Owner;
    UxImGuiContextHookCallback    Callback;
    void*                       UserData;

    UxImGuiContextHook()          { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// [SECTION] UxImGuiContext (main Dear UxImGui context)
//-----------------------------------------------------------------------------

struct UxImGuiContext
{
    bool                    Initialized;
    UxImGuiIO                 IO;
    UxImGuiPlatformIO         PlatformIO;
    UxImGuiStyle              Style;
    UxImGuiConfigFlags        ConfigFlagsCurrFrame;               // = g.IO.ConfigFlags at the time of NewFrame()
    UxImGuiConfigFlags        ConfigFlagsLastFrame;
    UxImVector<UxImFontAtlas*>  FontAtlases;                        // List of font atlases used by the context (generally only contains g.IO.Fonts aka the main font atlas)
    UxImFont*                 Font;                               // Currently bound font. (== FontStack.back().Font)
    UxImFontBaked*            FontBaked;                          // Currently bound font at currently bound size. (== Font->GetFontBaked(FontSize))
    float                   FontSize;                           // Currently bound font size == line height (== FontSizeBeforeScaling * io.FontGlobalScale * font->Scale * g.CurrentWindow->FontWindowScale).
    float                   FontSizeBeforeScaling;              // == value passed to PushFontSize()
    float                   FontScale;                          // == FontBaked->Size / Font->FontSize. Scale factor over baked size.
    float                   FontRasterizerDensity;              // Current font density. Used by all calls to GetFontBaked().
    float                   CurrentDpiScale;                    // Current window/viewport DpiScale == CurrentViewport->DpiScale
    UxImDrawListSharedData    DrawListSharedData;
    double                  Time;
    int                     FrameCount;
    int                     FrameCountEnded;
    int                     FrameCountPlatformEnded;
    int                     FrameCountRendered;
    UxImGuiID                 WithinEndChildID;                   // Set within EndChild()
    bool                    WithinFrameScope;                   // Set by NewFrame(), cleared by EndFrame()
    bool                    WithinFrameScopeWithImplicitWindow; // Set by NewFrame(), cleared by EndFrame() when the implicit debug window has been pushed
    bool                    GcCompactAll;                       // Request full GC
    bool                    TestEngineHookItems;                // Will call test engine hooks: UxImGuiTestEngineHook_ItemAdd(), UxImGuiTestEngineHook_ItemInfo(), UxImGuiTestEngineHook_Log()
    void*                   TestEngine;                         // Test engine user data
    char                    ContextName[16];                    // Storage for a context name (to facilitate debugging multi-context setups)

    // Inputs
    UxImVector<UxImGuiInputEvent> InputEventsQueue;                 // Input events which will be trickled/written into IO structure.
    UxImVector<UxImGuiInputEvent> InputEventsTrail;                 // Past input events processed in NewFrame(). This is to allow domain-specific application to access e.g mouse/pen trail.
    UxImGuiMouseSource        InputEventsNextMouseSource;
    UxImU32                   InputEventsNextEventId;

    // Windows state
    UxImVector<UxImGuiWindow*>  Windows;                            // Windows, sorted in display order, back to front
    UxImVector<UxImGuiWindow*>  WindowsFocusOrder;                  // Root windows, sorted in focus order, back to front.
    UxImVector<UxImGuiWindow*>  WindowsTempSortBuffer;              // Temporary buffer used in EndFrame() to reorder windows so parents are kept before their child
    UxImVector<UxImGuiWindowStackData> CurrentWindowStack;
    UxImGuiStorage            WindowsById;                        // Map window's UxImGuiID to UxImGuiWindow*
    int                     WindowsActiveCount;                 // Number of unique windows submitted by frame
    float                   WindowsBorderHoverPadding;          // Padding around resizable windows for which hovering on counts as hovering the window == UxImMax(style.TouchExtraPadding, style.WindowBorderHoverPadding). This isn't so multi-dpi friendly.
    UxImGuiID                 DebugBreakInWindow;                 // Set to break in Begin() call.
    UxImGuiWindow*            CurrentWindow;                      // Window being drawn into
    UxImGuiWindow*            HoveredWindow;                      // Window the mouse is hovering. Will typically catch mouse inputs.
    UxImGuiWindow*            HoveredWindowUnderMovingWindow;     // Hovered window ignoring MovingWindow. Only set if MovingWindow is set.
    UxImGuiWindow*            HoveredWindowBeforeClear;           // Window the mouse is hovering. Filled even with _NoMouse. This is currently useful for multi-context compositors.
    UxImGuiWindow*            MovingWindow;                       // Track the window we clicked on (in order to preserve focus). The actual window that is moved is generally MovingWindow->RootWindowDockTree.
    UxImGuiWindow*            WheelingWindow;                     // Track the window we started mouse-wheeling on. Until a timer elapse or mouse has moved, generally keep scrolling the same window even if during the course of scrolling the mouse ends up hovering a child window.
    UxImVec2                  WheelingWindowRefMousePos;
    int                     WheelingWindowStartFrame;           // This may be set one frame before WheelingWindow is != NULL
    int                     WheelingWindowScrolledFrame;
    float                   WheelingWindowReleaseTimer;
    UxImVec2                  WheelingWindowWheelRemainder;
    UxImVec2                  WheelingAxisAvg;

    // Item/widgets state and tracking information
    UxImGuiID                 DebugDrawIdConflicts;               // Set when we detect multiple items with the same identifier
    UxImGuiID                 DebugHookIdInfo;                    // Will call core hooks: DebugHookIdInfo() from GetID functions, used by ID Stack Tool [next HoveredId/ActiveId to not pull in an extra cache-line]
    UxImGuiID                 HoveredId;                          // Hovered widget, filled during the frame
    UxImGuiID                 HoveredIdPreviousFrame;
    int                     HoveredIdPreviousFrameItemCount;    // Count numbers of items using the same ID as last frame's hovered id
    float                   HoveredIdTimer;                     // Measure contiguous hovering time
    float                   HoveredIdNotActiveTimer;            // Measure contiguous hovering time where the item has not been active
    bool                    HoveredIdAllowOverlap;
    bool                    HoveredIdIsDisabled;                // At least one widget passed the rect test, but has been discarded by disabled flag or popup inhibit. May be true even if HoveredId == 0.
    bool                    ItemUnclipByLog;                    // Disable ItemAdd() clipping, essentially a memory-locality friendly copy of LogEnabled
    UxImGuiID                 ActiveId;                           // Active widget
    UxImGuiID                 ActiveIdIsAlive;                    // Active widget has been seen this frame (we can't use a bool as the ActiveId may change within the frame)
    float                   ActiveIdTimer;
    bool                    ActiveIdIsJustActivated;            // Set at the time of activation for one frame
    bool                    ActiveIdAllowOverlap;               // Active widget allows another widget to steal active id (generally for overlapping widgets, but not always)
    bool                    ActiveIdNoClearOnFocusLoss;         // Disable losing active id if the active id window gets unfocused.
    bool                    ActiveIdHasBeenPressedBefore;       // Track whether the active id led to a press (this is to allow changing between PressOnClick and PressOnRelease without pressing twice). Used by range_select branch.
    bool                    ActiveIdHasBeenEditedBefore;        // Was the value associated to the widget Edited over the course of the Active state.
    bool                    ActiveIdHasBeenEditedThisFrame;
    bool                    ActiveIdFromShortcut;
    int                     ActiveIdMouseButton : 8;
    UxImVec2                  ActiveIdClickOffset;                // Clicked offset from upper-left corner, if applicable (currently only set by ButtonBehavior)
    UxImGuiWindow*            ActiveIdWindow;
    UxImGuiInputSource        ActiveIdSource;                     // Activating source: UxImGuiInputSource_Mouse OR UxImGuiInputSource_Keyboard OR UxImGuiInputSource_Gamepad
    UxImGuiID                 ActiveIdPreviousFrame;
    UxImGuiDeactivatedItemData DeactivatedItemData;
    UxImGuiDataTypeStorage    ActiveIdValueOnActivation;          // Backup of initial value at the time of activation. ONLY SET BY SPECIFIC WIDGETS: DragXXX and SliderXXX.
    UxImGuiID                 LastActiveId;                       // Store the last non-zero ActiveId, useful for animation.
    float                   LastActiveIdTimer;                  // Store the last non-zero ActiveId timer since the beginning of activation, useful for animation.

    // Key/Input Ownership + Shortcut Routing system
    // - The idea is that instead of "eating" a given key, we can link to an owner.
    // - Input query can then read input by specifying UxImGuiKeyOwner_Any (== 0), UxImGuiKeyOwner_NoOwner (== -1) or a custom ID.
    // - Routing is requested ahead of time for a given chord (Key + Mods) and granted in NewFrame().
    double                  LastKeyModsChangeTime;              // Record the last time key mods changed (affect repeat delay when using shortcut logic)
    double                  LastKeyModsChangeFromNoneTime;      // Record the last time key mods changed away from being 0 (affect repeat delay when using shortcut logic)
    double                  LastKeyboardKeyPressTime;           // Record the last time a keyboard key (ignore mouse/gamepad ones) was pressed.
    UxImBitArrayForNamedKeys  KeysMayBeCharInput;                 // Lookup to tell if a key can emit char input, see IsKeyChordPotentiallyCharInput(). sizeof() = 20 bytes
    UxImGuiKeyOwnerData       KeysOwnerData[UxImGuiKey_NamedKey_COUNT];
    UxImGuiKeyRoutingTable    KeysRoutingTable;
    UxImU32                   ActiveIdUsingNavDirMask;            // Active widget will want to read those nav move requests (e.g. can activate a button and move away from it)
    bool                    ActiveIdUsingAllKeyboardKeys;       // Active widget will want to read all keyboard keys inputs. (this is a shortcut for not taking ownership of 100+ keys, frequently used by drag operations)
    UxImGuiKeyChord           DebugBreakInShortcutRouting;        // Set to break in SetShortcutRouting()/Shortcut() calls.
    //UxImU32                 ActiveIdUsingNavInputMask;          // [OBSOLETE] Since (IMGUI_VERSION_NUM >= 18804) : 'g.ActiveIdUsingNavInputMask |= (1 << UxImGuiNavInput_Cancel);' becomes --> 'SetKeyOwner(UxImGuiKey_Escape, g.ActiveId) and/or SetKeyOwner(UxImGuiKey_NavGamepadCancel, g.ActiveId);'

    // Next window/item data
    UxImGuiID                 CurrentFocusScopeId;                // Value for currently appending items == g.FocusScopeStack.back(). Not to be mistaken with g.NavFocusScopeId.
    UxImGuiItemFlags          CurrentItemFlags;                   // Value for currently appending items == g.ItemFlagsStack.back()
    UxImGuiID                 DebugLocateId;                      // Storage for DebugLocateItemOnHover() feature: this is read by ItemAdd() so we keep it in a hot/cached location
    UxImGuiNextItemData       NextItemData;                       // Storage for SetNextItem** functions
    UxImGuiLastItemData       LastItemData;                       // Storage for last submitted item (setup by ItemAdd)
    UxImGuiNextWindowData     NextWindowData;                     // Storage for SetNextWindow** functions
    bool                    DebugShowGroupRects;

    // Shared stacks
    UxImGuiCol                        DebugFlashStyleColorIdx;    // (Keep close to ColorStack to share cache line)
    UxImVector<UxImGuiColorMod>         ColorStack;                 // Stack for PushStyleColor()/PopStyleColor() - inherited by Begin()
    UxImVector<UxImGuiStyleMod>         StyleVarStack;              // Stack for PushStyleVar()/PopStyleVar() - inherited by Begin()
    UxImVector<UxImFontStackData>       FontStack;                  // Stack for PushFont()/PopFont() - inherited by Begin()
    UxImVector<UxImGuiFocusScopeData>   FocusScopeStack;            // Stack for PushFocusScope()/PopFocusScope() - inherited by BeginChild(), pushed into by Begin()
    UxImVector<UxImGuiItemFlags>        ItemFlagsStack;             // Stack for PushItemFlag()/PopItemFlag() - inherited by Begin()
    UxImVector<UxImGuiGroupData>        GroupStack;                 // Stack for BeginGroup()/EndGroup() - not inherited by Begin()
    UxImVector<UxImGuiPopupData>        OpenPopupStack;             // Which popups are open (persistent)
    UxImVector<UxImGuiPopupData>        BeginPopupStack;            // Which level of BeginPopup() we are in (reset every frame)
    UxImVector<UxImGuiTreeNodeStackData>TreeNodeStack;              // Stack for TreeNode()

    // Viewports
    UxImVector<UxImGuiViewportP*> Viewports;                        // Active viewports (always 1+, and generally 1 unless multi-viewports are enabled). Each viewports hold their copy of UxImDrawData.
    UxImGuiViewportP*         CurrentViewport;                    // We track changes of viewport (happening in Begin) so we can call Platform_OnChangedViewport()
    UxImGuiViewportP*         MouseViewport;
    UxImGuiViewportP*         MouseLastHoveredViewport;           // Last known viewport that was hovered by mouse (even if we are not hovering any viewport any more) + honoring the _NoInputs flag.
    UxImGuiID                 PlatformLastFocusedViewportId;
    UxImGuiPlatformMonitor    FallbackMonitor;                    // Virtual monitor used as fallback if backend doesn't provide monitor information.
    UxImRect                  PlatformMonitorsFullWorkRect;       // Bounding box of all platform monitors
    int                     ViewportCreatedCount;               // Unique sequential creation counter (mostly for testing/debugging)
    int                     PlatformWindowsCreatedCount;        // Unique sequential creation counter (mostly for testing/debugging)
    int                     ViewportFocusedStampCount;          // Every time the front-most window changes, we stamp its viewport with an incrementing counter

    // Keyboard/Gamepad Navigation
    bool                    NavCursorVisible;                   // Nav focus cursor/rectangle is visible? We hide it after a mouse click. We show it after a nav move.
    bool                    NavHighlightItemUnderNav;           // Disable mouse hovering highlight. Highlight navigation focused item instead of mouse hovered item.
    //bool                  NavDisableHighlight;                // Old name for !g.NavCursorVisible before 1.91.4 (2024/10/18). OPPOSITE VALUE (g.NavDisableHighlight == !g.NavCursorVisible)
    //bool                  NavDisableMouseHover;               // Old name for g.NavHighlightItemUnderNav before 1.91.1 (2024/10/18) this was called When user starts using keyboard/gamepad, we hide mouse hovering highlight until mouse is touched again.
    bool                    NavMousePosDirty;                   // When set we will update mouse position if io.ConfigNavMoveSetMousePos is set (not enabled by default)
    bool                    NavIdIsAlive;                       // Nav widget has been seen this frame ~~ NavRectRel is valid
    UxImGuiID                 NavId;                              // Focused item for navigation
    UxImGuiWindow*            NavWindow;                          // Focused window for navigation. Could be called 'FocusedWindow'
    UxImGuiID                 NavFocusScopeId;                    // Focused focus scope (e.g. selection code often wants to "clear other items" when landing on an item of the same scope)
    UxImGuiNavLayer           NavLayer;                           // Focused layer (main scrolling layer, or menu/title bar layer)
    UxImGuiID                 NavActivateId;                      // ~~ (g.ActiveId == 0) && (IsKeyPressed(UxImGuiKey_Space) || IsKeyDown(UxImGuiKey_Enter) || IsKeyPressed(UxImGuiKey_NavGamepadActivate)) ? NavId : 0, also set when calling ActivateItem()
    UxImGuiID                 NavActivateDownId;                  // ~~ IsKeyDown(UxImGuiKey_Space) || IsKeyDown(UxImGuiKey_Enter) || IsKeyDown(UxImGuiKey_NavGamepadActivate) ? NavId : 0
    UxImGuiID                 NavActivatePressedId;               // ~~ IsKeyPressed(UxImGuiKey_Space) || IsKeyPressed(UxImGuiKey_Enter) || IsKeyPressed(UxImGuiKey_NavGamepadActivate) ? NavId : 0 (no repeat)
    UxImGuiActivateFlags      NavActivateFlags;
    UxImVector<UxImGuiFocusScopeData> NavFocusRoute;                // Reversed copy focus scope stack for NavId (should contains NavFocusScopeId). This essentially follow the window->ParentWindowForFocusRoute chain.
    UxImGuiID                 NavHighlightActivatedId;
    float                   NavHighlightActivatedTimer;
    UxImGuiID                 NavNextActivateId;                  // Set by ActivateItem(), queued until next frame.
    UxImGuiActivateFlags      NavNextActivateFlags;
    UxImGuiInputSource        NavInputSource;                     // Keyboard or Gamepad mode? THIS CAN ONLY BE UxImGuiInputSource_Keyboard or UxImGuiInputSource_Mouse
    UxImGuiSelectionUserData  NavLastValidSelectionUserData;      // Last valid data passed to SetNextItemSelectionUser(), or -1. For current window. Not reset when focusing an item that doesn't have selection data.
    UxImS8                    NavCursorHideFrames;

    // Navigation: Init & Move Requests
    bool                    NavAnyRequest;                      // ~~ NavMoveRequest || NavInitRequest this is to perform early out in ItemAdd()
    bool                    NavInitRequest;                     // Init request for appearing window to select first item
    bool                    NavInitRequestFromMove;
    UxImGuiNavItemData        NavInitResult;                      // Init request result (first item of the window, or one for which SetItemDefaultFocus() was called)
    bool                    NavMoveSubmitted;                   // Move request submitted, will process result on next NewFrame()
    bool                    NavMoveScoringItems;                // Move request submitted, still scoring incoming items
    bool                    NavMoveForwardToNextFrame;
    UxImGuiNavMoveFlags       NavMoveFlags;
    UxImGuiScrollFlags        NavMoveScrollFlags;
    UxImGuiKeyChord           NavMoveKeyMods;
    UxImGuiDir                NavMoveDir;                         // Direction of the move request (left/right/up/down)
    UxImGuiDir                NavMoveDirForDebug;
    UxImGuiDir                NavMoveClipDir;                     // FIXME-NAV: Describe the purpose of this better. Might want to rename?
    UxImRect                  NavScoringRect;                     // Rectangle used for scoring, in screen space. Based of window->NavRectRel[], modified for directional navigation scoring.
    UxImRect                  NavScoringNoClipRect;               // Some nav operations (such as PageUp/PageDown) enforce a region which clipper will attempt to always keep submitted
    int                     NavScoringDebugCount;               // Metrics for debugging
    int                     NavTabbingDir;                      // Generally -1 or +1, 0 when tabbing without a nav id
    int                     NavTabbingCounter;                  // >0 when counting items for tabbing
    UxImGuiNavItemData        NavMoveResultLocal;                 // Best move request candidate within NavWindow
    UxImGuiNavItemData        NavMoveResultLocalVisible;          // Best move request candidate within NavWindow that are mostly visible (when using UxImGuiNavMoveFlags_AlsoScoreVisibleSet flag)
    UxImGuiNavItemData        NavMoveResultOther;                 // Best move request candidate within NavWindow's flattened hierarchy (when using UxImGuiWindowFlags_NavFlattened flag)
    UxImGuiNavItemData        NavTabbingResultFirst;              // First tabbing request candidate within NavWindow and flattened hierarchy

    // Navigation: record of last move request
    UxImGuiID                 NavJustMovedFromFocusScopeId;       // Just navigated from this focus scope id (result of a successfully MoveRequest).
    UxImGuiID                 NavJustMovedToId;                   // Just navigated to this id (result of a successfully MoveRequest).
    UxImGuiID                 NavJustMovedToFocusScopeId;         // Just navigated to this focus scope id (result of a successfully MoveRequest).
    UxImGuiKeyChord           NavJustMovedToKeyMods;
    bool                    NavJustMovedToIsTabbing;            // Copy of UxImGuiNavMoveFlags_IsTabbing. Maybe we should store whole flags.
    bool                    NavJustMovedToHasSelectionData;     // Copy of move result's ItemFlags & UxImGuiItemFlags_HasSelectionUserData). Maybe we should just store UxImGuiNavItemData.

    // Navigation: Windowing (CTRL+TAB for list, or Menu button + keys or directional pads to move/resize)
    bool                    ConfigNavWindowingWithGamepad;      // = true. Enable CTRL+TAB by holding UxImGuiKey_GamepadFaceLeft (== UxImGuiKey_NavGamepadMenu). When false, the button may still be used to toggle Menu layer.
    UxImGuiKeyChord           ConfigNavWindowingKeyNext;          // = UxImGuiMod_Ctrl | UxImGuiKey_Tab (or UxImGuiMod_Super | UxImGuiKey_Tab on OS X). For reconfiguration (see #4828)
    UxImGuiKeyChord           ConfigNavWindowingKeyPrev;          // = UxImGuiMod_Ctrl | UxImGuiMod_Shift | UxImGuiKey_Tab (or UxImGuiMod_Super | UxImGuiMod_Shift | UxImGuiKey_Tab on OS X)
    UxImGuiWindow*            NavWindowingTarget;                 // Target window when doing CTRL+Tab (or Pad Menu + FocusPrev/Next), this window is temporarily displayed top-most!
    UxImGuiWindow*            NavWindowingTargetAnim;             // Record of last valid NavWindowingTarget until DimBgRatio and NavWindowingHighlightAlpha becomes 0.0f, so the fade-out can stay on it.
    UxImGuiWindow*            NavWindowingListWindow;             // Internal window actually listing the CTRL+Tab contents
    float                   NavWindowingTimer;
    float                   NavWindowingHighlightAlpha;
    UxImGuiInputSource        NavWindowingInputSource;
    bool                    NavWindowingToggleLayer;
    UxImGuiKey                NavWindowingToggleKey;
    UxImVec2                  NavWindowingAccumDeltaPos;
    UxImVec2                  NavWindowingAccumDeltaSize;

    // Render
    float                   DimBgRatio;                         // 0.0..1.0 animation when fading in a dimming background (for modal window and CTRL+TAB list)

    // Drag and Drop
    bool                    DragDropActive;
    bool                    DragDropWithinSource;               // Set when within a BeginDragDropXXX/EndDragDropXXX block for a drag source.
    bool                    DragDropWithinTarget;               // Set when within a BeginDragDropXXX/EndDragDropXXX block for a drag target.
    UxImGuiDragDropFlags      DragDropSourceFlags;
    int                     DragDropSourceFrameCount;
    int                     DragDropMouseButton;
    UxImGuiPayload            DragDropPayload;
    UxImRect                  DragDropTargetRect;                 // Store rectangle of current target candidate (we favor small targets when overlapping)
    UxImRect                  DragDropTargetClipRect;             // Store ClipRect at the time of item's drawing
    UxImGuiID                 DragDropTargetId;
    UxImGuiDragDropFlags      DragDropAcceptFlags;
    float                   DragDropAcceptIdCurrRectSurface;    // Target item surface (we resolve overlapping targets by prioritizing the smaller surface)
    UxImGuiID                 DragDropAcceptIdCurr;               // Target item id (set at the time of accepting the payload)
    UxImGuiID                 DragDropAcceptIdPrev;               // Target item id from previous frame (we need to store this to allow for overlapping drag and drop targets)
    int                     DragDropAcceptFrameCount;           // Last time a target expressed a desire to accept the source
    UxImGuiID                 DragDropHoldJustPressedId;          // Set when holding a payload just made ButtonBehavior() return a press.
    UxImVector<unsigned char> DragDropPayloadBufHeap;             // We don't expose the UxImVector<> directly, UxImGuiPayload only holds pointer+size
    unsigned char           DragDropPayloadBufLocal[16];        // Local buffer for small payloads

    // Clipper
    int                             ClipperTempDataStacked;
    UxImVector<UxImGuiListClipperData>  ClipperTempData;

    // Tables
    UxImGuiTable*                     CurrentTable;
    UxImGuiID                         DebugBreakInTable;          // Set to break in BeginTable() call.
    int                             TablesTempDataStacked;      // Temporary table data size (because we leave previous instances undestructed, we generally don't use TablesTempData.Size)
    UxImVector<UxImGuiTableTempData>    TablesTempData;             // Temporary table data (buffers reused/shared across instances, support nesting)
    UxImPool<UxImGuiTable>              Tables;                     // Persistent table data
    UxImVector<float>                 TablesLastTimeActive;       // Last used timestamp of each tables (SOA, for efficient GC)
    UxImVector<UxImDrawChannel>         DrawChannelsTempMergeBuffer;

    // Tab bars
    UxImGuiTabBar*                    CurrentTabBar;
    UxImPool<UxImGuiTabBar>             TabBars;
    UxImVector<UxImGuiPtrOrIndex>       CurrentTabBarStack;
    UxImVector<UxImGuiShrinkWidthItem>  ShrinkWidthBuffer;

    // Multi-Select state
    UxImGuiBoxSelectState             BoxSelectState;
    UxImGuiMultiSelectTempData*       CurrentMultiSelect;
    int                             MultiSelectTempDataStacked; // Temporary multi-select data size (because we leave previous instances undestructed, we generally don't use MultiSelectTempData.Size)
    UxImVector<UxImGuiMultiSelectTempData> MultiSelectTempData;
    UxImPool<UxImGuiMultiSelectState>   MultiSelectStorage;

    // Hover Delay system
    UxImGuiID                 HoverItemDelayId;
    UxImGuiID                 HoverItemDelayIdPreviousFrame;
    float                   HoverItemDelayTimer;                // Currently used by IsItemHovered()
    float                   HoverItemDelayClearTimer;           // Currently used by IsItemHovered(): grace time before g.TooltipHoverTimer gets cleared.
    UxImGuiID                 HoverItemUnlockedStationaryId;      // Mouse has once been stationary on this item. Only reset after departing the item.
    UxImGuiID                 HoverWindowUnlockedStationaryId;    // Mouse has once been stationary on this window. Only reset after departing the window.

    // Mouse state
    UxImGuiMouseCursor        MouseCursor;
    float                   MouseStationaryTimer;               // Time the mouse has been stationary (with some loose heuristic)
    UxImVec2                  MouseLastValidPos;

    // Widget state
    UxImGuiInputTextState     InputTextState;
    UxImGuiInputTextDeactivatedState InputTextDeactivatedState;
    UxImFontBaked             InputTextPasswordFontBackupBaked;
    UxImFontFlags             InputTextPasswordFontBackupFlags;
    UxImGuiID                 TempInputId;                        // Temporary text input when CTRL+clicking on a slider, etc.
    UxImGuiDataTypeStorage    DataTypeZeroValue;                  // 0 for all data types
    int                     BeginMenuDepth;
    int                     BeginComboDepth;
    UxImGuiColorEditFlags     ColorEditOptions;                   // Store user options for color edit widgets
    UxImGuiID                 ColorEditCurrentID;                 // Set temporarily while inside of the parent-most ColorEdit4/ColorPicker4 (because they call each others).
    UxImGuiID                 ColorEditSavedID;                   // ID we are saving/restoring HS for
    float                   ColorEditSavedHue;                  // Backup of last Hue associated to LastColor, so we can restore Hue in lossy RGB<>HSV round trips
    float                   ColorEditSavedSat;                  // Backup of last Saturation associated to LastColor, so we can restore Saturation in lossy RGB<>HSV round trips
    UxImU32                   ColorEditSavedColor;                // RGB value with alpha set to 0.
    UxImVec4                  ColorPickerRef;                     // Initial/reference color at the time of opening the color picker.
    UxImGuiComboPreviewData   ComboPreviewData;
    UxImRect                  WindowResizeBorderExpectedRect;     // Expected border rect, switch to relative edit if moving
    bool                    WindowResizeRelativeMode;
    short                   ScrollbarSeekMode;                  // 0: scroll to clicked location, -1/+1: prev/next page.
    float                   ScrollbarClickDeltaToGrabCenter;    // When scrolling to mouse location: distance between mouse and center of grab box, normalized in parent space.
    float                   SliderGrabClickOffset;
    float                   SliderCurrentAccum;                 // Accumulated slider delta when using navigation controls.
    bool                    SliderCurrentAccumDirty;            // Has the accumulated slider delta changed since last time we tried to apply it?
    bool                    DragCurrentAccumDirty;
    float                   DragCurrentAccum;                   // Accumulator for dragging modification. Always high-precision, not rounded by end-user precision settings
    float                   DragSpeedDefaultRatio;              // If speed == 0.0f, uses (max-min) * DragSpeedDefaultRatio
    float                   DisabledAlphaBackup;                // Backup for style.Alpha for BeginDisabled()
    short                   DisabledStackSize;
    short                   TooltipOverrideCount;
    UxImGuiWindow*            TooltipPreviousWindow;              // Window of last tooltip submitted during the frame
    UxImVector<char>          ClipboardHandlerData;               // If no custom clipboard handler is defined
    UxImVector<UxImGuiID>       MenusIdSubmittedThisFrame;          // A list of menu IDs that were rendered at least once
    UxImGuiTypingSelectState  TypingSelectState;                  // State for GetTypingSelectRequest()

    // Platform support
    UxImGuiPlatformImeData    PlatformImeData;                    // Data updated by current frame. Will be applied at end of the frame. For some backends, this is required to have WantVisible=true in order to receive text message.
    UxImGuiPlatformImeData    PlatformImeDataPrev;                // Previous frame data. When changed we call the platform_io.Platform_SetImeDataFn() handler.

    // Extensions
    // FIXME: We could provide an API to register one slot in an array held in UxImGuiContext?
    UxImVector<UxImTextureData*> UserTextures;                      // List of textures created/managed by user or third-party extension. Automatically appended into platform_io.Textures[].
    UxImGuiDockContext        DockContext;
    void                    (*DockNodeWindowMenuHandler)(UxImGuiContext* ctx, UxImGuiDockNode* node, UxImGuiTabBar* tab_bar);

    // Settings
    bool                    SettingsLoaded;
    float                   SettingsDirtyTimer;                 // Save .ini Settings to memory when time reaches zero
    UxImGuiTextBuffer         SettingsIniData;                    // In memory .ini settings
    UxImVector<UxImGuiSettingsHandler>      SettingsHandlers;       // List of .ini settings handlers
    UxImChunkStream<UxImGuiWindowSettings>  SettingsWindows;        // UxImGuiWindow .ini settings entries
    UxImChunkStream<UxImGuiTableSettings>   SettingsTables;         // UxImGuiTable .ini settings entries
    UxImVector<UxImGuiContextHook>          Hooks;                  // Hooks for extensions (e.g. test engine)
    UxImGuiID                             HookIdNext;             // Next available HookId

    // Localization
    const char*             LocalizationTable[UxImGuiLocKey_COUNT];

    // Capture/Logging
    bool                    LogEnabled;                         // Currently capturing
    UxImGuiLogFlags           LogFlags;                           // Capture flags/type
    UxImGuiWindow*            LogWindow;
    UxImFileHandle            LogFile;                            // If != NULL log to stdout/ file
    UxImGuiTextBuffer         LogBuffer;                          // Accumulation buffer when log to clipboard. This is pointer so our GUxImGui static constructor doesn't call heap allocators.
    const char*             LogNextPrefix;
    const char*             LogNextSuffix;
    float                   LogLinePosY;
    bool                    LogLineFirstItem;
    int                     LogDepthRef;
    int                     LogDepthToExpand;
    int                     LogDepthToExpandDefault;            // Default/stored value for LogDepthMaxExpand if not specified in the LogXXX function call.

    // Error Handling
    UxImGuiErrorCallback      ErrorCallback;                      // = NULL. May be exposed in public API eventually.
    void*                   ErrorCallbackUserData;              // = NULL
    UxImVec2                  ErrorTooltipLockedPos;
    bool                    ErrorFirst;
    int                     ErrorCountCurrentFrame;             // [Internal] Number of errors submitted this frame.
    UxImGuiErrorRecoveryState StackSizesInNewFrame;               // [Internal]
    UxImGuiErrorRecoveryState*StackSizesInBeginForCurrentWindow;  // [Internal]

    // Debug Tools
    // (some of the highly frequently used data are interleaved in other structures above: DebugBreakXXX fields, DebugHookIdInfo, DebugLocateId etc.)
    int                     DebugDrawIdConflictsCount;          // Locked count (preserved when holding CTRL)
    UxImGuiDebugLogFlags      DebugLogFlags;
    UxImGuiTextBuffer         DebugLogBuf;
    UxImGuiTextIndex          DebugLogIndex;
    int                     DebugLogSkippedErrors;
    UxImGuiDebugLogFlags      DebugLogAutoDisableFlags;
    UxImU8                    DebugLogAutoDisableFrames;
    UxImU8                    DebugLocateFrames;                  // For DebugLocateItemOnHover(). This is used together with DebugLocateId which is in a hot/cached spot above.
    bool                    DebugBreakInLocateId;               // Debug break in ItemAdd() call for g.DebugLocateId.
    UxImGuiKeyChord           DebugBreakKeyChord;                 // = UxImGuiKey_Pause
    UxImS8                    DebugBeginReturnValueCullDepth;     // Cycle between 0..9 then wrap around.
    bool                    DebugItemPickerActive;              // Item picker is active (started with DebugStartItemPicker())
    UxImU8                    DebugItemPickerMouseButton;
    UxImGuiID                 DebugItemPickerBreakId;             // Will call IM_DEBUG_BREAK() when encountering this ID
    float                   DebugFlashStyleColorTime;
    UxImVec4                  DebugFlashStyleColorBackup;
    UxImGuiMetricsConfig      DebugMetricsConfig;
    UxImGuiIDStackTool        DebugIDStackTool;
    UxImGuiDebugAllocInfo     DebugAllocInfo;
    UxImGuiDockNode*          DebugHoveredDockNode;               // Hovered dock node.

    // Misc
    float                   FramerateSecPerFrame[60];           // Calculate estimate of framerate for user over the last 60 frames..
    int                     FramerateSecPerFrameIdx;
    int                     FramerateSecPerFrameCount;
    float                   FramerateSecPerFrameAccum;
    int                     WantCaptureMouseNextFrame;          // Explicit capture override via SetNextFrameWantCaptureMouse()/SetNextFrameWantCaptureKeyboard(). Default to -1.
    int                     WantCaptureKeyboardNextFrame;       // "
    int                     WantTextInputNextFrame;             // Copied in EndFrame() from g.PlatformImeData.WanttextInput. Needs to be set for some backends (SDL3) to emit character inputs.
    UxImVector<char>          TempBuffer;                         // Temporary text buffer
    char                    TempKeychordName[64];

    UxImGuiContext(UxImFontAtlas* shared_font_atlas);
};

//-----------------------------------------------------------------------------
// [SECTION] UxImGuiWindowTempData, UxImGuiWindow
//-----------------------------------------------------------------------------

// Transient per-window data, reset at the beginning of the frame. This used to be called UxImGuiDrawContext, hence the DC variable name in UxImGuiWindow.
// (That's theory, in practice the delimitation between UxImGuiWindow and UxImGuiWindowTempData is quite tenuous and could be reconsidered..)
// (This doesn't need a constructor because we zero-clear it as part of UxImGuiWindow and all frame-temporary data are setup on Begin)
struct IMGUI_API UxImGuiWindowTempData
{
    // Layout
    UxImVec2                  CursorPos;              // Current emitting position, in absolute coordinates.
    UxImVec2                  CursorPosPrevLine;
    UxImVec2                  CursorStartPos;         // Initial position after Begin(), generally ~ window position + WindowPadding.
    UxImVec2                  CursorMaxPos;           // Used to implicitly calculate ContentSize at the beginning of next frame, for scrolling range and auto-resize. Always growing during the frame.
    UxImVec2                  IdealMaxPos;            // Used to implicitly calculate ContentSizeIdeal at the beginning of next frame, for auto-resize only. Always growing during the frame.
    UxImVec2                  CurrLineSize;
    UxImVec2                  PrevLineSize;
    float                   CurrLineTextBaseOffset; // Baseline offset (0.0f by default on a new line, generally == style.FramePadding.y when a framed item has been added).
    float                   PrevLineTextBaseOffset;
    bool                    IsSameLine;
    bool                    IsSetPos;
    UxImVec1                  Indent;                 // Indentation / start position from left of window (increased by TreePush/TreePop, etc.)
    UxImVec1                  ColumnsOffset;          // Offset to the current column (if ColumnsCurrent > 0). FIXME: This and the above should be a stack to allow use cases like Tree->Column->Tree. Need revamp columns API.
    UxImVec1                  GroupOffset;
    UxImVec2                  CursorStartPosLossyness;// Record the loss of precision of CursorStartPos due to really large scrolling amount. This is used by clipper to compensate and fix the most common use case of large scroll area.

    // Keyboard/Gamepad navigation
    UxImGuiNavLayer           NavLayerCurrent;        // Current layer, 0..31 (we currently only use 0..1)
    short                   NavLayersActiveMask;    // Which layers have been written to (result from previous frame)
    short                   NavLayersActiveMaskNext;// Which layers have been written to (accumulator for current frame)
    bool                    NavIsScrollPushableX;   // Set when current work location may be scrolled horizontally when moving left / right. This is generally always true UNLESS within a column.
    bool                    NavHideHighlightOneFrame;
    bool                    NavWindowHasScrollY;    // Set per window when scrolling can be used (== ScrollMax.y > 0.0f)

    // Miscellaneous
    bool                    MenuBarAppending;       // FIXME: Remove this
    UxImVec2                  MenuBarOffset;          // MenuBarOffset.x is sort of equivalent of a per-layer CursorPos.x, saved/restored as we switch to the menu bar. The only situation when MenuBarOffset.y is > 0 if when (SafeAreaPadding.y > FramePadding.y), often used on TVs.
    UxImGuiMenuColumns        MenuColumns;            // Simplified columns storage for menu items measurement
    int                     TreeDepth;              // Current tree depth.
    UxImU32                   TreeHasStackDataDepthMask;      // Store whether given depth has UxImGuiTreeNodeStackData data. Could be turned into a UxImU64 if necessary.
    UxImU32                   TreeRecordsClippedNodesY2Mask;  // Store whether we should keep recording Y2. Cleared when passing clip max. Equivalent TreeHasStackDataDepthMask value should always be set.
    UxImVector<UxImGuiWindow*>  ChildWindows;
    UxImGuiStorage*           StateStorage;           // Current persistent per-window storage (store e.g. tree node open/close state)
    UxImGuiOldColumns*        CurrentColumns;         // Current columns set
    int                     CurrentTableIdx;        // Current table index (into g.Tables)
    UxImGuiLayoutType         LayoutType;
    UxImGuiLayoutType         ParentLayoutType;       // Layout type of parent window at the time of Begin()
    UxImU32                   ModalDimBgColor;

    // Status flags
    UxImGuiItemStatusFlags    WindowItemStatusFlags;
    UxImGuiItemStatusFlags    ChildItemStatusFlags;
    UxImGuiItemStatusFlags    DockTabItemStatusFlags;
    UxImRect                  DockTabItemRect;

    // Local parameters stacks
    // We store the current settings outside of the vectors to increase memory locality (reduce cache misses). The vectors are rarely modified. Also it allows us to not heap allocate for short-lived windows which are not using those settings.
    float                   ItemWidth;              // Current item width (>0.0: width in pixels, <0.0: align xx pixels to the right of window).
    float                   TextWrapPos;            // Current text wrap pos.
    UxImVector<float>         ItemWidthStack;         // Store item widths to restore (attention: .back() is not == ItemWidth)
    UxImVector<float>         TextWrapPosStack;       // Store text wrap pos to restore (attention: .back() is not == TextWrapPos)
};

// Storage for one window
struct IMGUI_API UxImGuiWindow
{
    UxImGuiContext*           Ctx;                                // Parent UI context (needs to be set explicitly by parent).
    char*                   Name;                               // Window name, owned by the window.
    UxImGuiID                 ID;                                 // == UxImHashStr(Name)
    UxImGuiWindowFlags        Flags, FlagsPreviousFrame;          // See enum UxImGuiWindowFlags_
    UxImGuiChildFlags         ChildFlags;                         // Set when window is a child window. See enum UxImGuiChildFlags_
    UxImGuiWindowClass        WindowClass;                        // Advanced users only. Set with SetNextWindowClass()
    UxImGuiViewportP*         Viewport;                           // Always set in Begin(). Inactive windows may have a NULL value here if their viewport was discarded.
    UxImGuiID                 ViewportId;                         // We backup the viewport id (since the viewport may disappear or never be created if the window is inactive)
    UxImVec2                  ViewportPos;                        // We backup the viewport position (since the viewport may disappear or never be created if the window is inactive)
    int                     ViewportAllowPlatformMonitorExtend; // Reset to -1 every frame (index is guaranteed to be valid between NewFrame..EndFrame), only used in the Appearing frame of a tooltip/popup to enforce clamping to a given monitor
    UxImVec2                  Pos;                                // Position (always rounded-up to nearest pixel)
    UxImVec2                  Size;                               // Current size (==SizeFull or collapsed title bar size)
    UxImVec2                  SizeFull;                           // Size when non collapsed
    UxImVec2                  ContentSize;                        // Size of contents/scrollable client area (calculated from the extents reach of the cursor) from previous frame. Does not include window decoration or window padding.
    UxImVec2                  ContentSizeIdeal;
    UxImVec2                  ContentSizeExplicit;                // Size of contents/scrollable client area explicitly request by the user via SetNextWindowContentSize().
    UxImVec2                  WindowPadding;                      // Window padding at the time of Begin().
    float                   WindowRounding;                     // Window rounding at the time of Begin(). May be clamped lower to avoid rendering artifacts with title bar, menu bar etc.
    float                   WindowBorderSize;                   // Window border size at the time of Begin().
    float                   TitleBarHeight, MenuBarHeight;      // Note that those used to be function before 2024/05/28. If you have old code calling TitleBarHeight() you can change it to TitleBarHeight.
    float                   DecoOuterSizeX1, DecoOuterSizeY1;   // Left/Up offsets. Sum of non-scrolling outer decorations (X1 generally == 0.0f. Y1 generally = TitleBarHeight + MenuBarHeight). Locked during Begin().
    float                   DecoOuterSizeX2, DecoOuterSizeY2;   // Right/Down offsets (X2 generally == ScrollbarSize.x, Y2 == ScrollbarSizes.y).
    float                   DecoInnerSizeX1, DecoInnerSizeY1;   // Applied AFTER/OVER InnerRect. Specialized for Tables as they use specialized form of clipping and frozen rows/columns are inside InnerRect (and not part of regular decoration sizes).
    int                     NameBufLen;                         // Size of buffer storing Name. May be larger than strlen(Name)!
    UxImGuiID                 MoveId;                             // == window->GetID("#MOVE")
    UxImGuiID                 TabId;                              // == window->GetID("#TAB")
    UxImGuiID                 ChildId;                            // ID of corresponding item in parent window (for navigation to return from child window to parent window)
    UxImGuiID                 PopupId;                            // ID in the popup stack when this window is used as a popup/menu (because we use generic Name/ID for recycling)
    UxImVec2                  Scroll;
    UxImVec2                  ScrollMax;
    UxImVec2                  ScrollTarget;                       // target scroll position. stored as cursor position with scrolling canceled out, so the highest point is always 0.0f. (FLT_MAX for no change)
    UxImVec2                  ScrollTargetCenterRatio;            // 0.0f = scroll so that target position is at top, 0.5f = scroll so that target position is centered
    UxImVec2                  ScrollTargetEdgeSnapDist;           // 0.0f = no snapping, >0.0f snapping threshold
    UxImVec2                  ScrollbarSizes;                     // Size taken by each scrollbars on their smaller axis. Pay attention! ScrollbarSizes.x == width of the vertical scrollbar, ScrollbarSizes.y = height of the horizontal scrollbar.
    bool                    ScrollbarX, ScrollbarY;             // Are scrollbars visible?
    bool                    ScrollbarXStabilizeEnabled;         // Was ScrollbarX previously auto-stabilized?
    UxImU8                    ScrollbarXStabilizeToggledHistory;  // Used to stabilize scrollbar visibility in case of feedback loops
    bool                    ViewportOwned;
    bool                    Active;                             // Set to true on Begin(), unless Collapsed
    bool                    WasActive;
    bool                    WriteAccessed;                      // Set to true when any widget access the current window
    bool                    Collapsed;                          // Set when collapsing window to become only title-bar
    bool                    WantCollapseToggle;
    bool                    SkipItems;                          // Set when items can safely be all clipped (e.g. window not visible or collapsed)
    bool                    SkipRefresh;                        // [EXPERIMENTAL] Reuse previous frame drawn contents, Begin() returns false.
    bool                    Appearing;                          // Set during the frame where the window is appearing (or re-appearing)
    bool                    Hidden;                             // Do not display (== HiddenFrames*** > 0)
    bool                    IsFallbackWindow;                   // Set on the "Debug##Default" window.
    bool                    IsExplicitChild;                    // Set when passed _ChildWindow, left to false by BeginDocked()
    bool                    HasCloseButton;                     // Set when the window has a close button (p_open != NULL)
    signed char             ResizeBorderHovered;                // Current border being hovered for resize (-1: none, otherwise 0-3)
    signed char             ResizeBorderHeld;                   // Current border being held for resize (-1: none, otherwise 0-3)
    short                   BeginCount;                         // Number of Begin() during the current frame (generally 0 or 1, 1+ if appending via multiple Begin/End pairs)
    short                   BeginCountPreviousFrame;            // Number of Begin() during the previous frame
    short                   BeginOrderWithinParent;             // Begin() order within immediate parent window, if we are a child window. Otherwise 0.
    short                   BeginOrderWithinContext;            // Begin() order within entire imgui context. This is mostly used for debugging submission order related issues.
    short                   FocusOrder;                         // Order within WindowsFocusOrder[], altered when windows are focused.
    UxImS8                    AutoFitFramesX, AutoFitFramesY;
    bool                    AutoFitOnlyGrows;
    UxImGuiDir                AutoPosLastDirection;
    UxImS8                    HiddenFramesCanSkipItems;           // Hide the window for N frames
    UxImS8                    HiddenFramesCannotSkipItems;        // Hide the window for N frames while allowing items to be submitted so we can measure their size
    UxImS8                    HiddenFramesForRenderOnly;          // Hide the window until frame N at Render() time only
    UxImS8                    DisableInputsFrames;                // Disable window interactions for N frames
    UxImGuiCond               SetWindowPosAllowFlags : 8;         // store acceptable condition flags for SetNextWindowPos() use.
    UxImGuiCond               SetWindowSizeAllowFlags : 8;        // store acceptable condition flags for SetNextWindowSize() use.
    UxImGuiCond               SetWindowCollapsedAllowFlags : 8;   // store acceptable condition flags for SetNextWindowCollapsed() use.
    UxImGuiCond               SetWindowDockAllowFlags : 8;        // store acceptable condition flags for SetNextWindowDock() use.
    UxImVec2                  SetWindowPosVal;                    // store window position when using a non-zero Pivot (position set needs to be processed when we know the window size)
    UxImVec2                  SetWindowPosPivot;                  // store window pivot for positioning. UxImVec2(0, 0) when positioning from top-left corner; UxImVec2(0.5f, 0.5f) for centering; UxImVec2(1, 1) for bottom right.

    UxImVector<UxImGuiID>       IDStack;                            // ID stack. ID are hashes seeded with the value at the top of the stack. (In theory this should be in the TempData structure)
    UxImGuiWindowTempData     DC;                                 // Temporary per-window data, reset at the beginning of the frame. This used to be called UxImGuiDrawContext, hence the "DC" variable name.

    // The best way to understand what those rectangles are is to use the 'Metrics->Tools->Show Windows Rectangles' viewer.
    // The main 'OuterRect', omitted as a field, is window->Rect().
    UxImRect                  OuterRectClipped;                   // == Window->Rect() just after setup in Begin(). == window->Rect() for root window.
    UxImRect                  InnerRect;                          // Inner rectangle (omit title bar, menu bar, scroll bar)
    UxImRect                  InnerClipRect;                      // == InnerRect shrunk by WindowPadding*0.5f on each side, clipped within viewport or parent clip rect.
    UxImRect                  WorkRect;                           // Initially covers the whole scrolling region. Reduced by containers e.g columns/tables when active. Shrunk by WindowPadding*1.0f on each side. This is meant to replace ContentRegionRect over time (from 1.71+ onward).
    UxImRect                  ParentWorkRect;                     // Backup of WorkRect before entering a container such as columns/tables. Used by e.g. SpanAllColumns functions to easily access. Stacked containers are responsible for maintaining this. // FIXME-WORKRECT: Could be a stack?
    UxImRect                  ClipRect;                           // Current clipping/scissoring rectangle, evolve as we are using PushClipRect(), etc. == DrawList->clip_rect_stack.back().
    UxImRect                  ContentRegionRect;                  // FIXME: This is currently confusing/misleading. It is essentially WorkRect but not handling of scrolling. We currently rely on it as right/bottom aligned sizing operation need some size to rely on.
    UxImVec2ih                HitTestHoleSize;                    // Define an optional rectangular hole where mouse will pass-through the window.
    UxImVec2ih                HitTestHoleOffset;

    int                     LastFrameActive;                    // Last frame number the window was Active.
    int                     LastFrameJustFocused;               // Last frame number the window was made Focused.
    float                   LastTimeActive;                     // Last timestamp the window was Active (using float as we don't need high precision there)
    float                   ItemWidthDefault;
    UxImGuiStorage            StateStorage;
    UxImVector<UxImGuiOldColumns> ColumnsStorage;
    float                   FontWindowScale;                    // User scale multiplier per-window, via SetWindowFontScale()
    float                   FontWindowScaleParents;
    float                   FontDpiScale;
    float                   FontRefSize;                        // This is a copy of window->CalcFontSize() at the time of Begin(), trying to phase out CalcFontSize() especially as it may be called on non-current window.
    int                     SettingsOffset;                     // Offset into SettingsWindows[] (offsets are always valid as we only grow the array from the back)

    UxImDrawList*             DrawList;                           // == &DrawListInst (for backward compatibility reason with code using imgui_internal.h we keep this a pointer)
    UxImDrawList              DrawListInst;
    UxImGuiWindow*            ParentWindow;                       // If we are a child _or_ popup _or_ docked window, this is pointing to our parent. Otherwise NULL.
    UxImGuiWindow*            ParentWindowInBeginStack;
    UxImGuiWindow*            RootWindow;                         // Point to ourself or first ancestor that is not a child window. Doesn't cross through popups/dock nodes.
    UxImGuiWindow*            RootWindowPopupTree;                // Point to ourself or first ancestor that is not a child window. Cross through popups parent<>child.
    UxImGuiWindow*            RootWindowDockTree;                 // Point to ourself or first ancestor that is not a child window. Cross through dock nodes.
    UxImGuiWindow*            RootWindowForTitleBarHighlight;     // Point to ourself or first ancestor which will display TitleBgActive color when this window is active.
    UxImGuiWindow*            RootWindowForNav;                   // Point to ourself or first ancestor which doesn't have the NavFlattened flag.
    UxImGuiWindow*            ParentWindowForFocusRoute;          // Set to manual link a window to its logical parent so that Shortcut() chain are honoerd (e.g. Tool linked to Document)

    UxImGuiWindow*            NavLastChildNavWindow;              // When going to the menu bar, we remember the child window we came from. (This could probably be made implicit if we kept g.Windows sorted by last focused including child window.)
    UxImGuiID                 NavLastIds[UxImGuiNavLayer_COUNT];    // Last known NavId for this window, per layer (0/1)
    UxImRect                  NavRectRel[UxImGuiNavLayer_COUNT];    // Reference rectangle, in window relative space
    UxImVec2                  NavPreferredScoringPosRel[UxImGuiNavLayer_COUNT]; // Preferred X/Y position updated when moving on a given axis, reset to FLT_MAX.
    UxImGuiID                 NavRootFocusScopeId;                // Focus Scope ID at the time of Begin()

    int                     MemoryDrawListIdxCapacity;          // Backup of last idx/vtx count, so when waking up the window we can preallocate and avoid iterative alloc/copy
    int                     MemoryDrawListVtxCapacity;
    bool                    MemoryCompacted;                    // Set when window extraneous data have been garbage collected

    // Docking
    bool                    DockIsActive        :1;             // When docking artifacts are actually visible. When this is set, DockNode is guaranteed to be != NULL. ~~ (DockNode != NULL) && (DockNode->Windows.Size > 1).
    bool                    DockNodeIsVisible   :1;
    bool                    DockTabIsVisible    :1;             // Is our window visible this frame? ~~ is the corresponding tab selected?
    bool                    DockTabWantClose    :1;
    short                   DockOrder;                          // Order of the last time the window was visible within its DockNode. This is used to reorder windows that are reappearing on the same frame. Same value between windows that were active and windows that were none are possible.
    UxImGuiWindowDockStyle    DockStyle;
    UxImGuiDockNode*          DockNode;                           // Which node are we docked into. Important: Prefer testing DockIsActive in many cases as this will still be set when the dock node is hidden.
    UxImGuiDockNode*          DockNodeAsHost;                     // Which node are we owning (for parent windows)
    UxImGuiID                 DockId;                             // Backup of last valid DockNode->ID, so single window remember their dock node id even when they are not bound any more

public:
    UxImGuiWindow(UxImGuiContext* context, const char* name);
    ~UxImGuiWindow();

    UxImGuiID     GetID(const char* str, const char* str_end = NULL);
    UxImGuiID     GetID(const void* ptr);
    UxImGuiID     GetID(int n);
    UxImGuiID     GetIDFromPos(const UxImVec2& p_abs);
    UxImGuiID     GetIDFromRectangle(const UxImRect& r_abs);

    // We don't use g.FontSize because the window may be != g.CurrentWindow.
    UxImRect      Rect() const            { return UxImRect(Pos.x, Pos.y, Pos.x + Size.x, Pos.y + Size.y); }
    UxImRect      TitleBarRect() const    { return UxImRect(Pos, UxImVec2(Pos.x + SizeFull.x, Pos.y + TitleBarHeight)); }
    UxImRect      MenuBarRect() const     { float y1 = Pos.y + TitleBarHeight; return UxImRect(Pos.x, y1, Pos.x + SizeFull.x, y1 + MenuBarHeight); }

    // [Obsolete] UxImGuiWindow::CalcFontSize() was removed in 1.92.x because error-prone/misleading. You can use window->FontRefSize for a copy of g.FontSize at the time of the last Begin() call for this window.
    //float     CalcFontSize() const    { UxImGuiContext& g = *Ctx; return g.FontSizeBeforeScaling * FontWindowScale * FontDpiScale * FontWindowScaleParents;
};

//-----------------------------------------------------------------------------
// [SECTION] Tab bar, Tab item support
//-----------------------------------------------------------------------------

// Extend UxImGuiTabBarFlags_
enum UxImGuiTabBarFlagsPrivate_
{
    UxImGuiTabBarFlags_DockNode                   = 1 << 20,  // Part of a dock node [we don't use this in the master branch but it facilitate branch syncing to keep this around]
    UxImGuiTabBarFlags_IsFocused                  = 1 << 21,
    UxImGuiTabBarFlags_SaveSettings               = 1 << 22,  // FIXME: Settings are handled by the docking system, this only request the tab bar to mark settings dirty when reordering tabs
};

// Extend UxImGuiTabItemFlags_
enum UxImGuiTabItemFlagsPrivate_
{
    UxImGuiTabItemFlags_SectionMask_              = UxImGuiTabItemFlags_Leading | UxImGuiTabItemFlags_Trailing,
    UxImGuiTabItemFlags_NoCloseButton             = 1 << 20,  // Track whether p_open was set or not (we'll need this info on the next frame to recompute ContentWidth during layout)
    UxImGuiTabItemFlags_Button                    = 1 << 21,  // Used by TabItemButton, change the tab item behavior to mimic a button
    UxImGuiTabItemFlags_Invisible                 = 1 << 22,  // To reserve space e.g. with UxImGuiTabItemFlags_Leading
    UxImGuiTabItemFlags_Unsorted                  = 1 << 23,  // [Docking] Trailing tabs with the _Unsorted flag will be sorted based on the DockOrder of their Window.
};

// Storage for one active tab item (sizeof() 48 bytes)
struct UxImGuiTabItem
{
    UxImGuiID             ID;
    UxImGuiTabItemFlags   Flags;
    UxImGuiWindow*        Window;                 // When TabItem is part of a DockNode's TabBar, we hold on to a window.
    int                 LastFrameVisible;
    int                 LastFrameSelected;      // This allows us to infer an ordered list of the last activated tabs with little maintenance
    float               Offset;                 // Position relative to beginning of tab
    float               Width;                  // Width currently displayed
    float               ContentWidth;           // Width of label, stored during BeginTabItem() call
    float               RequestedWidth;         // Width optionally requested by caller, -1.0f is unused
    UxImS32               NameOffset;             // When Window==NULL, offset to name within parent UxImGuiTabBar::TabsNames
    UxImS16               BeginOrder;             // BeginTabItem() order, used to re-order tabs after toggling UxImGuiTabBarFlags_Reorderable
    UxImS16               IndexDuringLayout;      // Index only used during TabBarLayout(). Tabs gets reordered so 'Tabs[n].IndexDuringLayout == n' but may mismatch during additions.
    bool                WantClose;              // Marked as closed by SetTabItemClosed()

    UxImGuiTabItem()      { memset(this, 0, sizeof(*this)); LastFrameVisible = LastFrameSelected = -1; RequestedWidth = -1.0f; NameOffset = -1; BeginOrder = IndexDuringLayout = -1; }
};

// Storage for a tab bar (sizeof() 160 bytes)
struct IMGUI_API UxImGuiTabBar
{
    UxImGuiWindow*        Window;
    UxImVector<UxImGuiTabItem> Tabs;
    UxImGuiTabBarFlags    Flags;
    UxImGuiID             ID;                     // Zero for tab-bars used by docking
    UxImGuiID             SelectedTabId;          // Selected tab/window
    UxImGuiID             NextSelectedTabId;      // Next selected tab/window. Will also trigger a scrolling animation
    UxImGuiID             VisibleTabId;           // Can occasionally be != SelectedTabId (e.g. when previewing contents for CTRL+TAB preview)
    int                 CurrFrameVisible;
    int                 PrevFrameVisible;
    UxImRect              BarRect;
    float               CurrTabsContentsHeight;
    float               PrevTabsContentsHeight; // Record the height of contents submitted below the tab bar
    float               WidthAllTabs;           // Actual width of all tabs (locked during layout)
    float               WidthAllTabsIdeal;      // Ideal width if all tabs were visible and not clipped
    float               ScrollingAnim;
    float               ScrollingTarget;
    float               ScrollingTargetDistToVisibility;
    float               ScrollingSpeed;
    float               ScrollingRectMinX;
    float               ScrollingRectMaxX;
    float               SeparatorMinX;
    float               SeparatorMaxX;
    UxImGuiID             ReorderRequestTabId;
    UxImS16               ReorderRequestOffset;
    UxImS8                BeginCount;
    bool                WantLayout;
    bool                VisibleTabWasSubmitted;
    bool                TabsAddedNew;           // Set to true when a new tab item or button has been added to the tab bar during last frame
    UxImS16               TabsActiveCount;        // Number of tabs submitted this frame.
    UxImS16               LastTabItemIdx;         // Index of last BeginTabItem() tab for use by EndTabItem()
    float               ItemSpacingY;
    UxImVec2              FramePadding;           // style.FramePadding locked at the time of BeginTabBar()
    UxImVec2              BackupCursorPos;
    UxImGuiTextBuffer     TabsNames;              // For non-docking tab bar we re-append names in a contiguous buffer.

    UxImGuiTabBar();
};

//-----------------------------------------------------------------------------
// [SECTION] Table support
//-----------------------------------------------------------------------------

#define IM_COL32_DISABLE                IM_COL32(0,0,0,1)   // Special sentinel code which cannot be used as a regular color.
#define IMGUI_TABLE_MAX_COLUMNS         512                 // Arbitrary "safety" maximum, may be lifted in the future if needed. Must fit in UxImGuiTableColumnIdx/UxImGuiTableDrawChannelIdx.

// [Internal] sizeof() ~ 112
// We use the terminology "Enabled" to refer to a column that is not Hidden by user/api.
// We use the terminology "Clipped" to refer to a column that is out of sight because of scrolling/clipping.
// This is in contrast with some user-facing api such as IsItemVisible() / IsRectVisible() which use "Visible" to mean "not clipped".
struct UxImGuiTableColumn
{
    UxImGuiTableColumnFlags   Flags;                          // Flags after some patching (not directly same as provided by user). See UxImGuiTableColumnFlags_
    float                   WidthGiven;                     // Final/actual width visible == (MaxX - MinX), locked in TableUpdateLayout(). May be > WidthRequest to honor minimum width, may be < WidthRequest to honor shrinking columns down in tight space.
    float                   MinX;                           // Absolute positions
    float                   MaxX;
    float                   WidthRequest;                   // Master width absolute value when !(Flags & _WidthStretch). When Stretch this is derived every frame from StretchWeight in TableUpdateLayout()
    float                   WidthAuto;                      // Automatic width
    float                   WidthMax;                       // Maximum width (FIXME: overwritten by each instance)
    float                   StretchWeight;                  // Master width weight when (Flags & _WidthStretch). Often around ~1.0f initially.
    float                   InitStretchWeightOrWidth;       // Value passed to TableSetupColumn(). For Width it is a content width (_without padding_).
    UxImRect                  ClipRect;                       // Clipping rectangle for the column
    UxImGuiID                 UserID;                         // Optional, value passed to TableSetupColumn()
    float                   WorkMinX;                       // Contents region min ~(MinX + CellPaddingX + CellSpacingX1) == cursor start position when entering column
    float                   WorkMaxX;                       // Contents region max ~(MaxX - CellPaddingX - CellSpacingX2)
    float                   ItemWidth;                      // Current item width for the column, preserved across rows
    float                   ContentMaxXFrozen;              // Contents maximum position for frozen rows (apart from headers), from which we can infer content width.
    float                   ContentMaxXUnfrozen;
    float                   ContentMaxXHeadersUsed;         // Contents maximum position for headers rows (regardless of freezing). TableHeader() automatically softclip itself + report ideal desired size, to avoid creating extraneous draw calls
    float                   ContentMaxXHeadersIdeal;
    UxImS16                   NameOffset;                     // Offset into parent ColumnsNames[]
    UxImGuiTableColumnIdx     DisplayOrder;                   // Index within Table's IndexToDisplayOrder[] (column may be reordered by users)
    UxImGuiTableColumnIdx     IndexWithinEnabledSet;          // Index within enabled/visible set (<= IndexToDisplayOrder)
    UxImGuiTableColumnIdx     PrevEnabledColumn;              // Index of prev enabled/visible column within Columns[], -1 if first enabled/visible column
    UxImGuiTableColumnIdx     NextEnabledColumn;              // Index of next enabled/visible column within Columns[], -1 if last enabled/visible column
    UxImGuiTableColumnIdx     SortOrder;                      // Index of this column within sort specs, -1 if not sorting on this column, 0 for single-sort, may be >0 on multi-sort
    UxImGuiTableDrawChannelIdx DrawChannelCurrent;            // Index within DrawSplitter.Channels[]
    UxImGuiTableDrawChannelIdx DrawChannelFrozen;             // Draw channels for frozen rows (often headers)
    UxImGuiTableDrawChannelIdx DrawChannelUnfrozen;           // Draw channels for unfrozen rows
    bool                    IsEnabled;                      // IsUserEnabled && (Flags & UxImGuiTableColumnFlags_Disabled) == 0
    bool                    IsUserEnabled;                  // Is the column not marked Hidden by the user? (unrelated to being off view, e.g. clipped by scrolling).
    bool                    IsUserEnabledNextFrame;
    bool                    IsVisibleX;                     // Is actually in view (e.g. overlapping the host window clipping rectangle, not scrolled).
    bool                    IsVisibleY;
    bool                    IsRequestOutput;                // Return value for TableSetColumnIndex() / TableNextColumn(): whether we request user to output contents or not.
    bool                    IsSkipItems;                    // Do we want item submissions to this column to be completely ignored (no layout will happen).
    bool                    IsPreserveWidthAuto;
    UxImS8                    NavLayerCurrent;                // UxImGuiNavLayer in 1 byte
    UxImU8                    AutoFitQueue;                   // Queue of 8 values for the next 8 frames to request auto-fit
    UxImU8                    CannotSkipItemsQueue;           // Queue of 8 values for the next 8 frames to disable Clipped/SkipItem
    UxImU8                    SortDirection : 2;              // UxImGuiSortDirection_Ascending or UxImGuiSortDirection_Descending
    UxImU8                    SortDirectionsAvailCount : 2;   // Number of available sort directions (0 to 3)
    UxImU8                    SortDirectionsAvailMask : 4;    // Mask of available sort directions (1-bit each)
    UxImU8                    SortDirectionsAvailList;        // Ordered list of available sort directions (2-bits each, total 8-bits)

    UxImGuiTableColumn()
    {
        memset(this, 0, sizeof(*this));
        StretchWeight = WidthRequest = -1.0f;
        NameOffset = -1;
        DisplayOrder = IndexWithinEnabledSet = -1;
        PrevEnabledColumn = NextEnabledColumn = -1;
        SortOrder = -1;
        SortDirection = UxImGuiSortDirection_None;
        DrawChannelCurrent = DrawChannelFrozen = DrawChannelUnfrozen = (UxImU8)-1;
    }
};

// Transient cell data stored per row.
// sizeof() ~ 6 bytes
struct UxImGuiTableCellData
{
    UxImU32                       BgColor;    // Actual color
    UxImGuiTableColumnIdx         Column;     // Column number
};

// Parameters for TableAngledHeadersRowEx()
// This may end up being refactored for more general purpose.
// sizeof() ~ 12 bytes
struct UxImGuiTableHeaderData
{
    UxImGuiTableColumnIdx         Index;      // Column index
    UxImU32                       TextColor;
    UxImU32                       BgColor0;
    UxImU32                       BgColor1;
};

// Per-instance data that needs preserving across frames (seemingly most others do not need to be preserved aside from debug needs. Does that means they could be moved to UxImGuiTableTempData?)
// sizeof() ~ 24 bytes
struct UxImGuiTableInstanceData
{
    UxImGuiID                     TableInstanceID;
    float                       LastOuterHeight;            // Outer height from last frame
    float                       LastTopHeadersRowHeight;    // Height of first consecutive header rows from last frame (FIXME: this is used assuming consecutive headers are in same frozen set)
    float                       LastFrozenHeight;           // Height of frozen section from last frame
    int                         HoveredRowLast;             // Index of row which was hovered last frame.
    int                         HoveredRowNext;             // Index of row hovered this frame, set after encountering it.

    UxImGuiTableInstanceData()    { TableInstanceID = 0; LastOuterHeight = LastTopHeadersRowHeight = LastFrozenHeight = 0.0f; HoveredRowLast = HoveredRowNext = -1; }
};

// sizeof() ~ 592 bytes + heap allocs described in TableBeginInitMemory()
struct IMGUI_API UxImGuiTable
{
    UxImGuiID                     ID;
    UxImGuiTableFlags             Flags;
    void*                       RawData;                    // Single allocation to hold Columns[], DisplayOrderToIndex[], and RowCellData[]
    UxImGuiTableTempData*         TempData;                   // Transient data while table is active. Point within g.CurrentTableStack[]
    UxImSpan<UxImGuiTableColumn>    Columns;                    // Point within RawData[]
    UxImSpan<UxImGuiTableColumnIdx> DisplayOrderToIndex;        // Point within RawData[]. Store display order of columns (when not reordered, the values are 0...Count-1)
    UxImSpan<UxImGuiTableCellData>  RowCellData;                // Point within RawData[]. Store cells background requests for current row.
    UxImBitArrayPtr               EnabledMaskByDisplayOrder;  // Column DisplayOrder -> IsEnabled map
    UxImBitArrayPtr               EnabledMaskByIndex;         // Column Index -> IsEnabled map (== not hidden by user/api) in a format adequate for iterating column without touching cold data
    UxImBitArrayPtr               VisibleMaskByIndex;         // Column Index -> IsVisibleX|IsVisibleY map (== not hidden by user/api && not hidden by scrolling/cliprect)
    UxImGuiTableFlags             SettingsLoadedFlags;        // Which data were loaded from the .ini file (e.g. when order is not altered we won't save order)
    int                         SettingsOffset;             // Offset in g.SettingsTables
    int                         LastFrameActive;
    int                         ColumnsCount;               // Number of columns declared in BeginTable()
    int                         CurrentRow;
    int                         CurrentColumn;
    UxImS16                       InstanceCurrent;            // Count of BeginTable() calls with same ID in the same frame (generally 0). This is a little bit similar to BeginCount for a window, but multiple tables with the same ID are multiple tables, they are just synced.
    UxImS16                       InstanceInteracted;         // Mark which instance (generally 0) of the same ID is being interacted with
    float                       RowPosY1;
    float                       RowPosY2;
    float                       RowMinHeight;               // Height submitted to TableNextRow()
    float                       RowCellPaddingY;            // Top and bottom padding. Reloaded during row change.
    float                       RowTextBaseline;
    float                       RowIndentOffsetX;
    UxImGuiTableRowFlags          RowFlags : 16;              // Current row flags, see UxImGuiTableRowFlags_
    UxImGuiTableRowFlags          LastRowFlags : 16;
    int                         RowBgColorCounter;          // Counter for alternating background colors (can be fast-forwarded by e.g clipper), not same as CurrentRow because header rows typically don't increase this.
    UxImU32                       RowBgColor[2];              // Background color override for current row.
    UxImU32                       BorderColorStrong;
    UxImU32                       BorderColorLight;
    float                       BorderX1;
    float                       BorderX2;
    float                       HostIndentX;
    float                       MinColumnWidth;
    float                       OuterPaddingX;
    float                       CellPaddingX;               // Padding from each borders. Locked in BeginTable()/Layout.
    float                       CellSpacingX1;              // Spacing between non-bordered cells. Locked in BeginTable()/Layout.
    float                       CellSpacingX2;
    float                       InnerWidth;                 // User value passed to BeginTable(), see comments at the top of BeginTable() for details.
    float                       ColumnsGivenWidth;          // Sum of current column width
    float                       ColumnsAutoFitWidth;        // Sum of ideal column width in order nothing to be clipped, used for auto-fitting and content width submission in outer window
    float                       ColumnsStretchSumWeights;   // Sum of weight of all enabled stretching columns
    float                       ResizedColumnNextWidth;
    float                       ResizeLockMinContentsX2;    // Lock minimum contents width while resizing down in order to not create feedback loops. But we allow growing the table.
    float                       RefScale;                   // Reference scale to be able to rescale columns on font/dpi changes.
    float                       AngledHeadersHeight;        // Set by TableAngledHeadersRow(), used in TableUpdateLayout()
    float                       AngledHeadersSlope;         // Set by TableAngledHeadersRow(), used in TableUpdateLayout()
    UxImRect                      OuterRect;                  // Note: for non-scrolling table, OuterRect.Max.y is often FLT_MAX until EndTable(), unless a height has been specified in BeginTable().
    UxImRect                      InnerRect;                  // InnerRect but without decoration. As with OuterRect, for non-scrolling tables, InnerRect.Max.y is "
    UxImRect                      WorkRect;
    UxImRect                      InnerClipRect;
    UxImRect                      BgClipRect;                 // We use this to cpu-clip cell background color fill, evolve during the frame as we cross frozen rows boundaries
    UxImRect                      Bg0ClipRectForDrawCmd;      // Actual UxImDrawCmd clip rect for BG0/1 channel. This tends to be == OuterWindow->ClipRect at BeginTable() because output in BG0/BG1 is cpu-clipped
    UxImRect                      Bg2ClipRectForDrawCmd;      // Actual UxImDrawCmd clip rect for BG2 channel. This tends to be a correct, tight-fit, because output to BG2 are done by widgets relying on regular ClipRect.
    UxImRect                      HostClipRect;               // This is used to check if we can eventually merge our columns draw calls into the current draw call of the current window.
    UxImRect                      HostBackupInnerClipRect;    // Backup of InnerWindow->ClipRect during PushTableBackground()/PopTableBackground()
    UxImGuiWindow*                OuterWindow;                // Parent window for the table
    UxImGuiWindow*                InnerWindow;                // Window holding the table data (== OuterWindow or a child window)
    UxImGuiTextBuffer             ColumnsNames;               // Contiguous buffer holding columns names
    UxImDrawListSplitter*         DrawSplitter;               // Shortcut to TempData->DrawSplitter while in table. Isolate draw commands per columns to avoid switching clip rect constantly
    UxImGuiTableInstanceData      InstanceDataFirst;
    UxImVector<UxImGuiTableInstanceData>    InstanceDataExtra;  // FIXME-OPT: Using a small-vector pattern would be good.
    UxImGuiTableColumnSortSpecs   SortSpecsSingle;
    UxImVector<UxImGuiTableColumnSortSpecs> SortSpecsMulti;     // FIXME-OPT: Using a small-vector pattern would be good.
    UxImGuiTableSortSpecs         SortSpecs;                  // Public facing sorts specs, this is what we return in TableGetSortSpecs()
    UxImGuiTableColumnIdx         SortSpecsCount;
    UxImGuiTableColumnIdx         ColumnsEnabledCount;        // Number of enabled columns (<= ColumnsCount)
    UxImGuiTableColumnIdx         ColumnsEnabledFixedCount;   // Number of enabled columns using fixed width (<= ColumnsCount)
    UxImGuiTableColumnIdx         DeclColumnsCount;           // Count calls to TableSetupColumn()
    UxImGuiTableColumnIdx         AngledHeadersCount;         // Count columns with angled headers
    UxImGuiTableColumnIdx         HoveredColumnBody;          // Index of column whose visible region is being hovered. Important: == ColumnsCount when hovering empty region after the right-most column!
    UxImGuiTableColumnIdx         HoveredColumnBorder;        // Index of column whose right-border is being hovered (for resizing).
    UxImGuiTableColumnIdx         HighlightColumnHeader;      // Index of column which should be highlighted.
    UxImGuiTableColumnIdx         AutoFitSingleColumn;        // Index of single column requesting auto-fit.
    UxImGuiTableColumnIdx         ResizedColumn;              // Index of column being resized. Reset when InstanceCurrent==0.
    UxImGuiTableColumnIdx         LastResizedColumn;          // Index of column being resized from previous frame.
    UxImGuiTableColumnIdx         HeldHeaderColumn;           // Index of column header being held.
    UxImGuiTableColumnIdx         ReorderColumn;              // Index of column being reordered. (not cleared)
    UxImGuiTableColumnIdx         ReorderColumnDir;           // -1 or +1
    UxImGuiTableColumnIdx         LeftMostEnabledColumn;      // Index of left-most non-hidden column.
    UxImGuiTableColumnIdx         RightMostEnabledColumn;     // Index of right-most non-hidden column.
    UxImGuiTableColumnIdx         LeftMostStretchedColumn;    // Index of left-most stretched column.
    UxImGuiTableColumnIdx         RightMostStretchedColumn;   // Index of right-most stretched column.
    UxImGuiTableColumnIdx         ContextPopupColumn;         // Column right-clicked on, of -1 if opening context menu from a neutral/empty spot
    UxImGuiTableColumnIdx         FreezeRowsRequest;          // Requested frozen rows count
    UxImGuiTableColumnIdx         FreezeRowsCount;            // Actual frozen row count (== FreezeRowsRequest, or == 0 when no scrolling offset)
    UxImGuiTableColumnIdx         FreezeColumnsRequest;       // Requested frozen columns count
    UxImGuiTableColumnIdx         FreezeColumnsCount;         // Actual frozen columns count (== FreezeColumnsRequest, or == 0 when no scrolling offset)
    UxImGuiTableColumnIdx         RowCellDataCurrent;         // Index of current RowCellData[] entry in current row
    UxImGuiTableDrawChannelIdx    DummyDrawChannel;           // Redirect non-visible columns here.
    UxImGuiTableDrawChannelIdx    Bg2DrawChannelCurrent;      // For Selectable() and other widgets drawing across columns after the freezing line. Index within DrawSplitter.Channels[]
    UxImGuiTableDrawChannelIdx    Bg2DrawChannelUnfrozen;
    UxImS8                        NavLayer;                   // UxImGuiNavLayer at the time of BeginTable().
    bool                        IsLayoutLocked;             // Set by TableUpdateLayout() which is called when beginning the first row.
    bool                        IsInsideRow;                // Set when inside TableBeginRow()/TableEndRow().
    bool                        IsInitializing;
    bool                        IsSortSpecsDirty;
    bool                        IsUsingHeaders;             // Set when the first row had the UxImGuiTableRowFlags_Headers flag.
    bool                        IsContextPopupOpen;         // Set when default context menu is open (also see: ContextPopupColumn, InstanceInteracted).
    bool                        DisableDefaultContextMenu;  // Disable default context menu contents. You may submit your own using TableBeginContextMenuPopup()/EndPopup()
    bool                        IsSettingsRequestLoad;
    bool                        IsSettingsDirty;            // Set when table settings have changed and needs to be reported into UxImGuiTableSetttings data.
    bool                        IsDefaultDisplayOrder;      // Set when display order is unchanged from default (DisplayOrder contains 0...Count-1)
    bool                        IsResetAllRequest;
    bool                        IsResetDisplayOrderRequest;
    bool                        IsUnfrozenRows;             // Set when we got past the frozen row.
    bool                        IsDefaultSizingPolicy;      // Set if user didn't explicitly set a sizing policy in BeginTable()
    bool                        IsActiveIdAliveBeforeTable;
    bool                        IsActiveIdInTable;
    bool                        HasScrollbarYCurr;          // Whether ANY instance of this table had a vertical scrollbar during the current frame.
    bool                        HasScrollbarYPrev;          // Whether ANY instance of this table had a vertical scrollbar during the previous.
    bool                        MemoryCompacted;
    bool                        HostSkipItems;              // Backup of InnerWindow->SkipItem at the end of BeginTable(), because we will overwrite InnerWindow->SkipItem on a per-column basis

    UxImGuiTable()                { memset(this, 0, sizeof(*this)); LastFrameActive = -1; }
    ~UxImGuiTable()               { IM_FREE(RawData); }
};

// Transient data that are only needed between BeginTable() and EndTable(), those buffers are shared (1 per level of stacked table).
// - Accessing those requires chasing an extra pointer so for very frequently used data we leave them in the main table structure.
// - We also leave out of this structure data that tend to be particularly useful for debugging/metrics.
// FIXME-TABLE: more transient data could be stored in a stacked UxImGuiTableTempData: e.g. SortSpecs.
// sizeof() ~ 136 bytes.
struct IMGUI_API UxImGuiTableTempData
{
    int                         TableIndex;                 // Index in g.Tables.Buf[] pool
    float                       LastTimeActive;             // Last timestamp this structure was used
    float                       AngledHeadersExtraWidth;    // Used in EndTable()
    UxImVector<UxImGuiTableHeaderData> AngledHeadersRequests;   // Used in TableAngledHeadersRow()

    UxImVec2                      UserOuterSize;              // outer_size.x passed to BeginTable()
    UxImDrawListSplitter          DrawSplitter;

    UxImRect                      HostBackupWorkRect;         // Backup of InnerWindow->WorkRect at the end of BeginTable()
    UxImRect                      HostBackupParentWorkRect;   // Backup of InnerWindow->ParentWorkRect at the end of BeginTable()
    UxImVec2                      HostBackupPrevLineSize;     // Backup of InnerWindow->DC.PrevLineSize at the end of BeginTable()
    UxImVec2                      HostBackupCurrLineSize;     // Backup of InnerWindow->DC.CurrLineSize at the end of BeginTable()
    UxImVec2                      HostBackupCursorMaxPos;     // Backup of InnerWindow->DC.CursorMaxPos at the end of BeginTable()
    UxImVec1                      HostBackupColumnsOffset;    // Backup of OuterWindow->DC.ColumnsOffset at the end of BeginTable()
    float                       HostBackupItemWidth;        // Backup of OuterWindow->DC.ItemWidth at the end of BeginTable()
    int                         HostBackupItemWidthStackSize;//Backup of OuterWindow->DC.ItemWidthStack.Size at the end of BeginTable()

    UxImGuiTableTempData()        { memset(this, 0, sizeof(*this)); LastTimeActive = -1.0f; }
};

// sizeof() ~ 16
struct UxImGuiTableColumnSettings
{
    float                   WidthOrWeight;
    UxImGuiID                 UserID;
    UxImGuiTableColumnIdx     Index;
    UxImGuiTableColumnIdx     DisplayOrder;
    UxImGuiTableColumnIdx     SortOrder;
    UxImU8                    SortDirection : 2;
    UxImS8                    IsEnabled : 2; // "Visible" in ini file
    UxImU8                    IsStretch : 1;

    UxImGuiTableColumnSettings()
    {
        WidthOrWeight = 0.0f;
        UserID = 0;
        Index = -1;
        DisplayOrder = SortOrder = -1;
        SortDirection = UxImGuiSortDirection_None;
        IsEnabled = -1;
        IsStretch = 0;
    }
};

// This is designed to be stored in a single UxImChunkStream (1 header followed by N UxImGuiTableColumnSettings, etc.)
struct UxImGuiTableSettings
{
    UxImGuiID                     ID;                     // Set to 0 to invalidate/delete the setting
    UxImGuiTableFlags             SaveFlags;              // Indicate data we want to save using the Resizable/Reorderable/Sortable/Hideable flags (could be using its own flags..)
    float                       RefScale;               // Reference scale to be able to rescale columns on font/dpi changes.
    UxImGuiTableColumnIdx         ColumnsCount;
    UxImGuiTableColumnIdx         ColumnsCountMax;        // Maximum number of columns this settings instance can store, we can recycle a settings instance with lower number of columns but not higher
    bool                        WantApply;              // Set when loaded from .ini data (to enable merging/loading .ini data into an already running context)

    UxImGuiTableSettings()        { memset(this, 0, sizeof(*this)); }
    UxImGuiTableColumnSettings*   GetColumnSettings()     { return (UxImGuiTableColumnSettings*)(this + 1); }
};

//-----------------------------------------------------------------------------
// [SECTION] UxImGui internal API
// No guarantee of forward compatibility here!
//-----------------------------------------------------------------------------

namespace UxImGui
{
    // Windows
    // We should always have a CurrentWindow in the stack (there is an implicit "Debug" window)
    // If this ever crashes because g.CurrentWindow is NULL, it means that either:
    // - UxImGui::NewFrame() has never been called, which is illegal.
    // - You are calling UxImGui functions after UxImGui::EndFrame()/UxImGui::Render() and before the next UxImGui::NewFrame(), which is also illegal.
    IMGUI_API UxImGuiIO&         GetIO(UxImGuiContext* ctx);
    IMGUI_API UxImGuiPlatformIO& GetPlatformIO(UxImGuiContext* ctx);
    inline    UxImGuiWindow*  GetCurrentWindowRead()      { UxImGuiContext& g = *GUxImGui; return g.CurrentWindow; }
    inline    UxImGuiWindow*  GetCurrentWindow()          { UxImGuiContext& g = *GUxImGui; g.CurrentWindow->WriteAccessed = true; return g.CurrentWindow; }
    IMGUI_API UxImGuiWindow*  FindWindowByID(UxImGuiID id);
    IMGUI_API UxImGuiWindow*  FindWindowByName(const char* name);
    IMGUI_API void          UpdateWindowParentAndRootLinks(UxImGuiWindow* window, UxImGuiWindowFlags flags, UxImGuiWindow* parent_window);
    IMGUI_API void          UpdateWindowSkipRefresh(UxImGuiWindow* window);
    IMGUI_API UxImVec2        CalcWindowNextAutoFitSize(UxImGuiWindow* window);
    IMGUI_API bool          IsWindowChildOf(UxImGuiWindow* window, UxImGuiWindow* potential_parent, bool popup_hierarchy, bool dock_hierarchy);
    IMGUI_API bool          IsWindowWithinBeginStackOf(UxImGuiWindow* window, UxImGuiWindow* potential_parent);
    IMGUI_API bool          IsWindowAbove(UxImGuiWindow* potential_above, UxImGuiWindow* potential_below);
    IMGUI_API bool          IsWindowNavFocusable(UxImGuiWindow* window);
    IMGUI_API void          SetWindowPos(UxImGuiWindow* window, const UxImVec2& pos, UxImGuiCond cond = 0);
    IMGUI_API void          SetWindowSize(UxImGuiWindow* window, const UxImVec2& size, UxImGuiCond cond = 0);
    IMGUI_API void          SetWindowCollapsed(UxImGuiWindow* window, bool collapsed, UxImGuiCond cond = 0);
    IMGUI_API void          SetWindowHitTestHole(UxImGuiWindow* window, const UxImVec2& pos, const UxImVec2& size);
    IMGUI_API void          SetWindowHiddenAndSkipItemsForCurrentFrame(UxImGuiWindow* window);
    inline void             SetWindowParentWindowForFocusRoute(UxImGuiWindow* window, UxImGuiWindow* parent_window) { window->ParentWindowForFocusRoute = parent_window; } // You may also use SetNextWindowClass()'s FocusRouteParentWindowId field.
    inline UxImRect           WindowRectAbsToRel(UxImGuiWindow* window, const UxImRect& r) { UxImVec2 off = window->DC.CursorStartPos; return UxImRect(r.Min.x - off.x, r.Min.y - off.y, r.Max.x - off.x, r.Max.y - off.y); }
    inline UxImRect           WindowRectRelToAbs(UxImGuiWindow* window, const UxImRect& r) { UxImVec2 off = window->DC.CursorStartPos; return UxImRect(r.Min.x + off.x, r.Min.y + off.y, r.Max.x + off.x, r.Max.y + off.y); }
    inline UxImVec2           WindowPosAbsToRel(UxImGuiWindow* window, const UxImVec2& p)  { UxImVec2 off = window->DC.CursorStartPos; return UxImVec2(p.x - off.x, p.y - off.y); }
    inline UxImVec2           WindowPosRelToAbs(UxImGuiWindow* window, const UxImVec2& p)  { UxImVec2 off = window->DC.CursorStartPos; return UxImVec2(p.x + off.x, p.y + off.y); }

    // Windows: Display Order and Focus Order
    IMGUI_API void          FocusWindow(UxImGuiWindow* window, UxImGuiFocusRequestFlags flags = 0);
    IMGUI_API void          FocusTopMostWindowUnderOne(UxImGuiWindow* under_this_window, UxImGuiWindow* ignore_window, UxImGuiViewport* filter_viewport, UxImGuiFocusRequestFlags flags);
    IMGUI_API void          BringWindowToFocusFront(UxImGuiWindow* window);
    IMGUI_API void          BringWindowToDisplayFront(UxImGuiWindow* window);
    IMGUI_API void          BringWindowToDisplayBack(UxImGuiWindow* window);
    IMGUI_API void          BringWindowToDisplayBehind(UxImGuiWindow* window, UxImGuiWindow* above_window);
    IMGUI_API int           FindWindowDisplayIndex(UxImGuiWindow* window);
    IMGUI_API UxImGuiWindow*  FindBottomMostVisibleWindowWithinBeginStack(UxImGuiWindow* window);

    // Windows: Idle, Refresh Policies [EXPERIMENTAL]
    IMGUI_API void          SetNextWindowRefreshPolicy(UxImGuiWindowRefreshFlags flags);

    // Fonts, drawing
    IMGUI_API void          RegisterUserTexture(UxImTextureData* tex); // Register external texture
    IMGUI_API void          UnregisterUserTexture(UxImTextureData* tex);
    IMGUI_API void          RegisterFontAtlas(UxImFontAtlas* atlas);
    IMGUI_API void          UnregisterFontAtlas(UxImFontAtlas* atlas);
    IMGUI_API void          SetCurrentFont(UxImFont* font, float font_size);
    IMGUI_API void          SetFontRasterizerDensity(float rasterizer_density);
    inline float            GetFontRasterizerDensity() { return GUxImGui->FontRasterizerDensity; }
    IMGUI_API void          UpdateCurrentFontSize();
    inline float            GetRoundedFontSize(float size) { return IM_ROUND(size); }
    inline UxImFont*          GetDefaultFont() { UxImGuiContext& g = *GUxImGui; return g.IO.FontDefault ? g.IO.FontDefault : g.IO.Fonts->Fonts[0]; }
    IMGUI_API void          PushPasswordFont();
    IMGUI_API void          PopPasswordFont();
    inline UxImDrawList*      GetForegroundDrawList(UxImGuiWindow* window) { return GetForegroundDrawList(window->Viewport); }
    IMGUI_API void          AddDrawListToDrawDataEx(UxImDrawData* draw_data, UxImVector<UxImDrawList*>* out_list, UxImDrawList* draw_list);

    // Init
    IMGUI_API void          Initialize();
    IMGUI_API void          Shutdown();    // Since 1.60 this is a _private_ function. You can call DestroyContext() to destroy the context created by CreateContext().

    // NewFrame
    IMGUI_API void          UpdateInputEvents(bool trickle_fast_inputs);
    IMGUI_API void          UpdateHoveredWindowAndCaptureFlags(const UxImVec2& mouse_pos);
    IMGUI_API void          FindHoveredWindowEx(const UxImVec2& pos, bool find_first_and_in_any_viewport, UxImGuiWindow** out_hovered_window, UxImGuiWindow** out_hovered_window_under_moving_window);
    IMGUI_API void          StartMouseMovingWindow(UxImGuiWindow* window);
    IMGUI_API void          StartMouseMovingWindowOrNode(UxImGuiWindow* window, UxImGuiDockNode* node, bool undock);
    IMGUI_API void          UpdateMouseMovingWindowNewFrame();
    IMGUI_API void          UpdateMouseMovingWindowEndFrame();

    // Generic context hooks
    IMGUI_API UxImGuiID       AddContextHook(UxImGuiContext* context, const UxImGuiContextHook* hook);
    IMGUI_API void          RemoveContextHook(UxImGuiContext* context, UxImGuiID hook_to_remove);
    IMGUI_API void          CallContextHooks(UxImGuiContext* context, UxImGuiContextHookType type);

    // Viewports
    IMGUI_API void          TranslateWindowsInViewport(UxImGuiViewportP* viewport, const UxImVec2& old_pos, const UxImVec2& new_pos, const UxImVec2& old_size, const UxImVec2& new_size);
    IMGUI_API void          ScaleWindowsInViewport(UxImGuiViewportP* viewport, float scale);
    IMGUI_API void          DestroyPlatformWindow(UxImGuiViewportP* viewport);
    IMGUI_API void          SetWindowViewport(UxImGuiWindow* window, UxImGuiViewportP* viewport);
    IMGUI_API void          SetCurrentViewport(UxImGuiWindow* window, UxImGuiViewportP* viewport);
    IMGUI_API const UxImGuiPlatformMonitor*   GetViewportPlatformMonitor(UxImGuiViewport* viewport);
    IMGUI_API UxImGuiViewportP*               FindHoveredViewportFromPlatformWindowStack(const UxImVec2& mouse_platform_pos);

    // Settings
    IMGUI_API void                  MarkIniSettingsDirty();
    IMGUI_API void                  MarkIniSettingsDirty(UxImGuiWindow* window);
    IMGUI_API void                  ClearIniSettings();
    IMGUI_API void                  AddSettingsHandler(const UxImGuiSettingsHandler* handler);
    IMGUI_API void                  RemoveSettingsHandler(const char* type_name);
    IMGUI_API UxImGuiSettingsHandler* FindSettingsHandler(const char* type_name);

    // Settings - Windows
    IMGUI_API UxImGuiWindowSettings*  CreateNewWindowSettings(const char* name);
    IMGUI_API UxImGuiWindowSettings*  FindWindowSettingsByID(UxImGuiID id);
    IMGUI_API UxImGuiWindowSettings*  FindWindowSettingsByWindow(UxImGuiWindow* window);
    IMGUI_API void                  ClearWindowSettings(const char* name);

    // Localization
    IMGUI_API void          LocalizeRegisterEntries(const UxImGuiLocEntry* entries, int count);
    inline const char*      LocalizeGetMsg(UxImGuiLocKey key) { UxImGuiContext& g = *GUxImGui; const char* msg = g.LocalizationTable[key]; return msg ? msg : "*Missing Text*"; }

    // Scrolling
    IMGUI_API void          SetScrollX(UxImGuiWindow* window, float scroll_x);
    IMGUI_API void          SetScrollY(UxImGuiWindow* window, float scroll_y);
    IMGUI_API void          SetScrollFromPosX(UxImGuiWindow* window, float local_x, float center_x_ratio);
    IMGUI_API void          SetScrollFromPosY(UxImGuiWindow* window, float local_y, float center_y_ratio);

    // Early work-in-progress API (ScrollToItem() will become public)
    IMGUI_API void          ScrollToItem(UxImGuiScrollFlags flags = 0);
    IMGUI_API void          ScrollToRect(UxImGuiWindow* window, const UxImRect& rect, UxImGuiScrollFlags flags = 0);
    IMGUI_API UxImVec2        ScrollToRectEx(UxImGuiWindow* window, const UxImRect& rect, UxImGuiScrollFlags flags = 0);
//#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    inline void             ScrollToBringRectIntoView(UxImGuiWindow* window, const UxImRect& rect) { ScrollToRect(window, rect, UxImGuiScrollFlags_KeepVisibleEdgeY); }
//#endif

    // Basic Accessors
    inline UxImGuiItemStatusFlags GetItemStatusFlags() { UxImGuiContext& g = *GUxImGui; return g.LastItemData.StatusFlags; }
    inline UxImGuiItemFlags   GetItemFlags()  { UxImGuiContext& g = *GUxImGui; return g.LastItemData.ItemFlags; }
    inline UxImGuiID          GetActiveID()   { UxImGuiContext& g = *GUxImGui; return g.ActiveId; }
    inline UxImGuiID          GetFocusID()    { UxImGuiContext& g = *GUxImGui; return g.NavId; }
    IMGUI_API void          SetActiveID(UxImGuiID id, UxImGuiWindow* window);
    IMGUI_API void          SetFocusID(UxImGuiID id, UxImGuiWindow* window);
    IMGUI_API void          ClearActiveID();
    IMGUI_API UxImGuiID       GetHoveredID();
    IMGUI_API void          SetHoveredID(UxImGuiID id);
    IMGUI_API void          KeepAliveID(UxImGuiID id);
    IMGUI_API void          MarkItemEdited(UxImGuiID id);     // Mark data associated to given item as "edited", used by IsItemDeactivatedAfterEdit() function.
    IMGUI_API void          PushOverrideID(UxImGuiID id);     // Push given value as-is at the top of the ID stack (whereas PushID combines old and new hashes)
    IMGUI_API UxImGuiID       GetIDWithSeed(const char* str_id_begin, const char* str_id_end, UxImGuiID seed);
    IMGUI_API UxImGuiID       GetIDWithSeed(int n, UxImGuiID seed);

    // Basic Helpers for widget code
    IMGUI_API void          ItemSize(const UxImVec2& size, float text_baseline_y = -1.0f);
    inline void             ItemSize(const UxImRect& bb, float text_baseline_y = -1.0f) { ItemSize(bb.GetSize(), text_baseline_y); } // FIXME: This is a misleading API since we expect CursorPos to be bb.Min.
    IMGUI_API bool          ItemAdd(const UxImRect& bb, UxImGuiID id, const UxImRect* nav_bb = NULL, UxImGuiItemFlags extra_flags = 0);
    IMGUI_API bool          ItemHoverable(const UxImRect& bb, UxImGuiID id, UxImGuiItemFlags item_flags);
    IMGUI_API bool          IsWindowContentHoverable(UxImGuiWindow* window, UxImGuiHoveredFlags flags = 0);
    IMGUI_API bool          IsClippedEx(const UxImRect& bb, UxImGuiID id);
    IMGUI_API void          SetLastItemData(UxImGuiID item_id, UxImGuiItemFlags item_flags, UxImGuiItemStatusFlags status_flags, const UxImRect& item_rect);
    IMGUI_API UxImVec2        CalcItemSize(UxImVec2 size, float default_w, float default_h);
    IMGUI_API float         CalcWrapWidthForPos(const UxImVec2& pos, float wrap_pos_x);
    IMGUI_API void          PushMultiItemsWidths(int components, float width_full);
    IMGUI_API void          ShrinkWidths(UxImGuiShrinkWidthItem* items, int count, float width_excess);

    // Parameter stacks (shared)
    IMGUI_API const UxImGuiStyleVarInfo* GetStyleVarInfo(UxImGuiStyleVar idx);
    IMGUI_API void          BeginDisabledOverrideReenable();
    IMGUI_API void          EndDisabledOverrideReenable();

    // Logging/Capture
    IMGUI_API void          LogBegin(UxImGuiLogFlags flags, int auto_open_depth);         // -> BeginCapture() when we design v2 api, for now stay under the radar by using the old name.
    IMGUI_API void          LogToBuffer(int auto_open_depth = -1);                      // Start logging/capturing to internal buffer
    IMGUI_API void          LogRenderedText(const UxImVec2* ref_pos, const char* text, const char* text_end = NULL);
    IMGUI_API void          LogSetNextTextDecoration(const char* prefix, const char* suffix);

    // Childs
    IMGUI_API bool          BeginChildEx(const char* name, UxImGuiID id, const UxImVec2& size_arg, UxImGuiChildFlags child_flags, UxImGuiWindowFlags window_flags);

    // Popups, Modals
    IMGUI_API bool          BeginPopupEx(UxImGuiID id, UxImGuiWindowFlags extra_window_flags);
    IMGUI_API bool          BeginPopupMenuEx(UxImGuiID id, const char* label, UxImGuiWindowFlags extra_window_flags);
    IMGUI_API void          OpenPopupEx(UxImGuiID id, UxImGuiPopupFlags popup_flags = UxImGuiPopupFlags_None);
    IMGUI_API void          ClosePopupToLevel(int remaining, bool restore_focus_to_window_under_popup);
    IMGUI_API void          ClosePopupsOverWindow(UxImGuiWindow* ref_window, bool restore_focus_to_window_under_popup);
    IMGUI_API void          ClosePopupsExceptModals();
    IMGUI_API bool          IsPopupOpen(UxImGuiID id, UxImGuiPopupFlags popup_flags);
    IMGUI_API UxImRect        GetPopupAllowedExtentRect(UxImGuiWindow* window);
    IMGUI_API UxImGuiWindow*  GetTopMostPopupModal();
    IMGUI_API UxImGuiWindow*  GetTopMostAndVisiblePopupModal();
    IMGUI_API UxImGuiWindow*  FindBlockingModal(UxImGuiWindow* window);
    IMGUI_API UxImVec2        FindBestWindowPosForPopup(UxImGuiWindow* window);
    IMGUI_API UxImVec2        FindBestWindowPosForPopupEx(const UxImVec2& ref_pos, const UxImVec2& size, UxImGuiDir* last_dir, const UxImRect& r_outer, const UxImRect& r_avoid, UxImGuiPopupPositionPolicy policy);

    // Tooltips
    IMGUI_API bool          BeginTooltipEx(UxImGuiTooltipFlags tooltip_flags, UxImGuiWindowFlags extra_window_flags);
    IMGUI_API bool          BeginTooltipHidden();

    // Menus
    IMGUI_API bool          BeginViewportSideBar(const char* name, UxImGuiViewport* viewport, UxImGuiDir dir, float size, UxImGuiWindowFlags window_flags);
    IMGUI_API bool          BeginMenuEx(const char* label, const char* icon, bool enabled = true);
    IMGUI_API bool          MenuItemEx(const char* label, const char* icon, const char* shortcut = NULL, bool selected = false, bool enabled = true);

    // Combos
    IMGUI_API bool          BeginComboPopup(UxImGuiID popup_id, const UxImRect& bb, UxImGuiComboFlags flags);
    IMGUI_API bool          BeginComboPreview();
    IMGUI_API void          EndComboPreview();

    // Keyboard/Gamepad Navigation
    IMGUI_API void          NavInitWindow(UxImGuiWindow* window, bool force_reinit);
    IMGUI_API void          NavInitRequestApplyResult();
    IMGUI_API bool          NavMoveRequestButNoResultYet();
    IMGUI_API void          NavMoveRequestSubmit(UxImGuiDir move_dir, UxImGuiDir clip_dir, UxImGuiNavMoveFlags move_flags, UxImGuiScrollFlags scroll_flags);
    IMGUI_API void          NavMoveRequestForward(UxImGuiDir move_dir, UxImGuiDir clip_dir, UxImGuiNavMoveFlags move_flags, UxImGuiScrollFlags scroll_flags);
    IMGUI_API void          NavMoveRequestResolveWithLastItem(UxImGuiNavItemData* result);
    IMGUI_API void          NavMoveRequestResolveWithPastTreeNode(UxImGuiNavItemData* result, const UxImGuiTreeNodeStackData* tree_node_data);
    IMGUI_API void          NavMoveRequestCancel();
    IMGUI_API void          NavMoveRequestApplyResult();
    IMGUI_API void          NavMoveRequestTryWrapping(UxImGuiWindow* window, UxImGuiNavMoveFlags move_flags);
    IMGUI_API void          NavHighlightActivated(UxImGuiID id);
    IMGUI_API void          NavClearPreferredPosForAxis(UxImGuiAxis axis);
    IMGUI_API void          SetNavCursorVisibleAfterMove();
    IMGUI_API void          NavUpdateCurrentWindowIsScrollPushableX();
    IMGUI_API void          SetNavWindow(UxImGuiWindow* window);
    IMGUI_API void          SetNavID(UxImGuiID id, UxImGuiNavLayer nav_layer, UxImGuiID focus_scope_id, const UxImRect& rect_rel);
    IMGUI_API void          SetNavFocusScope(UxImGuiID focus_scope_id);

    // Focus/Activation
    // This should be part of a larger set of API: FocusItem(offset = -1), FocusItemByID(id), ActivateItem(offset = -1), ActivateItemByID(id) etc. which are
    // much harder to design and implement than expected. I have a couple of private branches on this matter but it's not simple. For now implementing the easy ones.
    IMGUI_API void          FocusItem();                    // Focus last item (no selection/activation).
    IMGUI_API void          ActivateItemByID(UxImGuiID id);   // Activate an item by ID (button, checkbox, tree node etc.). Activation is queued and processed on the next frame when the item is encountered again.

    // Inputs
    // FIXME: Eventually we should aim to move e.g. IsActiveIdUsingKey() into IsKeyXXX functions.
    inline bool             IsNamedKey(UxImGuiKey key)                    { return key >= UxImGuiKey_NamedKey_BEGIN && key < UxImGuiKey_NamedKey_END; }
    inline bool             IsNamedKeyOrMod(UxImGuiKey key)               { return (key >= UxImGuiKey_NamedKey_BEGIN && key < UxImGuiKey_NamedKey_END) || key == UxImGuiMod_Ctrl || key == UxImGuiMod_Shift || key == UxImGuiMod_Alt || key == UxImGuiMod_Super; }
    inline bool             IsLegacyKey(UxImGuiKey key)                   { return key >= UxImGuiKey_LegacyNativeKey_BEGIN && key < UxImGuiKey_LegacyNativeKey_END; }
    inline bool             IsKeyboardKey(UxImGuiKey key)                 { return key >= UxImGuiKey_Keyboard_BEGIN && key < UxImGuiKey_Keyboard_END; }
    inline bool             IsGamepadKey(UxImGuiKey key)                  { return key >= UxImGuiKey_Gamepad_BEGIN && key < UxImGuiKey_Gamepad_END; }
    inline bool             IsMouseKey(UxImGuiKey key)                    { return key >= UxImGuiKey_Mouse_BEGIN && key < UxImGuiKey_Mouse_END; }
    inline bool             IsAliasKey(UxImGuiKey key)                    { return key >= UxImGuiKey_Aliases_BEGIN && key < UxImGuiKey_Aliases_END; }
    inline bool             IsLRModKey(UxImGuiKey key)                    { return key >= UxImGuiKey_LeftCtrl && key <= UxImGuiKey_RightSuper; }
    UxImGuiKeyChord           FixupKeyChord(UxImGuiKeyChord key_chord);
    inline UxImGuiKey         ConvertSingleModFlagToKey(UxImGuiKey key)
    {
        if (key == UxImGuiMod_Ctrl) return UxImGuiKey_ReservedForModCtrl;
        if (key == UxImGuiMod_Shift) return UxImGuiKey_ReservedForModShift;
        if (key == UxImGuiMod_Alt) return UxImGuiKey_ReservedForModAlt;
        if (key == UxImGuiMod_Super) return UxImGuiKey_ReservedForModSuper;
        return key;
    }

    IMGUI_API UxImGuiKeyData* GetKeyData(UxImGuiContext* ctx, UxImGuiKey key);
    inline UxImGuiKeyData*    GetKeyData(UxImGuiKey key)                                    { UxImGuiContext& g = *GUxImGui; return GetKeyData(&g, key); }
    IMGUI_API const char*   GetKeyChordName(UxImGuiKeyChord key_chord);
    inline UxImGuiKey         MouseButtonToKey(UxImGuiMouseButton button)                   { IM_ASSERT(button >= 0 && button < UxImGuiMouseButton_COUNT); return (UxImGuiKey)(UxImGuiKey_MouseLeft + button); }
    IMGUI_API bool          IsMouseDragPastThreshold(UxImGuiMouseButton button, float lock_threshold = -1.0f);
    IMGUI_API UxImVec2        GetKeyMagnitude2d(UxImGuiKey key_left, UxImGuiKey key_right, UxImGuiKey key_up, UxImGuiKey key_down);
    IMGUI_API float         GetNavTweakPressedAmount(UxImGuiAxis axis);
    IMGUI_API int           CalcTypematicRepeatAmount(float t0, float t1, float repeat_delay, float repeat_rate);
    IMGUI_API void          GetTypematicRepeatRate(UxImGuiInputFlags flags, float* repeat_delay, float* repeat_rate);
    IMGUI_API void          TeleportMousePos(const UxImVec2& pos);
    IMGUI_API void          SetActiveIdUsingAllKeyboardKeys();
    inline bool             IsActiveIdUsingNavDir(UxImGuiDir dir)                         { UxImGuiContext& g = *GUxImGui; return (g.ActiveIdUsingNavDirMask & (1 << dir)) != 0; }

    // [EXPERIMENTAL] Low-Level: Key/Input Ownership
    // - The idea is that instead of "eating" a given input, we can link to an owner id.
    // - Ownership is most often claimed as a result of reacting to a press/down event (but occasionally may be claimed ahead).
    // - Input queries can then read input by specifying UxImGuiKeyOwner_Any (== 0), UxImGuiKeyOwner_NoOwner (== -1) or a custom ID.
    // - Legacy input queries (without specifying an owner or _Any or _None) are equivalent to using UxImGuiKeyOwner_Any (== 0).
    // - Input ownership is automatically released on the frame after a key is released. Therefore:
    //   - for ownership registration happening as a result of a down/press event, the SetKeyOwner() call may be done once (common case).
    //   - for ownership registration happening ahead of a down/press event, the SetKeyOwner() call needs to be made every frame (happens if e.g. claiming ownership on hover).
    // - SetItemKeyOwner() is a shortcut for common simple case. A custom widget will probably want to call SetKeyOwner() multiple times directly based on its interaction state.
    // - This is marked experimental because not all widgets are fully honoring the Set/Test idioms. We will need to move forward step by step.
    //   Please open a GitHub Issue to submit your usage scenario or if there's a use case you need solved.
    IMGUI_API UxImGuiID       GetKeyOwner(UxImGuiKey key);
    IMGUI_API void          SetKeyOwner(UxImGuiKey key, UxImGuiID owner_id, UxImGuiInputFlags flags = 0);
    IMGUI_API void          SetKeyOwnersForKeyChord(UxImGuiKeyChord key, UxImGuiID owner_id, UxImGuiInputFlags flags = 0);
    IMGUI_API void          SetItemKeyOwner(UxImGuiKey key, UxImGuiInputFlags flags);       // Set key owner to last item if it is hovered or active. Equivalent to 'if (IsItemHovered() || IsItemActive()) { SetKeyOwner(key, GetItemID());'.
    IMGUI_API bool          TestKeyOwner(UxImGuiKey key, UxImGuiID owner_id);               // Test that key is either not owned, either owned by 'owner_id'
    inline UxImGuiKeyOwnerData* GetKeyOwnerData(UxImGuiContext* ctx, UxImGuiKey key)          { if (key & UxImGuiMod_Mask_) key = ConvertSingleModFlagToKey(key); IM_ASSERT(IsNamedKey(key)); return &ctx->KeysOwnerData[key - UxImGuiKey_NamedKey_BEGIN]; }

    // [EXPERIMENTAL] High-Level: Input Access functions w/ support for Key/Input Ownership
    // - Important: legacy IsKeyPressed(UxImGuiKey, bool repeat=true) _DEFAULTS_ to repeat, new IsKeyPressed() requires _EXPLICIT_ UxImGuiInputFlags_Repeat flag.
    // - Expected to be later promoted to public API, the prototypes are designed to replace existing ones (since owner_id can default to Any == 0)
    // - Specifying a value for 'UxImGuiID owner' will test that EITHER the key is NOT owned (UNLESS locked), EITHER the key is owned by 'owner'.
    //   Legacy functions use UxImGuiKeyOwner_Any meaning that they typically ignore ownership, unless a call to SetKeyOwner() explicitly used UxImGuiInputFlags_LockThisFrame or UxImGuiInputFlags_LockUntilRelease.
    // - Binding generators may want to ignore those for now, or suffix them with Ex() until we decide if this gets moved into public API.
    IMGUI_API bool          IsKeyDown(UxImGuiKey key, UxImGuiID owner_id);
    IMGUI_API bool          IsKeyPressed(UxImGuiKey key, UxImGuiInputFlags flags, UxImGuiID owner_id = 0);    // Important: when transitioning from old to new IsKeyPressed(): old API has "bool repeat = true", so would default to repeat. New API requiress explicit UxImGuiInputFlags_Repeat.
    IMGUI_API bool          IsKeyReleased(UxImGuiKey key, UxImGuiID owner_id);
    IMGUI_API bool          IsKeyChordPressed(UxImGuiKeyChord key_chord, UxImGuiInputFlags flags, UxImGuiID owner_id = 0);
    IMGUI_API bool          IsMouseDown(UxImGuiMouseButton button, UxImGuiID owner_id);
    IMGUI_API bool          IsMouseClicked(UxImGuiMouseButton button, UxImGuiInputFlags flags, UxImGuiID owner_id = 0);
    IMGUI_API bool          IsMouseReleased(UxImGuiMouseButton button, UxImGuiID owner_id);
    IMGUI_API bool          IsMouseDoubleClicked(UxImGuiMouseButton button, UxImGuiID owner_id);

    // Shortcut Testing & Routing
    // - Set Shortcut() and SetNextItemShortcut() in imgui.h
    // - When a policy (except for UxImGuiInputFlags_RouteAlways *) is set, Shortcut() will register itself with SetShortcutRouting(),
    //   allowing the system to decide where to route the input among other route-aware calls.
    //   (* using UxImGuiInputFlags_RouteAlways is roughly equivalent to calling IsKeyChordPressed(key) and bypassing route registration and check)
    // - When using one of the routing option:
    //   - The default route is UxImGuiInputFlags_RouteFocused (accept inputs if window is in focus stack. Deep-most focused window takes inputs. ActiveId takes inputs over deep-most focused window.)
    //   - Routes are requested given a chord (key + modifiers) and a routing policy.
    //   - Routes are resolved during NewFrame(): if keyboard modifiers are matching current ones: SetKeyOwner() is called + route is granted for the frame.
    //   - Each route may be granted to a single owner. When multiple requests are made we have policies to select the winning route (e.g. deep most window).
    //   - Multiple read sites may use the same owner id can all access the granted route.
    //   - When owner_id is 0 we use the current Focus Scope ID as a owner ID in order to identify our location.
    // - You can chain two unrelated windows in the focus stack using SetWindowParentWindowForFocusRoute()
    //   e.g. if you have a tool window associated to a document, and you want document shortcuts to run when the tool is focused.
    IMGUI_API bool          Shortcut(UxImGuiKeyChord key_chord, UxImGuiInputFlags flags, UxImGuiID owner_id);
    IMGUI_API bool          SetShortcutRouting(UxImGuiKeyChord key_chord, UxImGuiInputFlags flags, UxImGuiID owner_id); // owner_id needs to be explicit and cannot be 0
    IMGUI_API bool          TestShortcutRouting(UxImGuiKeyChord key_chord, UxImGuiID owner_id);
    IMGUI_API UxImGuiKeyRoutingData* GetShortcutRoutingData(UxImGuiKeyChord key_chord);

    // Docking
    // (some functions are only declared in imgui.cpp, see Docking section)
    IMGUI_API void          DockContextInitialize(UxImGuiContext* ctx);
    IMGUI_API void          DockContextShutdown(UxImGuiContext* ctx);
    IMGUI_API void          DockContextClearNodes(UxImGuiContext* ctx, UxImGuiID root_id, bool clear_settings_refs); // Use root_id==0 to clear all
    IMGUI_API void          DockContextRebuildNodes(UxImGuiContext* ctx);
    IMGUI_API void          DockContextNewFrameUpdateUndocking(UxImGuiContext* ctx);
    IMGUI_API void          DockContextNewFrameUpdateDocking(UxImGuiContext* ctx);
    IMGUI_API void          DockContextEndFrame(UxImGuiContext* ctx);
    IMGUI_API UxImGuiID       DockContextGenNodeID(UxImGuiContext* ctx);
    IMGUI_API void          DockContextQueueDock(UxImGuiContext* ctx, UxImGuiWindow* target, UxImGuiDockNode* target_node, UxImGuiWindow* payload, UxImGuiDir split_dir, float split_ratio, bool split_outer);
    IMGUI_API void          DockContextQueueUndockWindow(UxImGuiContext* ctx, UxImGuiWindow* window);
    IMGUI_API void          DockContextQueueUndockNode(UxImGuiContext* ctx, UxImGuiDockNode* node);
    IMGUI_API void          DockContextProcessUndockWindow(UxImGuiContext* ctx, UxImGuiWindow* window, bool clear_persistent_docking_ref = true);
    IMGUI_API void          DockContextProcessUndockNode(UxImGuiContext* ctx, UxImGuiDockNode* node);
    IMGUI_API bool          DockContextCalcDropPosForDocking(UxImGuiWindow* target, UxImGuiDockNode* target_node, UxImGuiWindow* payload_window, UxImGuiDockNode* payload_node, UxImGuiDir split_dir, bool split_outer, UxImVec2* out_pos);
    IMGUI_API UxImGuiDockNode*DockContextFindNodeByID(UxImGuiContext* ctx, UxImGuiID id);
    IMGUI_API void          DockNodeWindowMenuHandler_Default(UxImGuiContext* ctx, UxImGuiDockNode* node, UxImGuiTabBar* tab_bar);
    IMGUI_API bool          DockNodeBeginAmendTabBar(UxImGuiDockNode* node);
    IMGUI_API void          DockNodeEndAmendTabBar();
    inline UxImGuiDockNode*   DockNodeGetRootNode(UxImGuiDockNode* node)                 { while (node->ParentNode) node = node->ParentNode; return node; }
    inline bool             DockNodeIsInHierarchyOf(UxImGuiDockNode* node, UxImGuiDockNode* parent) { while (node) { if (node == parent) return true; node = node->ParentNode; } return false; }
    inline int              DockNodeGetDepth(const UxImGuiDockNode* node)              { int depth = 0; while (node->ParentNode) { node = node->ParentNode; depth++; } return depth; }
    inline UxImGuiID          DockNodeGetWindowMenuButtonId(const UxImGuiDockNode* node) { return UxImHashStr("#COLLAPSE", 0, node->ID); }
    inline UxImGuiDockNode*   GetWindowDockNode()                                      { UxImGuiContext& g = *GUxImGui; return g.CurrentWindow->DockNode; }
    IMGUI_API bool          GetWindowAlwaysWantOwnTabBar(UxImGuiWindow* window);
    IMGUI_API void          BeginDocked(UxImGuiWindow* window, bool* p_open);
    IMGUI_API void          BeginDockableDragDropSource(UxImGuiWindow* window);
    IMGUI_API void          BeginDockableDragDropTarget(UxImGuiWindow* window);
    IMGUI_API void          SetWindowDock(UxImGuiWindow* window, UxImGuiID dock_id, UxImGuiCond cond);

    // Docking - Builder function needs to be generally called before the node is used/submitted.
    // - The DockBuilderXXX functions are designed to _eventually_ become a public API, but it is too early to expose it and guarantee stability.
    // - Do not hold on UxImGuiDockNode* pointers! They may be invalidated by any split/merge/remove operation and every frame.
    // - To create a DockSpace() node, make sure to set the UxImGuiDockNodeFlags_DockSpace flag when calling DockBuilderAddNode().
    //   You can create dockspace nodes (attached to a window) _or_ floating nodes (carry its own window) with this API.
    // - DockBuilderSplitNode() create 2 child nodes within 1 node. The initial node becomes a parent node.
    // - If you intend to split the node immediately after creation using DockBuilderSplitNode(), make sure
    //   to call DockBuilderSetNodeSize() beforehand. If you don't, the resulting split sizes may not be reliable.
    // - Call DockBuilderFinish() after you are done.
    IMGUI_API void          DockBuilderDockWindow(const char* window_name, UxImGuiID node_id);
    IMGUI_API UxImGuiDockNode*DockBuilderGetNode(UxImGuiID node_id);
    inline UxImGuiDockNode*   DockBuilderGetCentralNode(UxImGuiID node_id)              { UxImGuiDockNode* node = DockBuilderGetNode(node_id); if (!node) return NULL; return DockNodeGetRootNode(node)->CentralNode; }
    IMGUI_API UxImGuiID       DockBuilderAddNode(UxImGuiID node_id = 0, UxImGuiDockNodeFlags flags = 0);
    IMGUI_API void          DockBuilderRemoveNode(UxImGuiID node_id);                 // Remove node and all its child, undock all windows
    IMGUI_API void          DockBuilderRemoveNodeDockedWindows(UxImGuiID node_id, bool clear_settings_refs = true);
    IMGUI_API void          DockBuilderRemoveNodeChildNodes(UxImGuiID node_id);       // Remove all split/hierarchy. All remaining docked windows will be re-docked to the remaining root node (node_id).
    IMGUI_API void          DockBuilderSetNodePos(UxImGuiID node_id, UxImVec2 pos);
    IMGUI_API void          DockBuilderSetNodeSize(UxImGuiID node_id, UxImVec2 size);
    IMGUI_API UxImGuiID       DockBuilderSplitNode(UxImGuiID node_id, UxImGuiDir split_dir, float size_ratio_for_node_at_dir, UxImGuiID* out_id_at_dir, UxImGuiID* out_id_at_opposite_dir); // Create 2 child nodes in this parent node.
    IMGUI_API void          DockBuilderCopyDockSpace(UxImGuiID src_dockspace_id, UxImGuiID dst_dockspace_id, UxImVector<const char*>* in_window_remap_pairs);
    IMGUI_API void          DockBuilderCopyNode(UxImGuiID src_node_id, UxImGuiID dst_node_id, UxImVector<UxImGuiID>* out_node_remap_pairs);
    IMGUI_API void          DockBuilderCopyWindowSettings(const char* src_name, const char* dst_name);
    IMGUI_API void          DockBuilderFinish(UxImGuiID node_id);

    // [EXPERIMENTAL] Focus Scope
    // This is generally used to identify a unique input location (for e.g. a selection set)
    // There is one per window (automatically set in Begin), but:
    // - Selection patterns generally need to react (e.g. clear a selection) when landing on one item of the set.
    //   So in order to identify a set multiple lists in same window may each need a focus scope.
    //   If you imagine an hypothetical BeginSelectionGroup()/EndSelectionGroup() api, it would likely call PushFocusScope()/EndFocusScope()
    // - Shortcut routing also use focus scope as a default location identifier if an owner is not provided.
    // We don't use the ID Stack for this as it is common to want them separate.
    IMGUI_API void          PushFocusScope(UxImGuiID id);
    IMGUI_API void          PopFocusScope();
    inline UxImGuiID          GetCurrentFocusScope() { UxImGuiContext& g = *GUxImGui; return g.CurrentFocusScopeId; }   // Focus scope we are outputting into, set by PushFocusScope()

    // Drag and Drop
    IMGUI_API bool          IsDragDropActive();
    IMGUI_API bool          BeginDragDropTargetCustom(const UxImRect& bb, UxImGuiID id);
    IMGUI_API void          ClearDragDrop();
    IMGUI_API bool          IsDragDropPayloadBeingAccepted();
    IMGUI_API void          RenderDragDropTargetRect(const UxImRect& bb, const UxImRect& item_clip_rect);

    // Typing-Select API
    // (provide Windows Explorer style "select items by typing partial name" + "cycle through items by typing same letter" feature)
    // (this is currently not documented nor used by main library, but should work. See "widgets_typingselect" in imgui_test_suite for usage code. Please let us know if you use this!)
    IMGUI_API UxImGuiTypingSelectRequest* GetTypingSelectRequest(UxImGuiTypingSelectFlags flags = UxImGuiTypingSelectFlags_None);
    IMGUI_API int           TypingSelectFindMatch(UxImGuiTypingSelectRequest* req, int items_count, const char* (*get_item_name_func)(void*, int), void* user_data, int nav_item_idx);
    IMGUI_API int           TypingSelectFindNextSingleCharMatch(UxImGuiTypingSelectRequest* req, int items_count, const char* (*get_item_name_func)(void*, int), void* user_data, int nav_item_idx);
    IMGUI_API int           TypingSelectFindBestLeadingMatch(UxImGuiTypingSelectRequest* req, int items_count, const char* (*get_item_name_func)(void*, int), void* user_data);

    // Box-Select API
    IMGUI_API bool          BeginBoxSelect(const UxImRect& scope_rect, UxImGuiWindow* window, UxImGuiID box_select_id, UxImGuiMultiSelectFlags ms_flags);
    IMGUI_API void          EndBoxSelect(const UxImRect& scope_rect, UxImGuiMultiSelectFlags ms_flags);

    // Multi-Select API
    IMGUI_API void          MultiSelectItemHeader(UxImGuiID id, bool* p_selected, UxImGuiButtonFlags* p_button_flags);
    IMGUI_API void          MultiSelectItemFooter(UxImGuiID id, bool* p_selected, bool* p_pressed);
    IMGUI_API void          MultiSelectAddSetAll(UxImGuiMultiSelectTempData* ms, bool selected);
    IMGUI_API void          MultiSelectAddSetRange(UxImGuiMultiSelectTempData* ms, bool selected, int range_dir, UxImGuiSelectionUserData first_item, UxImGuiSelectionUserData last_item);
    inline UxImGuiBoxSelectState*     GetBoxSelectState(UxImGuiID id)   { UxImGuiContext& g = *GUxImGui; return (id != 0 && g.BoxSelectState.ID == id && g.BoxSelectState.IsActive) ? &g.BoxSelectState : NULL; }
    inline UxImGuiMultiSelectState*   GetMultiSelectState(UxImGuiID id) { UxImGuiContext& g = *GUxImGui; return g.MultiSelectStorage.GetByKey(id); }

    // Internal Columns API (this is not exposed because we will encourage transitioning to the Tables API)
    IMGUI_API void          SetWindowClipRectBeforeSetChannel(UxImGuiWindow* window, const UxImRect& clip_rect);
    IMGUI_API void          BeginColumns(const char* str_id, int count, UxImGuiOldColumnFlags flags = 0); // setup number of columns. use an identifier to distinguish multiple column sets. close with EndColumns().
    IMGUI_API void          EndColumns();                                                               // close columns
    IMGUI_API void          PushColumnClipRect(int column_index);
    IMGUI_API void          PushColumnsBackground();
    IMGUI_API void          PopColumnsBackground();
    IMGUI_API UxImGuiID       GetColumnsID(const char* str_id, int count);
    IMGUI_API UxImGuiOldColumns* FindOrCreateColumns(UxImGuiWindow* window, UxImGuiID id);
    IMGUI_API float         GetColumnOffsetFromNorm(const UxImGuiOldColumns* columns, float offset_norm);
    IMGUI_API float         GetColumnNormFromOffset(const UxImGuiOldColumns* columns, float offset);

    // Tables: Candidates for public API
    IMGUI_API void          TableOpenContextMenu(int column_n = -1);
    IMGUI_API void          TableSetColumnWidth(int column_n, float width);
    IMGUI_API void          TableSetColumnSortDirection(int column_n, UxImGuiSortDirection sort_direction, bool append_to_sort_specs);
    IMGUI_API int           TableGetHoveredRow();       // Retrieve *PREVIOUS FRAME* hovered row. This difference with TableGetHoveredColumn() is the reason why this is not public yet.
    IMGUI_API float         TableGetHeaderRowHeight();
    IMGUI_API float         TableGetHeaderAngledMaxLabelWidth();
    IMGUI_API void          TablePushBackgroundChannel();
    IMGUI_API void          TablePopBackgroundChannel();
    IMGUI_API void          TablePushColumnChannel(int column_n);
    IMGUI_API void          TablePopColumnChannel();
    IMGUI_API void          TableAngledHeadersRowEx(UxImGuiID row_id, float angle, float max_label_width, const UxImGuiTableHeaderData* data, int data_count);

    // Tables: Internals
    inline    UxImGuiTable*   GetCurrentTable() { UxImGuiContext& g = *GUxImGui; return g.CurrentTable; }
    IMGUI_API UxImGuiTable*   TableFindByID(UxImGuiID id);
    IMGUI_API bool          BeginTableEx(const char* name, UxImGuiID id, int columns_count, UxImGuiTableFlags flags = 0, const UxImVec2& outer_size = UxImVec2(0, 0), float inner_width = 0.0f);
    IMGUI_API void          TableBeginInitMemory(UxImGuiTable* table, int columns_count);
    IMGUI_API void          TableBeginApplyRequests(UxImGuiTable* table);
    IMGUI_API void          TableSetupDrawChannels(UxImGuiTable* table);
    IMGUI_API void          TableUpdateLayout(UxImGuiTable* table);
    IMGUI_API void          TableUpdateBorders(UxImGuiTable* table);
    IMGUI_API void          TableUpdateColumnsWeightFromWidth(UxImGuiTable* table);
    IMGUI_API void          TableDrawBorders(UxImGuiTable* table);
    IMGUI_API void          TableDrawDefaultContextMenu(UxImGuiTable* table, UxImGuiTableFlags flags_for_section_to_display);
    IMGUI_API bool          TableBeginContextMenuPopup(UxImGuiTable* table);
    IMGUI_API void          TableMergeDrawChannels(UxImGuiTable* table);
    inline UxImGuiTableInstanceData*  TableGetInstanceData(UxImGuiTable* table, int instance_no) { if (instance_no == 0) return &table->InstanceDataFirst; return &table->InstanceDataExtra[instance_no - 1]; }
    inline UxImGuiID                  TableGetInstanceID(UxImGuiTable* table, int instance_no)   { return TableGetInstanceData(table, instance_no)->TableInstanceID; }
    IMGUI_API void          TableSortSpecsSanitize(UxImGuiTable* table);
    IMGUI_API void          TableSortSpecsBuild(UxImGuiTable* table);
    IMGUI_API UxImGuiSortDirection TableGetColumnNextSortDirection(UxImGuiTableColumn* column);
    IMGUI_API void          TableFixColumnSortDirection(UxImGuiTable* table, UxImGuiTableColumn* column);
    IMGUI_API float         TableGetColumnWidthAuto(UxImGuiTable* table, UxImGuiTableColumn* column);
    IMGUI_API void          TableBeginRow(UxImGuiTable* table);
    IMGUI_API void          TableEndRow(UxImGuiTable* table);
    IMGUI_API void          TableBeginCell(UxImGuiTable* table, int column_n);
    IMGUI_API void          TableEndCell(UxImGuiTable* table);
    IMGUI_API UxImRect        TableGetCellBgRect(const UxImGuiTable* table, int column_n);
    IMGUI_API const char*   TableGetColumnName(const UxImGuiTable* table, int column_n);
    IMGUI_API UxImGuiID       TableGetColumnResizeID(UxImGuiTable* table, int column_n, int instance_no = 0);
    IMGUI_API float         TableCalcMaxColumnWidth(const UxImGuiTable* table, int column_n);
    IMGUI_API void          TableSetColumnWidthAutoSingle(UxImGuiTable* table, int column_n);
    IMGUI_API void          TableSetColumnWidthAutoAll(UxImGuiTable* table);
    IMGUI_API void          TableRemove(UxImGuiTable* table);
    IMGUI_API void          TableGcCompactTransientBuffers(UxImGuiTable* table);
    IMGUI_API void          TableGcCompactTransientBuffers(UxImGuiTableTempData* table);
    IMGUI_API void          TableGcCompactSettings();

    // Tables: Settings
    IMGUI_API void                  TableLoadSettings(UxImGuiTable* table);
    IMGUI_API void                  TableSaveSettings(UxImGuiTable* table);
    IMGUI_API void                  TableResetSettings(UxImGuiTable* table);
    IMGUI_API UxImGuiTableSettings*   TableGetBoundSettings(UxImGuiTable* table);
    IMGUI_API void                  TableSettingsAddSettingsHandler();
    IMGUI_API UxImGuiTableSettings*   TableSettingsCreate(UxImGuiID id, int columns_count);
    IMGUI_API UxImGuiTableSettings*   TableSettingsFindByID(UxImGuiID id);

    // Tab Bars
    inline    UxImGuiTabBar*  GetCurrentTabBar() { UxImGuiContext& g = *GUxImGui; return g.CurrentTabBar; }
    IMGUI_API bool          BeginTabBarEx(UxImGuiTabBar* tab_bar, const UxImRect& bb, UxImGuiTabBarFlags flags);
    IMGUI_API UxImGuiTabItem* TabBarFindTabByID(UxImGuiTabBar* tab_bar, UxImGuiID tab_id);
    IMGUI_API UxImGuiTabItem* TabBarFindTabByOrder(UxImGuiTabBar* tab_bar, int order);
    IMGUI_API UxImGuiTabItem* TabBarFindMostRecentlySelectedTabForActiveWindow(UxImGuiTabBar* tab_bar);
    IMGUI_API UxImGuiTabItem* TabBarGetCurrentTab(UxImGuiTabBar* tab_bar);
    inline int              TabBarGetTabOrder(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab) { return tab_bar->Tabs.index_from_ptr(tab); }
    IMGUI_API const char*   TabBarGetTabName(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab);
    IMGUI_API void          TabBarAddTab(UxImGuiTabBar* tab_bar, UxImGuiTabItemFlags tab_flags, UxImGuiWindow* window);
    IMGUI_API void          TabBarRemoveTab(UxImGuiTabBar* tab_bar, UxImGuiID tab_id);
    IMGUI_API void          TabBarCloseTab(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab);
    IMGUI_API void          TabBarQueueFocus(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab);
    IMGUI_API void          TabBarQueueFocus(UxImGuiTabBar* tab_bar, const char* tab_name);
    IMGUI_API void          TabBarQueueReorder(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab, int offset);
    IMGUI_API void          TabBarQueueReorderFromMousePos(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab, UxImVec2 mouse_pos);
    IMGUI_API bool          TabBarProcessReorder(UxImGuiTabBar* tab_bar);
    IMGUI_API bool          TabItemEx(UxImGuiTabBar* tab_bar, const char* label, bool* p_open, UxImGuiTabItemFlags flags, UxImGuiWindow* docked_window);
    IMGUI_API void          TabItemSpacing(const char* str_id, UxImGuiTabItemFlags flags, float width);
    IMGUI_API UxImVec2        TabItemCalcSize(const char* label, bool has_close_button_or_unsaved_marker);
    IMGUI_API UxImVec2        TabItemCalcSize(UxImGuiWindow* window);
    IMGUI_API void          TabItemBackground(UxImDrawList* draw_list, const UxImRect& bb, UxImGuiTabItemFlags flags, UxImU32 col);
    IMGUI_API void          TabItemLabelAndCloseButton(UxImDrawList* draw_list, const UxImRect& bb, UxImGuiTabItemFlags flags, UxImVec2 frame_padding, const char* label, UxImGuiID tab_id, UxImGuiID close_button_id, bool is_contents_visible, bool* out_just_closed, bool* out_text_clipped);

    // Render helpers
    // AVOID USING OUTSIDE OF IMGUI.CPP! NOT FOR PUBLIC CONSUMPTION. THOSE FUNCTIONS ARE A MESS. THEIR SIGNATURE AND BEHAVIOR WILL CHANGE, THEY NEED TO BE REFACTORED INTO SOMETHING DECENT.
    // NB: All position are in absolute pixels coordinates (we are never using window coordinates internally)
    IMGUI_API void          RenderText(UxImVec2 pos, const char* text, const char* text_end = NULL, bool hide_text_after_hash = true);
    IMGUI_API void          RenderTextWrapped(UxImVec2 pos, const char* text, const char* text_end, float wrap_width);
    IMGUI_API void          RenderTextClipped(const UxImVec2& pos_min, const UxImVec2& pos_max, const char* text, const char* text_end, const UxImVec2* text_size_if_known, const UxImVec2& align = UxImVec2(0, 0), const UxImRect* clip_rect = NULL);
    IMGUI_API void          RenderTextClippedEx(UxImDrawList* draw_list, const UxImVec2& pos_min, const UxImVec2& pos_max, const char* text, const char* text_end, const UxImVec2* text_size_if_known, const UxImVec2& align = UxImVec2(0, 0), const UxImRect* clip_rect = NULL);
    IMGUI_API void          RenderTextEllipsis(UxImDrawList* draw_list, const UxImVec2& pos_min, const UxImVec2& pos_max, float ellipsis_max_x, const char* text, const char* text_end, const UxImVec2* text_size_if_known);
    IMGUI_API void          RenderFrame(UxImVec2 p_min, UxImVec2 p_max, UxImU32 fill_col, bool borders = true, float rounding = 0.0f);
    IMGUI_API void          RenderFrameBorder(UxImVec2 p_min, UxImVec2 p_max, float rounding = 0.0f);
    IMGUI_API void          RenderColorRectWithAlphaCheckerboard(UxImDrawList* draw_list, UxImVec2 p_min, UxImVec2 p_max, UxImU32 fill_col, float grid_step, UxImVec2 grid_off, float rounding = 0.0f, UxImDrawFlags flags = 0);
    IMGUI_API void          RenderNavCursor(const UxImRect& bb, UxImGuiID id, UxImGuiNavRenderCursorFlags flags = UxImGuiNavRenderCursorFlags_None); // Navigation highlight
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    inline    void          RenderNavHighlight(const UxImRect& bb, UxImGuiID id, UxImGuiNavRenderCursorFlags flags = UxImGuiNavRenderCursorFlags_None) { RenderNavCursor(bb, id, flags); } // Renamed in 1.91.4
#endif
    IMGUI_API const char*   FindRenderedTextEnd(const char* text, const char* text_end = NULL); // Find the optional ## from which we stop displaying text.
    IMGUI_API void          RenderMouseCursor(UxImVec2 pos, float scale, UxImGuiMouseCursor mouse_cursor, UxImU32 col_fill, UxImU32 col_border, UxImU32 col_shadow);

    // Render helpers (those functions don't access any UxImGui state!)
    IMGUI_API void          RenderArrow(UxImDrawList* draw_list, UxImVec2 pos, UxImU32 col, UxImGuiDir dir, float scale = 1.0f);
    IMGUI_API void          RenderBullet(UxImDrawList* draw_list, UxImVec2 pos, UxImU32 col);
    IMGUI_API void          RenderCheckMark(UxImDrawList* draw_list, UxImVec2 pos, UxImU32 col, float sz);
    IMGUI_API void          RenderArrowPointingAt(UxImDrawList* draw_list, UxImVec2 pos, UxImVec2 half_sz, UxImGuiDir direction, UxImU32 col);
    IMGUI_API void          RenderArrowDockMenu(UxImDrawList* draw_list, UxImVec2 p_min, float sz, UxImU32 col);
    IMGUI_API void          RenderRectFilledRangeH(UxImDrawList* draw_list, const UxImRect& rect, UxImU32 col, float x_start_norm, float x_end_norm, float rounding);
    IMGUI_API void          RenderRectFilledWithHole(UxImDrawList* draw_list, const UxImRect& outer, const UxImRect& inner, UxImU32 col, float rounding);
    IMGUI_API UxImDrawFlags   CalcRoundingFlagsForRectInRect(const UxImRect& r_in, const UxImRect& r_outer, float threshold);

    // Widgets: Text
    IMGUI_API void          TextEx(const char* text, const char* text_end = NULL, UxImGuiTextFlags flags = 0);
    IMGUI_API void          TextAligned(float align_x, float size_x, const char* fmt, ...);               // FIXME-WIP: Works but API is likely to be reworked. This is designed for 1 item on the line. (#7024)
    IMGUI_API void          TextAlignedV(float align_x, float size_x, const char* fmt, va_list args);

    // Widgets
    IMGUI_API bool          ButtonEx(const char* label, const UxImVec2& size_arg = UxImVec2(0, 0), UxImGuiButtonFlags flags = 0);
    IMGUI_API bool          ArrowButtonEx(const char* str_id, UxImGuiDir dir, UxImVec2 size_arg, UxImGuiButtonFlags flags = 0);
    IMGUI_API bool          ImageButtonEx(UxImGuiID id, UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1, const UxImVec4& bg_col, const UxImVec4& tint_col, UxImGuiButtonFlags flags = 0);
    IMGUI_API void          SeparatorEx(UxImGuiSeparatorFlags flags, float thickness = 1.0f);
    IMGUI_API void          SeparatorTextEx(UxImGuiID id, const char* label, const char* label_end, float extra_width);
    IMGUI_API bool          CheckboxFlags(const char* label, UxImS64* flags, UxImS64 flags_value);
    IMGUI_API bool          CheckboxFlags(const char* label, UxImU64* flags, UxImU64 flags_value);

    // Widgets: Window Decorations
    IMGUI_API bool          CloseButton(UxImGuiID id, const UxImVec2& pos);
    IMGUI_API bool          CollapseButton(UxImGuiID id, const UxImVec2& pos, UxImGuiDockNode* dock_node);
    IMGUI_API void          Scrollbar(UxImGuiAxis axis);
    IMGUI_API bool          ScrollbarEx(const UxImRect& bb, UxImGuiID id, UxImGuiAxis axis, UxImS64* p_scroll_v, UxImS64 avail_v, UxImS64 contents_v, UxImDrawFlags draw_rounding_flags = 0);
    IMGUI_API UxImRect        GetWindowScrollbarRect(UxImGuiWindow* window, UxImGuiAxis axis);
    IMGUI_API UxImGuiID       GetWindowScrollbarID(UxImGuiWindow* window, UxImGuiAxis axis);
    IMGUI_API UxImGuiID       GetWindowResizeCornerID(UxImGuiWindow* window, int n); // 0..3: corners
    IMGUI_API UxImGuiID       GetWindowResizeBorderID(UxImGuiWindow* window, UxImGuiDir dir);

    // Widgets low-level behaviors
    IMGUI_API bool          ButtonBehavior(const UxImRect& bb, UxImGuiID id, bool* out_hovered, bool* out_held, UxImGuiButtonFlags flags = 0);
    IMGUI_API bool          DragBehavior(UxImGuiID id, UxImGuiDataType data_type, void* p_v, float v_speed, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags);
    IMGUI_API bool          SliderBehavior(const UxImRect& bb, UxImGuiID id, UxImGuiDataType data_type, void* p_v, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags, UxImRect* out_grab_bb);
    IMGUI_API bool          SplitterBehavior(const UxImRect& bb, UxImGuiID id, UxImGuiAxis axis, float* size1, float* size2, float min_size1, float min_size2, float hover_extend = 0.0f, float hover_visibility_delay = 0.0f, UxImU32 bg_col = 0);

    // Widgets: Tree Nodes
    IMGUI_API bool          TreeNodeBehavior(UxImGuiID id, UxImGuiTreeNodeFlags flags, const char* label, const char* label_end = NULL);
    IMGUI_API void          TreeNodeDrawLineToChildNode(const UxImVec2& target_pos);
    IMGUI_API void          TreeNodeDrawLineToTreePop(const UxImGuiTreeNodeStackData* data);
    IMGUI_API void          TreePushOverrideID(UxImGuiID id);
    IMGUI_API bool          TreeNodeGetOpen(UxImGuiID storage_id);
    IMGUI_API void          TreeNodeSetOpen(UxImGuiID storage_id, bool open);
    IMGUI_API bool          TreeNodeUpdateNextOpen(UxImGuiID storage_id, UxImGuiTreeNodeFlags flags);   // Return open state. Consume previous SetNextItemOpen() data, if any. May return true when logging.

    // Template functions are instantiated in imgui_widgets.cpp for a finite number of types.
    // To use them externally (for custom widget) you may need an "extern template" statement in your code in order to link to existing instances and silence Clang warnings (see #2036).
    // e.g. " extern template IMGUI_API float RoundScalarWithFormatT<float, float>(const char* format, UxImGuiDataType data_type, float v); "
    template<typename T, typename SIGNED_T, typename FLOAT_T>   IMGUI_API float ScaleRatioFromValueT(UxImGuiDataType data_type, T v, T v_min, T v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_size);
    template<typename T, typename SIGNED_T, typename FLOAT_T>   IMGUI_API T     ScaleValueFromRatioT(UxImGuiDataType data_type, float t, T v_min, T v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_size);
    template<typename T, typename SIGNED_T, typename FLOAT_T>   IMGUI_API bool  DragBehaviorT(UxImGuiDataType data_type, T* v, float v_speed, T v_min, T v_max, const char* format, UxImGuiSliderFlags flags);
    template<typename T, typename SIGNED_T, typename FLOAT_T>   IMGUI_API bool  SliderBehaviorT(const UxImRect& bb, UxImGuiID id, UxImGuiDataType data_type, T* v, T v_min, T v_max, const char* format, UxImGuiSliderFlags flags, UxImRect* out_grab_bb);
    template<typename T>                                        IMGUI_API T     RoundScalarWithFormatT(const char* format, UxImGuiDataType data_type, T v);
    template<typename T>                                        IMGUI_API bool  CheckboxFlagsT(const char* label, T* flags, T flags_value);

    // Data type helpers
    IMGUI_API const UxImGuiDataTypeInfo*  DataTypeGetInfo(UxImGuiDataType data_type);
    IMGUI_API int           DataTypeFormatString(char* buf, int buf_size, UxImGuiDataType data_type, const void* p_data, const char* format);
    IMGUI_API void          DataTypeApplyOp(UxImGuiDataType data_type, int op, void* output, const void* arg_1, const void* arg_2);
    IMGUI_API bool          DataTypeApplyFromText(const char* buf, UxImGuiDataType data_type, void* p_data, const char* format, void* p_data_when_empty = NULL);
    IMGUI_API int           DataTypeCompare(UxImGuiDataType data_type, const void* arg_1, const void* arg_2);
    IMGUI_API bool          DataTypeClamp(UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max);
    IMGUI_API bool          DataTypeIsZero(UxImGuiDataType data_type, const void* p_data);

    // InputText
    IMGUI_API bool          InputTextEx(const char* label, const char* hint, char* buf, int buf_size, const UxImVec2& size_arg, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    IMGUI_API void          InputTextDeactivateHook(UxImGuiID id);
    IMGUI_API bool          TempInputText(const UxImRect& bb, UxImGuiID id, const char* label, char* buf, int buf_size, UxImGuiInputTextFlags flags);
    IMGUI_API bool          TempInputScalar(const UxImRect& bb, UxImGuiID id, const char* label, UxImGuiDataType data_type, void* p_data, const char* format, const void* p_clamp_min = NULL, const void* p_clamp_max = NULL);
    inline bool             TempInputIsActive(UxImGuiID id)       { UxImGuiContext& g = *GUxImGui; return (g.ActiveId == id && g.TempInputId == id); }
    inline UxImGuiInputTextState* GetInputTextState(UxImGuiID id)   { UxImGuiContext& g = *GUxImGui; return (id != 0 && g.InputTextState.ID == id) ? &g.InputTextState : NULL; } // Get input text state if active
    IMGUI_API void          SetNextItemRefVal(UxImGuiDataType data_type, void* p_data);
    inline bool             IsItemActiveAsInputText() { UxImGuiContext& g = *GUxImGui; return g.ActiveId != 0 && g.ActiveId == g.LastItemData.ID && g.InputTextState.ID == g.LastItemData.ID; } // This may be useful to apply workaround that a based on distinguish whenever an item is active as a text input field.

    // Color
    IMGUI_API void          ColorTooltip(const char* text, const float* col, UxImGuiColorEditFlags flags);
    IMGUI_API void          ColorEditOptionsPopup(const float* col, UxImGuiColorEditFlags flags);
    IMGUI_API void          ColorPickerOptionsPopup(const float* ref_col, UxImGuiColorEditFlags flags);

    // Plot
    IMGUI_API int           PlotEx(UxImGuiPlotType plot_type, const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, const UxImVec2& size_arg);

    // Shade functions (write over already created vertices)
    IMGUI_API void          ShadeVertsLinearColorGradientKeepAlpha(UxImDrawList* draw_list, int vert_start_idx, int vert_end_idx, UxImVec2 gradient_p0, UxImVec2 gradient_p1, UxImU32 col0, UxImU32 col1);
    IMGUI_API void          ShadeVertsLinearUV(UxImDrawList* draw_list, int vert_start_idx, int vert_end_idx, const UxImVec2& a, const UxImVec2& b, const UxImVec2& uv_a, const UxImVec2& uv_b, bool clamp);
    IMGUI_API void          ShadeVertsTransformPos(UxImDrawList* draw_list, int vert_start_idx, int vert_end_idx, const UxImVec2& pivot_in, float cos_a, float sin_a, const UxImVec2& pivot_out);

    // Garbage collection
    IMGUI_API void          GcCompactTransientMiscBuffers();
    IMGUI_API void          GcCompactTransientWindowBuffers(UxImGuiWindow* window);
    IMGUI_API void          GcAwakeTransientWindowBuffers(UxImGuiWindow* window);

    // Error handling, State Recovery
    IMGUI_API bool          ErrorLog(const char* msg);
    IMGUI_API void          ErrorRecoveryStoreState(UxImGuiErrorRecoveryState* state_out);
    IMGUI_API void          ErrorRecoveryTryToRecoverState(const UxImGuiErrorRecoveryState* state_in);
    IMGUI_API void          ErrorRecoveryTryToRecoverWindowState(const UxImGuiErrorRecoveryState* state_in);
    IMGUI_API void          ErrorCheckUsingSetCursorPosToExtendParentBoundaries();
    IMGUI_API void          ErrorCheckEndFrameFinalizeErrorTooltip();
    IMGUI_API bool          BeginErrorTooltip();
    IMGUI_API void          EndErrorTooltip();

    // Debug Tools
    IMGUI_API void          DebugAllocHook(UxImGuiDebugAllocInfo* info, int frame_count, void* ptr, size_t size); // size >= 0 : alloc, size = -1 : free
    IMGUI_API void          DebugDrawCursorPos(UxImU32 col = IM_COL32(255, 0, 0, 255));
    IMGUI_API void          DebugDrawLineExtents(UxImU32 col = IM_COL32(255, 0, 0, 255));
    IMGUI_API void          DebugDrawItemRect(UxImU32 col = IM_COL32(255, 0, 0, 255));
    IMGUI_API void          DebugTextUnformattedWithLocateItem(const char* line_begin, const char* line_end);
    IMGUI_API void          DebugLocateItem(UxImGuiID target_id);                     // Call sparingly: only 1 at the same time!
    IMGUI_API void          DebugLocateItemOnHover(UxImGuiID target_id);              // Only call on reaction to a mouse Hover: because only 1 at the same time!
    IMGUI_API void          DebugLocateItemResolveWithLastItem();
    IMGUI_API void          DebugBreakClearData();
    IMGUI_API bool          DebugBreakButton(const char* label, const char* description_of_location);
    IMGUI_API void          DebugBreakButtonTooltip(bool keyboard_only, const char* description_of_location);
    IMGUI_API void          ShowFontAtlas(UxImFontAtlas* atlas);
    IMGUI_API void          DebugHookIdInfo(UxImGuiID id, UxImGuiDataType data_type, const void* data_id, const void* data_id_end);
    IMGUI_API void          DebugNodeColumns(UxImGuiOldColumns* columns);
    IMGUI_API void          DebugNodeDockNode(UxImGuiDockNode* node, const char* label);
    IMGUI_API void          DebugNodeDrawList(UxImGuiWindow* window, UxImGuiViewportP* viewport, const UxImDrawList* draw_list, const char* label);
    IMGUI_API void          DebugNodeDrawCmdShowMeshAndBoundingBox(UxImDrawList* out_draw_list, const UxImDrawList* draw_list, const UxImDrawCmd* draw_cmd, bool show_mesh, bool show_aabb);
    IMGUI_API void          DebugNodeFont(UxImFont* font);
    IMGUI_API void          DebugNodeFontGlyphesForSrcMask(UxImFont* font, UxImFontBaked* baked, int src_mask);
    IMGUI_API void          DebugNodeFontGlyph(UxImFont* font, const UxImFontGlyph* glyph);
    IMGUI_API void          DebugNodeTexture(UxImTextureData* tex, int int_id, const UxImFontAtlasRect* highlight_rect = NULL); // ID used to facilitate persisting the "current" texture.
    IMGUI_API void          DebugNodeStorage(UxImGuiStorage* storage, const char* label);
    IMGUI_API void          DebugNodeTabBar(UxImGuiTabBar* tab_bar, const char* label);
    IMGUI_API void          DebugNodeTable(UxImGuiTable* table);
    IMGUI_API void          DebugNodeTableSettings(UxImGuiTableSettings* settings);
    IMGUI_API void          DebugNodeInputTextState(UxImGuiInputTextState* state);
    IMGUI_API void          DebugNodeTypingSelectState(UxImGuiTypingSelectState* state);
    IMGUI_API void          DebugNodeMultiSelectState(UxImGuiMultiSelectState* state);
    IMGUI_API void          DebugNodeWindow(UxImGuiWindow* window, const char* label);
    IMGUI_API void          DebugNodeWindowSettings(UxImGuiWindowSettings* settings);
    IMGUI_API void          DebugNodeWindowsList(UxImVector<UxImGuiWindow*>* windows, const char* label);
    IMGUI_API void          DebugNodeWindowsListByBeginStackParent(UxImGuiWindow** windows, int windows_size, UxImGuiWindow* parent_in_begin_stack);
    IMGUI_API void          DebugNodeViewport(UxImGuiViewportP* viewport);
    IMGUI_API void          DebugNodePlatformMonitor(UxImGuiPlatformMonitor* monitor, const char* label, int idx);
    IMGUI_API void          DebugRenderKeyboardPreview(UxImDrawList* draw_list);
    IMGUI_API void          DebugRenderViewportThumbnail(UxImDrawList* draw_list, UxImGuiViewportP* viewport, const UxImRect& bb);

    // Obsolete functions
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    //inline void   SetItemUsingMouseWheel()                                            { SetItemKeyOwner(UxImGuiKey_MouseWheelY); }      // Changed in 1.89
    //inline bool   TreeNodeBehaviorIsOpen(UxImGuiID id, UxImGuiTreeNodeFlags flags = 0)    { return TreeNodeUpdateNextOpen(id, flags); }   // Renamed in 1.89
    //inline bool   IsKeyPressedMap(UxImGuiKey key, bool repeat = true)                   { IM_ASSERT(IsNamedKey(key)); return IsKeyPressed(key, repeat); } // Removed in 1.87: Mapping from named key is always identity!

    // Refactored focus/nav/tabbing system in 1.82 and 1.84. If you have old/custom copy-and-pasted widgets which used FocusableItemRegister():
    //  (Old) IMGUI_VERSION_NUM  < 18209: using 'ItemAdd(....)'                              and 'bool tab_focused = FocusableItemRegister(...)'
    //  (Old) IMGUI_VERSION_NUM >= 18209: using 'ItemAdd(..., UxImGuiItemAddFlags_Focusable)'  and 'bool tab_focused = (g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_Focused) != 0'
    //  (New) IMGUI_VERSION_NUM >= 18413: using 'ItemAdd(..., UxImGuiItemFlags_Inputable)'     and 'bool tab_focused = (g.NavActivateId == id && (g.NavActivateFlags & UxImGuiActivateFlags_PreferInput))'
    //inline bool   FocusableItemRegister(UxImGuiWindow* window, UxImGuiID id)              // -> pass UxImGuiItemAddFlags_Inputable flag to ItemAdd()
    //inline void   FocusableItemUnregister(UxImGuiWindow* window)                        // -> unnecessary: TempInputText() uses UxImGuiInputTextFlags_MergedItem
#endif

} // namespace UxImGui


//-----------------------------------------------------------------------------
// [SECTION] UxImFontLoader
//-----------------------------------------------------------------------------

// Hooks and storage for a given font backend.
// This structure is likely to evolve as we add support for incremental atlas updates.
// Conceptually this could be public, but API is still going to be evolve.
struct UxImFontLoader
{
    const char*     Name;
    bool            (*LoaderInit)(UxImFontAtlas* atlas);
    void            (*LoaderShutdown)(UxImFontAtlas* atlas);
    bool            (*FontSrcInit)(UxImFontAtlas* atlas, UxImFontConfig* src);
    void            (*FontSrcDestroy)(UxImFontAtlas* atlas, UxImFontConfig* src);
    bool            (*FontSrcContainsGlyph)(UxImFontAtlas* atlas, UxImFontConfig* src, UxImWchar codepoint);
    bool            (*FontBakedInit)(UxImFontAtlas* atlas, UxImFontConfig* src, UxImFontBaked* baked, void* loader_data_for_baked_src);
    void            (*FontBakedDestroy)(UxImFontAtlas* atlas, UxImFontConfig* src, UxImFontBaked* baked, void* loader_data_for_baked_src);
    bool            (*FontBakedLoadGlyph)(UxImFontAtlas* atlas, UxImFontConfig* src, UxImFontBaked* baked, void* loader_data_for_baked_src, UxImWchar codepoint, UxImFontGlyph* out_glyph);

    // Size of backend data, Per Baked * Per Source. Buffers are managed by core to avoid excessive allocations.
    // FIXME: At this point the two other types of buffers may be managed by core to be consistent?
    size_t          FontBakedSrcLoaderDataSize;

    UxImFontLoader()  { memset(this, 0, sizeof(*this)); }
};

#ifdef IMGUI_ENABLE_STB_TRUETYPE
IMGUI_API const UxImFontLoader* UxImFontAtlasGetFontLoaderForStbTruetype();
#endif
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
typedef UxImFontLoader UxImFontBuilderIO; // [renamed/changed in 1.92] The types are not actually compatible but we provide this as a compile-time error report helper.
#endif

//-----------------------------------------------------------------------------
// [SECTION] UxImFontAtlas internal API
//-----------------------------------------------------------------------------

// Helpers: UxImTextureRef ==/!= operators provided as convenience
// (note that _TexID and _TexData are never set simultaneously)
static inline bool operator==(const UxImTextureRef& lhs, const UxImTextureRef& rhs) { return lhs._TexID == rhs._TexID && lhs._TexData == rhs._TexData; }
static inline bool operator!=(const UxImTextureRef& lhs, const UxImTextureRef& rhs) { return lhs._TexID != rhs._TexID || lhs._TexData != rhs._TexData; }

// Refer to UxImFontAtlasPackGetRect() to better understand how this works.
#define UxImFontAtlasRectId_IndexMask_        (0x000FFFFF)    // 20-bits: index to access builder->RectsIndex[].
#define UxImFontAtlasRectId_GenerationMask_   (0x3FF00000)    // 10-bits: entry generation, so each ID is unique and get can safely detected old identifiers.
#define UxImFontAtlasRectId_GenerationShift_  (20)
inline int               UxImFontAtlasRectId_GetIndex(UxImFontAtlasRectId id)       { return id & UxImFontAtlasRectId_IndexMask_; }
inline int               UxImFontAtlasRectId_GetGeneration(UxImFontAtlasRectId id)  { return (id & UxImFontAtlasRectId_GenerationMask_) >> UxImFontAtlasRectId_GenerationShift_; }
inline UxImFontAtlasRectId UxImFontAtlasRectId_Make(int index_idx, int gen_idx)     { IM_ASSERT(index_idx < UxImFontAtlasRectId_IndexMask_ && gen_idx < (UxImFontAtlasRectId_GenerationMask_ >> UxImFontAtlasRectId_GenerationShift_)); return (UxImFontAtlasRectId)(index_idx | (gen_idx << UxImFontAtlasRectId_GenerationShift_)); }

// Packed rectangle lookup entry (we need an indirection to allow removing/reordering rectangles)
// User are returned UxImFontAtlasRectId values which are meant to be persistent.
// We handle this with an indirection. While Rects[] may be in theory shuffled, compacted etc., RectsIndex[] cannot it is keyed by UxImFontAtlasRectId.
// RectsIndex[] is used both as an index into Rects[] and an index into itself. This is basically a free-list. See UxImFontAtlasBuildAllocRectIndexEntry() code.
// Having this also makes it easier to e.g. sort rectangles during repack.
struct UxImFontAtlasRectEntry
{
    int                 TargetIndex : 20;   // When Used: UxImFontAtlasRectId -> into Rects[]. When unused: index to next unused RectsIndex[] slot to consume free-list.
    int                 Generation : 10;    // Increased each time the entry is reused for a new rectangle.
    unsigned int        IsUsed : 1;
};

// Data available to potential texture post-processing functions
struct UxImFontAtlasPostProcessData
{
    UxImFontAtlas*        FontAtlas;
    UxImFont*             Font;
    UxImFontConfig*       FontSrc;
    UxImFontBaked*        FontBaked;
    UxImFontGlyph*        Glyph;

    // Pixel data
    unsigned char*      Pixels;
    UxImTextureFormat     Format;
    int                 Pitch;
    int                 Width;
    int                 Height;
};

// We avoid dragging imstb_rectpack.h into public header (partly because binding generators are having issues with it)
#ifdef IMGUI_STB_NAMESPACE
namespace IMGUI_STB_NAMESPACE { struct stbrp_node; }
typedef IMGUI_STB_NAMESPACE::stbrp_node stbrp_node_im;
#else
struct stbrp_node;
typedef stbrp_node stbrp_node_im;
#endif
struct stbrp_context_opaque { char data[80]; };

// Internal storage for incrementally packing and building a UxImFontAtlas
struct UxImFontAtlasBuilder
{
    stbrp_context_opaque        PackContext;            // Actually 'stbrp_context' but we don't want to define this in the header file.
    UxImVector<stbrp_node_im>     PackNodes;
    UxImVector<UxImTextureRect>     Rects;
    UxImVector<UxImFontAtlasRectEntry> RectsIndex;          // UxImFontAtlasRectId -> index into Rects[]
    UxImVector<unsigned char>     TempBuffer;             // Misc scratch buffer
    int                         RectsIndexFreeListStart;// First unused entry
    int                         RectsPackedCount;       // Number of packed rectangles.
    int                         RectsPackedSurface;     // Number of packed pixels. Used when compacting to heuristically find the ideal texture size.
    int                         RectsDiscardedCount;
    int                         RectsDiscardedSurface;
    int                         FrameCount;             // Current frame count
    UxImVec2i                     MaxRectSize;            // Largest rectangle to pack (de-facto used as a "minimum texture size")
    UxImVec2i                     MaxRectBounds;          // Bottom-right most used pixels
    bool                        LockDisableResize;      // Disable resizing texture
    bool                        PreloadedAllGlyphsRanges; // Set when missing UxImGuiBackendFlags_RendererHasTextures features forces atlas to preload everything.

    // Cache of all UxImFontBaked
    UxImStableVector<UxImFontBaked,32> BakedPool;
    UxImGuiStorage                BakedMap;               // BakedId --> UxImFontBaked*
    int                         BakedDiscardedCount;

    // Custom rectangle identifiers
    UxImFontAtlasRectId           PackIdMouseCursors;     // White pixel + mouse cursors. Also happen to be fallback in case of packing failure.
    UxImFontAtlasRectId           PackIdLinesTexData;

    UxImFontAtlasBuilder()        { memset(this, 0, sizeof(*this)); FrameCount = -1; RectsIndexFreeListStart = -1; PackIdMouseCursors = PackIdLinesTexData = -1; }
};

IMGUI_API void              UxImFontAtlasBuildInit(UxImFontAtlas* atlas);
IMGUI_API void              UxImFontAtlasBuildDestroy(UxImFontAtlas* atlas);
IMGUI_API void              UxImFontAtlasBuildMain(UxImFontAtlas* atlas);
IMGUI_API void              UxImFontAtlasBuildSetupFontLoader(UxImFontAtlas* atlas, const UxImFontLoader* font_loader);
IMGUI_API void              UxImFontAtlasBuildUpdatePointers(UxImFontAtlas* atlas);
IMGUI_API void              UxImFontAtlasBuildRenderBitmapFromString(UxImFontAtlas* atlas, int x, int y, int w, int h, const char* in_str, char in_marker_char);
IMGUI_API void              UxImFontAtlasBuildClear(UxImFontAtlas* atlas); // Clear output and custom rects

IMGUI_API UxImTextureData*    UxImFontAtlasTextureAdd(UxImFontAtlas* atlas, int w, int h);
IMGUI_API void              UxImFontAtlasTextureMakeSpace(UxImFontAtlas* atlas);
IMGUI_API void              UxImFontAtlasTextureRepack(UxImFontAtlas* atlas, int w, int h);
IMGUI_API void              UxImFontAtlasTextureGrow(UxImFontAtlas* atlas, int old_w = -1, int old_h = -1);
IMGUI_API void              UxImFontAtlasTextureCompact(UxImFontAtlas* atlas);
IMGUI_API UxImVec2i           UxImFontAtlasTextureGetSizeEstimate(UxImFontAtlas* atlas);

IMGUI_API void              UxImFontAtlasBuildSetupFontSpecialGlyphs(UxImFontAtlas* atlas, UxImFont* font, UxImFontConfig* src);
IMGUI_API void              UxImFontAtlasBuildPreloadAllGlyphRanges(UxImFontAtlas* atlas); // Legacy
IMGUI_API void              UxImFontAtlasBuildGetOversampleFactors(UxImFontConfig* src, UxImFontBaked* baked, int* out_oversample_h, int* out_oversample_v);
IMGUI_API void              UxImFontAtlasBuildDiscardBakes(UxImFontAtlas* atlas, int unused_frames);

IMGUI_API bool              UxImFontAtlasFontSourceInit(UxImFontAtlas* atlas, UxImFontConfig* src);
IMGUI_API void              UxImFontAtlasFontSourceAddToFont(UxImFontAtlas* atlas, UxImFont* font, UxImFontConfig* src);
IMGUI_API void              UxImFontAtlasFontDestroySourceData(UxImFontAtlas* atlas, UxImFontConfig* src);
IMGUI_API bool              UxImFontAtlasFontInitOutput(UxImFontAtlas* atlas, UxImFont* font); // Using FontDestroyOutput/FontInitOutput sequence useful notably if font loader params have changed
IMGUI_API void              UxImFontAtlasFontDestroyOutput(UxImFontAtlas* atlas, UxImFont* font);
IMGUI_API void              UxImFontAtlasFontDiscardBakes(UxImFontAtlas* atlas, UxImFont* font, int unused_frames);

IMGUI_API UxImGuiID           UxImFontAtlasBakedGetId(UxImGuiID font_id, float baked_size, float rasterizer_density);
IMGUI_API UxImFontBaked*      UxImFontAtlasBakedGetOrAdd(UxImFontAtlas* atlas, UxImFont* font, float font_size, float font_rasterizer_density);
IMGUI_API UxImFontBaked*      UxImFontAtlasBakedGetClosestMatch(UxImFontAtlas* atlas, UxImFont* font, float font_size, float font_rasterizer_density);
IMGUI_API UxImFontBaked*      UxImFontAtlasBakedAdd(UxImFontAtlas* atlas, UxImFont* font, float font_size, float font_rasterizer_density, UxImGuiID baked_id);
IMGUI_API void              UxImFontAtlasBakedDiscard(UxImFontAtlas* atlas, UxImFont* font, UxImFontBaked* baked);
IMGUI_API UxImFontGlyph*      UxImFontAtlasBakedAddFontGlyph(UxImFontAtlas* atlas, UxImFontBaked* baked, UxImFontConfig* src, const UxImFontGlyph* in_glyph);
IMGUI_API void              UxImFontAtlasBakedDiscardFontGlyph(UxImFontAtlas* atlas, UxImFont* font, UxImFontBaked* baked, UxImFontGlyph* glyph);
IMGUI_API void              UxImFontAtlasBakedSetFontGlyphBitmap(UxImFontAtlas* atlas, UxImFontBaked* baked, UxImFontConfig* src, UxImFontGlyph* glyph, UxImTextureRect* r, const unsigned char* src_pixels, UxImTextureFormat src_fmt, int src_pitch);

IMGUI_API void              UxImFontAtlasPackInit(UxImFontAtlas* atlas);
IMGUI_API UxImFontAtlasRectId UxImFontAtlasPackAddRect(UxImFontAtlas* atlas, int w, int h, UxImFontAtlasRectEntry* overwrite_entry = NULL);
IMGUI_API UxImTextureRect*    UxImFontAtlasPackGetRect(UxImFontAtlas* atlas, UxImFontAtlasRectId id);
IMGUI_API UxImTextureRect*    UxImFontAtlasPackGetRectSafe(UxImFontAtlas* atlas, UxImFontAtlasRectId id);
IMGUI_API void              UxImFontAtlasPackDiscardRect(UxImFontAtlas* atlas, UxImFontAtlasRectId id);

IMGUI_API void              UxImFontAtlasUpdateNewFrame(UxImFontAtlas* atlas, int frame_count);
IMGUI_API void              UxImFontAtlasAddDrawListSharedData(UxImFontAtlas* atlas, UxImDrawListSharedData* data);
IMGUI_API void              UxImFontAtlasRemoveDrawListSharedData(UxImFontAtlas* atlas, UxImDrawListSharedData* data);
IMGUI_API void              UxImFontAtlasUpdateDrawListsTextures(UxImFontAtlas* atlas, UxImTextureRef old_tex, UxImTextureRef new_tex);
IMGUI_API void              UxImFontAtlasUpdateDrawListsSharedData(UxImFontAtlas* atlas);

IMGUI_API void              UxImFontAtlasTextureBlockConvert(const unsigned char* src_pixels, UxImTextureFormat src_fmt, int src_pitch, unsigned char* dst_pixels, UxImTextureFormat dst_fmt, int dst_pitch, int w, int h);
IMGUI_API void              UxImFontAtlasTextureBlockPostProcess(UxImFontAtlasPostProcessData* data);
IMGUI_API void              UxImFontAtlasTextureBlockPostProcessMultiply(UxImFontAtlasPostProcessData* data, float multiply_factor);
IMGUI_API void              UxImFontAtlasTextureBlockFill(UxImTextureData* dst_tex, int dst_x, int dst_y, int w, int h, UxImU32 col);
IMGUI_API void              UxImFontAtlasTextureBlockCopy(UxImTextureData* src_tex, int src_x, int src_y, UxImTextureData* dst_tex, int dst_x, int dst_y, int w, int h);
IMGUI_API void              UxImFontAtlasTextureBlockQueueUpload(UxImFontAtlas* atlas, UxImTextureData* tex, int x, int y, int w, int h);

IMGUI_API int               UxImTextureDataGetFormatBytesPerPixel(UxImTextureFormat format);
IMGUI_API const char*       UxImTextureDataGetStatusName(UxImTextureStatus status);
IMGUI_API const char*       UxImTextureDataGetFormatName(UxImTextureFormat format);

#ifndef IMGUI_DISABLE_DEBUG_TOOLS
IMGUI_API void              UxImFontAtlasDebugLogTextureRequests(UxImFontAtlas* atlas);
#endif

IMGUI_API bool      UxImFontAtlasGetMouseCursorTexData(UxImFontAtlas* atlas, UxImGuiMouseCursor cursor_type, UxImVec2* out_offset, UxImVec2* out_size, UxImVec2 out_uv_border[2], UxImVec2 out_uv_fill[2]);

//-----------------------------------------------------------------------------
// [SECTION] Test Engine specific hooks (imgui_test_engine)
//-----------------------------------------------------------------------------

#ifdef IMGUI_ENABLE_TEST_ENGINE
extern void         UxImGuiTestEngineHook_ItemAdd(UxImGuiContext* ctx, UxImGuiID id, const UxImRect& bb, const UxImGuiLastItemData* item_data);           // item_data may be NULL
extern void         UxImGuiTestEngineHook_ItemInfo(UxImGuiContext* ctx, UxImGuiID id, const char* label, UxImGuiItemStatusFlags flags);
extern void         UxImGuiTestEngineHook_Log(UxImGuiContext* ctx, const char* fmt, ...);
extern const char*  UxImGuiTestEngine_FindItemDebugLabel(UxImGuiContext* ctx, UxImGuiID id);

// In IMGUI_VERSION_NUM >= 18934: changed IMGUI_TEST_ENGINE_ITEM_ADD(bb,id) to IMGUI_TEST_ENGINE_ITEM_ADD(id,bb,item_data);
#define IMGUI_TEST_ENGINE_ITEM_ADD(_ID,_BB,_ITEM_DATA)      if (g.TestEngineHookItems) UxImGuiTestEngineHook_ItemAdd(&g, _ID, _BB, _ITEM_DATA)    // Register item bounding box
#define IMGUI_TEST_ENGINE_ITEM_INFO(_ID,_LABEL,_FLAGS)      if (g.TestEngineHookItems) UxImGuiTestEngineHook_ItemInfo(&g, _ID, _LABEL, _FLAGS)    // Register item label and status flags (optional)
#define IMGUI_TEST_ENGINE_LOG(_FMT,...)                     UxImGuiTestEngineHook_Log(&g, _FMT, __VA_ARGS__)                                      // Custom log entry from user land into test log
#else
#define IMGUI_TEST_ENGINE_ITEM_ADD(_BB,_ID)                 ((void)0)
#define IMGUI_TEST_ENGINE_ITEM_INFO(_ID,_LABEL,_FLAGS)      ((void)g)
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

#endif // #ifndef IMGUI_DISABLE


