// dear imgui, v1.92.0 WIP
// (widgets code)

/*

Index of this file:

// [SECTION] Forward Declarations
// [SECTION] Widgets: Text, etc.
// [SECTION] Widgets: Main (Button, Image, Checkbox, RadioButton, ProgressBar, Bullet, etc.)
// [SECTION] Widgets: Low-level Layout helpers (Spacing, Dummy, NewLine, Separator, etc.)
// [SECTION] Widgets: ComboBox
// [SECTION] Data Type and Data Formatting Helpers
// [SECTION] Widgets: DragScalar, DragFloat, DragInt, etc.
// [SECTION] Widgets: SliderScalar, SliderFloat, SliderInt, etc.
// [SECTION] Widgets: InputScalar, InputFloat, InputInt, etc.
// [SECTION] Widgets: InputText, InputTextMultiline
// [SECTION] Widgets: ColorEdit, ColorPicker, ColorButton, etc.
// [SECTION] Widgets: TreeNode, CollapsingHeader, etc.
// [SECTION] Widgets: Selectable
// [SECTION] Widgets: Typing-Select support
// [SECTION] Widgets: Box-Select support
// [SECTION] Widgets: Multi-Select support
// [SECTION] Widgets: Multi-Select helpers
// [SECTION] Widgets: ListBox
// [SECTION] Widgets: PlotLines, PlotHistogram
// [SECTION] Widgets: Value helpers
// [SECTION] Widgets: MenuItem, BeginMenu, EndMenu, etc.
// [SECTION] Widgets: BeginTabBar, EndTabBar, etc.
// [SECTION] Widgets: BeginTabItem, EndTabItem, etc.
// [SECTION] Widgets: Columns, BeginColumns, EndColumns, etc.

*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_internal.h"

// System includes
#include <stdint.h>     // intptr_t

//-------------------------------------------------------------------------
// Warnings
//-------------------------------------------------------------------------

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127)     // condition expression is constant
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#if defined(_MSC_VER) && _MSC_VER >= 1922 // MSVC 2019 16.2 or later
#pragma warning (disable: 5054)     // operator '|': deprecated between enumerations of different types
#endif
#pragma warning (disable: 26451)    // [Static Analyzer] Arithmetic overflow : Using operator 'xxx' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator 'xxx' to avoid overflow(io.2).
#pragma warning (disable: 26812)    // [Static Analyzer] The enum type 'xxx' is unscoped. Prefer 'enum class' over 'enum' (Enum.3).
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'                      // not all warnings are known by all Clang versions and they tend to be rename-happy.. so ignoring warnings triggers new warnings on some configuration. Great!
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast                            // yes, they are more terse.
#pragma clang diagnostic ignored "-Wfloat-equal"                    // warning: comparing floating point with == or != is unsafe // storing and comparing against same constants (typically 0.0f) is ok.
#pragma clang diagnostic ignored "-Wformat"                         // warning: format specifies type 'int' but the argument has type 'unsigned int'
#pragma clang diagnostic ignored "-Wformat-nonliteral"              // warning: format string is not a string literal            // passing non-literal to vsnformat(). yes, user passing incorrect format strings can crash the code.
#pragma clang diagnostic ignored "-Wsign-conversion"                // warning: implicit conversion changes signedness
#pragma clang diagnostic ignored "-Wunused-macros"                  // warning: macro is not used                                // we define snprintf/vsnprintf on Windows so they are available, but not always used.
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant                    // some standard header variations use #define NULL 0
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function  // using printf() is a misery with this as C++ va_arg ellipsis changes float to double.
#pragma clang diagnostic ignored "-Wenum-enum-conversion"           // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_')
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"// warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"            // warning: 'xxx' is an unsafe pointer used for buffer access
#pragma clang diagnostic ignored "-Wnontrivial-memaccess"           // warning: first argument in call to 'memset' is a pointer to non-trivially copyable type
#pragma clang diagnostic ignored "-Wswitch-default"                 // warning: 'switch' missing 'default' label
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpragmas"                          // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wfloat-equal"                      // warning: comparing floating-point with '==' or '!=' is unsafe
#pragma GCC diagnostic ignored "-Wformat"                           // warning: format '%p' expects argument of type 'int'/'void*', but argument X has type 'unsigned int'/'UxImGuiWindow*'
#pragma GCC diagnostic ignored "-Wformat-nonliteral"                // warning: format not a string literal, format string not checked
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"  // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#pragma GCC diagnostic ignored "-Wdouble-promotion"                 // warning: implicit conversion from 'float' to 'double' when passing argument to function
#pragma GCC diagnostic ignored "-Wstrict-overflow"                  // warning: assuming signed overflow does not occur when simplifying division / ..when changing X +- C1 cmp C2 to X cmp C2 -+ C1
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#pragma GCC diagnostic ignored "-Wcast-qual"                        // warning: cast from type 'const xxxx *' to type 'xxxx *' casts away qualifiers
#endif

//-------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------

// Widgets
static const float          DRAGDROP_HOLD_TO_OPEN_TIMER = 0.70f;    // Time for drag-hold to activate items accepting the UxImGuiButtonFlags_PressedOnDragDropHold button behavior.
static const float          DRAG_MOUSE_THRESHOLD_FACTOR = 0.50f;    // Multiplier for the default value of io.MouseDragThreshold to make DragFloat/DragInt react faster to mouse drags.

// Those MIN/MAX values are not define because we need to point to them
static const signed char    IM_S8_MIN  = -128;
static const signed char    IM_S8_MAX  = 127;
static const unsigned char  IM_U8_MIN  = 0;
static const unsigned char  IM_U8_MAX  = 0xFF;
static const signed short   IM_S16_MIN = -32768;
static const signed short   IM_S16_MAX = 32767;
static const unsigned short IM_U16_MIN = 0;
static const unsigned short IM_U16_MAX = 0xFFFF;
static const UxImS32          IM_S32_MIN = INT_MIN;    // (-2147483647 - 1), (0x80000000);
static const UxImS32          IM_S32_MAX = INT_MAX;    // (2147483647), (0x7FFFFFFF)
static const UxImU32          IM_U32_MIN = 0;
static const UxImU32          IM_U32_MAX = UINT_MAX;   // (0xFFFFFFFF)
#ifdef LLONG_MIN
static const UxImS64          IM_S64_MIN = LLONG_MIN;  // (-9223372036854775807ll - 1ll);
static const UxImS64          IM_S64_MAX = LLONG_MAX;  // (9223372036854775807ll);
#else
static const UxImS64          IM_S64_MIN = -9223372036854775807LL - 1;
static const UxImS64          IM_S64_MAX = 9223372036854775807LL;
#endif
static const UxImU64          IM_U64_MIN = 0;
#ifdef ULLONG_MAX
static const UxImU64          IM_U64_MAX = ULLONG_MAX; // (0xFFFFFFFFFFFFFFFFull);
#else
static const UxImU64          IM_U64_MAX = (2ULL * 9223372036854775807LL + 1);
#endif

//-------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-------------------------------------------------------------------------

// For InputTextEx()
static bool     InputTextFilterCharacter(UxImGuiContext* ctx, unsigned int* p_char, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback, void* user_data, bool input_source_is_clipboard = false);
static int      InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end);
static UxImVec2   InputTextCalcTextSize(UxImGuiContext* ctx, const char* text_begin, const char* text_end, const char** remaining = NULL, UxImVec2* out_offset = NULL, bool stop_on_new_line = false);

//-------------------------------------------------------------------------
// [SECTION] Widgets: Text, etc.
//-------------------------------------------------------------------------
// - TextEx() [Internal]
// - TextUnformatted()
// - Text()
// - TextV()
// - TextColored()
// - TextColoredV()
// - TextDisabled()
// - TextDisabledV()
// - TextWrapped()
// - TextWrappedV()
// - LabelText()
// - LabelTextV()
// - BulletText()
// - BulletTextV()
//-------------------------------------------------------------------------

void UxImGui::TextEx(const char* text, const char* text_end, UxImGuiTextFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    UxImGuiContext& g = *GUxImGui;

    // Accept null ranges
    if (text == text_end)
        text = text_end = "";

    // Calculate length
    const char* text_begin = text;
    if (text_end == NULL)
        text_end = text + UxImStrlen(text); // FIXME-OPT

    const UxImVec2 text_pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    const float wrap_pos_x = window->DC.TextWrapPos;
    const bool wrap_enabled = (wrap_pos_x >= 0.0f);
    if (text_end - text <= 2000 || wrap_enabled)
    {
        // Common case
        const float wrap_width = wrap_enabled ? CalcWrapWidthForPos(window->DC.CursorPos, wrap_pos_x) : 0.0f;
        const UxImVec2 text_size = CalcTextSize(text_begin, text_end, false, wrap_width);

        UxImRect bb(text_pos, text_pos + text_size);
        ItemSize(text_size, 0.0f);
        if (!ItemAdd(bb, 0))
            return;

        // Render (we don't hide text after ## in this end-user function)
        RenderTextWrapped(bb.Min, text_begin, text_end, wrap_width);
    }
    else
    {
        // Long text!
        // Perform manual coarse clipping to optimize for long multi-line text
        // - From this point we will only compute the width of lines that are visible. Optimization only available when word-wrapping is disabled.
        // - We also don't vertically center the text within the line full height, which is unlikely to matter because we are likely the biggest and only item on the line.
        // - We use memchr(), pay attention that well optimized versions of those str/mem functions are much faster than a casually written loop.
        const char* line = text;
        const float line_height = GetTextLineHeight();
        UxImVec2 text_size(0, 0);

        // Lines to skip (can't skip when logging text)
        UxImVec2 pos = text_pos;
        if (!g.LogEnabled)
        {
            int lines_skippable = (int)((window->ClipRect.Min.y - text_pos.y) / line_height);
            if (lines_skippable > 0)
            {
                int lines_skipped = 0;
                while (line < text_end && lines_skipped < lines_skippable)
                {
                    const char* line_end = (const char*)UxImMemchr(line, '\n', text_end - line);
                    if (!line_end)
                        line_end = text_end;
                    if ((flags & UxImGuiTextFlags_NoWidthForLargeClippedText) == 0)
                        text_size.x = UxImMax(text_size.x, CalcTextSize(line, line_end).x);
                    line = line_end + 1;
                    lines_skipped++;
                }
                pos.y += lines_skipped * line_height;
            }
        }

        // Lines to render
        if (line < text_end)
        {
            UxImRect line_rect(pos, pos + UxImVec2(FLT_MAX, line_height));
            while (line < text_end)
            {
                if (IsClippedEx(line_rect, 0))
                    break;

                const char* line_end = (const char*)UxImMemchr(line, '\n', text_end - line);
                if (!line_end)
                    line_end = text_end;
                text_size.x = UxImMax(text_size.x, CalcTextSize(line, line_end).x);
                RenderText(pos, line, line_end, false);
                line = line_end + 1;
                line_rect.Min.y += line_height;
                line_rect.Max.y += line_height;
                pos.y += line_height;
            }

            // Count remaining lines
            int lines_skipped = 0;
            while (line < text_end)
            {
                const char* line_end = (const char*)UxImMemchr(line, '\n', text_end - line);
                if (!line_end)
                    line_end = text_end;
                if ((flags & UxImGuiTextFlags_NoWidthForLargeClippedText) == 0)
                    text_size.x = UxImMax(text_size.x, CalcTextSize(line, line_end).x);
                line = line_end + 1;
                lines_skipped++;
            }
            pos.y += lines_skipped * line_height;
        }
        text_size.y = (pos - text_pos).y;

        UxImRect bb(text_pos, text_pos + text_size);
        ItemSize(text_size, 0.0f);
        ItemAdd(bb, 0);
    }
}

void UxImGui::TextUnformatted(const char* text, const char* text_end)
{
    TextEx(text, text_end, UxImGuiTextFlags_NoWidthForLargeClippedText);
}

void UxImGui::Text(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextV(fmt, args);
    va_end(args);
}

void UxImGui::TextV(const char* fmt, va_list args)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const char* text, *text_end;
    UxImFormatStringToTempBufferV(&text, &text_end, fmt, args);
    TextEx(text, text_end, UxImGuiTextFlags_NoWidthForLargeClippedText);
}

void UxImGui::TextColored(const UxImVec4& col, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextColoredV(col, fmt, args);
    va_end(args);
}

void UxImGui::TextColoredV(const UxImVec4& col, const char* fmt, va_list args)
{
    PushStyleColor(UxImGuiCol_Text, col);
    TextV(fmt, args);
    PopStyleColor();
}

void UxImGui::TextDisabled(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextDisabledV(fmt, args);
    va_end(args);
}

void UxImGui::TextDisabledV(const char* fmt, va_list args)
{
    UxImGuiContext& g = *GUxImGui;
    PushStyleColor(UxImGuiCol_Text, g.Style.Colors[UxImGuiCol_TextDisabled]);
    TextV(fmt, args);
    PopStyleColor();
}

void UxImGui::TextWrapped(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextWrappedV(fmt, args);
    va_end(args);
}

void UxImGui::TextWrappedV(const char* fmt, va_list args)
{
    UxImGuiContext& g = *GUxImGui;
    const bool need_backup = (g.CurrentWindow->DC.TextWrapPos < 0.0f);  // Keep existing wrap position if one is already set
    if (need_backup)
        PushTextWrapPos(0.0f);
    TextV(fmt, args);
    if (need_backup)
        PopTextWrapPos();
}

void UxImGui::TextAligned(float align_x, float size_x, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextAlignedV(align_x, size_x, fmt, args);
    va_end(args);
}

// align_x: 0.0f = left, 0.5f = center, 1.0f = right.
// size_x : 0.0f = shortcut for GetContentRegionAvail().x
// FIXME-WIP: Works but API is likely to be reworked. This is designed for 1 item on the line. (#7024)
void UxImGui::TextAlignedV(float align_x, float size_x, const char* fmt, va_list args)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const char* text, *text_end;
    UxImFormatStringToTempBufferV(&text, &text_end, fmt, args);
    const UxImVec2 text_size = CalcTextSize(text, text_end);
    size_x = CalcItemSize(UxImVec2(size_x, 0.0f), 0.0f, text_size.y).x;

    UxImVec2 pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    UxImVec2 pos_max(pos.x + size_x, window->ClipRect.Max.y);
    UxImVec2 size(UxImMin(size_x, text_size.x), text_size.y);
    window->DC.CursorMaxPos.x = UxImMax(window->DC.CursorMaxPos.x, pos.x + text_size.x);
    window->DC.IdealMaxPos.x = UxImMax(window->DC.IdealMaxPos.x, pos.x + text_size.x);
    if (align_x > 0.0f && text_size.x < size_x)
        pos.x += UxImTrunc((size_x - text_size.x) * align_x);
    RenderTextEllipsis(window->DrawList, pos, pos_max, pos_max.x, text, text_end, &text_size);

    const UxImVec2 backup_max_pos = window->DC.CursorMaxPos;
    ItemSize(size);
    ItemAdd(UxImRect(pos, pos + size), 0);
    window->DC.CursorMaxPos.x = backup_max_pos.x; // Cancel out extending content size because right-aligned text would otherwise mess it up.

    if (size_x < text_size.x && IsItemHovered(UxImGuiHoveredFlags_NoNavOverride | UxImGuiHoveredFlags_AllowWhenDisabled | UxImGuiHoveredFlags_ForTooltip))
        SetTooltip("%.*s", (int)(text_end - text), text);
}

void UxImGui::LabelText(const char* label, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LabelTextV(label, fmt, args);
    va_end(args);
}

// Add a label+text combo aligned to other label+value widgets
void UxImGui::LabelTextV(const char* label, const char* fmt, va_list args)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const float w = CalcItemWidth();

    const char* value_text_begin, *value_text_end;
    UxImFormatStringToTempBufferV(&value_text_begin, &value_text_end, fmt, args);
    const UxImVec2 value_size = CalcTextSize(value_text_begin, value_text_end, false);
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);

    const UxImVec2 pos = window->DC.CursorPos;
    const UxImRect value_bb(pos, pos + UxImVec2(w, value_size.y + style.FramePadding.y * 2));
    const UxImRect total_bb(pos, pos + UxImVec2(w + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), UxImMax(value_size.y, label_size.y) + style.FramePadding.y * 2));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0))
        return;

    // Render
    RenderTextClipped(value_bb.Min + style.FramePadding, value_bb.Max, value_text_begin, value_text_end, &value_size, UxImVec2(0.0f, 0.0f));
    if (label_size.x > 0.0f)
        RenderText(UxImVec2(value_bb.Max.x + style.ItemInnerSpacing.x, value_bb.Min.y + style.FramePadding.y), label);
}

void UxImGui::BulletText(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    BulletTextV(fmt, args);
    va_end(args);
}

// Text with a little bullet aligned to the typical tree node.
void UxImGui::BulletTextV(const char* fmt, va_list args)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;

    const char* text_begin, *text_end;
    UxImFormatStringToTempBufferV(&text_begin, &text_end, fmt, args);
    const UxImVec2 label_size = CalcTextSize(text_begin, text_end, false);
    const UxImVec2 total_size = UxImVec2(g.FontSize + (label_size.x > 0.0f ? (label_size.x + style.FramePadding.x * 2) : 0.0f), label_size.y);  // Empty text doesn't add padding
    UxImVec2 pos = window->DC.CursorPos;
    pos.y += window->DC.CurrLineTextBaseOffset;
    ItemSize(total_size, 0.0f);
    const UxImRect bb(pos, pos + total_size);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    UxImU32 text_col = GetColorU32(UxImGuiCol_Text);
    RenderBullet(window->DrawList, bb.Min + UxImVec2(style.FramePadding.x + g.FontSize * 0.5f, g.FontSize * 0.5f), text_col);
    RenderText(bb.Min + UxImVec2(g.FontSize + style.FramePadding.x * 2, 0.0f), text_begin, text_end, false);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Main
//-------------------------------------------------------------------------
// - ButtonBehavior() [Internal]
// - Button()
// - SmallButton()
// - InvisibleButton()
// - ArrowButton()
// - CloseButton() [Internal]
// - CollapseButton() [Internal]
// - GetWindowScrollbarID() [Internal]
// - GetWindowScrollbarRect() [Internal]
// - Scrollbar() [Internal]
// - ScrollbarEx() [Internal]
// - Image()
// - ImageButton()
// - Checkbox()
// - CheckboxFlagsT() [Internal]
// - CheckboxFlags()
// - RadioButton()
// - ProgressBar()
// - Bullet()
// - Hyperlink()
//-------------------------------------------------------------------------

// The ButtonBehavior() function is key to many interactions and used by many/most widgets.
// Because we handle so many cases (keyboard/gamepad navigation, drag and drop) and many specific behavior (via UxImGuiButtonFlags_),
// this code is a little complex.
// By far the most common path is interacting with the Mouse using the default UxImGuiButtonFlags_PressedOnClickRelease button behavior.
// See the series of events below and the corresponding state reported by dear imgui:
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnClickRelease:             return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+0 (mouse is outside bb)        -             -                -               -                  -                    -
//   Frame N+1 (mouse moves inside bb)      -             true             -               -                  -                    -
//   Frame N+2 (mouse button is down)       -             true             true            true               -                    true
//   Frame N+3 (mouse button is down)       -             true             true            -                  -                    -
//   Frame N+4 (mouse moves outside bb)     -             -                true            -                  -                    -
//   Frame N+5 (mouse moves inside bb)      -             true             true            -                  -                    -
//   Frame N+6 (mouse button is released)   true          true             -               -                  true                 -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//   Frame N+8 (mouse moves outside bb)     -             -                -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnClick:                    return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+2 (mouse button is down)       true          true             true            true               -                    true
//   Frame N+3 (mouse button is down)       -             true             true            -                  -                    -
//   Frame N+6 (mouse button is released)   -             true             -               -                  true                 -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnRelease:                  return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+2 (mouse button is down)       -             true             -               -                  -                    true
//   Frame N+3 (mouse button is down)       -             true             -               -                  -                    -
//   Frame N+6 (mouse button is released)   true          true             -               -                  -                    -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnDoubleClick:              return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+0 (mouse button is down)       -             true             -               -                  -                    true
//   Frame N+1 (mouse button is down)       -             true             -               -                  -                    -
//   Frame N+2 (mouse button is released)   -             true             -               -                  -                    -
//   Frame N+3 (mouse button is released)   -             true             -               -                  -                    -
//   Frame N+4 (mouse button is down)       true          true             true            true               -                    true
//   Frame N+5 (mouse button is down)       -             true             true            -                  -                    -
//   Frame N+6 (mouse button is released)   -             true             -               -                  true                 -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// Note that some combinations are supported,
// - PressedOnDragDropHold can generally be associated with any flag.
// - PressedOnDoubleClick can be associated by PressedOnClickRelease/PressedOnRelease, in which case the second release event won't be reported.
//------------------------------------------------------------------------------------------------------------------------------------------------
// The behavior of the return-value changes when UxImGuiItemFlags_ButtonRepeat is set:
//                                         Repeat+                  Repeat+           Repeat+             Repeat+
//                                         PressedOnClickRelease    PressedOnClick    PressedOnRelease    PressedOnDoubleClick
//-------------------------------------------------------------------------------------------------------------------------------------------------
//   Frame N+0 (mouse button is down)       -                        true              -                   true
//   ...                                    -                        -                 -                   -
//   Frame N + RepeatDelay                  true                     true              -                   true
//   ...                                    -                        -                 -                   -
//   Frame N + RepeatDelay + RepeatRate*N   true                     true              -                   true
//-------------------------------------------------------------------------------------------------------------------------------------------------

// - FIXME: For refactor we could output flags, incl mouse hovered vs nav keyboard vs nav triggered etc.
//   And better standardize how widgets use 'GetColor32((held && hovered) ? ... : hovered ? ...)' vs 'GetColor32(held ? ... : hovered ? ...);'
//   For mouse feedback we typically prefer the 'held && hovered' test, but for nav feedback not always. Outputting hovered=true on Activation may be misleading.
// - Since v1.91.2 (Sept 2024) we included io.ConfigDebugHighlightIdConflicts feature.
//   One idiom which was previously valid which will now emit a warning is when using multiple overlaid ButtonBehavior()
//   with same ID and different MouseButton (see #8030). You can fix it by:
//       (1) switching to use a single ButtonBehavior() with multiple _MouseButton flags.
//    or (2) surrounding those calls with PushItemFlag(UxImGuiItemFlags_AllowDuplicateId, true); ... PopItemFlag()
bool UxImGui::ButtonBehavior(const UxImRect& bb, UxImGuiID id, bool* out_hovered, bool* out_held, UxImGuiButtonFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();

    // Default behavior inherited from item flags
    // Note that _both_ ButtonFlags and ItemFlags are valid sources, so copy one into the item_flags and only check that.
    UxImGuiItemFlags item_flags = (g.LastItemData.ID == id ? g.LastItemData.ItemFlags : g.CurrentItemFlags);
    if (flags & UxImGuiButtonFlags_AllowOverlap)
        item_flags |= UxImGuiItemFlags_AllowOverlap;

    // Default only reacts to left mouse button
    if ((flags & UxImGuiButtonFlags_MouseButtonMask_) == 0)
        flags |= UxImGuiButtonFlags_MouseButtonLeft;

    // Default behavior requires click + release inside bounding box
    if ((flags & UxImGuiButtonFlags_PressedOnMask_) == 0)
        flags |= (item_flags & UxImGuiItemFlags_ButtonRepeat) ? UxImGuiButtonFlags_PressedOnClick : UxImGuiButtonFlags_PressedOnDefault_;

    UxImGuiWindow* backup_hovered_window = g.HoveredWindow;
    const bool flatten_hovered_children = (flags & UxImGuiButtonFlags_FlattenChildren) && g.HoveredWindow && g.HoveredWindow->RootWindowDockTree == window->RootWindowDockTree;
    if (flatten_hovered_children)
        g.HoveredWindow = window;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    // Alternate registration spot, for when caller didn't use ItemAdd()
    if (g.LastItemData.ID != id)
        IMGUI_TEST_ENGINE_ITEM_ADD(id, bb, NULL);
#endif

    bool pressed = false;
    bool hovered = ItemHoverable(bb, id, item_flags);

    // Special mode for Drag and Drop where holding button pressed for a long time while dragging another item triggers the button
    if (g.DragDropActive && (flags & UxImGuiButtonFlags_PressedOnDragDropHold) && !(g.DragDropSourceFlags & UxImGuiDragDropFlags_SourceNoHoldToOpenOthers))
        if (IsItemHovered(UxImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            hovered = true;
            SetHoveredID(id);
            if (g.HoveredIdTimer - g.IO.DeltaTime <= DRAGDROP_HOLD_TO_OPEN_TIMER && g.HoveredIdTimer >= DRAGDROP_HOLD_TO_OPEN_TIMER)
            {
                pressed = true;
                g.DragDropHoldJustPressedId = id;
                FocusWindow(window);
            }
        }

    if (flatten_hovered_children)
        g.HoveredWindow = backup_hovered_window;

    // Mouse handling
    const UxImGuiID test_owner_id = (flags & UxImGuiButtonFlags_NoTestKeyOwner) ? UxImGuiKeyOwner_Any : id;
    if (hovered)
    {
        IM_ASSERT(id != 0); // Lazily check inside rare path.

        // Poll mouse buttons
        // - 'mouse_button_clicked' is generally carried into ActiveIdMouseButton when setting ActiveId.
        // - Technically we only need some values in one code path, but since this is gated by hovered test this is fine.
        int mouse_button_clicked = -1;
        int mouse_button_released = -1;
        for (int button = 0; button < 3; button++)
            if (flags & (UxImGuiButtonFlags_MouseButtonLeft << button)) // Handle UxImGuiButtonFlags_MouseButtonRight and UxImGuiButtonFlags_MouseButtonMiddle here.
            {
                if (IsMouseClicked(button, UxImGuiInputFlags_None, test_owner_id) && mouse_button_clicked == -1) { mouse_button_clicked = button; }
                if (IsMouseReleased(button, test_owner_id) && mouse_button_released == -1) { mouse_button_released = button; }
            }

        // Process initial action
        const bool mods_ok = !(flags & UxImGuiButtonFlags_NoKeyModsAllowed) || (!g.IO.KeyCtrl && !g.IO.KeyShift && !g.IO.KeyAlt);
        if (mods_ok)
        {
            if (mouse_button_clicked != -1 && g.ActiveId != id)
            {
                if (!(flags & UxImGuiButtonFlags_NoSetKeyOwner))
                    SetKeyOwner(MouseButtonToKey(mouse_button_clicked), id);
                if (flags & (UxImGuiButtonFlags_PressedOnClickRelease | UxImGuiButtonFlags_PressedOnClickReleaseAnywhere))
                {
                    SetActiveID(id, window);
                    g.ActiveIdMouseButton = mouse_button_clicked;
                    if (!(flags & UxImGuiButtonFlags_NoNavFocus))
                    {
                        SetFocusID(id, window);
                        FocusWindow(window);
                    }
                    else
                    {
                        FocusWindow(window, UxImGuiFocusRequestFlags_RestoreFocusedChild); // Still need to focus and bring to front, but try to avoid losing NavId when navigating a child
                    }
                }
                if ((flags & UxImGuiButtonFlags_PressedOnClick) || ((flags & UxImGuiButtonFlags_PressedOnDoubleClick) && g.IO.MouseClickedCount[mouse_button_clicked] == 2))
                {
                    pressed = true;
                    if (flags & UxImGuiButtonFlags_NoHoldingActiveId)
                        ClearActiveID();
                    else
                        SetActiveID(id, window); // Hold on ID
                    g.ActiveIdMouseButton = mouse_button_clicked;
                    if (!(flags & UxImGuiButtonFlags_NoNavFocus))
                    {
                        SetFocusID(id, window);
                        FocusWindow(window);
                    }
                    else
                    {
                        FocusWindow(window, UxImGuiFocusRequestFlags_RestoreFocusedChild); // Still need to focus and bring to front, but try to avoid losing NavId when navigating a child
                    }
                }
            }
            if (flags & UxImGuiButtonFlags_PressedOnRelease)
            {
                if (mouse_button_released != -1)
                {
                    const bool has_repeated_at_least_once = (item_flags & UxImGuiItemFlags_ButtonRepeat) && g.IO.MouseDownDurationPrev[mouse_button_released] >= g.IO.KeyRepeatDelay; // Repeat mode trumps on release behavior
                    if (!has_repeated_at_least_once)
                        pressed = true;
                    if (!(flags & UxImGuiButtonFlags_NoNavFocus))
                        SetFocusID(id, window); // FIXME: Lack of FocusWindow() call here is inconsistent with other paths. Research why.
                    ClearActiveID();
                }
            }

            // 'Repeat' mode acts when held regardless of _PressedOn flags (see table above).
            // Relies on repeat logic of IsMouseClicked() but we may as well do it ourselves if we end up exposing finer RepeatDelay/RepeatRate settings.
            if (g.ActiveId == id && (item_flags & UxImGuiItemFlags_ButtonRepeat))
                if (g.IO.MouseDownDuration[g.ActiveIdMouseButton] > 0.0f && IsMouseClicked(g.ActiveIdMouseButton, UxImGuiInputFlags_Repeat, test_owner_id))
                    pressed = true;
        }

        if (pressed && g.IO.ConfigNavCursorVisibleAuto)
            g.NavCursorVisible = false;
    }

    // Keyboard/Gamepad navigation handling
    // We report navigated and navigation-activated items as hovered but we don't set g.HoveredId to not interfere with mouse.
    if (g.NavId == id && g.NavCursorVisible && g.NavHighlightItemUnderNav)
        if (!(flags & UxImGuiButtonFlags_NoHoveredOnFocus))
            hovered = true;
    if (g.NavActivateDownId == id)
    {
        bool nav_activated_by_code = (g.NavActivateId == id);
        bool nav_activated_by_inputs = (g.NavActivatePressedId == id);
        if (!nav_activated_by_inputs && (item_flags & UxImGuiItemFlags_ButtonRepeat))
        {
            // Avoid pressing multiple keys from triggering excessive amount of repeat events
            const UxImGuiKeyData* key1 = GetKeyData(UxImGuiKey_Space);
            const UxImGuiKeyData* key2 = GetKeyData(UxImGuiKey_Enter);
            const UxImGuiKeyData* key3 = GetKeyData(UxImGuiKey_NavGamepadActivate);
            const float t1 = UxImMax(UxImMax(key1->DownDuration, key2->DownDuration), key3->DownDuration);
            nav_activated_by_inputs = CalcTypematicRepeatAmount(t1 - g.IO.DeltaTime, t1, g.IO.KeyRepeatDelay, g.IO.KeyRepeatRate) > 0;
        }
        if (nav_activated_by_code || nav_activated_by_inputs)
        {
            // Set active id so it can be queried by user via IsItemActive(), equivalent of holding the mouse button.
            pressed = true;
            SetActiveID(id, window);
            g.ActiveIdSource = g.NavInputSource;
            if (!(flags & UxImGuiButtonFlags_NoNavFocus) && !(g.NavActivateFlags & UxImGuiActivateFlags_FromShortcut))
                SetFocusID(id, window);
            if (g.NavActivateFlags & UxImGuiActivateFlags_FromShortcut)
                g.ActiveIdFromShortcut = true;
        }
    }

    // Process while held
    bool held = false;
    if (g.ActiveId == id)
    {
        if (g.ActiveIdSource == UxImGuiInputSource_Mouse)
        {
            if (g.ActiveIdIsJustActivated)
                g.ActiveIdClickOffset = g.IO.MousePos - bb.Min;

            const int mouse_button = g.ActiveIdMouseButton;
            if (mouse_button == -1)
            {
                // Fallback for the rare situation were g.ActiveId was set programmatically or from another widget (e.g. #6304).
                ClearActiveID();
            }
            else if (IsMouseDown(mouse_button, test_owner_id))
            {
                held = true;
            }
            else
            {
                bool release_in = hovered && (flags & UxImGuiButtonFlags_PressedOnClickRelease) != 0;
                bool release_anywhere = (flags & UxImGuiButtonFlags_PressedOnClickReleaseAnywhere) != 0;
                if ((release_in || release_anywhere) && !g.DragDropActive)
                {
                    // Report as pressed when releasing the mouse (this is the most common path)
                    bool is_double_click_release = (flags & UxImGuiButtonFlags_PressedOnDoubleClick) && g.IO.MouseReleased[mouse_button] && g.IO.MouseClickedLastCount[mouse_button] == 2;
                    bool is_repeating_already = (item_flags & UxImGuiItemFlags_ButtonRepeat) && g.IO.MouseDownDurationPrev[mouse_button] >= g.IO.KeyRepeatDelay; // Repeat mode trumps <on release>
                    bool is_button_avail_or_owned = TestKeyOwner(MouseButtonToKey(mouse_button), test_owner_id);
                    if (!is_double_click_release && !is_repeating_already && is_button_avail_or_owned)
                        pressed = true;
                }
                ClearActiveID();
            }
            if (!(flags & UxImGuiButtonFlags_NoNavFocus) && g.IO.ConfigNavCursorVisibleAuto)
                g.NavCursorVisible = false;
        }
        else if (g.ActiveIdSource == UxImGuiInputSource_Keyboard || g.ActiveIdSource == UxImGuiInputSource_Gamepad)
        {
            // When activated using Nav, we hold on the ActiveID until activation button is released
            if (g.NavActivateDownId == id)
                held = true; // hovered == true not true as we are already likely hovered on direct activation.
            else
                ClearActiveID();
        }
        if (pressed)
            g.ActiveIdHasBeenPressedBefore = true;
    }

    // Activation highlight (this may be a remote activation)
    if (g.NavHighlightActivatedId == id)
        hovered = true;

    if (out_hovered) *out_hovered = hovered;
    if (out_held) *out_held = held;

    return pressed;
}

bool UxImGui::ButtonEx(const char* label, const UxImVec2& size_arg, UxImGuiButtonFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);

    UxImVec2 pos = window->DC.CursorPos;
    if ((flags & UxImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    UxImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const UxImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const UxImU32 col = GetColorU32((held && hovered) ? UxImGuiCol_ButtonActive : hovered ? UxImGuiCol_ButtonHovered : UxImGuiCol_Button);
    RenderNavCursor(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    if (g.LogEnabled)
        LogSetNextTextDecoration("[", "]");
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

    // Automatically close popups
    //if (pressed && !(flags & UxImGuiButtonFlags_DontClosePopups) && (window->Flags & UxImGuiWindowFlags_Popup))
    //    CloseCurrentPopup();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

bool UxImGui::Button(const char* label, const UxImVec2& size_arg)
{
    return ButtonEx(label, size_arg, UxImGuiButtonFlags_None);
}

// Small buttons fits within text without additional vertical spacing.
bool UxImGui::SmallButton(const char* label)
{
    UxImGuiContext& g = *GUxImGui;
    float backup_padding_y = g.Style.FramePadding.y;
    g.Style.FramePadding.y = 0.0f;
    bool pressed = ButtonEx(label, UxImVec2(0, 0), UxImGuiButtonFlags_AlignTextBaseLine);
    g.Style.FramePadding.y = backup_padding_y;
    return pressed;
}

// Tip: use UxImGui::PushID()/PopID() to push indices or pointers in the ID stack.
// Then you can keep 'str_id' empty or the same for all your buttons (instead of creating a string based on a non-string id)
bool UxImGui::InvisibleButton(const char* str_id, const UxImVec2& size_arg, UxImGuiButtonFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // Cannot use zero-size for InvisibleButton(). Unlike Button() there is not way to fallback using the label size.
    IM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f);

    const UxImGuiID id = window->GetID(str_id);
    UxImVec2 size = CalcItemSize(size_arg, 0.0f, 0.0f);
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id, NULL, (flags & UxImGuiButtonFlags_EnableNav) ? UxImGuiItemFlags_None : UxImGuiItemFlags_NoNav))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);
    RenderNavCursor(bb, id);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, str_id, g.LastItemData.StatusFlags);
    return pressed;
}

bool UxImGui::ArrowButtonEx(const char* str_id, UxImGuiDir dir, UxImVec2 size, UxImGuiButtonFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const UxImGuiID id = window->GetID(str_id);
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const float default_size = GetFrameHeight();
    ItemSize(size, (size.y >= default_size) ? g.Style.FramePadding.y : -1.0f);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const UxImU32 bg_col = GetColorU32((held && hovered) ? UxImGuiCol_ButtonActive : hovered ? UxImGuiCol_ButtonHovered : UxImGuiCol_Button);
    const UxImU32 text_col = GetColorU32(UxImGuiCol_Text);
    RenderNavCursor(bb, id);
    RenderFrame(bb.Min, bb.Max, bg_col, true, g.Style.FrameRounding);
    RenderArrow(window->DrawList, bb.Min + UxImVec2(UxImMax(0.0f, (size.x - g.FontSize) * 0.5f), UxImMax(0.0f, (size.y - g.FontSize) * 0.5f)), text_col, dir);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, str_id, g.LastItemData.StatusFlags);
    return pressed;
}

bool UxImGui::ArrowButton(const char* str_id, UxImGuiDir dir)
{
    float sz = GetFrameHeight();
    return ArrowButtonEx(str_id, dir, UxImVec2(sz, sz), UxImGuiButtonFlags_None);
}

// Button to close a window
bool UxImGui::CloseButton(UxImGuiID id, const UxImVec2& pos)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    // Tweak 1: Shrink hit-testing area if button covers an abnormally large proportion of the visible region. That's in order to facilitate moving the window away. (#3825)
    // This may better be applied as a general hit-rect reduction mechanism for all widgets to ensure the area to move window is always accessible?
    const UxImRect bb(pos, pos + UxImVec2(g.FontSize, g.FontSize));
    UxImRect bb_interact = bb;
    const float area_to_visible_ratio = window->OuterRectClipped.GetArea() / bb.GetArea();
    if (area_to_visible_ratio < 1.5f)
        bb_interact.Expand(UxImTrunc(bb_interact.GetSize() * -0.25f));

    // Tweak 2: We intentionally allow interaction when clipped so that a mechanical Alt,Right,Activate sequence can always close a window.
    // (this isn't the common behavior of buttons, but it doesn't affect the user because navigation tends to keep items visible in scrolling layer).
    bool is_clipped = !ItemAdd(bb_interact, id);

    bool hovered, held;
    bool pressed = ButtonBehavior(bb_interact, id, &hovered, &held);
    if (is_clipped)
        return pressed;

    // Render
    UxImU32 bg_col = GetColorU32(held ? UxImGuiCol_ButtonActive : UxImGuiCol_ButtonHovered);
    if (hovered)
        window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_col);
    RenderNavCursor(bb, id, UxImGuiNavRenderCursorFlags_Compact);
    const UxImU32 cross_col = GetColorU32(UxImGuiCol_Text);
    const UxImVec2 cross_center = bb.GetCenter() - UxImVec2(0.5f, 0.5f);
    const float cross_extent = g.FontSize * 0.5f * 0.7071f - 1.0f;
    const float cross_thickness = 1.0f; // FIXME-DPI
    window->DrawList->AddLine(cross_center + UxImVec2(+cross_extent, +cross_extent), cross_center + UxImVec2(-cross_extent, -cross_extent), cross_col, cross_thickness);
    window->DrawList->AddLine(cross_center + UxImVec2(+cross_extent, -cross_extent), cross_center + UxImVec2(-cross_extent, +cross_extent), cross_col, cross_thickness);

    return pressed;
}

// The Collapse button also functions as a Dock Menu button.
bool UxImGui::CollapseButton(UxImGuiID id, const UxImVec2& pos, UxImGuiDockNode* dock_node)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    UxImRect bb(pos, pos + UxImVec2(g.FontSize, g.FontSize));
    bool is_clipped = !ItemAdd(bb, id);
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, UxImGuiButtonFlags_None);
    if (is_clipped)
        return pressed;

    // Render
    //bool is_dock_menu = (window->DockNodeAsHost && !window->Collapsed);
    UxImU32 bg_col = GetColorU32((held && hovered) ? UxImGuiCol_ButtonActive : hovered ? UxImGuiCol_ButtonHovered : UxImGuiCol_Button);
    UxImU32 text_col = GetColorU32(UxImGuiCol_Text);
    if (hovered || held)
        window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_col);
    RenderNavCursor(bb, id, UxImGuiNavRenderCursorFlags_Compact);

    if (dock_node)
        RenderArrowDockMenu(window->DrawList, bb.Min, g.FontSize, text_col);
    else
        RenderArrow(window->DrawList, bb.Min, text_col, window->Collapsed ? UxImGuiDir_Right : UxImGuiDir_Down, 1.0f);

    // Switch to moving the window after mouse is moved beyond the initial drag threshold
    if (IsItemActive() && IsMouseDragging(0))
        StartMouseMovingWindowOrNode(window, dock_node, true); // Undock from window/collapse menu button

    return pressed;
}

UxImGuiID UxImGui::GetWindowScrollbarID(UxImGuiWindow* window, UxImGuiAxis axis)
{
    return window->GetID(axis == UxImGuiAxis_X ? "#SCROLLX" : "#SCROLLY");
}

// Return scrollbar rectangle, must only be called for corresponding axis if window->ScrollbarX/Y is set.
UxImRect UxImGui::GetWindowScrollbarRect(UxImGuiWindow* window, UxImGuiAxis axis)
{
    UxImGuiContext& g = *GUxImGui;
    const UxImRect outer_rect = window->Rect();
    const UxImRect inner_rect = window->InnerRect;
    const float scrollbar_size = window->ScrollbarSizes[axis ^ 1]; // (ScrollbarSizes.x = width of Y scrollbar; ScrollbarSizes.y = height of X scrollbar)
    IM_ASSERT(scrollbar_size >= 0.0f);
    const float border_size = IM_ROUND(window->WindowBorderSize * 0.5f);
    const float border_top = (window->Flags & UxImGuiWindowFlags_MenuBar) ? IM_ROUND(g.Style.FrameBorderSize * 0.5f) : 0.0f;
    if (axis == UxImGuiAxis_X)
        return UxImRect(inner_rect.Min.x + border_size, UxImMax(outer_rect.Min.y + border_size, outer_rect.Max.y - border_size - scrollbar_size), inner_rect.Max.x - border_size, outer_rect.Max.y - border_size);
    else
        return UxImRect(UxImMax(outer_rect.Min.x, outer_rect.Max.x - border_size - scrollbar_size), inner_rect.Min.y + border_top, outer_rect.Max.x - border_size, inner_rect.Max.y - border_size);
}

void UxImGui::Scrollbar(UxImGuiAxis axis)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    const UxImGuiID id = GetWindowScrollbarID(window, axis);

    // Calculate scrollbar bounding box
    UxImRect bb = GetWindowScrollbarRect(window, axis);
    UxImDrawFlags rounding_corners = UxImDrawFlags_RoundCornersNone;
    if (axis == UxImGuiAxis_X)
    {
        rounding_corners |= UxImDrawFlags_RoundCornersBottomLeft;
        if (!window->ScrollbarY)
            rounding_corners |= UxImDrawFlags_RoundCornersBottomRight;
    }
    else
    {
        if ((window->Flags & UxImGuiWindowFlags_NoTitleBar) && !(window->Flags & UxImGuiWindowFlags_MenuBar))
            rounding_corners |= UxImDrawFlags_RoundCornersTopRight;
        if (!window->ScrollbarX)
            rounding_corners |= UxImDrawFlags_RoundCornersBottomRight;
    }
    float size_visible = window->InnerRect.Max[axis] - window->InnerRect.Min[axis];
    float size_contents = window->ContentSize[axis] + window->WindowPadding[axis] * 2.0f;
    UxImS64 scroll = (UxImS64)window->Scroll[axis];
    ScrollbarEx(bb, id, axis, &scroll, (UxImS64)size_visible, (UxImS64)size_contents, rounding_corners);
    window->Scroll[axis] = (float)scroll;
}

// Vertical/Horizontal scrollbar
// The entire piece of code below is rather confusing because:
// - We handle absolute seeking (when first clicking outside the grab) and relative manipulation (afterward or when clicking inside the grab)
// - We store values as normalized ratio and in a form that allows the window content to change while we are holding on a scrollbar
// - We handle both horizontal and vertical scrollbars, which makes the terminology not ideal.
// Still, the code should probably be made simpler..
bool UxImGui::ScrollbarEx(const UxImRect& bb_frame, UxImGuiID id, UxImGuiAxis axis, UxImS64* p_scroll_v, UxImS64 size_visible_v, UxImS64 size_contents_v, UxImDrawFlags draw_rounding_flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    const float bb_frame_width = bb_frame.GetWidth();
    const float bb_frame_height = bb_frame.GetHeight();
    if (bb_frame_width <= 0.0f || bb_frame_height <= 0.0f)
        return false;

    // When we are too small, start hiding and disabling the grab (this reduce visual noise on very small window and facilitate using the window resize grab)
    float alpha = 1.0f;
    if ((axis == UxImGuiAxis_Y) && bb_frame_height < bb_frame_width)
        alpha = UxImSaturate(bb_frame_height / UxImMax(bb_frame_width * 2.0f, 1.0f));
    if (alpha <= 0.0f)
        return false;

    const UxImGuiStyle& style = g.Style;
    const bool allow_interaction = (alpha >= 1.0f);

    UxImRect bb = bb_frame;
    bb.Expand(UxImVec2(-UxImClamp(IM_TRUNC((bb_frame_width - 2.0f) * 0.5f), 0.0f, 3.0f), -UxImClamp(IM_TRUNC((bb_frame_height - 2.0f) * 0.5f), 0.0f, 3.0f)));

    // V denote the main, longer axis of the scrollbar (= height for a vertical scrollbar)
    const float scrollbar_size_v = (axis == UxImGuiAxis_X) ? bb.GetWidth() : bb.GetHeight();

    // Calculate the height of our grabbable box. It generally represent the amount visible (vs the total scrollable amount)
    // But we maintain a minimum size in pixel to allow for the user to still aim inside.
    IM_ASSERT(UxImMax(size_contents_v, size_visible_v) > 0.0f); // Adding this assert to check if the UxImMax(XXX,1.0f) is still needed. PLEASE CONTACT ME if this triggers.
    const UxImS64 win_size_v = UxImMax(UxImMax(size_contents_v, size_visible_v), (UxImS64)1);
    const float grab_h_minsize = UxImMin(bb.GetSize()[axis], style.GrabMinSize);
    const float grab_h_pixels = UxImClamp(scrollbar_size_v * ((float)size_visible_v / (float)win_size_v), grab_h_minsize, scrollbar_size_v);
    const float grab_h_norm = grab_h_pixels / scrollbar_size_v;

    // Handle input right away. None of the code of Begin() is relying on scrolling position before calling Scrollbar().
    bool held = false;
    bool hovered = false;
    ItemAdd(bb_frame, id, NULL, UxImGuiItemFlags_NoNav);
    ButtonBehavior(bb, id, &hovered, &held, UxImGuiButtonFlags_NoNavFocus);

    const UxImS64 scroll_max = UxImMax((UxImS64)1, size_contents_v - size_visible_v);
    float scroll_ratio = UxImSaturate((float)*p_scroll_v / (float)scroll_max);
    float grab_v_norm = scroll_ratio * (scrollbar_size_v - grab_h_pixels) / scrollbar_size_v; // Grab position in normalized space
    if (held && allow_interaction && grab_h_norm < 1.0f)
    {
        const float scrollbar_pos_v = bb.Min[axis];
        const float mouse_pos_v = g.IO.MousePos[axis];

        // Click position in scrollbar normalized space (0.0f->1.0f)
        const float clicked_v_norm = UxImSaturate((mouse_pos_v - scrollbar_pos_v) / scrollbar_size_v);

        const int held_dir = (clicked_v_norm < grab_v_norm) ? -1 : (clicked_v_norm > grab_v_norm + grab_h_norm) ? +1 : 0;
        if (g.ActiveIdIsJustActivated)
        {
            // On initial click when held_dir == 0 (clicked over grab): calculate the distance between mouse and the center of the grab
            const bool scroll_to_clicked_location = (g.IO.ConfigScrollbarScrollByPage == false || g.IO.KeyShift || held_dir == 0);
            g.ScrollbarSeekMode = scroll_to_clicked_location ? 0 : (short)held_dir;
            g.ScrollbarClickDeltaToGrabCenter = (held_dir == 0 && !g.IO.KeyShift) ? clicked_v_norm - grab_v_norm - grab_h_norm * 0.5f : 0.0f;
        }

        // Apply scroll (p_scroll_v will generally point on one member of window->Scroll)
        // It is ok to modify Scroll here because we are being called in Begin() after the calculation of ContentSize and before setting up our starting position
        if (g.ScrollbarSeekMode == 0)
        {
            // Absolute seeking
            const float scroll_v_norm = UxImSaturate((clicked_v_norm - g.ScrollbarClickDeltaToGrabCenter - grab_h_norm * 0.5f) / (1.0f - grab_h_norm));
            *p_scroll_v = (UxImS64)(scroll_v_norm * scroll_max);
        }
        else
        {
            // Page by page
            if (IsMouseClicked(UxImGuiMouseButton_Left, UxImGuiInputFlags_Repeat) && held_dir == g.ScrollbarSeekMode)
            {
                float page_dir = (g.ScrollbarSeekMode > 0.0f) ? +1.0f : -1.0f;
                *p_scroll_v = UxImClamp(*p_scroll_v + (UxImS64)(page_dir * size_visible_v), (UxImS64)0, scroll_max);
            }
        }

        // Update values for rendering
        scroll_ratio = UxImSaturate((float)*p_scroll_v / (float)scroll_max);
        grab_v_norm = scroll_ratio * (scrollbar_size_v - grab_h_pixels) / scrollbar_size_v;

        // Update distance to grab now that we have seek'ed and saturated
        //if (seek_absolute)
        //    g.ScrollbarClickDeltaToGrabCenter = clicked_v_norm - grab_v_norm - grab_h_norm * 0.5f;
    }

    // Render
    const UxImU32 bg_col = GetColorU32(UxImGuiCol_ScrollbarBg);
    const UxImU32 grab_col = GetColorU32(held ? UxImGuiCol_ScrollbarGrabActive : hovered ? UxImGuiCol_ScrollbarGrabHovered : UxImGuiCol_ScrollbarGrab, alpha);
    window->DrawList->AddRectFilled(bb_frame.Min, bb_frame.Max, bg_col, window->WindowRounding, draw_rounding_flags);
    UxImRect grab_rect;
    if (axis == UxImGuiAxis_X)
        grab_rect = UxImRect(UxImLerp(bb.Min.x, bb.Max.x, grab_v_norm), bb.Min.y, UxImLerp(bb.Min.x, bb.Max.x, grab_v_norm) + grab_h_pixels, bb.Max.y);
    else
        grab_rect = UxImRect(bb.Min.x, UxImLerp(bb.Min.y, bb.Max.y, grab_v_norm), bb.Max.x, UxImLerp(bb.Min.y, bb.Max.y, grab_v_norm) + grab_h_pixels);
    window->DrawList->AddRectFilled(grab_rect.Min, grab_rect.Max, grab_col, style.ScrollbarRounding);

    return held;
}

// - Read about UxImTextureID/UxImTextureRef here: https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
// - 'uv0' and 'uv1' are texture coordinates. Read about them from the same link above.
void UxImGui::ImageWithBg(UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1, const UxImVec4& bg_col, const UxImVec4& tint_col)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const UxImVec2 padding(g.Style.ImageBorderSize, g.Style.ImageBorderSize);
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + image_size + padding * 2.0f);
    ItemSize(bb);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    if (g.Style.ImageBorderSize > 0.0f)
        window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(UxImGuiCol_Border), 0.0f, UxImDrawFlags_None, g.Style.ImageBorderSize);
    if (bg_col.w > 0.0f)
        window->DrawList->AddRectFilled(bb.Min + padding, bb.Max - padding, GetColorU32(bg_col));
    window->DrawList->AddImage(tex_ref, bb.Min + padding, bb.Max - padding, uv0, uv1, GetColorU32(tint_col));
}

void UxImGui::Image(UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1)
{
    ImageWithBg(tex_ref, image_size, uv0, uv1);
}

// 1.91.9 (February 2025) removed 'tint_col' and 'border_col' parameters, made border size not depend on color value. (#8131, #8238)
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
void UxImGui::Image(UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1, const UxImVec4& tint_col, const UxImVec4& border_col)
{
    UxImGuiContext& g = *GUxImGui;
    PushStyleVar(UxImGuiStyleVar_ImageBorderSize, (border_col.w > 0.0f) ? UxImMax(1.0f, g.Style.ImageBorderSize) : 0.0f); // Preserve legacy behavior where border is always visible when border_col's Alpha is >0.0f
    PushStyleColor(UxImGuiCol_Border, border_col);
    ImageWithBg(tex_ref, image_size, uv0, uv1, UxImVec4(0, 0, 0, 0), tint_col);
    PopStyleColor();
    PopStyleVar();
}
#endif

bool UxImGui::ImageButtonEx(UxImGuiID id, UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1, const UxImVec4& bg_col, const UxImVec4& tint_col, UxImGuiButtonFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const UxImVec2 padding = g.Style.FramePadding;
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + image_size + padding * 2.0f);
    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const UxImU32 col = GetColorU32((held && hovered) ? UxImGuiCol_ButtonActive : hovered ? UxImGuiCol_ButtonHovered : UxImGuiCol_Button);
    RenderNavCursor(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, UxImClamp((float)UxImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));
    if (bg_col.w > 0.0f)
        window->DrawList->AddRectFilled(bb.Min + padding, bb.Max - padding, GetColorU32(bg_col));
    window->DrawList->AddImage(tex_ref, bb.Min + padding, bb.Max - padding, uv0, uv1, GetColorU32(tint_col));

    return pressed;
}

// - ImageButton() adds style.FramePadding*2.0f to provided size. This is in order to facilitate fitting an image in a button.
// - ImageButton() draws a background based on regular Button() color + optionally an inner background if specified. (#8165) // FIXME: Maybe that's not the best design?
bool UxImGui::ImageButton(const char* str_id, UxImTextureRef tex_ref, const UxImVec2& image_size, const UxImVec2& uv0, const UxImVec2& uv1, const UxImVec4& bg_col, const UxImVec4& tint_col)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    return ImageButtonEx(window->GetID(str_id), tex_ref, image_size, uv0, uv1, bg_col, tint_col);
}

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
// Legacy API obsoleted in 1.89. Two differences with new ImageButton()
// - old ImageButton() used UxImTextureID as item id (created issue with multiple buttons with same image, transient texture id values, opaque computation of ID)
// - new ImageButton() requires an explicit 'const char* str_id'
// - old ImageButton() had frame_padding' override argument.
// - new ImageButton() always use style.FramePadding.
/*
bool UxImGui::ImageButton(UxImTextureID user_texture_id, const UxImVec2& size, const UxImVec2& uv0, const UxImVec2& uv1, int frame_padding, const UxImVec4& bg_col, const UxImVec4& tint_col)
{
    // Default to using texture ID as ID. User can still push string/integer prefixes.
    PushID((UxImTextureID)(intptr_t)user_texture_id);
    if (frame_padding >= 0)
        PushStyleVar(UxImGuiStyleVar_FramePadding, UxImVec2((float)frame_padding, (float)frame_padding));
    bool ret = ImageButton("", user_texture_id, size, uv0, uv1, bg_col, tint_col);
    if (frame_padding >= 0)
        PopStyleVar();
    PopID();
    return ret;
}
*/
#endif // #ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS

bool UxImGui::Checkbox(const char* label, bool* v)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);

    const float square_sz = GetFrameHeight();
    const UxImVec2 pos = window->DC.CursorPos;
    const UxImRect total_bb(pos, pos + UxImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));
    ItemSize(total_bb, style.FramePadding.y);
    const bool is_visible = ItemAdd(total_bb, id);
    const bool is_multi_select = (g.LastItemData.ItemFlags & UxImGuiItemFlags_IsMultiSelect) != 0;
    if (!is_visible)
        if (!is_multi_select || !g.BoxSelectState.UnclipMode || !g.BoxSelectState.UnclipRect.Overlaps(total_bb)) // Extra layer of "no logic clip" for box-select support
        {
            IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | UxImGuiItemStatusFlags_Checkable | (*v ? UxImGuiItemStatusFlags_Checked : 0));
            return false;
        }

    // Range-Selection/Multi-selection support (header)
    bool checked = *v;
    if (is_multi_select)
        MultiSelectItemHeader(id, &checked, NULL);

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    // Range-Selection/Multi-selection support (footer)
    if (is_multi_select)
        MultiSelectItemFooter(id, &checked, &pressed);
    else if (pressed)
        checked = !checked;

    if (*v != checked)
    {
        *v = checked;
        pressed = true; // return value
        MarkItemEdited(id);
    }

    const UxImRect check_bb(pos, pos + UxImVec2(square_sz, square_sz));
    const bool mixed_value = (g.LastItemData.ItemFlags & UxImGuiItemFlags_MixedValue) != 0;
    if (is_visible)
    {
        RenderNavCursor(total_bb, id);
        RenderFrame(check_bb.Min, check_bb.Max, GetColorU32((held && hovered) ? UxImGuiCol_FrameBgActive : hovered ? UxImGuiCol_FrameBgHovered : UxImGuiCol_FrameBg), true, style.FrameRounding);
        UxImU32 check_col = GetColorU32(UxImGuiCol_CheckMark);
        if (mixed_value)
        {
            // Undocumented tristate/mixed/indeterminate checkbox (#2644)
            // This may seem awkwardly designed because the aim is to make UxImGuiItemFlags_MixedValue supported by all widgets (not just checkbox)
            UxImVec2 pad(UxImMax(1.0f, IM_TRUNC(square_sz / 3.6f)), UxImMax(1.0f, IM_TRUNC(square_sz / 3.6f)));
            window->DrawList->AddRectFilled(check_bb.Min + pad, check_bb.Max - pad, check_col, style.FrameRounding);
        }
        else if (*v)
        {
            const float pad = UxImMax(1.0f, IM_TRUNC(square_sz / 6.0f));
            RenderCheckMark(window->DrawList, check_bb.Min + UxImVec2(pad, pad), check_col, square_sz - pad * 2.0f);
        }
    }
    const UxImVec2 label_pos = UxImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
    if (g.LogEnabled)
        LogRenderedText(&label_pos, mixed_value ? "[~]" : *v ? "[x]" : "[ ]");
    if (is_visible && label_size.x > 0.0f)
        RenderText(label_pos, label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | UxImGuiItemStatusFlags_Checkable | (*v ? UxImGuiItemStatusFlags_Checked : 0));
    return pressed;
}

template<typename T>
bool UxImGui::CheckboxFlagsT(const char* label, T* flags, T flags_value)
{
    bool all_on = (*flags & flags_value) == flags_value;
    bool any_on = (*flags & flags_value) != 0;
    bool pressed;
    if (!all_on && any_on)
    {
        UxImGuiContext& g = *GUxImGui;
        g.NextItemData.ItemFlags |= UxImGuiItemFlags_MixedValue;
        pressed = Checkbox(label, &all_on);
    }
    else
    {
        pressed = Checkbox(label, &all_on);

    }
    if (pressed)
    {
        if (all_on)
            *flags |= flags_value;
        else
            *flags &= ~flags_value;
    }
    return pressed;
}

bool UxImGui::CheckboxFlags(const char* label, int* flags, int flags_value)
{
    return CheckboxFlagsT(label, flags, flags_value);
}

bool UxImGui::CheckboxFlags(const char* label, unsigned int* flags, unsigned int flags_value)
{
    return CheckboxFlagsT(label, flags, flags_value);
}

bool UxImGui::CheckboxFlags(const char* label, UxImS64* flags, UxImS64 flags_value)
{
    return CheckboxFlagsT(label, flags, flags_value);
}

bool UxImGui::CheckboxFlags(const char* label, UxImU64* flags, UxImU64 flags_value)
{
    return CheckboxFlagsT(label, flags, flags_value);
}

bool UxImGui::RadioButton(const char* label, bool active)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);

    const float square_sz = GetFrameHeight();
    const UxImVec2 pos = window->DC.CursorPos;
    const UxImRect check_bb(pos, pos + UxImVec2(square_sz, square_sz));
    const UxImRect total_bb(pos, pos + UxImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
        return false;

    UxImVec2 center = check_bb.GetCenter();
    center.x = IM_ROUND(center.x);
    center.y = IM_ROUND(center.y);
    const float radius = (square_sz - 1.0f) * 0.5f;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
        MarkItemEdited(id);

    RenderNavCursor(total_bb, id);
    const int num_segment = window->DrawList->_CalcCircleAutoSegmentCount(radius);
    window->DrawList->AddCircleFilled(center, radius, GetColorU32((held && hovered) ? UxImGuiCol_FrameBgActive : hovered ? UxImGuiCol_FrameBgHovered : UxImGuiCol_FrameBg), num_segment);
    if (active)
    {
        const float pad = UxImMax(1.0f, IM_TRUNC(square_sz / 6.0f));
        window->DrawList->AddCircleFilled(center, radius - pad, GetColorU32(UxImGuiCol_CheckMark));
    }

    if (style.FrameBorderSize > 0.0f)
    {
        window->DrawList->AddCircle(center + UxImVec2(1, 1), radius, GetColorU32(UxImGuiCol_BorderShadow), num_segment, style.FrameBorderSize);
        window->DrawList->AddCircle(center, radius, GetColorU32(UxImGuiCol_Border), num_segment, style.FrameBorderSize);
    }

    UxImVec2 label_pos = UxImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
    if (g.LogEnabled)
        LogRenderedText(&label_pos, active ? "(x)" : "( )");
    if (label_size.x > 0.0f)
        RenderText(label_pos, label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

// FIXME: This would work nicely if it was a public template, e.g. 'template<T> RadioButton(const char* label, T* v, T v_button)', but I'm not sure how we would expose it..
bool UxImGui::RadioButton(const char* label, int* v, int v_button)
{
    const bool pressed = RadioButton(label, *v == v_button);
    if (pressed)
        *v = v_button;
    return pressed;
}

// size_arg (for each axis) < 0.0f: align to end, 0.0f: auto, > 0.0f: specified size
void UxImGui::ProgressBar(float fraction, const UxImVec2& size_arg, const char* overlay)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;

    UxImVec2 pos = window->DC.CursorPos;
    UxImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
    UxImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, 0))
        return;

    // Fraction < 0.0f will display an indeterminate progress bar animation
    // The value must be animated along with time, so e.g. passing '-1.0f * UxImGui::GetTime()' as fraction works.
    const bool is_indeterminate = (fraction < 0.0f);
    if (!is_indeterminate)
        fraction = UxImSaturate(fraction);

    // Out of courtesy we accept a NaN fraction without crashing
    float fill_n0 = 0.0f;
    float fill_n1 = (fraction == fraction) ? fraction : 0.0f;

    if (is_indeterminate)
    {
        const float fill_width_n = 0.2f;
        fill_n0 = UxImFmod(-fraction, 1.0f) * (1.0f + fill_width_n) - fill_width_n;
        fill_n1 = UxImSaturate(fill_n0 + fill_width_n);
        fill_n0 = UxImSaturate(fill_n0);
    }

    // Render
    RenderFrame(bb.Min, bb.Max, GetColorU32(UxImGuiCol_FrameBg), true, style.FrameRounding);
    bb.Expand(UxImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
    RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(UxImGuiCol_PlotHistogram), fill_n0, fill_n1, style.FrameRounding);

    // Default displaying the fraction as percentage string, but user can override it
    // Don't display text for indeterminate bars by default
    char overlay_buf[32];
    if (!is_indeterminate || overlay != NULL)
    {
        if (!overlay)
        {
            UxImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.0f%%", fraction * 100 + 0.01f);
            overlay = overlay_buf;
        }

        UxImVec2 overlay_size = CalcTextSize(overlay, NULL);
        if (overlay_size.x > 0.0f)
        {
            float text_x = is_indeterminate ? (bb.Min.x + bb.Max.x - overlay_size.x) * 0.5f : UxImLerp(bb.Min.x, bb.Max.x, fill_n1) + style.ItemSpacing.x;
            RenderTextClipped(UxImVec2(UxImClamp(text_x, bb.Min.x, bb.Max.x - overlay_size.x - style.ItemInnerSpacing.x), bb.Min.y), bb.Max, overlay, NULL, &overlay_size, UxImVec2(0.0f, 0.5f), &bb);
        }
    }
}

void UxImGui::Bullet()
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const float line_height = UxImMax(UxImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), g.FontSize);
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + UxImVec2(g.FontSize, line_height));
    ItemSize(bb);
    if (!ItemAdd(bb, 0))
    {
        SameLine(0, style.FramePadding.x * 2);
        return;
    }

    // Render and stay on same line
    UxImU32 text_col = GetColorU32(UxImGuiCol_Text);
    RenderBullet(window->DrawList, bb.Min + UxImVec2(style.FramePadding.x + g.FontSize * 0.5f, line_height * 0.5f), text_col);
    SameLine(0, style.FramePadding.x * 2.0f);
}

// This is provided as a convenience for being an often requested feature.
// FIXME-STYLE: we delayed adding as there is a larger plan to revamp the styling system.
// Because of this we currently don't provide many styling options for this widget
// (e.g. hovered/active colors are automatically inferred from a single color).
bool UxImGui::TextLink(const char* label)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiID id = window->GetID(label);
    const char* label_end = FindRenderedTextEnd(label);

    UxImVec2 pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    UxImVec2 size = CalcTextSize(label, label_end, true);
    UxImRect bb(pos, pos + size);
    ItemSize(size, 0.0f);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    RenderNavCursor(bb, id);

    if (hovered)
        SetMouseCursor(UxImGuiMouseCursor_Hand);

    UxImVec4 text_colf = g.Style.Colors[UxImGuiCol_TextLink];
    UxImVec4 line_colf = text_colf;
    {
        // FIXME-STYLE: Read comments above. This widget is NOT written in the same style as some earlier widgets,
        // as we are currently experimenting/planning a different styling system.
        float h, s, v;
        ColorConvertRGBtoHSV(text_colf.x, text_colf.y, text_colf.z, h, s, v);
        if (held || hovered)
        {
            v = UxImSaturate(v + (held ? 0.4f : 0.3f));
            h = UxImFmod(h + 0.02f, 1.0f);
        }
        ColorConvertHSVtoRGB(h, s, v, text_colf.x, text_colf.y, text_colf.z);
        v = UxImSaturate(v - 0.20f);
        ColorConvertHSVtoRGB(h, s, v, line_colf.x, line_colf.y, line_colf.z);
    }

    float line_y = bb.Max.y + UxImFloor(g.FontBaked->Descent * g.FontScale * 0.20f);
    window->DrawList->AddLine(UxImVec2(bb.Min.x, line_y), UxImVec2(bb.Max.x, line_y), GetColorU32(line_colf)); // FIXME-TEXT: Underline mode // FIXME-DPI

    PushStyleColor(UxImGuiCol_Text, GetColorU32(text_colf));
    RenderText(bb.Min, label, label_end);
    PopStyleColor();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

void UxImGui::TextLinkOpenURL(const char* label, const char* url)
{
    UxImGuiContext& g = *GUxImGui;
    if (url == NULL)
        url = label;
    if (TextLink(label))
        if (g.PlatformIO.Platform_OpenInShellFn != NULL)
            g.PlatformIO.Platform_OpenInShellFn(&g, url);
    SetItemTooltip(LocalizeGetMsg(UxImGuiLocKey_OpenLink_s), url); // It is more reassuring for user to _always_ display URL when we same as label
    if (BeginPopupContextItem())
    {
        if (MenuItem(LocalizeGetMsg(UxImGuiLocKey_CopyLink)))
            SetClipboardText(url);
        EndPopup();
    }
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Low-level Layout helpers
//-------------------------------------------------------------------------
// - Spacing()
// - Dummy()
// - NewLine()
// - AlignTextToFramePadding()
// - SeparatorEx() [Internal]
// - Separator()
// - SplitterBehavior() [Internal]
// - ShrinkWidths() [Internal]
//-------------------------------------------------------------------------

void UxImGui::Spacing()
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    ItemSize(UxImVec2(0, 0));
}

void UxImGui::Dummy(const UxImVec2& size)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(size);
    ItemAdd(bb, 0);
}

void UxImGui::NewLine()
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiLayoutType backup_layout_type = window->DC.LayoutType;
    window->DC.LayoutType = UxImGuiLayoutType_Vertical;
    window->DC.IsSameLine = false;
    if (window->DC.CurrLineSize.y > 0.0f)     // In the event that we are on a line with items that is smaller that FontSize high, we will preserve its height.
        ItemSize(UxImVec2(0, 0));
    else
        ItemSize(UxImVec2(0.0f, g.FontSize));
    window->DC.LayoutType = backup_layout_type;
}

void UxImGui::AlignTextToFramePadding()
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    window->DC.CurrLineSize.y = UxImMax(window->DC.CurrLineSize.y, g.FontSize + g.Style.FramePadding.y * 2);
    window->DC.CurrLineTextBaseOffset = UxImMax(window->DC.CurrLineTextBaseOffset, g.Style.FramePadding.y);
}

// Horizontal/vertical separating line
// FIXME: Surprisingly, this seemingly trivial widget is a victim of many different legacy/tricky layout issues.
// Note how thickness == 1.0f is handled specifically as not moving CursorPos by 'thickness', but other values are.
void UxImGui::SeparatorEx(UxImGuiSeparatorFlags flags, float thickness)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    UxImGuiContext& g = *GUxImGui;
    IM_ASSERT(UxImIsPowerOfTwo(flags & (UxImGuiSeparatorFlags_Horizontal | UxImGuiSeparatorFlags_Vertical)));   // Check that only 1 option is selected
    IM_ASSERT(thickness > 0.0f);

    if (flags & UxImGuiSeparatorFlags_Vertical)
    {
        // Vertical separator, for menu bars (use current line height).
        float y1 = window->DC.CursorPos.y;
        float y2 = window->DC.CursorPos.y + window->DC.CurrLineSize.y;
        const UxImRect bb(UxImVec2(window->DC.CursorPos.x, y1), UxImVec2(window->DC.CursorPos.x + thickness, y2));
        ItemSize(UxImVec2(thickness, 0.0f));
        if (!ItemAdd(bb, 0))
            return;

        // Draw
        window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(UxImGuiCol_Separator));
        if (g.LogEnabled)
            LogText(" |");
    }
    else if (flags & UxImGuiSeparatorFlags_Horizontal)
    {
        // Horizontal Separator
        float x1 = window->DC.CursorPos.x;
        float x2 = window->WorkRect.Max.x;

        // Preserve legacy behavior inside Columns()
        // Before Tables API happened, we relied on Separator() to span all columns of a Columns() set.
        // We currently don't need to provide the same feature for tables because tables naturally have border features.
        UxImGuiOldColumns* columns = (flags & UxImGuiSeparatorFlags_SpanAllColumns) ? window->DC.CurrentColumns : NULL;
        if (columns)
        {
            x1 = window->Pos.x + window->DC.Indent.x; // Used to be Pos.x before 2023/10/03
            x2 = window->Pos.x + window->Size.x;
            PushColumnsBackground();
        }

        // We don't provide our width to the layout so that it doesn't get feed back into AutoFit
        // FIXME: This prevents ->CursorMaxPos based bounding box evaluation from working (e.g. TableEndCell)
        const float thickness_for_layout = (thickness == 1.0f) ? 0.0f : thickness; // FIXME: See 1.70/1.71 Separator() change: makes legacy 1-px separator not affect layout yet. Should change.
        const UxImRect bb(UxImVec2(x1, window->DC.CursorPos.y), UxImVec2(x2, window->DC.CursorPos.y + thickness));
        ItemSize(UxImVec2(0.0f, thickness_for_layout));

        if (ItemAdd(bb, 0))
        {
            // Draw
            window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(UxImGuiCol_Separator));
            if (g.LogEnabled)
                LogRenderedText(&bb.Min, "--------------------------------\n");

        }
        if (columns)
        {
            PopColumnsBackground();
            columns->LineMinY = window->DC.CursorPos.y;
        }
    }
}

void UxImGui::Separator()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    // Those flags should eventually be configurable by the user
    // FIXME: We cannot g.Style.SeparatorTextBorderSize for thickness as it relates to SeparatorText() which is a decorated separator, not defaulting to 1.0f.
    UxImGuiSeparatorFlags flags = (window->DC.LayoutType == UxImGuiLayoutType_Horizontal) ? UxImGuiSeparatorFlags_Vertical : UxImGuiSeparatorFlags_Horizontal;

    // Only applies to legacy Columns() api as they relied on Separator() a lot.
    if (window->DC.CurrentColumns)
        flags |= UxImGuiSeparatorFlags_SpanAllColumns;

    SeparatorEx(flags, 1.0f);
}

void UxImGui::SeparatorTextEx(UxImGuiID id, const char* label, const char* label_end, float extra_w)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    UxImGuiStyle& style = g.Style;

    const UxImVec2 label_size = CalcTextSize(label, label_end, false);
    const UxImVec2 pos = window->DC.CursorPos;
    const UxImVec2 padding = style.SeparatorTextPadding;

    const float separator_thickness = style.SeparatorTextBorderSize;
    const UxImVec2 min_size(label_size.x + extra_w + padding.x * 2.0f, UxImMax(label_size.y + padding.y * 2.0f, separator_thickness));
    const UxImRect bb(pos, UxImVec2(window->WorkRect.Max.x, pos.y + min_size.y));
    const float text_baseline_y = UxImTrunc((bb.GetHeight() - label_size.y) * style.SeparatorTextAlign.y + 0.99999f); //UxImMax(padding.y, UxImTrunc((style.SeparatorTextSize - label_size.y) * 0.5f));
    ItemSize(min_size, text_baseline_y);
    if (!ItemAdd(bb, id))
        return;

    const float sep1_x1 = pos.x;
    const float sep2_x2 = bb.Max.x;
    const float seps_y = UxImTrunc((bb.Min.y + bb.Max.y) * 0.5f + 0.99999f);

    const float label_avail_w = UxImMax(0.0f, sep2_x2 - sep1_x1 - padding.x * 2.0f);
    const UxImVec2 label_pos(pos.x + padding.x + UxImMax(0.0f, (label_avail_w - label_size.x - extra_w) * style.SeparatorTextAlign.x), pos.y + text_baseline_y); // FIXME-ALIGN

    // This allows using SameLine() to position something in the 'extra_w'
    window->DC.CursorPosPrevLine.x = label_pos.x + label_size.x;

    const UxImU32 separator_col = GetColorU32(UxImGuiCol_Separator);
    if (label_size.x > 0.0f)
    {
        const float sep1_x2 = label_pos.x - style.ItemSpacing.x;
        const float sep2_x1 = label_pos.x + label_size.x + extra_w + style.ItemSpacing.x;
        if (sep1_x2 > sep1_x1 && separator_thickness > 0.0f)
            window->DrawList->AddLine(UxImVec2(sep1_x1, seps_y), UxImVec2(sep1_x2, seps_y), separator_col, separator_thickness);
        if (sep2_x2 > sep2_x1 && separator_thickness > 0.0f)
            window->DrawList->AddLine(UxImVec2(sep2_x1, seps_y), UxImVec2(sep2_x2, seps_y), separator_col, separator_thickness);
        if (g.LogEnabled)
            LogSetNextTextDecoration("---", NULL);
        RenderTextEllipsis(window->DrawList, label_pos, UxImVec2(bb.Max.x, bb.Max.y + style.ItemSpacing.y), bb.Max.x, label, label_end, &label_size);
    }
    else
    {
        if (g.LogEnabled)
            LogText("---");
        if (separator_thickness > 0.0f)
            window->DrawList->AddLine(UxImVec2(sep1_x1, seps_y), UxImVec2(sep2_x2, seps_y), separator_col, separator_thickness);
    }
}

void UxImGui::SeparatorText(const char* label)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    // The SeparatorText() vs SeparatorTextEx() distinction is designed to be considerate that we may want:
    // - allow separator-text to be draggable items (would require a stable ID + a noticeable highlight)
    // - this high-level entry point to allow formatting? (which in turns may require ID separate from formatted string)
    // - because of this we probably can't turn 'const char* label' into 'const char* fmt, ...'
    // Otherwise, we can decide that users wanting to drag this would layout a dedicated drag-item,
    // and then we can turn this into a format function.
    SeparatorTextEx(0, label, FindRenderedTextEnd(label), 0.0f);
}

// Using 'hover_visibility_delay' allows us to hide the highlight and mouse cursor for a short time, which can be convenient to reduce visual noise.
bool UxImGui::SplitterBehavior(const UxImRect& bb, UxImGuiID id, UxImGuiAxis axis, float* size1, float* size2, float min_size1, float min_size2, float hover_extend, float hover_visibility_delay, UxImU32 bg_col)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    if (!ItemAdd(bb, id, NULL, UxImGuiItemFlags_NoNav))
        return false;

    // FIXME: AFAIK the only leftover reason for passing UxImGuiButtonFlags_AllowOverlap here is
    // to allow caller of SplitterBehavior() to call SetItemAllowOverlap() after the item.
    // Nowadays we would instead want to use SetNextItemAllowOverlap() before the item.
    UxImGuiButtonFlags button_flags = UxImGuiButtonFlags_FlattenChildren;
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    button_flags |= UxImGuiButtonFlags_AllowOverlap;
#endif

    bool hovered, held;
    UxImRect bb_interact = bb;
    bb_interact.Expand(axis == UxImGuiAxis_Y ? UxImVec2(0.0f, hover_extend) : UxImVec2(hover_extend, 0.0f));
    ButtonBehavior(bb_interact, id, &hovered, &held, button_flags);
    if (hovered)
        g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_HoveredRect; // for IsItemHovered(), because bb_interact is larger than bb

    if (held || (hovered && g.HoveredIdPreviousFrame == id && g.HoveredIdTimer >= hover_visibility_delay))
        SetMouseCursor(axis == UxImGuiAxis_Y ? UxImGuiMouseCursor_ResizeNS : UxImGuiMouseCursor_ResizeEW);

    UxImRect bb_render = bb;
    if (held)
    {
        float mouse_delta = (g.IO.MousePos - g.ActiveIdClickOffset - bb_interact.Min)[axis];

        // Minimum pane size
        float size_1_maximum_delta = UxImMax(0.0f, *size1 - min_size1);
        float size_2_maximum_delta = UxImMax(0.0f, *size2 - min_size2);
        if (mouse_delta < -size_1_maximum_delta)
            mouse_delta = -size_1_maximum_delta;
        if (mouse_delta > size_2_maximum_delta)
            mouse_delta = size_2_maximum_delta;

        // Apply resize
        if (mouse_delta != 0.0f)
        {
            *size1 = UxImMax(*size1 + mouse_delta, min_size1);
            *size2 = UxImMax(*size2 - mouse_delta, min_size2);
            bb_render.Translate((axis == UxImGuiAxis_X) ? UxImVec2(mouse_delta, 0.0f) : UxImVec2(0.0f, mouse_delta));
            MarkItemEdited(id);
        }
    }

    // Render at new position
    if (bg_col & IM_COL32_A_MASK)
        window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, bg_col, 0.0f);
    const UxImU32 col = GetColorU32(held ? UxImGuiCol_SeparatorActive : (hovered && g.HoveredIdTimer >= hover_visibility_delay) ? UxImGuiCol_SeparatorHovered : UxImGuiCol_Separator);
    window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, col, 0.0f);

    return held;
}

static int IMGUI_CDECL ShrinkWidthItemComparer(const void* lhs, const void* rhs)
{
    const UxImGuiShrinkWidthItem* a = (const UxImGuiShrinkWidthItem*)lhs;
    const UxImGuiShrinkWidthItem* b = (const UxImGuiShrinkWidthItem*)rhs;
    if (int d = (int)(b->Width - a->Width))
        return d;
    return (b->Index - a->Index);
}

// Shrink excess width from a set of item, by removing width from the larger items first.
// Set items Width to -1.0f to disable shrinking this item.
void UxImGui::ShrinkWidths(UxImGuiShrinkWidthItem* items, int count, float width_excess)
{
    if (count == 1)
    {
        if (items[0].Width >= 0.0f)
            items[0].Width = UxImMax(items[0].Width - width_excess, 1.0f);
        return;
    }
    UxImQsort(items, (size_t)count, sizeof(UxImGuiShrinkWidthItem), ShrinkWidthItemComparer);
    int count_same_width = 1;
    while (width_excess > 0.0f && count_same_width < count)
    {
        while (count_same_width < count && items[0].Width <= items[count_same_width].Width)
            count_same_width++;
        float max_width_to_remove_per_item = (count_same_width < count && items[count_same_width].Width >= 0.0f) ? (items[0].Width - items[count_same_width].Width) : (items[0].Width - 1.0f);
        if (max_width_to_remove_per_item <= 0.0f)
            break;
        float width_to_remove_per_item = UxImMin(width_excess / count_same_width, max_width_to_remove_per_item);
        for (int item_n = 0; item_n < count_same_width; item_n++)
            items[item_n].Width -= width_to_remove_per_item;
        width_excess -= width_to_remove_per_item * count_same_width;
    }

    // Round width and redistribute remainder
    // Ensure that e.g. the right-most tab of a shrunk tab-bar always reaches exactly at the same distance from the right-most edge of the tab bar separator.
    width_excess = 0.0f;
    for (int n = 0; n < count; n++)
    {
        float width_rounded = UxImTrunc(items[n].Width);
        width_excess += items[n].Width - width_rounded;
        items[n].Width = width_rounded;
    }
    while (width_excess > 0.0f)
        for (int n = 0; n < count && width_excess > 0.0f; n++)
        {
            float width_to_add = UxImMin(items[n].InitialWidth - items[n].Width, 1.0f);
            items[n].Width += width_to_add;
            width_excess -= width_to_add;
        }
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: ComboBox
//-------------------------------------------------------------------------
// - CalcMaxPopupHeightFromItemCount() [Internal]
// - BeginCombo()
// - BeginComboPopup() [Internal]
// - EndCombo()
// - BeginComboPreview() [Internal]
// - EndComboPreview() [Internal]
// - Combo()
//-------------------------------------------------------------------------

static float CalcMaxPopupHeightFromItemCount(int items_count)
{
    UxImGuiContext& g = *GUxImGui;
    if (items_count <= 0)
        return FLT_MAX;
    return (g.FontSize + g.Style.ItemSpacing.y) * items_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2);
}

bool UxImGui::BeginCombo(const char* label, const char* preview_value, UxImGuiComboFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();

    UxImGuiNextWindowDataFlags backup_next_window_data_flags = g.NextWindowData.HasFlags;
    g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
    if (window->SkipItems)
        return false;

    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    IM_ASSERT((flags & (UxImGuiComboFlags_NoArrowButton | UxImGuiComboFlags_NoPreview)) != (UxImGuiComboFlags_NoArrowButton | UxImGuiComboFlags_NoPreview)); // Can't use both flags together
    if (flags & UxImGuiComboFlags_WidthFitPreview)
        IM_ASSERT((flags & (UxImGuiComboFlags_NoPreview | (UxImGuiComboFlags)UxImGuiComboFlags_CustomPreview)) == 0);

    const float arrow_size = (flags & UxImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);
    const float preview_width = ((flags & UxImGuiComboFlags_WidthFitPreview) && (preview_value != NULL)) ? CalcTextSize(preview_value, NULL, true).x : 0.0f;
    const float w = (flags & UxImGuiComboFlags_NoPreview) ? arrow_size : ((flags & UxImGuiComboFlags_WidthFitPreview) ? (arrow_size + preview_width + style.FramePadding.x * 2.0f) : CalcItemWidth());
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + UxImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const UxImRect total_bb(bb.Min, bb.Max + UxImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &bb))
        return false;

    // Open on click
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    const UxImGuiID popup_id = UxImHashStr("##ComboPopup", 0, id);
    bool popup_open = IsPopupOpen(popup_id, UxImGuiPopupFlags_None);
    if (pressed && !popup_open)
    {
        OpenPopupEx(popup_id, UxImGuiPopupFlags_None);
        popup_open = true;
    }

    // Render shape
    const UxImU32 frame_col = GetColorU32(hovered ? UxImGuiCol_FrameBgHovered : UxImGuiCol_FrameBg);
    const float value_x2 = UxImMax(bb.Min.x, bb.Max.x - arrow_size);
    RenderNavCursor(bb, id);
    if (!(flags & UxImGuiComboFlags_NoPreview))
        window->DrawList->AddRectFilled(bb.Min, UxImVec2(value_x2, bb.Max.y), frame_col, style.FrameRounding, (flags & UxImGuiComboFlags_NoArrowButton) ? UxImDrawFlags_RoundCornersAll : UxImDrawFlags_RoundCornersLeft);
    if (!(flags & UxImGuiComboFlags_NoArrowButton))
    {
        UxImU32 bg_col = GetColorU32((popup_open || hovered) ? UxImGuiCol_ButtonHovered : UxImGuiCol_Button);
        UxImU32 text_col = GetColorU32(UxImGuiCol_Text);
        window->DrawList->AddRectFilled(UxImVec2(value_x2, bb.Min.y), bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? UxImDrawFlags_RoundCornersAll : UxImDrawFlags_RoundCornersRight);
        if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
            RenderArrow(window->DrawList, UxImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y), text_col, UxImGuiDir_Down, 1.0f);
    }
    RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);

    // Custom preview
    if (flags & UxImGuiComboFlags_CustomPreview)
    {
        g.ComboPreviewData.PreviewRect = UxImRect(bb.Min.x, bb.Min.y, value_x2, bb.Max.y);
        IM_ASSERT(preview_value == NULL || preview_value[0] == 0);
        preview_value = NULL;
    }

    // Render preview and label
    if (preview_value != NULL && !(flags & UxImGuiComboFlags_NoPreview))
    {
        if (g.LogEnabled)
            LogSetNextTextDecoration("{", "}");
        RenderTextClipped(bb.Min + style.FramePadding, UxImVec2(value_x2, bb.Max.y), preview_value, NULL, NULL);
    }
    if (label_size.x > 0)
        RenderText(UxImVec2(bb.Max.x + style.ItemInnerSpacing.x, bb.Min.y + style.FramePadding.y), label);

    if (!popup_open)
        return false;

    g.NextWindowData.HasFlags = backup_next_window_data_flags;
    return BeginComboPopup(popup_id, bb, flags);
}

bool UxImGui::BeginComboPopup(UxImGuiID popup_id, const UxImRect& bb, UxImGuiComboFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    if (!IsPopupOpen(popup_id, UxImGuiPopupFlags_None))
    {
        g.NextWindowData.ClearFlags();
        return false;
    }

    // Set popup size
    float w = bb.GetWidth();
    if (g.NextWindowData.HasFlags & UxImGuiNextWindowDataFlags_HasSizeConstraint)
    {
        g.NextWindowData.SizeConstraintRect.Min.x = UxImMax(g.NextWindowData.SizeConstraintRect.Min.x, w);
    }
    else
    {
        if ((flags & UxImGuiComboFlags_HeightMask_) == 0)
            flags |= UxImGuiComboFlags_HeightRegular;
        IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiComboFlags_HeightMask_)); // Only one
        int popup_max_height_in_items = -1;
        if (flags & UxImGuiComboFlags_HeightRegular)     popup_max_height_in_items = 8;
        else if (flags & UxImGuiComboFlags_HeightSmall)  popup_max_height_in_items = 4;
        else if (flags & UxImGuiComboFlags_HeightLarge)  popup_max_height_in_items = 20;
        UxImVec2 constraint_min(0.0f, 0.0f), constraint_max(FLT_MAX, FLT_MAX);
        if ((g.NextWindowData.HasFlags & UxImGuiNextWindowDataFlags_HasSize) == 0 || g.NextWindowData.SizeVal.x <= 0.0f) // Don't apply constraints if user specified a size
            constraint_min.x = w;
        if ((g.NextWindowData.HasFlags & UxImGuiNextWindowDataFlags_HasSize) == 0 || g.NextWindowData.SizeVal.y <= 0.0f)
            constraint_max.y = CalcMaxPopupHeightFromItemCount(popup_max_height_in_items);
        SetNextWindowSizeConstraints(constraint_min, constraint_max);
    }

    // This is essentially a specialized version of BeginPopupEx()
    char name[16];
    UxImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginComboDepth); // Recycle windows based on depth

    // Set position given a custom constraint (peak into expected window size so we can position it)
    // FIXME: This might be easier to express with an hypothetical SetNextWindowPosConstraints() function?
    // FIXME: This might be moved to Begin() or at least around the same spot where Tooltips and other Popups are calling FindBestWindowPosForPopupEx()?
    if (UxImGuiWindow* popup_window = FindWindowByName(name))
        if (popup_window->WasActive)
        {
            // Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
            UxImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
            popup_window->AutoPosLastDirection = (flags & UxImGuiComboFlags_PopupAlignLeft) ? UxImGuiDir_Left : UxImGuiDir_Down; // Left = "Below, Toward Left", Down = "Below, Toward Right (default)"
            UxImRect r_outer = GetPopupAllowedExtentRect(popup_window);
            UxImVec2 pos = FindBestWindowPosForPopupEx(bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, bb, UxImGuiPopupPositionPolicy_ComboBox);
            SetNextWindowPos(pos);
        }

    // We don't use BeginPopupEx() solely because we have a custom name string, which we could make an argument to BeginPopupEx()
    UxImGuiWindowFlags window_flags = UxImGuiWindowFlags_AlwaysAutoResize | UxImGuiWindowFlags_Popup | UxImGuiWindowFlags_NoTitleBar | UxImGuiWindowFlags_NoResize | UxImGuiWindowFlags_NoSavedSettings | UxImGuiWindowFlags_NoMove;
    PushStyleVarX(UxImGuiStyleVar_WindowPadding, g.Style.FramePadding.x); // Horizontally align ourselves with the framed text
    bool ret = Begin(name, NULL, window_flags);
    PopStyleVar();
    if (!ret)
    {
        EndPopup();
        IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
        return false;
    }
    g.BeginComboDepth++;
    return true;
}

void UxImGui::EndCombo()
{
    UxImGuiContext& g = *GUxImGui;
    EndPopup();
    g.BeginComboDepth--;
}

// Call directly after the BeginCombo/EndCombo block. The preview is designed to only host non-interactive elements
// (Experimental, see GitHub issues: #1658, #4168)
bool UxImGui::BeginComboPreview()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    UxImGuiComboPreviewData* preview_data = &g.ComboPreviewData;

    if (window->SkipItems || !(g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_Visible))
        return false;
    IM_ASSERT(g.LastItemData.Rect.Min.x == preview_data->PreviewRect.Min.x && g.LastItemData.Rect.Min.y == preview_data->PreviewRect.Min.y); // Didn't call after BeginCombo/EndCombo block or forgot to pass UxImGuiComboFlags_CustomPreview flag?
    if (!window->ClipRect.Overlaps(preview_data->PreviewRect)) // Narrower test (optional)
        return false;

    // FIXME: This could be contained in a PushWorkRect() api
    preview_data->BackupCursorPos = window->DC.CursorPos;
    preview_data->BackupCursorMaxPos = window->DC.CursorMaxPos;
    preview_data->BackupCursorPosPrevLine = window->DC.CursorPosPrevLine;
    preview_data->BackupPrevLineTextBaseOffset = window->DC.PrevLineTextBaseOffset;
    preview_data->BackupLayout = window->DC.LayoutType;
    window->DC.CursorPos = preview_data->PreviewRect.Min + g.Style.FramePadding;
    window->DC.CursorMaxPos = window->DC.CursorPos;
    window->DC.LayoutType = UxImGuiLayoutType_Horizontal;
    window->DC.IsSameLine = false;
    PushClipRect(preview_data->PreviewRect.Min, preview_data->PreviewRect.Max, true);

    return true;
}

void UxImGui::EndComboPreview()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    UxImGuiComboPreviewData* preview_data = &g.ComboPreviewData;

    // FIXME: Using CursorMaxPos approximation instead of correct AABB which we will store in UxImDrawCmd in the future
    UxImDrawList* draw_list = window->DrawList;
    if (window->DC.CursorMaxPos.x < preview_data->PreviewRect.Max.x && window->DC.CursorMaxPos.y < preview_data->PreviewRect.Max.y)
        if (draw_list->CmdBuffer.Size > 1) // Unlikely case that the PushClipRect() didn't create a command
        {
            draw_list->_CmdHeader.ClipRect = draw_list->CmdBuffer[draw_list->CmdBuffer.Size - 1].ClipRect = draw_list->CmdBuffer[draw_list->CmdBuffer.Size - 2].ClipRect;
            draw_list->_TryMergeDrawCmds();
        }
    PopClipRect();
    window->DC.CursorPos = preview_data->BackupCursorPos;
    window->DC.CursorMaxPos = UxImMax(window->DC.CursorMaxPos, preview_data->BackupCursorMaxPos);
    window->DC.CursorPosPrevLine = preview_data->BackupCursorPosPrevLine;
    window->DC.PrevLineTextBaseOffset = preview_data->BackupPrevLineTextBaseOffset;
    window->DC.LayoutType = preview_data->BackupLayout;
    window->DC.IsSameLine = false;
    preview_data->PreviewRect = UxImRect();
}

// Getter for the old Combo() API: const char*[]
static const char* Items_ArrayGetter(void* data, int idx)
{
    const char* const* items = (const char* const*)data;
    return items[idx];
}

// Getter for the old Combo() API: "item1\0item2\0item3\0"
static const char* Items_SingleStringGetter(void* data, int idx)
{
    const char* items_separated_by_zeros = (const char*)data;
    int items_count = 0;
    const char* p = items_separated_by_zeros;
    while (*p)
    {
        if (idx == items_count)
            break;
        p += UxImStrlen(p) + 1;
        items_count++;
    }
    return *p ? p : NULL;
}

// Old API, prefer using BeginCombo() nowadays if you can.
bool UxImGui::Combo(const char* label, int* current_item, const char* (*getter)(void* user_data, int idx), void* user_data, int items_count, int popup_max_height_in_items)
{
    UxImGuiContext& g = *GUxImGui;

    // Call the getter to obtain the preview string which is a parameter to BeginCombo()
    const char* preview_value = NULL;
    if (*current_item >= 0 && *current_item < items_count)
        preview_value = getter(user_data, *current_item);

    // The old Combo() API exposed "popup_max_height_in_items". The new more general BeginCombo() API doesn't have/need it, but we emulate it here.
    if (popup_max_height_in_items != -1 && !(g.NextWindowData.HasFlags & UxImGuiNextWindowDataFlags_HasSizeConstraint))
        SetNextWindowSizeConstraints(UxImVec2(0, 0), UxImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items)));

    if (!BeginCombo(label, preview_value, UxImGuiComboFlags_None))
        return false;

    // Display items
    bool value_changed = false;
    UxImGuiListClipper clipper;
    clipper.Begin(items_count);
    clipper.IncludeItemByIndex(*current_item);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const char* item_text = getter(user_data, i);
            if (item_text == NULL)
                item_text = "*Unknown item*";

            PushID(i);
            const bool item_selected = (i == *current_item);
            if (Selectable(item_text, item_selected) && *current_item != i)
            {
                value_changed = true;
                *current_item = i;
            }
            if (item_selected)
                SetItemDefaultFocus();
            PopID();
        }

    EndCombo();
    if (value_changed)
        MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

// Combo box helper allowing to pass an array of strings.
bool UxImGui::Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items)
{
    const bool value_changed = Combo(label, current_item, Items_ArrayGetter, (void*)items, items_count, height_in_items);
    return value_changed;
}

// Combo box helper allowing to pass all items in a single string literal holding multiple zero-terminated items "item1\0item2\0"
bool UxImGui::Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int height_in_items)
{
    int items_count = 0;
    const char* p = items_separated_by_zeros;       // FIXME-OPT: Avoid computing this, or at least only when combo is open
    while (*p)
    {
        p += UxImStrlen(p) + 1;
        items_count++;
    }
    bool value_changed = Combo(label, current_item, Items_SingleStringGetter, (void*)items_separated_by_zeros, items_count, height_in_items);
    return value_changed;
}

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS

struct UxImGuiGetNameFromIndexOldToNewCallbackData { void* UserData; bool (*OldCallback)(void*, int, const char**); };
static const char* UxImGuiGetNameFromIndexOldToNewCallback(void* user_data, int idx)
{
    UxImGuiGetNameFromIndexOldToNewCallbackData* data = (UxImGuiGetNameFromIndexOldToNewCallbackData*)user_data;
    const char* s = NULL;
    data->OldCallback(data->UserData, idx, &s);
    return s;
}

bool UxImGui::ListBox(const char* label, int* current_item, bool (*old_getter)(void*, int, const char**), void* user_data, int items_count, int height_in_items)
{
    UxImGuiGetNameFromIndexOldToNewCallbackData old_to_new_data = { user_data, old_getter };
    return ListBox(label, current_item, UxImGuiGetNameFromIndexOldToNewCallback, &old_to_new_data, items_count, height_in_items);
}
bool UxImGui::Combo(const char* label, int* current_item, bool (*old_getter)(void*, int, const char**), void* user_data, int items_count, int popup_max_height_in_items)
{
    UxImGuiGetNameFromIndexOldToNewCallbackData old_to_new_data = { user_data, old_getter };
    return Combo(label, current_item, UxImGuiGetNameFromIndexOldToNewCallback, &old_to_new_data, items_count, popup_max_height_in_items);
}

#endif

//-------------------------------------------------------------------------
// [SECTION] Data Type and Data Formatting Helpers [Internal]
//-------------------------------------------------------------------------
// - DataTypeGetInfo()
// - DataTypeFormatString()
// - DataTypeApplyOp()
// - DataTypeApplyFromText()
// - DataTypeCompare()
// - DataTypeClamp()
// - GetMinimumStepAtDecimalPrecision
// - RoundScalarWithFormat<>()
//-------------------------------------------------------------------------

static const UxImGuiDataTypeInfo GDataTypeInfo[] =
{
    { sizeof(char),             "S8",   "%d",   "%d"    },  // UxImGuiDataType_S8
    { sizeof(unsigned char),    "U8",   "%u",   "%u"    },
    { sizeof(short),            "S16",  "%d",   "%d"    },  // UxImGuiDataType_S16
    { sizeof(unsigned short),   "U16",  "%u",   "%u"    },
    { sizeof(int),              "S32",  "%d",   "%d"    },  // UxImGuiDataType_S32
    { sizeof(unsigned int),     "U32",  "%u",   "%u"    },
#ifdef _MSC_VER
    { sizeof(UxImS64),            "S64",  "%I64d","%I64d" },  // UxImGuiDataType_S64
    { sizeof(UxImU64),            "U64",  "%I64u","%I64u" },
#else
    { sizeof(UxImS64),            "S64",  "%lld", "%lld"  },  // UxImGuiDataType_S64
    { sizeof(UxImU64),            "U64",  "%llu", "%llu"  },
#endif
    { sizeof(float),            "float", "%.3f","%f"    },  // UxImGuiDataType_Float (float are promoted to double in va_arg)
    { sizeof(double),           "double","%f",  "%lf"   },  // UxImGuiDataType_Double
    { sizeof(bool),             "bool", "%d",   "%d"    },  // UxImGuiDataType_Bool
    { 0,                        "char*","%s",   "%s"    },  // UxImGuiDataType_String
};
IM_STATIC_ASSERT(IM_ARRAYSIZE(GDataTypeInfo) == UxImGuiDataType_COUNT);

const UxImGuiDataTypeInfo* UxImGui::DataTypeGetInfo(UxImGuiDataType data_type)
{
    IM_ASSERT(data_type >= 0 && data_type < UxImGuiDataType_COUNT);
    return &GDataTypeInfo[data_type];
}

int UxImGui::DataTypeFormatString(char* buf, int buf_size, UxImGuiDataType data_type, const void* p_data, const char* format)
{
    // Signedness doesn't matter when pushing integer arguments
    if (data_type == UxImGuiDataType_S32 || data_type == UxImGuiDataType_U32)
        return UxImFormatString(buf, buf_size, format, *(const UxImU32*)p_data);
    if (data_type == UxImGuiDataType_S64 || data_type == UxImGuiDataType_U64)
        return UxImFormatString(buf, buf_size, format, *(const UxImU64*)p_data);
    if (data_type == UxImGuiDataType_Float)
        return UxImFormatString(buf, buf_size, format, *(const float*)p_data);
    if (data_type == UxImGuiDataType_Double)
        return UxImFormatString(buf, buf_size, format, *(const double*)p_data);
    if (data_type == UxImGuiDataType_S8)
        return UxImFormatString(buf, buf_size, format, *(const UxImS8*)p_data);
    if (data_type == UxImGuiDataType_U8)
        return UxImFormatString(buf, buf_size, format, *(const UxImU8*)p_data);
    if (data_type == UxImGuiDataType_S16)
        return UxImFormatString(buf, buf_size, format, *(const UxImS16*)p_data);
    if (data_type == UxImGuiDataType_U16)
        return UxImFormatString(buf, buf_size, format, *(const UxImU16*)p_data);
    IM_ASSERT(0);
    return 0;
}

void UxImGui::DataTypeApplyOp(UxImGuiDataType data_type, int op, void* output, const void* arg1, const void* arg2)
{
    IM_ASSERT(op == '+' || op == '-');
    switch (data_type)
    {
        case UxImGuiDataType_S8:
            if (op == '+') { *(UxImS8*)output  = UxImAddClampOverflow(*(const UxImS8*)arg1,  *(const UxImS8*)arg2,  IM_S8_MIN,  IM_S8_MAX); }
            if (op == '-') { *(UxImS8*)output  = UxImSubClampOverflow(*(const UxImS8*)arg1,  *(const UxImS8*)arg2,  IM_S8_MIN,  IM_S8_MAX); }
            return;
        case UxImGuiDataType_U8:
            if (op == '+') { *(UxImU8*)output  = UxImAddClampOverflow(*(const UxImU8*)arg1,  *(const UxImU8*)arg2,  IM_U8_MIN,  IM_U8_MAX); }
            if (op == '-') { *(UxImU8*)output  = UxImSubClampOverflow(*(const UxImU8*)arg1,  *(const UxImU8*)arg2,  IM_U8_MIN,  IM_U8_MAX); }
            return;
        case UxImGuiDataType_S16:
            if (op == '+') { *(UxImS16*)output = UxImAddClampOverflow(*(const UxImS16*)arg1, *(const UxImS16*)arg2, IM_S16_MIN, IM_S16_MAX); }
            if (op == '-') { *(UxImS16*)output = UxImSubClampOverflow(*(const UxImS16*)arg1, *(const UxImS16*)arg2, IM_S16_MIN, IM_S16_MAX); }
            return;
        case UxImGuiDataType_U16:
            if (op == '+') { *(UxImU16*)output = UxImAddClampOverflow(*(const UxImU16*)arg1, *(const UxImU16*)arg2, IM_U16_MIN, IM_U16_MAX); }
            if (op == '-') { *(UxImU16*)output = UxImSubClampOverflow(*(const UxImU16*)arg1, *(const UxImU16*)arg2, IM_U16_MIN, IM_U16_MAX); }
            return;
        case UxImGuiDataType_S32:
            if (op == '+') { *(UxImS32*)output = UxImAddClampOverflow(*(const UxImS32*)arg1, *(const UxImS32*)arg2, IM_S32_MIN, IM_S32_MAX); }
            if (op == '-') { *(UxImS32*)output = UxImSubClampOverflow(*(const UxImS32*)arg1, *(const UxImS32*)arg2, IM_S32_MIN, IM_S32_MAX); }
            return;
        case UxImGuiDataType_U32:
            if (op == '+') { *(UxImU32*)output = UxImAddClampOverflow(*(const UxImU32*)arg1, *(const UxImU32*)arg2, IM_U32_MIN, IM_U32_MAX); }
            if (op == '-') { *(UxImU32*)output = UxImSubClampOverflow(*(const UxImU32*)arg1, *(const UxImU32*)arg2, IM_U32_MIN, IM_U32_MAX); }
            return;
        case UxImGuiDataType_S64:
            if (op == '+') { *(UxImS64*)output = UxImAddClampOverflow(*(const UxImS64*)arg1, *(const UxImS64*)arg2, IM_S64_MIN, IM_S64_MAX); }
            if (op == '-') { *(UxImS64*)output = UxImSubClampOverflow(*(const UxImS64*)arg1, *(const UxImS64*)arg2, IM_S64_MIN, IM_S64_MAX); }
            return;
        case UxImGuiDataType_U64:
            if (op == '+') { *(UxImU64*)output = UxImAddClampOverflow(*(const UxImU64*)arg1, *(const UxImU64*)arg2, IM_U64_MIN, IM_U64_MAX); }
            if (op == '-') { *(UxImU64*)output = UxImSubClampOverflow(*(const UxImU64*)arg1, *(const UxImU64*)arg2, IM_U64_MIN, IM_U64_MAX); }
            return;
        case UxImGuiDataType_Float:
            if (op == '+') { *(float*)output = *(const float*)arg1 + *(const float*)arg2; }
            if (op == '-') { *(float*)output = *(const float*)arg1 - *(const float*)arg2; }
            return;
        case UxImGuiDataType_Double:
            if (op == '+') { *(double*)output = *(const double*)arg1 + *(const double*)arg2; }
            if (op == '-') { *(double*)output = *(const double*)arg1 - *(const double*)arg2; }
            return;
        case UxImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
}

// User can input math operators (e.g. +100) to edit a numerical values.
// NB: This is _not_ a full expression evaluator. We should probably add one and replace this dumb mess..
bool UxImGui::DataTypeApplyFromText(const char* buf, UxImGuiDataType data_type, void* p_data, const char* format, void* p_data_when_empty)
{
    // Copy the value in an opaque buffer so we can compare at the end of the function if it changed at all.
    const UxImGuiDataTypeInfo* type_info = DataTypeGetInfo(data_type);
    UxImGuiDataTypeStorage data_backup;
    memcpy(&data_backup, p_data, type_info->Size);

    while (UxImCharIsBlankA(*buf))
        buf++;
    if (!buf[0])
    {
        if (p_data_when_empty != NULL)
        {
            memcpy(p_data, p_data_when_empty, type_info->Size);
            return memcmp(&data_backup, p_data, type_info->Size) != 0;
        }
        return false;
    }

    // Sanitize format
    // - For float/double we have to ignore format with precision (e.g. "%.2f") because sscanf doesn't take them in, so force them into %f and %lf
    // - In theory could treat empty format as using default, but this would only cover rare/bizarre case of using InputScalar() + integer + format string without %.
    char format_sanitized[32];
    if (data_type == UxImGuiDataType_Float || data_type == UxImGuiDataType_Double)
        format = type_info->ScanFmt;
    else
        format = UxImParseFormatSanitizeForScanning(format, format_sanitized, IM_ARRAYSIZE(format_sanitized));

    // Small types need a 32-bit buffer to receive the result from scanf()
    int v32 = 0;
    if (sscanf(buf, format, type_info->Size >= 4 ? p_data : &v32) < 1)
        return false;
    if (type_info->Size < 4)
    {
        if (data_type == UxImGuiDataType_S8)
            *(UxImS8*)p_data = (UxImS8)UxImClamp(v32, (int)IM_S8_MIN, (int)IM_S8_MAX);
        else if (data_type == UxImGuiDataType_U8)
            *(UxImU8*)p_data = (UxImU8)UxImClamp(v32, (int)IM_U8_MIN, (int)IM_U8_MAX);
        else if (data_type == UxImGuiDataType_S16)
            *(UxImS16*)p_data = (UxImS16)UxImClamp(v32, (int)IM_S16_MIN, (int)IM_S16_MAX);
        else if (data_type == UxImGuiDataType_U16)
            *(UxImU16*)p_data = (UxImU16)UxImClamp(v32, (int)IM_U16_MIN, (int)IM_U16_MAX);
        else
            IM_ASSERT(0);
    }

    return memcmp(&data_backup, p_data, type_info->Size) != 0;
}

template<typename T>
static int DataTypeCompareT(const T* lhs, const T* rhs)
{
    if (*lhs < *rhs) return -1;
    if (*lhs > *rhs) return +1;
    return 0;
}

int UxImGui::DataTypeCompare(UxImGuiDataType data_type, const void* arg_1, const void* arg_2)
{
    switch (data_type)
    {
    case UxImGuiDataType_S8:     return DataTypeCompareT<UxImS8  >((const UxImS8*  )arg_1, (const UxImS8*  )arg_2);
    case UxImGuiDataType_U8:     return DataTypeCompareT<UxImU8  >((const UxImU8*  )arg_1, (const UxImU8*  )arg_2);
    case UxImGuiDataType_S16:    return DataTypeCompareT<UxImS16 >((const UxImS16* )arg_1, (const UxImS16* )arg_2);
    case UxImGuiDataType_U16:    return DataTypeCompareT<UxImU16 >((const UxImU16* )arg_1, (const UxImU16* )arg_2);
    case UxImGuiDataType_S32:    return DataTypeCompareT<UxImS32 >((const UxImS32* )arg_1, (const UxImS32* )arg_2);
    case UxImGuiDataType_U32:    return DataTypeCompareT<UxImU32 >((const UxImU32* )arg_1, (const UxImU32* )arg_2);
    case UxImGuiDataType_S64:    return DataTypeCompareT<UxImS64 >((const UxImS64* )arg_1, (const UxImS64* )arg_2);
    case UxImGuiDataType_U64:    return DataTypeCompareT<UxImU64 >((const UxImU64* )arg_1, (const UxImU64* )arg_2);
    case UxImGuiDataType_Float:  return DataTypeCompareT<float >((const float* )arg_1, (const float* )arg_2);
    case UxImGuiDataType_Double: return DataTypeCompareT<double>((const double*)arg_1, (const double*)arg_2);
    case UxImGuiDataType_COUNT:  break;
    }
    IM_ASSERT(0);
    return 0;
}

template<typename T>
static bool DataTypeClampT(T* v, const T* v_min, const T* v_max)
{
    // Clamp, both sides are optional, return true if modified
    if (v_min && *v < *v_min) { *v = *v_min; return true; }
    if (v_max && *v > *v_max) { *v = *v_max; return true; }
    return false;
}

bool UxImGui::DataTypeClamp(UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max)
{
    switch (data_type)
    {
    case UxImGuiDataType_S8:     return DataTypeClampT<UxImS8  >((UxImS8*  )p_data, (const UxImS8*  )p_min, (const UxImS8*  )p_max);
    case UxImGuiDataType_U8:     return DataTypeClampT<UxImU8  >((UxImU8*  )p_data, (const UxImU8*  )p_min, (const UxImU8*  )p_max);
    case UxImGuiDataType_S16:    return DataTypeClampT<UxImS16 >((UxImS16* )p_data, (const UxImS16* )p_min, (const UxImS16* )p_max);
    case UxImGuiDataType_U16:    return DataTypeClampT<UxImU16 >((UxImU16* )p_data, (const UxImU16* )p_min, (const UxImU16* )p_max);
    case UxImGuiDataType_S32:    return DataTypeClampT<UxImS32 >((UxImS32* )p_data, (const UxImS32* )p_min, (const UxImS32* )p_max);
    case UxImGuiDataType_U32:    return DataTypeClampT<UxImU32 >((UxImU32* )p_data, (const UxImU32* )p_min, (const UxImU32* )p_max);
    case UxImGuiDataType_S64:    return DataTypeClampT<UxImS64 >((UxImS64* )p_data, (const UxImS64* )p_min, (const UxImS64* )p_max);
    case UxImGuiDataType_U64:    return DataTypeClampT<UxImU64 >((UxImU64* )p_data, (const UxImU64* )p_min, (const UxImU64* )p_max);
    case UxImGuiDataType_Float:  return DataTypeClampT<float >((float* )p_data, (const float* )p_min, (const float* )p_max);
    case UxImGuiDataType_Double: return DataTypeClampT<double>((double*)p_data, (const double*)p_min, (const double*)p_max);
    case UxImGuiDataType_COUNT:  break;
    }
    IM_ASSERT(0);
    return false;
}

bool UxImGui::DataTypeIsZero(UxImGuiDataType data_type, const void* p_data)
{
    UxImGuiContext& g = *GUxImGui;
    return DataTypeCompare(data_type, p_data, &g.DataTypeZeroValue) == 0;
}

static float GetMinimumStepAtDecimalPrecision(int decimal_precision)
{
    static const float min_steps[10] = { 1.0f, 0.1f, 0.01f, 0.001f, 0.0001f, 0.00001f, 0.000001f, 0.0000001f, 0.00000001f, 0.000000001f };
    if (decimal_precision < 0)
        return FLT_MIN;
    return (decimal_precision < IM_ARRAYSIZE(min_steps)) ? min_steps[decimal_precision] : UxImPow(10.0f, (float)-decimal_precision);
}

template<typename TYPE>
TYPE UxImGui::RoundScalarWithFormatT(const char* format, UxImGuiDataType data_type, TYPE v)
{
    IM_UNUSED(data_type);
    IM_ASSERT(data_type == UxImGuiDataType_Float || data_type == UxImGuiDataType_Double);
    const char* fmt_start = UxImParseFormatFindStart(format);
    if (fmt_start[0] != '%' || fmt_start[1] == '%') // Don't apply if the value is not visible in the format string
        return v;

    // Sanitize format
    char fmt_sanitized[32];
    UxImParseFormatSanitizeForPrinting(fmt_start, fmt_sanitized, IM_ARRAYSIZE(fmt_sanitized));
    fmt_start = fmt_sanitized;

    // Format value with our rounding, and read back
    char v_str[64];
    UxImFormatString(v_str, IM_ARRAYSIZE(v_str), fmt_start, v);
    const char* p = v_str;
    while (*p == ' ')
        p++;
    v = (TYPE)UxImAtof(p);

    return v;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: DragScalar, DragFloat, DragInt, etc.
//-------------------------------------------------------------------------
// - DragBehaviorT<>() [Internal]
// - DragBehavior() [Internal]
// - DragScalar()
// - DragScalarN()
// - DragFloat()
// - DragFloat2()
// - DragFloat3()
// - DragFloat4()
// - DragFloatRange2()
// - DragInt()
// - DragInt2()
// - DragInt3()
// - DragInt4()
// - DragIntRange2()
//-------------------------------------------------------------------------

// This is called by DragBehavior() when the widget is active (held by mouse or being manipulated with Nav controls)
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
bool UxImGui::DragBehaviorT(UxImGuiDataType data_type, TYPE* v, float v_speed, const TYPE v_min, const TYPE v_max, const char* format, UxImGuiSliderFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    const UxImGuiAxis axis = (flags & UxImGuiSliderFlags_Vertical) ? UxImGuiAxis_Y : UxImGuiAxis_X;
    const bool is_bounded = (v_min < v_max) || ((v_min == v_max) && (v_min != 0.0f || (flags & UxImGuiSliderFlags_ClampZeroRange)));
    const bool is_wrapped = is_bounded && (flags & UxImGuiSliderFlags_WrapAround);
    const bool is_logarithmic = (flags & UxImGuiSliderFlags_Logarithmic) != 0;
    const bool is_floating_point = (data_type == UxImGuiDataType_Float) || (data_type == UxImGuiDataType_Double);

    // Default tweak speed
    if (v_speed == 0.0f && is_bounded && (v_max - v_min < FLT_MAX))
        v_speed = (float)((v_max - v_min) * g.DragSpeedDefaultRatio);

    // Inputs accumulates into g.DragCurrentAccum, which is flushed into the current value as soon as it makes a difference with our precision settings
    float adjust_delta = 0.0f;
    if (g.ActiveIdSource == UxImGuiInputSource_Mouse && IsMousePosValid() && IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * DRAG_MOUSE_THRESHOLD_FACTOR))
    {
        adjust_delta = g.IO.MouseDelta[axis];
        if (g.IO.KeyAlt && !(flags & UxImGuiSliderFlags_NoSpeedTweaks))
            adjust_delta *= 1.0f / 100.0f;
        if (g.IO.KeyShift && !(flags & UxImGuiSliderFlags_NoSpeedTweaks))
            adjust_delta *= 10.0f;
    }
    else if (g.ActiveIdSource == UxImGuiInputSource_Keyboard || g.ActiveIdSource == UxImGuiInputSource_Gamepad)
    {
        const int decimal_precision = is_floating_point ? UxImParseFormatPrecision(format, 3) : 0;
        const bool tweak_slow = IsKeyDown((g.NavInputSource == UxImGuiInputSource_Gamepad) ? UxImGuiKey_NavGamepadTweakSlow : UxImGuiKey_NavKeyboardTweakSlow);
        const bool tweak_fast = IsKeyDown((g.NavInputSource == UxImGuiInputSource_Gamepad) ? UxImGuiKey_NavGamepadTweakFast : UxImGuiKey_NavKeyboardTweakFast);
        const float tweak_factor = (flags & UxImGuiSliderFlags_NoSpeedTweaks) ? 1.0f : tweak_slow ? 1.0f / 10.0f : tweak_fast ? 10.0f : 1.0f;
        adjust_delta = GetNavTweakPressedAmount(axis) * tweak_factor;
        v_speed = UxImMax(v_speed, GetMinimumStepAtDecimalPrecision(decimal_precision));
    }
    adjust_delta *= v_speed;

    // For vertical drag we currently assume that Up=higher value (like we do with vertical sliders). This may become a parameter.
    if (axis == UxImGuiAxis_Y)
        adjust_delta = -adjust_delta;

    // For logarithmic use our range is effectively 0..1 so scale the delta into that range
    if (is_logarithmic && (v_max - v_min < FLT_MAX) && ((v_max - v_min) > 0.000001f)) // Epsilon to avoid /0
        adjust_delta /= (float)(v_max - v_min);

    // Clear current value on activation
    // Avoid altering values and clamping when we are _already_ past the limits and heading in the same direction, so e.g. if range is 0..255, current value is 300 and we are pushing to the right side, keep the 300.
    const bool is_just_activated = g.ActiveIdIsJustActivated;
    const bool is_already_past_limits_and_pushing_outward = is_bounded && !is_wrapped && ((*v >= v_max && adjust_delta > 0.0f) || (*v <= v_min && adjust_delta < 0.0f));
    if (is_just_activated || is_already_past_limits_and_pushing_outward)
    {
        g.DragCurrentAccum = 0.0f;
        g.DragCurrentAccumDirty = false;
    }
    else if (adjust_delta != 0.0f)
    {
        g.DragCurrentAccum += adjust_delta;
        g.DragCurrentAccumDirty = true;
    }

    if (!g.DragCurrentAccumDirty)
        return false;

    TYPE v_cur = *v;
    FLOATTYPE v_old_ref_for_accum_remainder = (FLOATTYPE)0.0f;

    float logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    const float zero_deadzone_halfsize = 0.0f; // Drag widgets have no deadzone (as it doesn't make sense)
    if (is_logarithmic)
    {
        // When using logarithmic sliders, we need to clamp to avoid hitting zero, but our choice of clamp value greatly affects slider precision. We attempt to use the specified precision to estimate a good lower bound.
        const int decimal_precision = is_floating_point ? UxImParseFormatPrecision(format, 3) : 1;
        logarithmic_zero_epsilon = UxImPow(0.1f, (float)decimal_precision);

        // Convert to parametric space, apply delta, convert back
        float v_old_parametric = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_cur, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        float v_new_parametric = v_old_parametric + g.DragCurrentAccum;
        v_cur = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_new_parametric, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        v_old_ref_for_accum_remainder = v_old_parametric;
    }
    else
    {
        v_cur += (SIGNEDTYPE)g.DragCurrentAccum;
    }

    // Round to user desired precision based on format string
    if (is_floating_point && !(flags & UxImGuiSliderFlags_NoRoundToFormat))
        v_cur = RoundScalarWithFormatT<TYPE>(format, data_type, v_cur);

    // Preserve remainder after rounding has been applied. This also allow slow tweaking of values.
    g.DragCurrentAccumDirty = false;
    if (is_logarithmic)
    {
        // Convert to parametric space, apply delta, convert back
        float v_new_parametric = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_cur, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        g.DragCurrentAccum -= (float)(v_new_parametric - v_old_ref_for_accum_remainder);
    }
    else
    {
        g.DragCurrentAccum -= (float)((SIGNEDTYPE)v_cur - (SIGNEDTYPE)*v);
    }

    // Lose zero sign for float/double
    if (v_cur == (TYPE)-0)
        v_cur = (TYPE)0;

    if (*v != v_cur && is_bounded)
    {
        if (is_wrapped)
        {
            // Wrap values
            if (v_cur < v_min)
                v_cur += v_max - v_min + (is_floating_point ? 0 : 1);
            if (v_cur > v_max)
                v_cur -= v_max - v_min + (is_floating_point ? 0 : 1);
        }
        else
        {
            // Clamp values + handle overflow/wrap-around for integer types.
            if (v_cur < v_min || (v_cur > *v && adjust_delta < 0.0f && !is_floating_point))
                v_cur = v_min;
            if (v_cur > v_max || (v_cur < *v && adjust_delta > 0.0f && !is_floating_point))
                v_cur = v_max;
        }
    }

    // Apply result
    if (*v == v_cur)
        return false;
    *v = v_cur;
    return true;
}

bool UxImGui::DragBehavior(UxImGuiID id, UxImGuiDataType data_type, void* p_v, float v_speed, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags)
{
    // Read imgui.cpp "API BREAKING CHANGES" section for 1.78 if you hit this assert.
    IM_ASSERT((flags == 1 || (flags & UxImGuiSliderFlags_InvalidMask_) == 0) && "Invalid UxImGuiSliderFlags flags! Has the legacy 'float power' argument been mistakenly cast to flags? Call function with UxImGuiSliderFlags_Logarithmic flags instead.");

    UxImGuiContext& g = *GUxImGui;
    if (g.ActiveId == id)
    {
        // Those are the things we can do easily outside the DragBehaviorT<> template, saves code generation.
        if (g.ActiveIdSource == UxImGuiInputSource_Mouse && !g.IO.MouseDown[0])
            ClearActiveID();
        else if ((g.ActiveIdSource == UxImGuiInputSource_Keyboard || g.ActiveIdSource == UxImGuiInputSource_Gamepad) && g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
            ClearActiveID();
    }
    if (g.ActiveId != id)
        return false;
    if ((g.LastItemData.ItemFlags & UxImGuiItemFlags_ReadOnly) || (flags & UxImGuiSliderFlags_ReadOnly))
        return false;

    switch (data_type)
    {
    case UxImGuiDataType_S8:     { UxImS32 v32 = (UxImS32)*(UxImS8*)p_v;  bool r = DragBehaviorT<UxImS32, UxImS32, float>(UxImGuiDataType_S32, &v32, v_speed, p_min ? *(const UxImS8*) p_min : IM_S8_MIN,  p_max ? *(const UxImS8*)p_max  : IM_S8_MAX,  format, flags); if (r) *(UxImS8*)p_v = (UxImS8)v32; return r; }
    case UxImGuiDataType_U8:     { UxImU32 v32 = (UxImU32)*(UxImU8*)p_v;  bool r = DragBehaviorT<UxImU32, UxImS32, float>(UxImGuiDataType_U32, &v32, v_speed, p_min ? *(const UxImU8*) p_min : IM_U8_MIN,  p_max ? *(const UxImU8*)p_max  : IM_U8_MAX,  format, flags); if (r) *(UxImU8*)p_v = (UxImU8)v32; return r; }
    case UxImGuiDataType_S16:    { UxImS32 v32 = (UxImS32)*(UxImS16*)p_v; bool r = DragBehaviorT<UxImS32, UxImS32, float>(UxImGuiDataType_S32, &v32, v_speed, p_min ? *(const UxImS16*)p_min : IM_S16_MIN, p_max ? *(const UxImS16*)p_max : IM_S16_MAX, format, flags); if (r) *(UxImS16*)p_v = (UxImS16)v32; return r; }
    case UxImGuiDataType_U16:    { UxImU32 v32 = (UxImU32)*(UxImU16*)p_v; bool r = DragBehaviorT<UxImU32, UxImS32, float>(UxImGuiDataType_U32, &v32, v_speed, p_min ? *(const UxImU16*)p_min : IM_U16_MIN, p_max ? *(const UxImU16*)p_max : IM_U16_MAX, format, flags); if (r) *(UxImU16*)p_v = (UxImU16)v32; return r; }
    case UxImGuiDataType_S32:    return DragBehaviorT<UxImS32, UxImS32, float >(data_type, (UxImS32*)p_v,  v_speed, p_min ? *(const UxImS32* )p_min : IM_S32_MIN, p_max ? *(const UxImS32* )p_max : IM_S32_MAX, format, flags);
    case UxImGuiDataType_U32:    return DragBehaviorT<UxImU32, UxImS32, float >(data_type, (UxImU32*)p_v,  v_speed, p_min ? *(const UxImU32* )p_min : IM_U32_MIN, p_max ? *(const UxImU32* )p_max : IM_U32_MAX, format, flags);
    case UxImGuiDataType_S64:    return DragBehaviorT<UxImS64, UxImS64, double>(data_type, (UxImS64*)p_v,  v_speed, p_min ? *(const UxImS64* )p_min : IM_S64_MIN, p_max ? *(const UxImS64* )p_max : IM_S64_MAX, format, flags);
    case UxImGuiDataType_U64:    return DragBehaviorT<UxImU64, UxImS64, double>(data_type, (UxImU64*)p_v,  v_speed, p_min ? *(const UxImU64* )p_min : IM_U64_MIN, p_max ? *(const UxImU64* )p_max : IM_U64_MAX, format, flags);
    case UxImGuiDataType_Float:  return DragBehaviorT<float, float, float >(data_type, (float*)p_v,  v_speed, p_min ? *(const float* )p_min : -FLT_MAX,   p_max ? *(const float* )p_max : FLT_MAX,    format, flags);
    case UxImGuiDataType_Double: return DragBehaviorT<double,double,double>(data_type, (double*)p_v, v_speed, p_min ? *(const double*)p_min : -DBL_MAX,   p_max ? *(const double*)p_max : DBL_MAX,    format, flags);
    case UxImGuiDataType_COUNT:  break;
    }
    IM_ASSERT(0);
    return false;
}

// Note: p_data, p_min and p_max are _pointers_ to a memory address holding the data. For a Drag widget, p_min and p_max are optional.
// Read code of e.g. DragFloat(), DragInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool UxImGui::DragScalar(const char* label, UxImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const UxImVec2 label_size = CalcTextSize(label, NULL, true);
    const UxImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + UxImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const UxImRect total_bb(frame_bb.Min, frame_bb.Max + UxImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const bool temp_input_allowed = (flags & UxImGuiSliderFlags_NoInput) == 0;
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? UxImGuiItemFlags_Inputable : 0))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        // Tabbing or CTRL+click on Drag turns it into an InputText
        const bool clicked = hovered && IsMouseClicked(0, UxImGuiInputFlags_None, id);
        const bool double_clicked = (hovered && g.IO.MouseClickedCount[0] == 2 && TestKeyOwner(UxImGuiKey_MouseLeft, id));
        const bool make_active = (clicked || double_clicked || g.NavActivateId == id);
        if (make_active && (clicked || double_clicked))
            SetKeyOwner(UxImGuiKey_MouseLeft, id);
        if (make_active && temp_input_allowed)
            if ((clicked && g.IO.KeyCtrl) || double_clicked || (g.NavActivateId == id && (g.NavActivateFlags & UxImGuiActivateFlags_PreferInput)))
                temp_input_is_active = true;

        // (Optional) simple click (without moving) turns Drag into an InputText
        if (g.IO.ConfigDragClickToInputText && temp_input_allowed && !temp_input_is_active)
            if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * DRAG_MOUSE_THRESHOLD_FACTOR))
            {
                g.NavActivateId = id;
                g.NavActivateFlags = UxImGuiActivateFlags_PreferInput;
                temp_input_is_active = true;
            }

        // Store initial value (not used by main lib but available as a convenience but some mods e.g. to revert)
        if (make_active)
            memcpy(&g.ActiveIdValueOnActivation, p_data, DataTypeGetInfo(data_type)->Size);

        if (make_active && !temp_input_is_active)
        {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask = (1 << UxImGuiDir_Left) | (1 << UxImGuiDir_Right);
        }
    }

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when UxImGuiSliderFlags_ClampOnInput is set (generally via UxImGuiSliderFlags_AlwaysClamp)
        bool clamp_enabled = false;
        if ((flags & UxImGuiSliderFlags_ClampOnInput) && (p_min != NULL || p_max != NULL))
        {
            const int clamp_range_dir = (p_min != NULL && p_max != NULL) ? DataTypeCompare(data_type, p_min, p_max) : 0; // -1 when *p_min < *p_max, == 0 when *p_min == *p_max
            if (p_min == NULL || p_max == NULL || clamp_range_dir < 0)
                clamp_enabled = true;
            else if (clamp_range_dir == 0)
                clamp_enabled = DataTypeIsZero(data_type, p_min) ? ((flags & UxImGuiSliderFlags_ClampZeroRange) != 0) : true;
        }
        return TempInputScalar(frame_bb, id, label, data_type, p_data, format, clamp_enabled ? p_min : NULL, clamp_enabled ? p_max : NULL);
    }

    // Draw frame
    const UxImU32 frame_col = GetColorU32(g.ActiveId == id ? UxImGuiCol_FrameBgActive : hovered ? UxImGuiCol_FrameBgHovered : UxImGuiCol_FrameBg);
    RenderNavCursor(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

    // Drag behavior
    const bool value_changed = DragBehavior(id, data_type, p_data, v_speed, p_min, p_max, format, flags);
    if (value_changed)
        MarkItemEdited(id);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    if (g.LogEnabled)
        LogSetNextTextDecoration("{", "}");
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, UxImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(UxImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? UxImGuiItemStatusFlags_Inputable : 0));
    return value_changed;
}

bool UxImGui::DragScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= DragScalar("", data_type, p_data, v_speed, p_min, p_max, format, flags);
        PopID();
        PopItemWidth();
        p_data = (void*)((char*)p_data + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool UxImGui::DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalar(label, UxImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags);
}

bool UxImGui::DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalarN(label, UxImGuiDataType_Float, v, 2, v_speed, &v_min, &v_max, format, flags);
}

bool UxImGui::DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalarN(label, UxImGuiDataType_Float, v, 3, v_speed, &v_min, &v_max, format, flags);
}

bool UxImGui::DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalarN(label, UxImGuiDataType_Float, v, 4, v_speed, &v_min, &v_max, format, flags);
}

// NB: You likely want to specify the UxImGuiSliderFlags_AlwaysClamp when using this.
bool UxImGui::DragFloatRange2(const char* label, float* v_current_min, float* v_current_max, float v_speed, float v_min, float v_max, const char* format, const char* format_max, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    PushID(label);
    BeginGroup();
    PushMultiItemsWidths(2, CalcItemWidth());

    float min_min = (v_min >= v_max) ? -FLT_MAX : v_min;
    float min_max = (v_min >= v_max) ? *v_current_max : UxImMin(v_max, *v_current_max);
    UxImGuiSliderFlags min_flags = flags | ((min_min == min_max) ? UxImGuiSliderFlags_ReadOnly : 0);
    bool value_changed = DragScalar("##min", UxImGuiDataType_Float, v_current_min, v_speed, &min_min, &min_max, format, min_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    float max_min = (v_min >= v_max) ? *v_current_min : UxImMax(v_min, *v_current_min);
    float max_max = (v_min >= v_max) ? FLT_MAX : v_max;
    UxImGuiSliderFlags max_flags = flags | ((max_min == max_max) ? UxImGuiSliderFlags_ReadOnly : 0);
    value_changed |= DragScalar("##max", UxImGuiDataType_Float, v_current_max, v_speed, &max_min, &max_max, format_max ? format_max : format, max_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    TextEx(label, FindRenderedTextEnd(label));
    EndGroup();
    PopID();

    return value_changed;
}

// NB: v_speed is float to allow adjusting the drag speed with more precision
bool UxImGui::DragInt(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalar(label, UxImGuiDataType_S32, v, v_speed, &v_min, &v_max, format, flags);
}

bool UxImGui::DragInt2(const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalarN(label, UxImGuiDataType_S32, v, 2, v_speed, &v_min, &v_max, format, flags);
}

bool UxImGui::DragInt3(const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalarN(label, UxImGuiDataType_S32, v, 3, v_speed, &v_min, &v_max, format, flags);
}

bool UxImGui::DragInt4(const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return DragScalarN(label, UxImGuiDataType_S32, v, 4, v_speed, &v_min, &v_max, format, flags);
}

// NB: You likely want to specify the UxImGuiSliderFlags_AlwaysClamp when using this.
bool UxImGui::DragIntRange2(const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min, int v_max, const char* format, const char* format_max, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    PushID(label);
    BeginGroup();
    PushMultiItemsWidths(2, CalcItemWidth());

    int min_min = (v_min >= v_max) ? INT_MIN : v_min;
    int min_max = (v_min >= v_max) ? *v_current_max : UxImMin(v_max, *v_current_max);
    UxImGuiSliderFlags min_flags = flags | ((min_min == min_max) ? UxImGuiSliderFlags_ReadOnly : 0);
    bool value_changed = DragInt("##min", v_current_min, v_speed, min_min, min_max, format, min_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    int max_min = (v_min >= v_max) ? *v_current_min : UxImMax(v_min, *v_current_min);
    int max_max = (v_min >= v_max) ? INT_MAX : v_max;
    UxImGuiSliderFlags max_flags = flags | ((max_min == max_max) ? UxImGuiSliderFlags_ReadOnly : 0);
    value_changed |= DragInt("##max", v_current_max, v_speed, max_min, max_max, format_max ? format_max : format, max_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    TextEx(label, FindRenderedTextEnd(label));
    EndGroup();
    PopID();

    return value_changed;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: SliderScalar, SliderFloat, SliderInt, etc.
//-------------------------------------------------------------------------
// - ScaleRatioFromValueT<> [Internal]
// - ScaleValueFromRatioT<> [Internal]
// - SliderBehaviorT<>() [Internal]
// - SliderBehavior() [Internal]
// - SliderScalar()
// - SliderScalarN()
// - SliderFloat()
// - SliderFloat2()
// - SliderFloat3()
// - SliderFloat4()
// - SliderAngle()
// - SliderInt()
// - SliderInt2()
// - SliderInt3()
// - SliderInt4()
// - VSliderScalar()
// - VSliderFloat()
// - VSliderInt()
//-------------------------------------------------------------------------

// Convert a value v in the output space of a slider into a parametric position on the slider itself (the logical opposite of ScaleValueFromRatioT)
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
float UxImGui::ScaleRatioFromValueT(UxImGuiDataType data_type, TYPE v, TYPE v_min, TYPE v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_halfsize)
{
    if (v_min == v_max)
        return 0.0f;
    IM_UNUSED(data_type);

    const TYPE v_clamped = (v_min < v_max) ? UxImClamp(v, v_min, v_max) : UxImClamp(v, v_max, v_min);
    if (is_logarithmic)
    {
        bool flipped = v_max < v_min;

        if (flipped) // Handle the case where the range is backwards
            UxImSwap(v_min, v_max);

        // Fudge min/max to avoid getting close to log(0)
        FLOATTYPE v_min_fudged = (UxImAbs((FLOATTYPE)v_min) < logarithmic_zero_epsilon) ? ((v_min < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_min;
        FLOATTYPE v_max_fudged = (UxImAbs((FLOATTYPE)v_max) < logarithmic_zero_epsilon) ? ((v_max < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_max;

        // Awkward special cases - we need ranges of the form (-100 .. 0) to convert to (-100 .. -epsilon), not (-100 .. epsilon)
        if ((v_min == 0.0f) && (v_max < 0.0f))
            v_min_fudged = -logarithmic_zero_epsilon;
        else if ((v_max == 0.0f) && (v_min < 0.0f))
            v_max_fudged = -logarithmic_zero_epsilon;

        float result;
        if (v_clamped <= v_min_fudged)
            result = 0.0f; // Workaround for values that are in-range but below our fudge
        else if (v_clamped >= v_max_fudged)
            result = 1.0f; // Workaround for values that are in-range but above our fudge
        else if ((v_min * v_max) < 0.0f) // Range crosses zero, so split into two portions
        {
            float zero_point_center = (-(float)v_min) / ((float)v_max - (float)v_min); // The zero point in parametric space.  There's an argument we should take the logarithmic nature into account when calculating this, but for now this should do (and the most common case of a symmetrical range works fine)
            float zero_point_snap_L = zero_point_center - zero_deadzone_halfsize;
            float zero_point_snap_R = zero_point_center + zero_deadzone_halfsize;
            if (v == 0.0f)
                result = zero_point_center; // Special case for exactly zero
            else if (v < 0.0f)
                result = (1.0f - (float)(UxImLog(-(FLOATTYPE)v_clamped / logarithmic_zero_epsilon) / UxImLog(-v_min_fudged / logarithmic_zero_epsilon))) * zero_point_snap_L;
            else
                result = zero_point_snap_R + ((float)(UxImLog((FLOATTYPE)v_clamped / logarithmic_zero_epsilon) / UxImLog(v_max_fudged / logarithmic_zero_epsilon)) * (1.0f - zero_point_snap_R));
        }
        else if ((v_min < 0.0f) || (v_max < 0.0f)) // Entirely negative slider
            result = 1.0f - (float)(UxImLog(-(FLOATTYPE)v_clamped / -v_max_fudged) / UxImLog(-v_min_fudged / -v_max_fudged));
        else
            result = (float)(UxImLog((FLOATTYPE)v_clamped / v_min_fudged) / UxImLog(v_max_fudged / v_min_fudged));

        return flipped ? (1.0f - result) : result;
    }
    else
    {
        // Linear slider
        return (float)((FLOATTYPE)(SIGNEDTYPE)(v_clamped - v_min) / (FLOATTYPE)(SIGNEDTYPE)(v_max - v_min));
    }
}

// Convert a parametric position on a slider into a value v in the output space (the logical opposite of ScaleRatioFromValueT)
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
TYPE UxImGui::ScaleValueFromRatioT(UxImGuiDataType data_type, float t, TYPE v_min, TYPE v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_halfsize)
{
    // We special-case the extents because otherwise our logarithmic fudging can lead to "mathematically correct"
    // but non-intuitive behaviors like a fully-left slider not actually reaching the minimum value. Also generally simpler.
    if (t <= 0.0f || v_min == v_max)
        return v_min;
    if (t >= 1.0f)
        return v_max;

    TYPE result = (TYPE)0;
    if (is_logarithmic)
    {
        // Fudge min/max to avoid getting silly results close to zero
        FLOATTYPE v_min_fudged = (UxImAbs((FLOATTYPE)v_min) < logarithmic_zero_epsilon) ? ((v_min < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_min;
        FLOATTYPE v_max_fudged = (UxImAbs((FLOATTYPE)v_max) < logarithmic_zero_epsilon) ? ((v_max < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_max;

        const bool flipped = v_max < v_min; // Check if range is "backwards"
        if (flipped)
            UxImSwap(v_min_fudged, v_max_fudged);

        // Awkward special case - we need ranges of the form (-100 .. 0) to convert to (-100 .. -epsilon), not (-100 .. epsilon)
        if ((v_max == 0.0f) && (v_min < 0.0f))
            v_max_fudged = -logarithmic_zero_epsilon;

        float t_with_flip = flipped ? (1.0f - t) : t; // t, but flipped if necessary to account for us flipping the range

        if ((v_min * v_max) < 0.0f) // Range crosses zero, so we have to do this in two parts
        {
            float zero_point_center = (-(float)UxImMin(v_min, v_max)) / UxImAbs((float)v_max - (float)v_min); // The zero point in parametric space
            float zero_point_snap_L = zero_point_center - zero_deadzone_halfsize;
            float zero_point_snap_R = zero_point_center + zero_deadzone_halfsize;
            if (t_with_flip >= zero_point_snap_L && t_with_flip <= zero_point_snap_R)
                result = (TYPE)0.0f; // Special case to make getting exactly zero possible (the epsilon prevents it otherwise)
            else if (t_with_flip < zero_point_center)
                result = (TYPE)-(logarithmic_zero_epsilon * UxImPow(-v_min_fudged / logarithmic_zero_epsilon, (FLOATTYPE)(1.0f - (t_with_flip / zero_point_snap_L))));
            else
                result = (TYPE)(logarithmic_zero_epsilon * UxImPow(v_max_fudged / logarithmic_zero_epsilon, (FLOATTYPE)((t_with_flip - zero_point_snap_R) / (1.0f - zero_point_snap_R))));
        }
        else if ((v_min < 0.0f) || (v_max < 0.0f)) // Entirely negative slider
            result = (TYPE)-(-v_max_fudged * UxImPow(-v_min_fudged / -v_max_fudged, (FLOATTYPE)(1.0f - t_with_flip)));
        else
            result = (TYPE)(v_min_fudged * UxImPow(v_max_fudged / v_min_fudged, (FLOATTYPE)t_with_flip));
    }
    else
    {
        // Linear slider
        const bool is_floating_point = (data_type == UxImGuiDataType_Float) || (data_type == UxImGuiDataType_Double);
        if (is_floating_point)
        {
            result = UxImLerp(v_min, v_max, t);
        }
        else if (t < 1.0)
        {
            // - For integer values we want the clicking position to match the grab box so we round above
            //   This code is carefully tuned to work with large values (e.g. high ranges of U64) while preserving this property..
            // - Not doing a *1.0 multiply at the end of a range as it tends to be lossy. While absolute aiming at a large s64/u64
            //   range is going to be imprecise anyway, with this check we at least make the edge values matches expected limits.
            FLOATTYPE v_new_off_f = (SIGNEDTYPE)(v_max - v_min) * t;
            result = (TYPE)((SIGNEDTYPE)v_min + (SIGNEDTYPE)(v_new_off_f + (FLOATTYPE)(v_min > v_max ? -0.5 : 0.5)));
        }
    }

    return result;
}

// FIXME: Try to move more of the code into shared SliderBehavior()
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
bool UxImGui::SliderBehaviorT(const UxImRect& bb, UxImGuiID id, UxImGuiDataType data_type, TYPE* v, const TYPE v_min, const TYPE v_max, const char* format, UxImGuiSliderFlags flags, UxImRect* out_grab_bb)
{
    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;

    const UxImGuiAxis axis = (flags & UxImGuiSliderFlags_Vertical) ? UxImGuiAxis_Y : UxImGuiAxis_X;
    const bool is_logarithmic = (flags & UxImGuiSliderFlags_Logarithmic) != 0;
    const bool is_floating_point = (data_type == UxImGuiDataType_Float) || (data_type == UxImGuiDataType_Double);
    const float v_range_f = (float)(v_min < v_max ? v_max - v_min : v_min - v_max); // We don't need high precision for what we do with it.

    // Calculate bounds
    const float grab_padding = 2.0f; // FIXME: Should be part of style.
    const float slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;
    float grab_sz = style.GrabMinSize;
    if (!is_floating_point && v_range_f >= 0.0f)                         // v_range_f < 0 may happen on integer overflows
        grab_sz = UxImMax(slider_sz / (v_range_f + 1), style.GrabMinSize); // For integer sliders: if possible have the grab size represent 1 unit
    grab_sz = UxImMin(grab_sz, slider_sz);
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    float logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    float zero_deadzone_halfsize = 0.0f; // Only valid when is_logarithmic is true
    if (is_logarithmic)
    {
        // When using logarithmic sliders, we need to clamp to avoid hitting zero, but our choice of clamp value greatly affects slider precision. We attempt to use the specified precision to estimate a good lower bound.
        const int decimal_precision = is_floating_point ? UxImParseFormatPrecision(format, 3) : 1;
        logarithmic_zero_epsilon = UxImPow(0.1f, (float)decimal_precision);
        zero_deadzone_halfsize = (style.LogSliderDeadzone * 0.5f) / UxImMax(slider_usable_sz, 1.0f);
    }

    // Process interacting with the slider
    bool value_changed = false;
    if (g.ActiveId == id)
    {
        bool set_new_value = false;
        float clicked_t = 0.0f;
        if (g.ActiveIdSource == UxImGuiInputSource_Mouse)
        {
            if (!g.IO.MouseDown[0])
            {
                ClearActiveID();
            }
            else
            {
                const float mouse_abs_pos = g.IO.MousePos[axis];
                if (g.ActiveIdIsJustActivated)
                {
                    float grab_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                    if (axis == UxImGuiAxis_Y)
                        grab_t = 1.0f - grab_t;
                    const float grab_pos = UxImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                    const bool clicked_around_grab = (mouse_abs_pos >= grab_pos - grab_sz * 0.5f - 1.0f) && (mouse_abs_pos <= grab_pos + grab_sz * 0.5f + 1.0f); // No harm being extra generous here.
                    g.SliderGrabClickOffset = (clicked_around_grab && is_floating_point) ? mouse_abs_pos - grab_pos : 0.0f;
                }
                if (slider_usable_sz > 0.0f)
                    clicked_t = UxImSaturate((mouse_abs_pos - g.SliderGrabClickOffset - slider_usable_pos_min) / slider_usable_sz);
                if (axis == UxImGuiAxis_Y)
                    clicked_t = 1.0f - clicked_t;
                set_new_value = true;
            }
        }
        else if (g.ActiveIdSource == UxImGuiInputSource_Keyboard || g.ActiveIdSource == UxImGuiInputSource_Gamepad)
        {
            if (g.ActiveIdIsJustActivated)
            {
                g.SliderCurrentAccum = 0.0f; // Reset any stored nav delta upon activation
                g.SliderCurrentAccumDirty = false;
            }

            float input_delta = (axis == UxImGuiAxis_X) ? GetNavTweakPressedAmount(axis) : -GetNavTweakPressedAmount(axis);
            if (input_delta != 0.0f)
            {
                const bool tweak_slow = IsKeyDown((g.NavInputSource == UxImGuiInputSource_Gamepad) ? UxImGuiKey_NavGamepadTweakSlow : UxImGuiKey_NavKeyboardTweakSlow);
                const bool tweak_fast = IsKeyDown((g.NavInputSource == UxImGuiInputSource_Gamepad) ? UxImGuiKey_NavGamepadTweakFast : UxImGuiKey_NavKeyboardTweakFast);
                const int decimal_precision = is_floating_point ? UxImParseFormatPrecision(format, 3) : 0;
                if (decimal_precision > 0)
                {
                    input_delta /= 100.0f; // Keyboard/Gamepad tweak speeds in % of slider bounds
                    if (tweak_slow)
                        input_delta /= 10.0f;
                }
                else
                {
                    if ((v_range_f >= -100.0f && v_range_f <= 100.0f && v_range_f != 0.0f) || tweak_slow)
                        input_delta = ((input_delta < 0.0f) ? -1.0f : +1.0f) / v_range_f; // Keyboard/Gamepad tweak speeds in integer steps
                    else
                        input_delta /= 100.0f;
                }
                if (tweak_fast)
                    input_delta *= 10.0f;

                g.SliderCurrentAccum += input_delta;
                g.SliderCurrentAccumDirty = true;
            }

            float delta = g.SliderCurrentAccum;
            if (g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
            {
                ClearActiveID();
            }
            else if (g.SliderCurrentAccumDirty)
            {
                clicked_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                if ((clicked_t >= 1.0f && delta > 0.0f) || (clicked_t <= 0.0f && delta < 0.0f)) // This is to avoid applying the saturation when already past the limits
                {
                    set_new_value = false;
                    g.SliderCurrentAccum = 0.0f; // If pushing up against the limits, don't continue to accumulate
                }
                else
                {
                    set_new_value = true;
                    float old_clicked_t = clicked_t;
                    clicked_t = UxImSaturate(clicked_t + delta);

                    // Calculate what our "new" clicked_t will be, and thus how far we actually moved the slider, and subtract this from the accumulator
                    TYPE v_new = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                    if (is_floating_point && !(flags & UxImGuiSliderFlags_NoRoundToFormat))
                        v_new = RoundScalarWithFormatT<TYPE>(format, data_type, v_new);
                    float new_clicked_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_new, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                    if (delta > 0)
                        g.SliderCurrentAccum -= UxImMin(new_clicked_t - old_clicked_t, delta);
                    else
                        g.SliderCurrentAccum -= UxImMax(new_clicked_t - old_clicked_t, delta);
                }

                g.SliderCurrentAccumDirty = false;
            }
        }

        if (set_new_value)
            if ((g.LastItemData.ItemFlags & UxImGuiItemFlags_ReadOnly) || (flags & UxImGuiSliderFlags_ReadOnly))
                set_new_value = false;

        if (set_new_value)
        {
            TYPE v_new = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

            // Round to user desired precision based on format string
            if (is_floating_point && !(flags & UxImGuiSliderFlags_NoRoundToFormat))
                v_new = RoundScalarWithFormatT<TYPE>(format, data_type, v_new);

            // Apply result
            if (*v != v_new)
            {
                *v = v_new;
                value_changed = true;
            }
        }
    }

    if (slider_sz < 1.0f)
    {
        *out_grab_bb = UxImRect(bb.Min, bb.Min);
    }
    else
    {
        // Output grab position so it can be displayed by the caller
        float grab_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        if (axis == UxImGuiAxis_Y)
            grab_t = 1.0f - grab_t;
        const float grab_pos = UxImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
        if (axis == UxImGuiAxis_X)
            *out_grab_bb = UxImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
        else
            *out_grab_bb = UxImRect(bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
    }

    return value_changed;
}

// For 32-bit and larger types, slider bounds are limited to half the natural type range.
// So e.g. an integer Slider between INT_MAX-10 and INT_MAX will fail, but an integer Slider between INT_MAX/2-10 and INT_MAX/2 will be ok.
// It would be possible to lift that limitation with some work but it doesn't seem to be worth it for sliders.
bool UxImGui::SliderBehavior(const UxImRect& bb, UxImGuiID id, UxImGuiDataType data_type, void* p_v, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags, UxImRect* out_grab_bb)
{
    // Read imgui.cpp "API BREAKING CHANGES" section for 1.78 if you hit this assert.
    IM_ASSERT((flags == 1 || (flags & UxImGuiSliderFlags_InvalidMask_) == 0) && "Invalid UxImGuiSliderFlags flags! Has the legacy 'float power' argument been mistakenly cast to flags? Call function with UxImGuiSliderFlags_Logarithmic flags instead.");
    IM_ASSERT((flags & UxImGuiSliderFlags_WrapAround) == 0); // Not supported by SliderXXX(), only by DragXXX()

    switch (data_type)
    {
    case UxImGuiDataType_S8:  { UxImS32 v32 = (UxImS32)*(UxImS8*)p_v;  bool r = SliderBehaviorT<UxImS32, UxImS32, float>(bb, id, UxImGuiDataType_S32, &v32, *(const UxImS8*)p_min,  *(const UxImS8*)p_max,  format, flags, out_grab_bb); if (r) *(UxImS8*)p_v  = (UxImS8)v32;  return r; }
    case UxImGuiDataType_U8:  { UxImU32 v32 = (UxImU32)*(UxImU8*)p_v;  bool r = SliderBehaviorT<UxImU32, UxImS32, float>(bb, id, UxImGuiDataType_U32, &v32, *(const UxImU8*)p_min,  *(const UxImU8*)p_max,  format, flags, out_grab_bb); if (r) *(UxImU8*)p_v  = (UxImU8)v32;  return r; }
    case UxImGuiDataType_S16: { UxImS32 v32 = (UxImS32)*(UxImS16*)p_v; bool r = SliderBehaviorT<UxImS32, UxImS32, float>(bb, id, UxImGuiDataType_S32, &v32, *(const UxImS16*)p_min, *(const UxImS16*)p_max, format, flags, out_grab_bb); if (r) *(UxImS16*)p_v = (UxImS16)v32; return r; }
    case UxImGuiDataType_U16: { UxImU32 v32 = (UxImU32)*(UxImU16*)p_v; bool r = SliderBehaviorT<UxImU32, UxImS32, float>(bb, id, UxImGuiDataType_U32, &v32, *(const UxImU16*)p_min, *(const UxImU16*)p_max, format, flags, out_grab_bb); if (r) *(UxImU16*)p_v = (UxImU16)v32; return r; }
    case UxImGuiDataType_S32:
        IM_ASSERT(*(const UxImS32*)p_min >= IM_S32_MIN / 2 && *(const UxImS32*)p_max <= IM_S32_MAX / 2);
        return SliderBehaviorT<UxImS32, UxImS32, float >(bb, id, data_type, (UxImS32*)p_v,  *(const UxImS32*)p_min,  *(const UxImS32*)p_max,  format, flags, out_grab_bb);
    case UxImGuiDataType_U32:
        IM_ASSERT(*(const UxImU32*)p_max <= IM_U32_MAX / 2);
        return SliderBehaviorT<UxImU32, UxImS32, float >(bb, id, data_type, (UxImU32*)p_v,  *(const UxImU32*)p_min,  *(const UxImU32*)p_max,  format, flags, out_grab_bb);
    case UxImGuiDataType_S64:
        IM_ASSERT(*(const UxImS64*)p_min >= IM_S64_MIN / 2 && *(const UxImS64*)p_max <= IM_S64_MAX / 2);
        return SliderBehaviorT<UxImS64, UxImS64, double>(bb, id, data_type, (UxImS64*)p_v,  *(const UxImS64*)p_min,  *(const UxImS64*)p_max,  format, flags, out_grab_bb);
    case UxImGuiDataType_U64:
        IM_ASSERT(*(const UxImU64*)p_max <= IM_U64_MAX / 2);
        return SliderBehaviorT<UxImU64, UxImS64, double>(bb, id, data_type, (UxImU64*)p_v,  *(const UxImU64*)p_min,  *(const UxImU64*)p_max,  format, flags, out_grab_bb);
    case UxImGuiDataType_Float:
        IM_ASSERT(*(const float*)p_min >= -FLT_MAX / 2.0f && *(const float*)p_max <= FLT_MAX / 2.0f);
        return SliderBehaviorT<float, float, float >(bb, id, data_type, (float*)p_v,  *(const float*)p_min,  *(const float*)p_max,  format, flags, out_grab_bb);
    case UxImGuiDataType_Double:
        IM_ASSERT(*(const double*)p_min >= -DBL_MAX / 2.0f && *(const double*)p_max <= DBL_MAX / 2.0f);
        return SliderBehaviorT<double, double, double>(bb, id, data_type, (double*)p_v, *(const double*)p_min, *(const double*)p_max, format, flags, out_grab_bb);
    case UxImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
    return false;
}

// Note: p_data, p_min and p_max are _pointers_ to a memory address holding the data. For a slider, they are all required.
// Read code of e.g. SliderFloat(), SliderInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool UxImGui::SliderScalar(const char* label, UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const UxImVec2 label_size = CalcTextSize(label, NULL, true);
    const UxImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + UxImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const UxImRect total_bb(frame_bb.Min, frame_bb.Max + UxImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const bool temp_input_allowed = (flags & UxImGuiSliderFlags_NoInput) == 0;
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? UxImGuiItemFlags_Inputable : 0))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        // Tabbing or CTRL+click on Slider turns it into an input box
        const bool clicked = hovered && IsMouseClicked(0, UxImGuiInputFlags_None, id);
        const bool make_active = (clicked || g.NavActivateId == id);
        if (make_active && clicked)
            SetKeyOwner(UxImGuiKey_MouseLeft, id);
        if (make_active && temp_input_allowed)
            if ((clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & UxImGuiActivateFlags_PreferInput)))
                temp_input_is_active = true;

        // Store initial value (not used by main lib but available as a convenience but some mods e.g. to revert)
        if (make_active)
            memcpy(&g.ActiveIdValueOnActivation, p_data, DataTypeGetInfo(data_type)->Size);

        if (make_active && !temp_input_is_active)
        {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << UxImGuiDir_Left) | (1 << UxImGuiDir_Right);
        }
    }

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when UxImGuiSliderFlags_ClampOnInput is set (generally via UxImGuiSliderFlags_AlwaysClamp)
        const bool clamp_enabled = (flags & UxImGuiSliderFlags_ClampOnInput) != 0;
        return TempInputScalar(frame_bb, id, label, data_type, p_data, format, clamp_enabled ? p_min : NULL, clamp_enabled ? p_max : NULL);
    }

    // Draw frame
    const UxImU32 frame_col = GetColorU32(g.ActiveId == id ? UxImGuiCol_FrameBgActive : hovered ? UxImGuiCol_FrameBgHovered : UxImGuiCol_FrameBg);
    RenderNavCursor(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    UxImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? UxImGuiCol_SliderGrabActive : UxImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    if (g.LogEnabled)
        LogSetNextTextDecoration("{", "}");
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, UxImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(UxImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? UxImGuiItemStatusFlags_Inputable : 0));
    return value_changed;
}

// Add multiple sliders on 1 line for compact edition of multiple components
bool UxImGui::SliderScalarN(const char* label, UxImGuiDataType data_type, void* v, int components, const void* v_min, const void* v_max, const char* format, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= SliderScalar("", data_type, v, v_min, v_max, format, flags);
        PopID();
        PopItemWidth();
        v = (void*)((char*)v + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool UxImGui::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalar(label, UxImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalarN(label, UxImGuiDataType_Float, v, 2, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalarN(label, UxImGuiDataType_Float, v, 3, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalarN(label, UxImGuiDataType_Float, v, 4, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderAngle(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, UxImGuiSliderFlags flags)
{
    if (format == NULL)
        format = "%.0f deg";
    float v_deg = (*v_rad) * 360.0f / (2 * IM_PI);
    bool value_changed = SliderFloat(label, &v_deg, v_degrees_min, v_degrees_max, format, flags);
    if (value_changed)
        *v_rad = v_deg * (2 * IM_PI) / 360.0f;
    return value_changed;
}

bool UxImGui::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalar(label, UxImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalarN(label, UxImGuiDataType_S32, v, 2, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalarN(label, UxImGuiDataType_S32, v, 3, &v_min, &v_max, format, flags);
}

bool UxImGui::SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return SliderScalarN(label, UxImGuiDataType_S32, v, 4, &v_min, &v_max, format, flags);
}

bool UxImGui::VSliderScalar(const char* label, const UxImVec2& size, UxImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, UxImGuiSliderFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);

    const UxImVec2 label_size = CalcTextSize(label, NULL, true);
    const UxImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const UxImRect bb(frame_bb.Min, frame_bb.Max + UxImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(bb, style.FramePadding.y);
    if (!ItemAdd(frame_bb, id))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    const bool clicked = hovered && IsMouseClicked(0, UxImGuiInputFlags_None, id);
    if (clicked || g.NavActivateId == id)
    {
        if (clicked)
            SetKeyOwner(UxImGuiKey_MouseLeft, id);
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << UxImGuiDir_Up) | (1 << UxImGuiDir_Down);
    }

    // Draw frame
    const UxImU32 frame_col = GetColorU32(g.ActiveId == id ? UxImGuiCol_FrameBgActive : hovered ? UxImGuiCol_FrameBgHovered : UxImGuiCol_FrameBg);
    RenderNavCursor(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    UxImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags | UxImGuiSliderFlags_Vertical, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    // Render grab
    if (grab_bb.Max.y > grab_bb.Min.y)
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? UxImGuiCol_SliderGrabActive : UxImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    // For the vertical slider we allow centered text to overlap the frame padding
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    RenderTextClipped(UxImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, value_buf, value_buf_end, NULL, UxImVec2(0.5f, 0.0f));
    if (label_size.x > 0.0f)
        RenderText(UxImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    return value_changed;
}

bool UxImGui::VSliderFloat(const char* label, const UxImVec2& size, float* v, float v_min, float v_max, const char* format, UxImGuiSliderFlags flags)
{
    return VSliderScalar(label, size, UxImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool UxImGui::VSliderInt(const char* label, const UxImVec2& size, int* v, int v_min, int v_max, const char* format, UxImGuiSliderFlags flags)
{
    return VSliderScalar(label, size, UxImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: InputScalar, InputFloat, InputInt, etc.
//-------------------------------------------------------------------------
// - UxImParseFormatFindStart() [Internal]
// - UxImParseFormatFindEnd() [Internal]
// - UxImParseFormatTrimDecorations() [Internal]
// - UxImParseFormatSanitizeForPrinting() [Internal]
// - UxImParseFormatSanitizeForScanning() [Internal]
// - UxImParseFormatPrecision() [Internal]
// - TempInputTextScalar() [Internal]
// - InputScalar()
// - InputScalarN()
// - InputFloat()
// - InputFloat2()
// - InputFloat3()
// - InputFloat4()
// - InputInt()
// - InputInt2()
// - InputInt3()
// - InputInt4()
// - InputDouble()
//-------------------------------------------------------------------------

// We don't use strchr() because our strings are usually very short and often start with '%'
const char* UxImParseFormatFindStart(const char* fmt)
{
    while (char c = fmt[0])
    {
        if (c == '%' && fmt[1] != '%')
            return fmt;
        else if (c == '%')
            fmt++;
        fmt++;
    }
    return fmt;
}

const char* UxImParseFormatFindEnd(const char* fmt)
{
    // Printf/scanf types modifiers: I/L/h/j/l/t/w/z. Other uppercase letters qualify as types aka end of the format.
    if (fmt[0] != '%')
        return fmt;
    const unsigned int ignored_uppercase_mask = (1 << ('I'-'A')) | (1 << ('L'-'A'));
    const unsigned int ignored_lowercase_mask = (1 << ('h'-'a')) | (1 << ('j'-'a')) | (1 << ('l'-'a')) | (1 << ('t'-'a')) | (1 << ('w'-'a')) | (1 << ('z'-'a'));
    for (char c; (c = *fmt) != 0; fmt++)
    {
        if (c >= 'A' && c <= 'Z' && ((1 << (c - 'A')) & ignored_uppercase_mask) == 0)
            return fmt + 1;
        if (c >= 'a' && c <= 'z' && ((1 << (c - 'a')) & ignored_lowercase_mask) == 0)
            return fmt + 1;
    }
    return fmt;
}

// Extract the format out of a format string with leading or trailing decorations
//  fmt = "blah blah"  -> return ""
//  fmt = "%.3f"       -> return fmt
//  fmt = "hello %.3f" -> return fmt + 6
//  fmt = "%.3f hello" -> return buf written with "%.3f"
const char* UxImParseFormatTrimDecorations(const char* fmt, char* buf, size_t buf_size)
{
    const char* fmt_start = UxImParseFormatFindStart(fmt);
    if (fmt_start[0] != '%')
        return "";
    const char* fmt_end = UxImParseFormatFindEnd(fmt_start);
    if (fmt_end[0] == 0) // If we only have leading decoration, we don't need to copy the data.
        return fmt_start;
    UxImStrncpy(buf, fmt_start, UxImMin((size_t)(fmt_end - fmt_start) + 1, buf_size));
    return buf;
}

// Sanitize format
// - Zero terminate so extra characters after format (e.g. "%f123") don't confuse atof/atoi
// - stb_sprintf.h supports several new modifiers which format numbers in a way that also makes them incompatible atof/atoi.
void UxImParseFormatSanitizeForPrinting(const char* fmt_in, char* fmt_out, size_t fmt_out_size)
{
    const char* fmt_end = UxImParseFormatFindEnd(fmt_in);
    IM_UNUSED(fmt_out_size);
    IM_ASSERT((size_t)(fmt_end - fmt_in + 1) < fmt_out_size); // Format is too long, let us know if this happens to you!
    while (fmt_in < fmt_end)
    {
        char c = *fmt_in++;
        if (c != '\'' && c != '$' && c != '_') // Custom flags provided by stb_sprintf.h. POSIX 2008 also supports '.
            *(fmt_out++) = c;
    }
    *fmt_out = 0; // Zero-terminate
}

// - For scanning we need to remove all width and precision fields and flags "%+3.7f" -> "%f". BUT don't strip types like "%I64d" which includes digits. ! "%07I64d" -> "%I64d"
const char* UxImParseFormatSanitizeForScanning(const char* fmt_in, char* fmt_out, size_t fmt_out_size)
{
    const char* fmt_end = UxImParseFormatFindEnd(fmt_in);
    const char* fmt_out_begin = fmt_out;
    IM_UNUSED(fmt_out_size);
    IM_ASSERT((size_t)(fmt_end - fmt_in + 1) < fmt_out_size); // Format is too long, let us know if this happens to you!
    bool has_type = false;
    while (fmt_in < fmt_end)
    {
        char c = *fmt_in++;
        if (!has_type && ((c >= '0' && c <= '9') || c == '.' || c == '+' || c == '#'))
            continue;
        has_type |= ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); // Stop skipping digits
        if (c != '\'' && c != '$' && c != '_') // Custom flags provided by stb_sprintf.h. POSIX 2008 also supports '.
            *(fmt_out++) = c;
    }
    *fmt_out = 0; // Zero-terminate
    return fmt_out_begin;
}

template<typename TYPE>
static const char* UxImAtoi(const char* src, TYPE* output)
{
    int negative = 0;
    if (*src == '-') { negative = 1; src++; }
    if (*src == '+') { src++; }
    TYPE v = 0;
    while (*src >= '0' && *src <= '9')
        v = (v * 10) + (*src++ - '0');
    *output = negative ? -v : v;
    return src;
}

// Parse display precision back from the display format string
// FIXME: This is still used by some navigation code path to infer a minimum tweak step, but we should aim to rework widgets so it isn't needed.
int UxImParseFormatPrecision(const char* fmt, int default_precision)
{
    fmt = UxImParseFormatFindStart(fmt);
    if (fmt[0] != '%')
        return default_precision;
    fmt++;
    while (*fmt >= '0' && *fmt <= '9')
        fmt++;
    int precision = INT_MAX;
    if (*fmt == '.')
    {
        fmt = UxImAtoi<int>(fmt + 1, &precision);
        if (precision < 0 || precision > 99)
            precision = default_precision;
    }
    if (*fmt == 'e' || *fmt == 'E') // Maximum precision with scientific notation
        precision = -1;
    if ((*fmt == 'g' || *fmt == 'G') && precision == INT_MAX)
        precision = -1;
    return (precision == INT_MAX) ? default_precision : precision;
}

// Create text input in place of another active widget (e.g. used when doing a CTRL+Click on drag/slider widgets)
// FIXME: Facilitate using this in variety of other situations.
// FIXME: Among other things, setting UxImGuiItemFlags_AllowDuplicateId in LastItemData is currently correct but
// the expected relationship between TempInputXXX functions and LastItemData is a little fishy.
bool UxImGui::TempInputText(const UxImRect& bb, UxImGuiID id, const char* label, char* buf, int buf_size, UxImGuiInputTextFlags flags)
{
    // On the first frame, g.TempInputTextId == 0, then on subsequent frames it becomes == id.
    // We clear ActiveID on the first frame to allow the InputText() taking it back.
    UxImGuiContext& g = *GUxImGui;
    const bool init = (g.TempInputId != id);
    if (init)
        ClearActiveID();

    g.CurrentWindow->DC.CursorPos = bb.Min;
    g.LastItemData.ItemFlags |= UxImGuiItemFlags_AllowDuplicateId;
    bool value_changed = InputTextEx(label, NULL, buf, buf_size, bb.GetSize(), flags | UxImGuiInputTextFlags_MergedItem);
    if (init)
    {
        // First frame we started displaying the InputText widget, we expect it to take the active id.
        IM_ASSERT(g.ActiveId == id);
        g.TempInputId = g.ActiveId;
    }
    return value_changed;
}

// Note that Drag/Slider functions are only forwarding the min/max values clamping values if the UxImGuiSliderFlags_AlwaysClamp flag is set!
// This is intended: this way we allow CTRL+Click manual input to set a value out of bounds, for maximum flexibility.
// However this may not be ideal for all uses, as some user code may break on out of bound values.
bool UxImGui::TempInputScalar(const UxImRect& bb, UxImGuiID id, const char* label, UxImGuiDataType data_type, void* p_data, const char* format, const void* p_clamp_min, const void* p_clamp_max)
{
    // FIXME: May need to clarify display behavior if format doesn't contain %.
    // "%d" -> "%d" / "There are %d items" -> "%d" / "items" -> "%d" (fallback). Also see #6405
    UxImGuiContext& g = *GUxImGui;
    const UxImGuiDataTypeInfo* type_info = DataTypeGetInfo(data_type);
    char fmt_buf[32];
    char data_buf[32];
    format = UxImParseFormatTrimDecorations(format, fmt_buf, IM_ARRAYSIZE(fmt_buf));
    if (format[0] == 0)
        format = type_info->PrintFmt;
    DataTypeFormatString(data_buf, IM_ARRAYSIZE(data_buf), data_type, p_data, format);
    UxImStrTrimBlanks(data_buf);

    UxImGuiInputTextFlags flags = UxImGuiInputTextFlags_AutoSelectAll | (UxImGuiInputTextFlags)UxImGuiInputTextFlags_LocalizeDecimalPoint;
    g.LastItemData.ItemFlags |= UxImGuiItemFlags_NoMarkEdited; // Because TempInputText() uses UxImGuiInputTextFlags_MergedItem it doesn't submit a new item, so we poke LastItemData.
    bool value_changed = false;
    if (TempInputText(bb, id, label, data_buf, IM_ARRAYSIZE(data_buf), flags))
    {
        // Backup old value
        size_t data_type_size = type_info->Size;
        UxImGuiDataTypeStorage data_backup;
        memcpy(&data_backup, p_data, data_type_size);

        // Apply new value (or operations) then clamp
        DataTypeApplyFromText(data_buf, data_type, p_data, format, NULL);
        if (p_clamp_min || p_clamp_max)
        {
            if (p_clamp_min && p_clamp_max && DataTypeCompare(data_type, p_clamp_min, p_clamp_max) > 0)
                UxImSwap(p_clamp_min, p_clamp_max);
            DataTypeClamp(data_type, p_data, p_clamp_min, p_clamp_max);
        }

        // Only mark as edited if new value is different
        g.LastItemData.ItemFlags &= ~UxImGuiItemFlags_NoMarkEdited;
        value_changed = memcmp(&data_backup, p_data, data_type_size) != 0;
        if (value_changed)
            MarkItemEdited(id);
    }
    return value_changed;
}

void UxImGui::SetNextItemRefVal(UxImGuiDataType data_type, void* p_data)
{
    UxImGuiContext& g = *GUxImGui;
    g.NextItemData.HasFlags |= UxImGuiNextItemDataFlags_HasRefVal;
    memcpy(&g.NextItemData.RefVal, p_data, DataTypeGetInfo(data_type)->Size);
}

// Note: p_data, p_step, p_step_fast are _pointers_ to a memory address holding the data. For an Input widget, p_step and p_step_fast are optional.
// Read code of e.g. InputFloat(), InputInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool UxImGui::InputScalar(const char* label, UxImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, UxImGuiInputTextFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    UxImGuiStyle& style = g.Style;
    IM_ASSERT((flags & UxImGuiInputTextFlags_EnterReturnsTrue) == 0); // Not supported by InputScalar(). Please open an issue if you this would be useful to you. Otherwise use IsItemDeactivatedAfterEdit()!

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    void* p_data_default = (g.NextItemData.HasFlags & UxImGuiNextItemDataFlags_HasRefVal) ? &g.NextItemData.RefVal : &g.DataTypeZeroValue;

    char buf[64];
    if ((flags & UxImGuiInputTextFlags_DisplayEmptyRefVal) && DataTypeCompare(data_type, p_data, p_data_default) == 0)
        buf[0] = 0;
    else
        DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, p_data, format);

    // Disable the MarkItemEdited() call in InputText but keep UxImGuiItemStatusFlags_Edited.
    // We call MarkItemEdited() ourselves by comparing the actual data rather than the string.
    g.NextItemData.ItemFlags |= UxImGuiItemFlags_NoMarkEdited;
    flags |= UxImGuiInputTextFlags_AutoSelectAll | (UxImGuiInputTextFlags)UxImGuiInputTextFlags_LocalizeDecimalPoint;

    bool value_changed = false;
    if (p_step == NULL)
    {
        if (InputText(label, buf, IM_ARRAYSIZE(buf), flags))
            value_changed = DataTypeApplyFromText(buf, data_type, p_data, format, (flags & UxImGuiInputTextFlags_ParseEmptyRefVal) ? p_data_default : NULL);
    }
    else
    {
        const float button_size = GetFrameHeight();

        BeginGroup(); // The only purpose of the group here is to allow the caller to query item data e.g. IsItemActive()
        PushID(label);
        SetNextItemWidth(UxImMax(1.0f, CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
        if (InputText("", buf, IM_ARRAYSIZE(buf), flags)) // PushId(label) + "" gives us the expected ID from outside point of view
            value_changed = DataTypeApplyFromText(buf, data_type, p_data, format, (flags & UxImGuiInputTextFlags_ParseEmptyRefVal) ? p_data_default : NULL);
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | UxImGuiItemStatusFlags_Inputable);

        // Step buttons
        const UxImVec2 backup_frame_padding = style.FramePadding;
        style.FramePadding.x = style.FramePadding.y;
        if (flags & UxImGuiInputTextFlags_ReadOnly)
            BeginDisabled();
        PushItemFlag(UxImGuiItemFlags_ButtonRepeat, true);
        SameLine(0, style.ItemInnerSpacing.x);
        if (ButtonEx("-", UxImVec2(button_size, button_size)))
        {
            DataTypeApplyOp(data_type, '-', p_data, p_data, g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
            value_changed = true;
        }
        SameLine(0, style.ItemInnerSpacing.x);
        if (ButtonEx("+", UxImVec2(button_size, button_size)))
        {
            DataTypeApplyOp(data_type, '+', p_data, p_data, g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
            value_changed = true;
        }
        PopItemFlag();
        if (flags & UxImGuiInputTextFlags_ReadOnly)
            EndDisabled();

        const char* label_end = FindRenderedTextEnd(label);
        if (label != label_end)
        {
            SameLine(0, style.ItemInnerSpacing.x);
            TextEx(label, label_end);
        }
        style.FramePadding = backup_frame_padding;

        PopID();
        EndGroup();
    }

    g.LastItemData.ItemFlags &= ~UxImGuiItemFlags_NoMarkEdited;
    if (value_changed)
        MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

bool UxImGui::InputScalarN(const char* label, UxImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, UxImGuiInputTextFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= InputScalar("", data_type, p_data, p_step, p_step_fast, format, flags);
        PopID();
        PopItemWidth();
        p_data = (void*)((char*)p_data + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0.0f, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool UxImGui::InputFloat(const char* label, float* v, float step, float step_fast, const char* format, UxImGuiInputTextFlags flags)
{
    return InputScalar(label, UxImGuiDataType_Float, (void*)v, (void*)(step > 0.0f ? &step : NULL), (void*)(step_fast > 0.0f ? &step_fast : NULL), format, flags);
}

bool UxImGui::InputFloat2(const char* label, float v[2], const char* format, UxImGuiInputTextFlags flags)
{
    return InputScalarN(label, UxImGuiDataType_Float, v, 2, NULL, NULL, format, flags);
}

bool UxImGui::InputFloat3(const char* label, float v[3], const char* format, UxImGuiInputTextFlags flags)
{
    return InputScalarN(label, UxImGuiDataType_Float, v, 3, NULL, NULL, format, flags);
}

bool UxImGui::InputFloat4(const char* label, float v[4], const char* format, UxImGuiInputTextFlags flags)
{
    return InputScalarN(label, UxImGuiDataType_Float, v, 4, NULL, NULL, format, flags);
}

bool UxImGui::InputInt(const char* label, int* v, int step, int step_fast, UxImGuiInputTextFlags flags)
{
    // Hexadecimal input provided as a convenience but the flag name is awkward. Typically you'd use InputText() to parse your own data, if you want to handle prefixes.
    const char* format = (flags & UxImGuiInputTextFlags_CharsHexadecimal) ? "%08X" : "%d";
    return InputScalar(label, UxImGuiDataType_S32, (void*)v, (void*)(step > 0 ? &step : NULL), (void*)(step_fast > 0 ? &step_fast : NULL), format, flags);
}

bool UxImGui::InputInt2(const char* label, int v[2], UxImGuiInputTextFlags flags)
{
    return InputScalarN(label, UxImGuiDataType_S32, v, 2, NULL, NULL, "%d", flags);
}

bool UxImGui::InputInt3(const char* label, int v[3], UxImGuiInputTextFlags flags)
{
    return InputScalarN(label, UxImGuiDataType_S32, v, 3, NULL, NULL, "%d", flags);
}

bool UxImGui::InputInt4(const char* label, int v[4], UxImGuiInputTextFlags flags)
{
    return InputScalarN(label, UxImGuiDataType_S32, v, 4, NULL, NULL, "%d", flags);
}

bool UxImGui::InputDouble(const char* label, double* v, double step, double step_fast, const char* format, UxImGuiInputTextFlags flags)
{
    return InputScalar(label, UxImGuiDataType_Double, (void*)v, (void*)(step > 0.0 ? &step : NULL), (void*)(step_fast > 0.0 ? &step_fast : NULL), format, flags);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: InputText, InputTextMultiline, InputTextWithHint
//-------------------------------------------------------------------------
// - imstb_textedit.h include
// - InputText()
// - InputTextWithHint()
// - InputTextMultiline()
// - InputTextGetCharInfo() [Internal]
// - InputTextReindexLines() [Internal]
// - InputTextReindexLinesRange() [Internal]
// - InputTextEx() [Internal]
// - DebugNodeInputTextState() [Internal]
//-------------------------------------------------------------------------

namespace UxImStb
{
#include "imstb_textedit.h"
}

bool UxImGui::InputText(const char* label, char* buf, size_t buf_size, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT(!(flags & UxImGuiInputTextFlags_Multiline)); // call InputTextMultiline()
    return InputTextEx(label, NULL, buf, (int)buf_size, UxImVec2(0, 0), flags, callback, user_data);
}

bool UxImGui::InputTextMultiline(const char* label, char* buf, size_t buf_size, const UxImVec2& size, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback, void* user_data)
{
    return InputTextEx(label, NULL, buf, (int)buf_size, size, flags | UxImGuiInputTextFlags_Multiline, callback, user_data);
}

bool UxImGui::InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT(!(flags & UxImGuiInputTextFlags_Multiline)); // call InputTextMultiline() or  InputTextEx() manually if you need multi-line + hint.
    return InputTextEx(label, hint, buf, (int)buf_size, UxImVec2(0, 0), flags, callback, user_data);
}

// This is only used in the path where the multiline widget is inactive.
static int InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end)
{
    int line_count = 0;
    const char* s = text_begin;
    while (true)
    {
        const char* s_eol = strchr(s, '\n');
        line_count++;
        if (s_eol == NULL)
        {
            s = s + UxImStrlen(s);
            break;
        }
        s = s_eol + 1;
    }
    *out_text_end = s;
    return line_count;
}

// FIXME: Ideally we'd share code with UxImFont::CalcTextSizeA()
static UxImVec2 InputTextCalcTextSize(UxImGuiContext* ctx, const char* text_begin, const char* text_end, const char** remaining, UxImVec2* out_offset, bool stop_on_new_line)
{
    UxImGuiContext& g = *ctx;
    //UxImFont* font = g.Font;
    UxImFontBaked* baked = g.FontBaked;
    const float line_height = g.FontSize;
    const float scale = line_height / baked->Size;

    UxImVec2 text_size = UxImVec2(0, 0);
    float line_width = 0.0f;

    const char* s = text_begin;
    while (s < text_end)
    {
        unsigned int c = (unsigned int)*s;
        if (c < 0x80)
            s += 1;
        else
            s += UxImTextCharFromUtf8(&c, s, text_end);

        if (c == '\n')
        {
            text_size.x = UxImMax(text_size.x, line_width);
            text_size.y += line_height;
            line_width = 0.0f;
            if (stop_on_new_line)
                break;
            continue;
        }
        if (c == '\r')
            continue;

        line_width += baked->GetCharAdvance((UxImWchar)c) * scale;
    }

    if (text_size.x < line_width)
        text_size.x = line_width;

    if (out_offset)
        *out_offset = UxImVec2(line_width, text_size.y + line_height);  // offset allow for the possibility of sitting after a trailing \n

    if (line_width > 0 || text_size.y == 0.0f)                        // whereas size.y will ignore the trailing \n
        text_size.y += line_height;

    if (remaining)
        *remaining = s;

    return text_size;
}

// Wrapper for stb_textedit.h to edit text (our wrapper is for: statically sized buffer, single-line, wchar characters. InputText converts between UTF-8 and wchar)
// With our UTF-8 use of stb_textedit:
// - STB_TEXTEDIT_GETCHAR is nothing more than a a "GETBYTE". It's only used to compare to ascii or to copy blocks of text so we are fine.
// - One exception is the STB_TEXTEDIT_IS_SPACE feature which would expect a full char in order to handle full-width space such as 0x3000 (see UxImCharIsBlankW).
// - ...but we don't use that feature.
namespace UxImStb
{
static int     STB_TEXTEDIT_STRINGLEN(const UxImGuiInputTextState* obj)                             { return obj->TextLen; }
static char    STB_TEXTEDIT_GETCHAR(const UxImGuiInputTextState* obj, int idx)                      { IM_ASSERT(idx <= obj->TextLen); return obj->TextSrc[idx]; }
static float   STB_TEXTEDIT_GETWIDTH(UxImGuiInputTextState* obj, int line_start_idx, int char_idx)  { unsigned int c; UxImTextCharFromUtf8(&c, obj->TextSrc + line_start_idx + char_idx, obj->TextSrc + obj->TextLen); if ((UxImWchar)c == '\n') return IMSTB_TEXTEDIT_GETWIDTH_NEWLINE; UxImGuiContext& g = *obj->Ctx; return g.FontBaked->GetCharAdvance((UxImWchar)c) * g.FontScale; }
static char    STB_TEXTEDIT_NEWLINE = '\n';
static void    STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, UxImGuiInputTextState* obj, int line_start_idx)
{
    const char* text = obj->TextSrc;
    const char* text_remaining = NULL;
    const UxImVec2 size = InputTextCalcTextSize(obj->Ctx, text + line_start_idx, text + obj->TextLen, &text_remaining, NULL, true);
    r->x0 = 0.0f;
    r->x1 = size.x;
    r->baseline_y_delta = size.y;
    r->ymin = 0.0f;
    r->ymax = size.y;
    r->num_chars = (int)(text_remaining - (text + line_start_idx));
}

#define IMSTB_TEXTEDIT_GETNEXTCHARINDEX  IMSTB_TEXTEDIT_GETNEXTCHARINDEX_IMPL
#define IMSTB_TEXTEDIT_GETPREVCHARINDEX  IMSTB_TEXTEDIT_GETPREVCHARINDEX_IMPL

static int IMSTB_TEXTEDIT_GETNEXTCHARINDEX_IMPL(UxImGuiInputTextState* obj, int idx)
{
    if (idx >= obj->TextLen)
        return obj->TextLen + 1;
    unsigned int c;
    return idx + UxImTextCharFromUtf8(&c, obj->TextSrc + idx, obj->TextSrc + obj->TextLen);
}

static int IMSTB_TEXTEDIT_GETPREVCHARINDEX_IMPL(UxImGuiInputTextState* obj, int idx)
{
    if (idx <= 0)
        return -1;
    const char* p = UxImTextFindPreviousUtf8Codepoint(obj->TextSrc, obj->TextSrc + idx);
    return (int)(p - obj->TextSrc);
}

static bool UxImCharIsSeparatorW(unsigned int c)
{
    static const unsigned int separator_list[] =
    {
        ',', 0x3001, '.', 0x3002, ';', 0xFF1B, '(', 0xFF08, ')', 0xFF09, '{', 0xFF5B, '}', 0xFF5D,
        '[', 0x300C, ']', 0x300D, '|', 0xFF5C, '!', 0xFF01, '\\', 0xFFE5, '/', 0x30FB, 0xFF0F,
        '\n', '\r',
    };
    for (unsigned int separator : separator_list)
        if (c == separator)
            return true;
    return false;
}

static int is_word_boundary_from_right(UxImGuiInputTextState* obj, int idx)
{
    // When UxImGuiInputTextFlags_Password is set, we don't want actions such as CTRL+Arrow to leak the fact that underlying data are blanks or separators.
    if ((obj->Flags & UxImGuiInputTextFlags_Password) || idx <= 0)
        return 0;

    const char* curr_p = obj->TextSrc + idx;
    const char* prev_p = UxImTextFindPreviousUtf8Codepoint(obj->TextSrc, curr_p);
    unsigned int curr_c; UxImTextCharFromUtf8(&curr_c, curr_p, obj->TextSrc + obj->TextLen);
    unsigned int prev_c; UxImTextCharFromUtf8(&prev_c, prev_p, obj->TextSrc + obj->TextLen);

    bool prev_white = UxImCharIsBlankW(prev_c);
    bool prev_separ = UxImCharIsSeparatorW(prev_c);
    bool curr_white = UxImCharIsBlankW(curr_c);
    bool curr_separ = UxImCharIsSeparatorW(curr_c);
    return ((prev_white || prev_separ) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
}
static int is_word_boundary_from_left(UxImGuiInputTextState* obj, int idx)
{
    if ((obj->Flags & UxImGuiInputTextFlags_Password) || idx <= 0)
        return 0;

    const char* curr_p = obj->TextSrc + idx;
    const char* prev_p = UxImTextFindPreviousUtf8Codepoint(obj->TextSrc, curr_p);
    unsigned int prev_c; UxImTextCharFromUtf8(&prev_c, curr_p, obj->TextSrc + obj->TextLen);
    unsigned int curr_c; UxImTextCharFromUtf8(&curr_c, prev_p, obj->TextSrc + obj->TextLen);

    bool prev_white = UxImCharIsBlankW(prev_c);
    bool prev_separ = UxImCharIsSeparatorW(prev_c);
    bool curr_white = UxImCharIsBlankW(curr_c);
    bool curr_separ = UxImCharIsSeparatorW(curr_c);
    return ((prev_white) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
}
static int  STB_TEXTEDIT_MOVEWORDLEFT_IMPL(UxImGuiInputTextState* obj, int idx)
{
    idx = IMSTB_TEXTEDIT_GETPREVCHARINDEX(obj, idx);
    while (idx >= 0 && !is_word_boundary_from_right(obj, idx))
        idx = IMSTB_TEXTEDIT_GETPREVCHARINDEX(obj, idx);
    return idx < 0 ? 0 : idx;
}
static int  STB_TEXTEDIT_MOVEWORDRIGHT_MAC(UxImGuiInputTextState* obj, int idx)
{
    int len = obj->TextLen;
    idx = IMSTB_TEXTEDIT_GETNEXTCHARINDEX(obj, idx);
    while (idx < len && !is_word_boundary_from_left(obj, idx))
        idx = IMSTB_TEXTEDIT_GETNEXTCHARINDEX(obj, idx);
    return idx > len ? len : idx;
}
static int  STB_TEXTEDIT_MOVEWORDRIGHT_WIN(UxImGuiInputTextState* obj, int idx)
{
    idx = IMSTB_TEXTEDIT_GETNEXTCHARINDEX(obj, idx);
    int len = obj->TextLen;
    while (idx < len && !is_word_boundary_from_right(obj, idx))
        idx = IMSTB_TEXTEDIT_GETNEXTCHARINDEX(obj, idx);
    return idx > len ? len : idx;
}
static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(UxImGuiInputTextState* obj, int idx)  { UxImGuiContext& g = *obj->Ctx; if (g.IO.ConfigMacOSXBehaviors) return STB_TEXTEDIT_MOVEWORDRIGHT_MAC(obj, idx); else return STB_TEXTEDIT_MOVEWORDRIGHT_WIN(obj, idx); }
#define STB_TEXTEDIT_MOVEWORDLEFT       STB_TEXTEDIT_MOVEWORDLEFT_IMPL  // They need to be #define for stb_textedit.h
#define STB_TEXTEDIT_MOVEWORDRIGHT      STB_TEXTEDIT_MOVEWORDRIGHT_IMPL

static void STB_TEXTEDIT_DELETECHARS(UxImGuiInputTextState* obj, int pos, int n)
{
    // Offset remaining text (+ copy zero terminator)
    IM_ASSERT(obj->TextSrc == obj->TextA.Data);
    char* dst = obj->TextA.Data + pos;
    char* src = obj->TextA.Data + pos + n;
    memmove(dst, src, obj->TextLen - n - pos + 1);
    obj->Edited = true;
    obj->TextLen -= n;
}

static bool STB_TEXTEDIT_INSERTCHARS(UxImGuiInputTextState* obj, int pos, const char* new_text, int new_text_len)
{
    const bool is_resizable = (obj->Flags & UxImGuiInputTextFlags_CallbackResize) != 0;
    const int text_len = obj->TextLen;
    IM_ASSERT(pos <= text_len);

    if (!is_resizable && (new_text_len + obj->TextLen + 1 > obj->BufCapacity))
        return false;

    // Grow internal buffer if needed
    IM_ASSERT(obj->TextSrc == obj->TextA.Data);
    if (new_text_len + text_len + 1 > obj->TextA.Size)
    {
        if (!is_resizable)
            return false;
        obj->TextA.resize(text_len + UxImClamp(new_text_len, 32, UxImMax(256, new_text_len)) + 1);
        obj->TextSrc = obj->TextA.Data;
    }

    char* text = obj->TextA.Data;
    if (pos != text_len)
        memmove(text + pos + new_text_len, text + pos, (size_t)(text_len - pos));
    memcpy(text + pos, new_text, (size_t)new_text_len);

    obj->Edited = true;
    obj->TextLen += new_text_len;
    obj->TextA[obj->TextLen] = '\0';

    return true;
}

// We don't use an enum so we can build even with conflicting symbols (if another user of stb_textedit.h leak their STB_TEXTEDIT_K_* symbols)
#define STB_TEXTEDIT_K_LEFT         0x200000 // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT        0x200001 // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP           0x200002 // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN         0x200003 // keyboard input to move cursor down
#define STB_TEXTEDIT_K_LINESTART    0x200004 // keyboard input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND      0x200005 // keyboard input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART    0x200006 // keyboard input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND      0x200007 // keyboard input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE       0x200008 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE    0x200009 // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO         0x20000A // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO         0x20000B // keyboard input to perform redo
#define STB_TEXTEDIT_K_WORDLEFT     0x20000C // keyboard input to move cursor left one word
#define STB_TEXTEDIT_K_WORDRIGHT    0x20000D // keyboard input to move cursor right one word
#define STB_TEXTEDIT_K_PGUP         0x20000E // keyboard input to move cursor up a page
#define STB_TEXTEDIT_K_PGDOWN       0x20000F // keyboard input to move cursor down a page
#define STB_TEXTEDIT_K_SHIFT        0x400000

#define IMSTB_TEXTEDIT_IMPLEMENTATION
#define IMSTB_TEXTEDIT_memmove memmove
#include "imstb_textedit.h"

// stb_textedit internally allows for a single undo record to do addition and deletion, but somehow, calling
// the stb_textedit_paste() function creates two separate records, so we perform it manually. (FIXME: Report to nothings/stb?)
static void stb_textedit_replace(UxImGuiInputTextState* str, STB_TexteditState* state, const IMSTB_TEXTEDIT_CHARTYPE* text, int text_len)
{
    stb_text_makeundo_replace(str, state, 0, str->TextLen, text_len);
    UxImStb::STB_TEXTEDIT_DELETECHARS(str, 0, str->TextLen);
    state->cursor = state->select_start = state->select_end = 0;
    if (text_len <= 0)
        return;
    if (UxImStb::STB_TEXTEDIT_INSERTCHARS(str, 0, text, text_len))
    {
        state->cursor = state->select_start = state->select_end = text_len;
        state->has_preferred_x = 0;
        return;
    }
    IM_ASSERT(0); // Failed to insert character, normally shouldn't happen because of how we currently use stb_textedit_replace()
}

} // namespace UxImStb

// We added an extra indirection where 'Stb' is heap-allocated, in order facilitate the work of bindings generators.
UxImGuiInputTextState::UxImGuiInputTextState()
{
    memset(this, 0, sizeof(*this));
    Stb = IM_NEW(UxImStbTexteditState);
    memset(Stb, 0, sizeof(*Stb));
}

UxImGuiInputTextState::~UxImGuiInputTextState()
{
    IM_DELETE(Stb);
}

void UxImGuiInputTextState::OnKeyPressed(int key)
{
    stb_textedit_key(this, Stb, key);
    CursorFollow = true;
    CursorAnimReset();
}

void UxImGuiInputTextState::OnCharPressed(unsigned int c)
{
    // Convert the key to a UTF8 byte sequence.
    // The changes we had to make to stb_textedit_key made it very much UTF-8 specific which is not too great.
    char utf8[5];
    UxImTextCharToUtf8(utf8, c);
    stb_textedit_text(this, Stb, utf8, (int)UxImStrlen(utf8));
    CursorFollow = true;
    CursorAnimReset();
}

// Those functions are not inlined in imgui_internal.h, allowing us to hide UxImStbTexteditState from that header.
void UxImGuiInputTextState::CursorAnimReset()                 { CursorAnim = -0.30f; } // After a user-input the cursor stays on for a while without blinking
void UxImGuiInputTextState::CursorClamp()                     { Stb->cursor = UxImMin(Stb->cursor, TextLen); Stb->select_start = UxImMin(Stb->select_start, TextLen); Stb->select_end = UxImMin(Stb->select_end, TextLen); }
bool UxImGuiInputTextState::HasSelection() const              { return Stb->select_start != Stb->select_end; }
void UxImGuiInputTextState::ClearSelection()                  { Stb->select_start = Stb->select_end = Stb->cursor; }
int  UxImGuiInputTextState::GetCursorPos() const              { return Stb->cursor; }
int  UxImGuiInputTextState::GetSelectionStart() const         { return Stb->select_start; }
int  UxImGuiInputTextState::GetSelectionEnd() const           { return Stb->select_end; }
void UxImGuiInputTextState::SelectAll()                       { Stb->select_start = 0; Stb->cursor = Stb->select_end = TextLen; Stb->has_preferred_x = 0; }
void UxImGuiInputTextState::ReloadUserBufAndSelectAll()       { WantReloadUserBuf = true; ReloadSelectionStart = 0; ReloadSelectionEnd = INT_MAX; }
void UxImGuiInputTextState::ReloadUserBufAndKeepSelection()   { WantReloadUserBuf = true; ReloadSelectionStart = Stb->select_start; ReloadSelectionEnd = Stb->select_end; }
void UxImGuiInputTextState::ReloadUserBufAndMoveToEnd()       { WantReloadUserBuf = true; ReloadSelectionStart = ReloadSelectionEnd = INT_MAX; }

UxImGuiInputTextCallbackData::UxImGuiInputTextCallbackData()
{
    memset(this, 0, sizeof(*this));
}

// Public API to manipulate UTF-8 text from within a callback.
// FIXME: The existence of this rarely exercised code path is a bit of a nuisance.
// Historically they existed because STB_TEXTEDIT_INSERTCHARS() etc. worked on our UxImWchar
// buffer, but nowadays they both work on UTF-8 data. Should aim to merge both.
void UxImGuiInputTextCallbackData::DeleteChars(int pos, int bytes_count)
{
    IM_ASSERT(pos + bytes_count <= BufTextLen);
    char* dst = Buf + pos;
    const char* src = Buf + pos + bytes_count;
    memmove(dst, src, BufTextLen - bytes_count - pos + 1);

    if (CursorPos >= pos + bytes_count)
        CursorPos -= bytes_count;
    else if (CursorPos >= pos)
        CursorPos = pos;
    SelectionStart = SelectionEnd = CursorPos;
    BufDirty = true;
    BufTextLen -= bytes_count;
}

void UxImGuiInputTextCallbackData::InsertChars(int pos, const char* new_text, const char* new_text_end)
{
    // Accept null ranges
    if (new_text == new_text_end)
        return;

    // Grow internal buffer if needed
    const bool is_resizable = (Flags & UxImGuiInputTextFlags_CallbackResize) != 0;
    const int new_text_len = new_text_end ? (int)(new_text_end - new_text) : (int)UxImStrlen(new_text);
    if (new_text_len + BufTextLen >= BufSize)
    {
        if (!is_resizable)
            return;

        UxImGuiContext& g = *Ctx;
        UxImGuiInputTextState* edit_state = &g.InputTextState;
        IM_ASSERT(edit_state->ID != 0 && g.ActiveId == edit_state->ID);
        IM_ASSERT(Buf == edit_state->TextA.Data);
        int new_buf_size = BufTextLen + UxImClamp(new_text_len * 4, 32, UxImMax(256, new_text_len)) + 1;
        edit_state->TextA.resize(new_buf_size + 1);
        edit_state->TextSrc = edit_state->TextA.Data;
        Buf = edit_state->TextA.Data;
        BufSize = edit_state->BufCapacity = new_buf_size;
    }

    if (BufTextLen != pos)
        memmove(Buf + pos + new_text_len, Buf + pos, (size_t)(BufTextLen - pos));
    memcpy(Buf + pos, new_text, (size_t)new_text_len * sizeof(char));
    Buf[BufTextLen + new_text_len] = '\0';

    if (CursorPos >= pos)
        CursorPos += new_text_len;
    SelectionStart = SelectionEnd = CursorPos;
    BufDirty = true;
    BufTextLen += new_text_len;
}

void UxImGui::PushPasswordFont()
{
    UxImGuiContext& g = *GUxImGui;
    UxImFontBaked* backup = &g.InputTextPasswordFontBackupBaked;
    IM_ASSERT(backup->IndexAdvanceX.Size == 0 && backup->IndexLookup.Size == 0);
    UxImFontGlyph* glyph = g.FontBaked->FindGlyph('*');
    g.InputTextPasswordFontBackupFlags = g.Font->Flags;
    backup->FallbackGlyphIndex = g.FontBaked->FallbackGlyphIndex;
    backup->FallbackAdvanceX = g.FontBaked->FallbackAdvanceX;
    backup->IndexLookup.swap(g.FontBaked->IndexLookup);
    backup->IndexAdvanceX.swap(g.FontBaked->IndexAdvanceX);
    g.Font->Flags |= UxImFontFlags_NoLoadGlyphs;
    g.FontBaked->FallbackGlyphIndex = g.FontBaked->Glyphs.index_from_ptr(glyph);
    g.FontBaked->FallbackAdvanceX = glyph->AdvanceX;
}

void UxImGui::PopPasswordFont()
{
    UxImGuiContext& g = *GUxImGui;
    UxImFontBaked* backup = &g.InputTextPasswordFontBackupBaked;
    g.Font->Flags = g.InputTextPasswordFontBackupFlags;
    g.FontBaked->FallbackGlyphIndex = backup->FallbackGlyphIndex;
    g.FontBaked->FallbackAdvanceX = backup->FallbackAdvanceX;
    g.FontBaked->IndexLookup.swap(backup->IndexLookup);
    g.FontBaked->IndexAdvanceX.swap(backup->IndexAdvanceX);
    IM_ASSERT(backup->IndexAdvanceX.Size == 0 && backup->IndexLookup.Size == 0);
}

// Return false to discard a character.
static bool InputTextFilterCharacter(UxImGuiContext* ctx, unsigned int* p_char, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback, void* user_data, bool input_source_is_clipboard)
{
    unsigned int c = *p_char;

    // Filter non-printable (NB: isprint is unreliable! see #2467)
    bool apply_named_filters = true;
    if (c < 0x20)
    {
        bool pass = false;
        pass |= (c == '\n') && (flags & UxImGuiInputTextFlags_Multiline) != 0;    // Note that an Enter KEY will emit \r and be ignored (we poll for KEY in InputText() code)
        if (c == '\n' && input_source_is_clipboard && (flags & UxImGuiInputTextFlags_Multiline) == 0) // In single line mode, replace \n with a space
        {
            c = *p_char = ' ';
            pass = true;
        }
        pass |= (c == '\n') && (flags & UxImGuiInputTextFlags_Multiline) != 0;
        pass |= (c == '\t') && (flags & UxImGuiInputTextFlags_AllowTabInput) != 0;
        if (!pass)
            return false;
        apply_named_filters = false; // Override named filters below so newline and tabs can still be inserted.
    }

    if (input_source_is_clipboard == false)
    {
        // We ignore Ascii representation of delete (emitted from Backspace on OSX, see #2578, #2817)
        if (c == 127)
            return false;

        // Filter private Unicode range. GLFW on OSX seems to send private characters for special keys like arrow keys (FIXME)
        if (c >= 0xE000 && c <= 0xF8FF)
            return false;
    }

    // Filter Unicode ranges we are not handling in this build
    if (c > IM_UNICODE_CODEPOINT_MAX)
        return false;

    // Generic named filters
    if (apply_named_filters && (flags & (UxImGuiInputTextFlags_CharsDecimal | UxImGuiInputTextFlags_CharsHexadecimal | UxImGuiInputTextFlags_CharsUppercase | UxImGuiInputTextFlags_CharsNoBlank | UxImGuiInputTextFlags_CharsScientific | (UxImGuiInputTextFlags)UxImGuiInputTextFlags_LocalizeDecimalPoint)))
    {
        // The libc allows overriding locale, with e.g. 'setlocale(LC_NUMERIC, "de_DE.UTF-8");' which affect the output/input of printf/scanf to use e.g. ',' instead of '.'.
        // The standard mandate that programs starts in the "C" locale where the decimal point is '.'.
        // We don't really intend to provide widespread support for it, but out of empathy for people stuck with using odd API, we support the bare minimum aka overriding the decimal point.
        // Change the default decimal_point with:
        //   UxImGui::GetPlatformIO()->Platform_LocaleDecimalPoint = *localeconv()->decimal_point;
        // Users of non-default decimal point (in particular ',') may be affected by word-selection logic (is_word_boundary_from_right/is_word_boundary_from_left) functions.
        UxImGuiContext& g = *ctx;
        const unsigned c_decimal_point = (unsigned int)g.PlatformIO.Platform_LocaleDecimalPoint;
        if (flags & (UxImGuiInputTextFlags_CharsDecimal | UxImGuiInputTextFlags_CharsScientific | (UxImGuiInputTextFlags)UxImGuiInputTextFlags_LocalizeDecimalPoint))
            if (c == '.' || c == ',')
                c = c_decimal_point;

        // Full-width -> half-width conversion for numeric fields (https://en.wikipedia.org/wiki/Halfwidth_and_Fullwidth_Forms_(Unicode_block)
        // While this is mostly convenient, this has the side-effect for uninformed users accidentally inputting full-width characters that they may
        // scratch their head as to why it works in numerical fields vs in generic text fields it would require support in the font.
        if (flags & (UxImGuiInputTextFlags_CharsDecimal | UxImGuiInputTextFlags_CharsScientific | UxImGuiInputTextFlags_CharsHexadecimal))
            if (c >= 0xFF01 && c <= 0xFF5E)
                c = c - 0xFF01 + 0x21;

        // Allow 0-9 . - + * /
        if (flags & UxImGuiInputTextFlags_CharsDecimal)
            if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/'))
                return false;

        // Allow 0-9 . - + * / e E
        if (flags & UxImGuiInputTextFlags_CharsScientific)
            if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/') && (c != 'e') && (c != 'E'))
                return false;

        // Allow 0-9 a-F A-F
        if (flags & UxImGuiInputTextFlags_CharsHexadecimal)
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F'))
                return false;

        // Turn a-z into A-Z
        if (flags & UxImGuiInputTextFlags_CharsUppercase)
            if (c >= 'a' && c <= 'z')
                c += (unsigned int)('A' - 'a');

        if (flags & UxImGuiInputTextFlags_CharsNoBlank)
            if (UxImCharIsBlankW(c))
                return false;

        *p_char = c;
    }

    // Custom callback filter
    if (flags & UxImGuiInputTextFlags_CallbackCharFilter)
    {
        UxImGuiContext& g = *GUxImGui;
        UxImGuiInputTextCallbackData callback_data;
        callback_data.Ctx = &g;
        callback_data.EventFlag = UxImGuiInputTextFlags_CallbackCharFilter;
        callback_data.EventChar = (UxImWchar)c;
        callback_data.Flags = flags;
        callback_data.UserData = user_data;
        if (callback(&callback_data) != 0)
            return false;
        *p_char = callback_data.EventChar;
        if (!callback_data.EventChar)
            return false;
    }

    return true;
}

// Find the shortest single replacement we can make to get from old_buf to new_buf
// Note that this doesn't directly alter state->TextA, state->TextLen. They are expected to be made valid separately.
// FIXME: Ideally we should transition toward (1) making InsertChars()/DeleteChars() update undo-stack (2) discourage (and keep reconcile) or obsolete (and remove reconcile) accessing buffer directly.
static void InputTextReconcileUndoState(UxImGuiInputTextState* state, const char* old_buf, int old_length, const char* new_buf, int new_length)
{
    const int shorter_length = UxImMin(old_length, new_length);
    int first_diff;
    for (first_diff = 0; first_diff < shorter_length; first_diff++)
        if (old_buf[first_diff] != new_buf[first_diff])
            break;
    if (first_diff == old_length && first_diff == new_length)
        return;

    int old_last_diff = old_length   - 1;
    int new_last_diff = new_length - 1;
    for (; old_last_diff >= first_diff && new_last_diff >= first_diff; old_last_diff--, new_last_diff--)
        if (old_buf[old_last_diff] != new_buf[new_last_diff])
            break;

    const int insert_len = new_last_diff - first_diff + 1;
    const int delete_len = old_last_diff - first_diff + 1;
    if (insert_len > 0 || delete_len > 0)
        if (IMSTB_TEXTEDIT_CHARTYPE* p = stb_text_createundo(&state->Stb->undostate, first_diff, delete_len, insert_len))
            for (int i = 0; i < delete_len; i++)
                p[i] = old_buf[first_diff + i];
}

// As InputText() retain textual data and we currently provide a path for user to not retain it (via local variables)
// we need some form of hook to reapply data back to user buffer on deactivation frame. (#4714)
// It would be more desirable that we discourage users from taking advantage of the "user not retaining data" trick,
// but that more likely be attractive when we do have _NoLiveEdit flag available.
void UxImGui::InputTextDeactivateHook(UxImGuiID id)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiInputTextState* state = &g.InputTextState;
    if (id == 0 || state->ID != id)
        return;
    g.InputTextDeactivatedState.ID = state->ID;
    if (state->Flags & UxImGuiInputTextFlags_ReadOnly)
    {
        g.InputTextDeactivatedState.TextA.resize(0); // In theory this data won't be used, but clear to be neat.
    }
    else
    {
        IM_ASSERT(state->TextA.Data != 0);
        IM_ASSERT(state->TextA[state->TextLen] == 0);
        g.InputTextDeactivatedState.TextA.resize(state->TextLen + 1);
        memcpy(g.InputTextDeactivatedState.TextA.Data, state->TextA.Data, state->TextLen + 1);
    }
}

// Edit a string of text
// - buf_size account for the zero-terminator, so a buf_size of 6 can hold "Hello" but not "Hello!".
//   This is so we can easily call InputText() on static arrays using ARRAYSIZE() and to match
//   Note that in std::string world, capacity() would omit 1 byte used by the zero-terminator.
// - When active, hold on a privately held copy of the text (and apply back to 'buf'). So changing 'buf' while the InputText is active has no effect.
// - If you want to use UxImGui::InputText() with std::string, see misc/cpp/imgui_stdlib.h
// (FIXME: Rather confusing and messy function, among the worse part of our codebase, expecting to rewrite a V2 at some point.. Partly because we are
//  doing UTF8 > U16 > UTF8 conversions on the go to easily interface with stb_textedit. Ideally should stay in UTF-8 all the time. See https://github.com/nothings/stb/issues/188)
bool UxImGui::InputTextEx(const char* label, const char* hint, char* buf, int buf_size, const UxImVec2& size_arg, UxImGuiInputTextFlags flags, UxImGuiInputTextCallback callback, void* callback_user_data)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    IM_ASSERT(buf != NULL && buf_size >= 0);
    IM_ASSERT(!((flags & UxImGuiInputTextFlags_CallbackHistory) && (flags & UxImGuiInputTextFlags_Multiline)));        // Can't use both together (they both use up/down keys)
    IM_ASSERT(!((flags & UxImGuiInputTextFlags_CallbackCompletion) && (flags & UxImGuiInputTextFlags_AllowTabInput))); // Can't use both together (they both use tab key)
    IM_ASSERT(!((flags & UxImGuiInputTextFlags_ElideLeft) && (flags & UxImGuiInputTextFlags_Multiline)));               // Multiline will not work with left-trimming

    UxImGuiContext& g = *GUxImGui;
    UxImGuiIO& io = g.IO;
    const UxImGuiStyle& style = g.Style;

    const bool RENDER_SELECTION_WHEN_INACTIVE = false;
    const bool is_multiline = (flags & UxImGuiInputTextFlags_Multiline) != 0;

    if (is_multiline) // Open group before calling GetID() because groups tracks id created within their scope (including the scrollbar)
        BeginGroup();
    const UxImGuiID id = window->GetID(label);
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);
    const UxImVec2 frame_size = CalcItemSize(size_arg, CalcItemWidth(), (is_multiline ? g.FontSize * 8.0f : label_size.y) + style.FramePadding.y * 2.0f); // Arbitrary default of 8 lines high for multi-line
    const UxImVec2 total_size = UxImVec2(frame_size.x + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), frame_size.y);

    const UxImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const UxImRect total_bb(frame_bb.Min, frame_bb.Min + total_size);

    UxImGuiWindow* draw_window = window;
    UxImVec2 inner_size = frame_size;
    UxImGuiLastItemData item_data_backup;
    if (is_multiline)
    {
        UxImVec2 backup_pos = window->DC.CursorPos;
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id, &frame_bb, UxImGuiItemFlags_Inputable))
        {
            EndGroup();
            return false;
        }
        item_data_backup = g.LastItemData;
        window->DC.CursorPos = backup_pos;

        // Prevent NavActivation from Tabbing when our widget accepts Tab inputs: this allows cycling through widgets without stopping.
        if (g.NavActivateId == id && (g.NavActivateFlags & UxImGuiActivateFlags_FromTabbing) && (flags & UxImGuiInputTextFlags_AllowTabInput))
            g.NavActivateId = 0;

        // Prevent NavActivate reactivating in BeginChild() when we are already active.
        const UxImGuiID backup_activate_id = g.NavActivateId;
        if (g.ActiveId == id) // Prevent reactivation
            g.NavActivateId = 0;

        // We reproduce the contents of BeginChildFrame() in order to provide 'label' so our window internal data are easier to read/debug.
        PushStyleColor(UxImGuiCol_ChildBg, style.Colors[UxImGuiCol_FrameBg]);
        PushStyleVar(UxImGuiStyleVar_ChildRounding, style.FrameRounding);
        PushStyleVar(UxImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
        PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach FramePadding edges
        bool child_visible = BeginChildEx(label, id, frame_bb.GetSize(), UxImGuiChildFlags_Borders, UxImGuiWindowFlags_NoMove);
        g.NavActivateId = backup_activate_id;
        PopStyleVar(3);
        PopStyleColor();
        if (!child_visible)
        {
            EndChild();
            EndGroup();
            return false;
        }
        draw_window = g.CurrentWindow; // Child window
        draw_window->DC.NavLayersActiveMaskNext |= (1 << draw_window->DC.NavLayerCurrent); // This is to ensure that EndChild() will display a navigation highlight so we can "enter" into it.
        draw_window->DC.CursorPos += style.FramePadding;
        inner_size.x -= draw_window->ScrollbarSizes.x;
    }
    else
    {
        // Support for internal UxImGuiInputTextFlags_MergedItem flag, which could be redesigned as an ItemFlags if needed (with test performed in ItemAdd)
        ItemSize(total_bb, style.FramePadding.y);
        if (!(flags & UxImGuiInputTextFlags_MergedItem))
            if (!ItemAdd(total_bb, id, &frame_bb, UxImGuiItemFlags_Inputable))
                return false;
    }

    // Ensure mouse cursor is set even after switching to keyboard/gamepad mode. May generalize further? (#6417)
    bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags | UxImGuiItemFlags_NoNavDisableMouseHover);
    if (hovered)
        SetMouseCursor(UxImGuiMouseCursor_TextInput);
    if (hovered && g.NavHighlightItemUnderNav)
        hovered = false;

    // We are only allowed to access the state if we are already the active widget.
    UxImGuiInputTextState* state = GetInputTextState(id);

    if (g.LastItemData.ItemFlags & UxImGuiItemFlags_ReadOnly)
        flags |= UxImGuiInputTextFlags_ReadOnly;
    const bool is_readonly = (flags & UxImGuiInputTextFlags_ReadOnly) != 0;
    const bool is_password = (flags & UxImGuiInputTextFlags_Password) != 0;
    const bool is_undoable = (flags & UxImGuiInputTextFlags_NoUndoRedo) == 0;
    const bool is_resizable = (flags & UxImGuiInputTextFlags_CallbackResize) != 0;
    if (is_resizable)
        IM_ASSERT(callback != NULL); // Must provide a callback if you set the UxImGuiInputTextFlags_CallbackResize flag!

    const bool input_requested_by_nav = (g.ActiveId != id) && ((g.NavActivateId == id) && ((g.NavActivateFlags & UxImGuiActivateFlags_PreferInput) || (g.NavInputSource == UxImGuiInputSource_Keyboard)));

    const bool user_clicked = hovered && io.MouseClicked[0];
    const bool user_scroll_finish = is_multiline && state != NULL && g.ActiveId == 0 && g.ActiveIdPreviousFrame == GetWindowScrollbarID(draw_window, UxImGuiAxis_Y);
    const bool user_scroll_active = is_multiline && state != NULL && g.ActiveId == GetWindowScrollbarID(draw_window, UxImGuiAxis_Y);
    bool clear_active_id = false;
    bool select_all = false;

    float scroll_y = is_multiline ? draw_window->Scroll.y : FLT_MAX;

    const bool init_reload_from_user_buf = (state != NULL && state->WantReloadUserBuf);
    const bool init_changed_specs = (state != NULL && state->Stb->single_line != !is_multiline); // state != NULL means its our state.
    const bool init_make_active = (user_clicked || user_scroll_finish || input_requested_by_nav);
    const bool init_state = (init_make_active || user_scroll_active);
    if (init_reload_from_user_buf)
    {
        int new_len = (int)UxImStrlen(buf);
        IM_ASSERT(new_len + 1 <= buf_size && "Is your input buffer properly zero-terminated?");
        state->WantReloadUserBuf = false;
        InputTextReconcileUndoState(state, state->TextA.Data, state->TextLen, buf, new_len);
        state->TextA.resize(buf_size + 1); // we use +1 to make sure that .Data is always pointing to at least an empty string.
        state->TextLen = new_len;
        memcpy(state->TextA.Data, buf, state->TextLen + 1);
        state->Stb->select_start = state->ReloadSelectionStart;
        state->Stb->cursor = state->Stb->select_end = state->ReloadSelectionEnd;
        state->CursorClamp();
    }
    else if ((init_state && g.ActiveId != id) || init_changed_specs)
    {
        // Access state even if we don't own it yet.
        state = &g.InputTextState;
        state->CursorAnimReset();

        // Backup state of deactivating item so they'll have a chance to do a write to output buffer on the same frame they report IsItemDeactivatedAfterEdit (#4714)
        InputTextDeactivateHook(state->ID);

        // Take a copy of the initial buffer value.
        // From the moment we focused we are normally ignoring the content of 'buf' (unless we are in read-only mode)
        const int buf_len = (int)UxImStrlen(buf);
        IM_ASSERT(buf_len + 1 <= buf_size && "Is your input buffer properly zero-terminated?");
        state->TextToRevertTo.resize(buf_len + 1);    // UTF-8. we use +1 to make sure that .Data is always pointing to at least an empty string.
        memcpy(state->TextToRevertTo.Data, buf, buf_len + 1);

        // Preserve cursor position and undo/redo stack if we come back to same widget
        // FIXME: Since we reworked this on 2022/06, may want to differentiate recycle_cursor vs recycle_undostate?
        bool recycle_state = (state->ID == id && !init_changed_specs);
        if (recycle_state && (state->TextLen != buf_len || (state->TextA.Data == NULL || strncmp(state->TextA.Data, buf, buf_len) != 0)))
            recycle_state = false;

        // Start edition
        state->ID = id;
        state->TextLen = buf_len;
        if (!is_readonly)
        {
            state->TextA.resize(buf_size + 1); // we use +1 to make sure that .Data is always pointing to at least an empty string.
            memcpy(state->TextA.Data, buf, state->TextLen + 1);
        }

        // Find initial scroll position for right alignment
        state->Scroll = UxImVec2(0.0f, 0.0f);
        if (flags & UxImGuiInputTextFlags_ElideLeft)
            state->Scroll.x += UxImMax(0.0f, CalcTextSize(buf).x - frame_size.x + style.FramePadding.x * 2.0f);

        // Recycle existing cursor/selection/undo stack but clamp position
        // Note a single mouse click will override the cursor/position immediately by calling stb_textedit_click handler.
        if (recycle_state)
            state->CursorClamp();
        else
            stb_textedit_initialize_state(state->Stb, !is_multiline);

        if (!is_multiline)
        {
            if (flags & UxImGuiInputTextFlags_AutoSelectAll)
                select_all = true;
            if (input_requested_by_nav && (!recycle_state || !(g.NavActivateFlags & UxImGuiActivateFlags_TryToPreserveState)))
                select_all = true;
            if (user_clicked && io.KeyCtrl)
                select_all = true;
        }

        if (flags & UxImGuiInputTextFlags_AlwaysOverwrite)
            state->Stb->insert_mode = 1; // stb field name is indeed incorrect (see #2863)
    }

    const bool is_osx = io.ConfigMacOSXBehaviors;
    if (g.ActiveId != id && init_make_active)
    {
        IM_ASSERT(state && state->ID == id);
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
    }
    if (g.ActiveId == id)
    {
        // Declare some inputs, the other are registered and polled via Shortcut() routing system.
        // FIXME: The reason we don't use Shortcut() is we would need a routing flag to specify multiple mods, or to all mods combination into individual shortcuts.
        const UxImGuiKey always_owned_keys[] = { UxImGuiKey_LeftArrow, UxImGuiKey_RightArrow, UxImGuiKey_Enter, UxImGuiKey_KeypadEnter, UxImGuiKey_Delete, UxImGuiKey_Backspace, UxImGuiKey_Home, UxImGuiKey_End };
        for (UxImGuiKey key : always_owned_keys)
            SetKeyOwner(key, id);
        if (user_clicked)
            SetKeyOwner(UxImGuiKey_MouseLeft, id);
        g.ActiveIdUsingNavDirMask |= (1 << UxImGuiDir_Left) | (1 << UxImGuiDir_Right);
        if (is_multiline || (flags & UxImGuiInputTextFlags_CallbackHistory))
        {
            g.ActiveIdUsingNavDirMask |= (1 << UxImGuiDir_Up) | (1 << UxImGuiDir_Down);
            SetKeyOwner(UxImGuiKey_UpArrow, id);
            SetKeyOwner(UxImGuiKey_DownArrow, id);
        }
        if (is_multiline)
        {
            SetKeyOwner(UxImGuiKey_PageUp, id);
            SetKeyOwner(UxImGuiKey_PageDown, id);
        }
        // FIXME: May be a problem to always steal Alt on OSX, would ideally still allow an uninterrupted Alt down-up to toggle menu
        if (is_osx)
            SetKeyOwner(UxImGuiMod_Alt, id);

        // Expose scroll in a manner that is agnostic to us using a child window
        if (is_multiline && state != NULL)
            state->Scroll.y = draw_window->Scroll.y;

        // Read-only mode always ever read from source buffer. Refresh TextLen when active.
        if (is_readonly && state != NULL)
            state->TextLen = (int)UxImStrlen(buf);
        //if (is_readonly && state != NULL)
        //    state->TextA.clear(); // Uncomment to facilitate debugging, but we otherwise prefer to keep/amortize th allocation.
    }
    if (state != NULL)
        state->TextSrc = is_readonly ? buf : state->TextA.Data;

    // We have an edge case if ActiveId was set through another widget (e.g. widget being swapped), clear id immediately (don't wait until the end of the function)
    if (g.ActiveId == id && state == NULL)
        ClearActiveID();

    // Release focus when we click outside
    if (g.ActiveId == id && io.MouseClicked[0] && !init_state && !init_make_active) //-V560
        clear_active_id = true;

    // Lock the decision of whether we are going to take the path displaying the cursor or selection
    bool render_cursor = (g.ActiveId == id) || (state && user_scroll_active);
    bool render_selection = state && (state->HasSelection() || select_all) && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
    bool value_changed = false;
    bool validated = false;

    // Select the buffer to render.
    const bool buf_display_from_state = (render_cursor || render_selection || g.ActiveId == id) && !is_readonly && state;
    bool is_displaying_hint = (hint != NULL && (buf_display_from_state ? state->TextA.Data : buf)[0] == 0);

    // Password pushes a temporary font with only a fallback glyph
    if (is_password && !is_displaying_hint)
        PushPasswordFont();

    // Process mouse inputs and character inputs
    if (g.ActiveId == id)
    {
        IM_ASSERT(state != NULL);
        state->Edited = false;
        state->BufCapacity = buf_size;
        state->Flags = flags;

        // Although we are active we don't prevent mouse from hovering other elements unless we are interacting right now with the widget.
        // Down the line we should have a cleaner library-wide concept of Selected vs Active.
        g.ActiveIdAllowOverlap = !io.MouseDown[0];

        // Edit in progress
        const float mouse_x = (io.MousePos.x - frame_bb.Min.x - style.FramePadding.x) + state->Scroll.x;
        const float mouse_y = (is_multiline ? (io.MousePos.y - draw_window->DC.CursorPos.y) : (g.FontSize * 0.5f));

        if (select_all)
        {
            state->SelectAll();
            state->SelectedAllMouseLock = true;
        }
        else if (hovered && io.MouseClickedCount[0] >= 2 && !io.KeyShift)
        {
            stb_textedit_click(state, state->Stb, mouse_x, mouse_y);
            const int multiclick_count = (io.MouseClickedCount[0] - 2);
            if ((multiclick_count % 2) == 0)
            {
                // Double-click: Select word
                // We always use the "Mac" word advance for double-click select vs CTRL+Right which use the platform dependent variant:
                // FIXME: There are likely many ways to improve this behavior, but there's no "right" behavior (depends on use-case, software, OS)
                const bool is_bol = (state->Stb->cursor == 0) || UxImStb::STB_TEXTEDIT_GETCHAR(state, state->Stb->cursor - 1) == '\n';
                if (STB_TEXT_HAS_SELECTION(state->Stb) || !is_bol)
                    state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT);
                //state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
                if (!STB_TEXT_HAS_SELECTION(state->Stb))
                    UxImStb::stb_textedit_prep_selection_at_cursor(state->Stb);
                state->Stb->cursor = UxImStb::STB_TEXTEDIT_MOVEWORDRIGHT_MAC(state, state->Stb->cursor);
                state->Stb->select_end = state->Stb->cursor;
                UxImStb::stb_textedit_clamp(state, state->Stb);
            }
            else
            {
                // Triple-click: Select line
                const bool is_eol = UxImStb::STB_TEXTEDIT_GETCHAR(state, state->Stb->cursor) == '\n';
                state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART);
                state->OnKeyPressed(STB_TEXTEDIT_K_LINEEND | STB_TEXTEDIT_K_SHIFT);
                state->OnKeyPressed(STB_TEXTEDIT_K_RIGHT | STB_TEXTEDIT_K_SHIFT);
                if (!is_eol && is_multiline)
                {
                    UxImSwap(state->Stb->select_start, state->Stb->select_end);
                    state->Stb->cursor = state->Stb->select_end;
                }
                state->CursorFollow = false;
            }
            state->CursorAnimReset();
        }
        else if (io.MouseClicked[0] && !state->SelectedAllMouseLock)
        {
            if (hovered)
            {
                if (io.KeyShift)
                    stb_textedit_drag(state, state->Stb, mouse_x, mouse_y);
                else
                    stb_textedit_click(state, state->Stb, mouse_x, mouse_y);
                state->CursorAnimReset();
            }
        }
        else if (io.MouseDown[0] && !state->SelectedAllMouseLock && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
        {
            stb_textedit_drag(state, state->Stb, mouse_x, mouse_y);
            state->CursorAnimReset();
            state->CursorFollow = true;
        }
        if (state->SelectedAllMouseLock && !io.MouseDown[0])
            state->SelectedAllMouseLock = false;

        // We expect backends to emit a Tab key but some also emit a Tab character which we ignore (#2467, #1336)
        // (For Tab and Enter: Win32/SFML/Allegro are sending both keys and chars, GLFW and SDL are only sending keys. For Space they all send all threes)
        if ((flags & UxImGuiInputTextFlags_AllowTabInput) && !is_readonly)
        {
            if (Shortcut(UxImGuiKey_Tab, UxImGuiInputFlags_Repeat, id))
            {
                unsigned int c = '\t'; // Insert TAB
                if (InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data))
                    state->OnCharPressed(c);
            }
            // FIXME: Implement Shift+Tab
            /*
            if (Shortcut(UxImGuiKey_Tab | UxImGuiMod_Shift, UxImGuiInputFlags_Repeat, id))
            {
            }
            */
        }

        // Process regular text input (before we check for Return because using some IME will effectively send a Return?)
        // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
        const bool ignore_char_inputs = (io.KeyCtrl && !io.KeyAlt) || (is_osx && io.KeyCtrl);
        if (io.InputQueueCharacters.Size > 0)
        {
            if (!ignore_char_inputs && !is_readonly && !input_requested_by_nav)
                for (int n = 0; n < io.InputQueueCharacters.Size; n++)
                {
                    // Insert character if they pass filtering
                    unsigned int c = (unsigned int)io.InputQueueCharacters[n];
                    if (c == '\t') // Skip Tab, see above.
                        continue;
                    if (InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data))
                        state->OnCharPressed(c);
                }

            // Consume characters
            io.InputQueueCharacters.resize(0);
        }
    }

    // Process other shortcuts/key-presses
    bool revert_edit = false;
    if (g.ActiveId == id && !g.ActiveIdIsJustActivated && !clear_active_id)
    {
        IM_ASSERT(state != NULL);

        const int row_count_per_page = UxImMax((int)((inner_size.y - style.FramePadding.y) / g.FontSize), 1);
        state->Stb->row_count_per_page = row_count_per_page;

        const int k_mask = (io.KeyShift ? STB_TEXTEDIT_K_SHIFT : 0);
        const bool is_wordmove_key_down = is_osx ? io.KeyAlt : io.KeyCtrl;                     // OS X style: Text editing cursor movement using Alt instead of Ctrl
        const bool is_startend_key_down = is_osx && io.KeyCtrl && !io.KeySuper && !io.KeyAlt;  // OS X style: Line/Text Start and End using Cmd+Arrows instead of Home/End

        // Using Shortcut() with UxImGuiInputFlags_RouteFocused (default policy) to allow routing operations for other code (e.g. calling window trying to use CTRL+A and CTRL+B: former would be handled by InputText)
        // Otherwise we could simply assume that we own the keys as we are active.
        const UxImGuiInputFlags f_repeat = UxImGuiInputFlags_Repeat;
        const bool is_cut   = (Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_X, f_repeat, id) || Shortcut(UxImGuiMod_Shift | UxImGuiKey_Delete, f_repeat, id)) && !is_readonly && !is_password && (!is_multiline || state->HasSelection());
        const bool is_copy  = (Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_C, 0,        id) || Shortcut(UxImGuiMod_Ctrl  | UxImGuiKey_Insert, 0,        id)) && !is_password && (!is_multiline || state->HasSelection());
        const bool is_paste = (Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_V, f_repeat, id) || Shortcut(UxImGuiMod_Shift | UxImGuiKey_Insert, f_repeat, id)) && !is_readonly;
        const bool is_undo  = (Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_Z, f_repeat, id)) && !is_readonly && is_undoable;
        const bool is_redo =  (Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_Y, f_repeat, id) || Shortcut(UxImGuiMod_Ctrl | UxImGuiMod_Shift | UxImGuiKey_Z, f_repeat, id)) && !is_readonly && is_undoable;
        const bool is_select_all = Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_A, 0, id);

        // We allow validate/cancel with Nav source (gamepad) to makes it easier to undo an accidental NavInput press with no keyboard wired, but otherwise it isn't very useful.
        const bool nav_gamepad_active = (io.ConfigFlags & UxImGuiConfigFlags_NavEnableGamepad) != 0 && (io.BackendFlags & UxImGuiBackendFlags_HasGamepad) != 0;
        const bool is_enter_pressed = IsKeyPressed(UxImGuiKey_Enter, true) || IsKeyPressed(UxImGuiKey_KeypadEnter, true);
        const bool is_gamepad_validate = nav_gamepad_active && (IsKeyPressed(UxImGuiKey_NavGamepadActivate, false) || IsKeyPressed(UxImGuiKey_NavGamepadInput, false));
        const bool is_cancel = Shortcut(UxImGuiKey_Escape, f_repeat, id) || (nav_gamepad_active && Shortcut(UxImGuiKey_NavGamepadCancel, f_repeat, id));

        // FIXME: Should use more Shortcut() and reduce IsKeyPressed()+SetKeyOwner(), but requires modifiers combination to be taken account of.
        // FIXME-OSX: Missing support for Alt(option)+Right/Left = go to end of line, or next line if already in end of line.
        if (IsKeyPressed(UxImGuiKey_LeftArrow))                        { state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_LINESTART : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT) | k_mask); }
        else if (IsKeyPressed(UxImGuiKey_RightArrow))                  { state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_LINEEND : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT) | k_mask); }
        else if (IsKeyPressed(UxImGuiKey_UpArrow) && is_multiline)     { if (io.KeyCtrl) SetScrollY(draw_window, UxImMax(draw_window->Scroll.y - g.FontSize, 0.0f)); else state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_UP) | k_mask); }
        else if (IsKeyPressed(UxImGuiKey_DownArrow) && is_multiline)   { if (io.KeyCtrl) SetScrollY(draw_window, UxImMin(draw_window->Scroll.y + g.FontSize, GetScrollMaxY())); else state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_DOWN) | k_mask); }
        else if (IsKeyPressed(UxImGuiKey_PageUp) && is_multiline)      { state->OnKeyPressed(STB_TEXTEDIT_K_PGUP | k_mask); scroll_y -= row_count_per_page * g.FontSize; }
        else if (IsKeyPressed(UxImGuiKey_PageDown) && is_multiline)    { state->OnKeyPressed(STB_TEXTEDIT_K_PGDOWN | k_mask); scroll_y += row_count_per_page * g.FontSize; }
        else if (IsKeyPressed(UxImGuiKey_Home))                        { state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTSTART | k_mask : STB_TEXTEDIT_K_LINESTART | k_mask); }
        else if (IsKeyPressed(UxImGuiKey_End))                         { state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTEND | k_mask : STB_TEXTEDIT_K_LINEEND | k_mask); }
        else if (IsKeyPressed(UxImGuiKey_Delete) && !is_readonly && !is_cut)
        {
            if (!state->HasSelection())
            {
                // OSX doesn't seem to have Super+Delete to delete until end-of-line, so we don't emulate that (as opposed to Super+Backspace)
                if (is_wordmove_key_down)
                    state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
            }
            state->OnKeyPressed(STB_TEXTEDIT_K_DELETE | k_mask);
        }
        else if (IsKeyPressed(UxImGuiKey_Backspace) && !is_readonly)
        {
            if (!state->HasSelection())
            {
                if (is_wordmove_key_down)
                    state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT | STB_TEXTEDIT_K_SHIFT);
                else if (is_osx && io.KeyCtrl && !io.KeyAlt && !io.KeySuper)
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART | STB_TEXTEDIT_K_SHIFT);
            }
            state->OnKeyPressed(STB_TEXTEDIT_K_BACKSPACE | k_mask);
        }
        else if (is_enter_pressed || is_gamepad_validate)
        {
            // Determine if we turn Enter into a \n character
            bool ctrl_enter_for_new_line = (flags & UxImGuiInputTextFlags_CtrlEnterForNewLine) != 0;
            if (!is_multiline || is_gamepad_validate || (ctrl_enter_for_new_line && !io.KeyCtrl) || (!ctrl_enter_for_new_line && io.KeyCtrl))
            {
                validated = true;
                if (io.ConfigInputTextEnterKeepActive && !is_multiline)
                    state->SelectAll(); // No need to scroll
                else
                    clear_active_id = true;
            }
            else if (!is_readonly)
            {
                unsigned int c = '\n'; // Insert new line
                if (InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data))
                    state->OnCharPressed(c);
            }
        }
        else if (is_cancel)
        {
            if (flags & UxImGuiInputTextFlags_EscapeClearsAll)
            {
                if (buf[0] != 0)
                {
                    revert_edit = true;
                }
                else
                {
                    render_cursor = render_selection = false;
                    clear_active_id = true;
                }
            }
            else
            {
                clear_active_id = revert_edit = true;
                render_cursor = render_selection = false;
            }
        }
        else if (is_undo || is_redo)
        {
            state->OnKeyPressed(is_undo ? STB_TEXTEDIT_K_UNDO : STB_TEXTEDIT_K_REDO);
            state->ClearSelection();
        }
        else if (is_select_all)
        {
            state->SelectAll();
            state->CursorFollow = true;
        }
        else if (is_cut || is_copy)
        {
            // Cut, Copy
            if (g.PlatformIO.Platform_SetClipboardTextFn != NULL)
            {
                // SetClipboardText() only takes null terminated strings + state->TextSrc may point to read-only user buffer, so we need to make a copy.
                const int ib = state->HasSelection() ? UxImMin(state->Stb->select_start, state->Stb->select_end) : 0;
                const int ie = state->HasSelection() ? UxImMax(state->Stb->select_start, state->Stb->select_end) : state->TextLen;
                g.TempBuffer.reserve(ie - ib + 1);
                memcpy(g.TempBuffer.Data, state->TextSrc + ib, ie - ib);
                g.TempBuffer.Data[ie - ib] = 0;
                SetClipboardText(g.TempBuffer.Data);
            }
            if (is_cut)
            {
                if (!state->HasSelection())
                    state->SelectAll();
                state->CursorFollow = true;
                stb_textedit_cut(state, state->Stb);
            }
        }
        else if (is_paste)
        {
            if (const char* clipboard = GetClipboardText())
            {
                // Filter pasted buffer
                const int clipboard_len = (int)UxImStrlen(clipboard);
                UxImVector<char> clipboard_filtered;
                clipboard_filtered.reserve(clipboard_len + 1);
                for (const char* s = clipboard; *s != 0; )
                {
                    unsigned int c;
                    int in_len = UxImTextCharFromUtf8(&c, s, NULL);
                    s += in_len;
                    if (!InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data, true))
                        continue;
                    char c_utf8[5];
                    UxImTextCharToUtf8(c_utf8, c);
                    int out_len = (int)UxImStrlen(c_utf8);
                    clipboard_filtered.resize(clipboard_filtered.Size + out_len);
                    memcpy(clipboard_filtered.Data + clipboard_filtered.Size - out_len, c_utf8, out_len);
                }
                if (clipboard_filtered.Size > 0) // If everything was filtered, ignore the pasting operation
                {
                    clipboard_filtered.push_back(0);
                    stb_textedit_paste(state, state->Stb, clipboard_filtered.Data, clipboard_filtered.Size - 1);
                    state->CursorFollow = true;
                }
            }
        }

        // Update render selection flag after events have been handled, so selection highlight can be displayed during the same frame.
        render_selection |= state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
    }

    // Process callbacks and apply result back to user's buffer.
    const char* apply_new_text = NULL;
    int apply_new_text_length = 0;
    if (g.ActiveId == id)
    {
        IM_ASSERT(state != NULL);
        if (revert_edit && !is_readonly)
        {
            if (flags & UxImGuiInputTextFlags_EscapeClearsAll)
            {
                // Clear input
                IM_ASSERT(buf[0] != 0);
                apply_new_text = "";
                apply_new_text_length = 0;
                value_changed = true;
                IMSTB_TEXTEDIT_CHARTYPE empty_string;
                stb_textedit_replace(state, state->Stb, &empty_string, 0);
            }
            else if (strcmp(buf, state->TextToRevertTo.Data) != 0)
            {
                apply_new_text = state->TextToRevertTo.Data;
                apply_new_text_length = state->TextToRevertTo.Size - 1;

                // Restore initial value. Only return true if restoring to the initial value changes the current buffer contents.
                // Push records into the undo stack so we can CTRL+Z the revert operation itself
                value_changed = true;
                stb_textedit_replace(state, state->Stb, state->TextToRevertTo.Data, state->TextToRevertTo.Size - 1);
            }
        }

        // FIXME-OPT: We always reapply the live buffer back to the input buffer before clearing ActiveId,
        // even though strictly speaking it wasn't modified on this frame. Should mark dirty state from the stb_textedit callbacks.
        // If we do that, need to ensure that as special case, 'validated == true' also writes back.
        // This also allows the user to use InputText() without maintaining any user-side storage.
        // (please note that if you use this property along UxImGuiInputTextFlags_CallbackResize you can end up with your temporary string object
        // unnecessarily allocating once a frame, either store your string data, either if you don't then don't use UxImGuiInputTextFlags_CallbackResize).
        const bool apply_edit_back_to_user_buffer = true;// !revert_edit || (validated && (flags & UxImGuiInputTextFlags_EnterReturnsTrue) != 0);
        if (apply_edit_back_to_user_buffer)
        {
            // Apply current edited text immediately.
            // Note that as soon as the input box is active, the in-widget value gets priority over any underlying modification of the input buffer

            // User callback
            if ((flags & (UxImGuiInputTextFlags_CallbackCompletion | UxImGuiInputTextFlags_CallbackHistory | UxImGuiInputTextFlags_CallbackEdit | UxImGuiInputTextFlags_CallbackAlways)) != 0)
            {
                IM_ASSERT(callback != NULL);

                // The reason we specify the usage semantic (Completion/History) is that Completion needs to disable keyboard TABBING at the moment.
                UxImGuiInputTextFlags event_flag = 0;
                UxImGuiKey event_key = UxImGuiKey_None;
                if ((flags & UxImGuiInputTextFlags_CallbackCompletion) != 0 && Shortcut(UxImGuiKey_Tab, 0, id))
                {
                    event_flag = UxImGuiInputTextFlags_CallbackCompletion;
                    event_key = UxImGuiKey_Tab;
                }
                else if ((flags & UxImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressed(UxImGuiKey_UpArrow))
                {
                    event_flag = UxImGuiInputTextFlags_CallbackHistory;
                    event_key = UxImGuiKey_UpArrow;
                }
                else if ((flags & UxImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressed(UxImGuiKey_DownArrow))
                {
                    event_flag = UxImGuiInputTextFlags_CallbackHistory;
                    event_key = UxImGuiKey_DownArrow;
                }
                else if ((flags & UxImGuiInputTextFlags_CallbackEdit) && state->Edited)
                {
                    event_flag = UxImGuiInputTextFlags_CallbackEdit;
                }
                else if (flags & UxImGuiInputTextFlags_CallbackAlways)
                {
                    event_flag = UxImGuiInputTextFlags_CallbackAlways;
                }

                if (event_flag)
                {
                    UxImGuiInputTextCallbackData callback_data;
                    callback_data.Ctx = &g;
                    callback_data.EventFlag = event_flag;
                    callback_data.Flags = flags;
                    callback_data.UserData = callback_user_data;

                    // FIXME-OPT: Undo stack reconcile needs a backup of the data until we rework API, see #7925
                    char* callback_buf = is_readonly ? buf : state->TextA.Data;
                    IM_ASSERT(callback_buf == state->TextSrc);
                    state->CallbackTextBackup.resize(state->TextLen + 1);
                    memcpy(state->CallbackTextBackup.Data, callback_buf, state->TextLen + 1);

                    callback_data.EventKey = event_key;
                    callback_data.Buf = callback_buf;
                    callback_data.BufTextLen = state->TextLen;
                    callback_data.BufSize = state->BufCapacity;
                    callback_data.BufDirty = false;

                    const int utf8_cursor_pos = callback_data.CursorPos = state->Stb->cursor;
                    const int utf8_selection_start = callback_data.SelectionStart = state->Stb->select_start;
                    const int utf8_selection_end = callback_data.SelectionEnd = state->Stb->select_end;

                    // Call user code
                    callback(&callback_data);

                    // Read back what user may have modified
                    callback_buf = is_readonly ? buf : state->TextA.Data; // Pointer may have been invalidated by a resize callback
                    IM_ASSERT(callback_data.Buf == callback_buf);         // Invalid to modify those fields
                    IM_ASSERT(callback_data.BufSize == state->BufCapacity);
                    IM_ASSERT(callback_data.Flags == flags);
                    const bool buf_dirty = callback_data.BufDirty;
                    if (callback_data.CursorPos != utf8_cursor_pos || buf_dirty)            { state->Stb->cursor = callback_data.CursorPos; state->CursorFollow = true; }
                    if (callback_data.SelectionStart != utf8_selection_start || buf_dirty)  { state->Stb->select_start = (callback_data.SelectionStart == callback_data.CursorPos) ? state->Stb->cursor : callback_data.SelectionStart; }
                    if (callback_data.SelectionEnd != utf8_selection_end || buf_dirty)      { state->Stb->select_end = (callback_data.SelectionEnd == callback_data.SelectionStart) ? state->Stb->select_start : callback_data.SelectionEnd; }
                    if (buf_dirty)
                    {
                        // Callback may update buffer and thus set buf_dirty even in read-only mode.
                        IM_ASSERT(callback_data.BufTextLen == (int)UxImStrlen(callback_data.Buf)); // You need to maintain BufTextLen if you change the text!
                        InputTextReconcileUndoState(state, state->CallbackTextBackup.Data, state->CallbackTextBackup.Size - 1, callback_data.Buf, callback_data.BufTextLen);
                        state->TextLen = callback_data.BufTextLen;  // Assume correct length and valid UTF-8 from user, saves us an extra strlen()
                        state->CursorAnimReset();
                    }
                }
            }

            // Will copy result string if modified
            if (!is_readonly && strcmp(state->TextSrc, buf) != 0)
            {
                apply_new_text = state->TextSrc;
                apply_new_text_length = state->TextLen;
                value_changed = true;
            }
        }
    }

    // Handle reapplying final data on deactivation (see InputTextDeactivateHook() for details)
    if (g.InputTextDeactivatedState.ID == id)
    {
        if (g.ActiveId != id && IsItemDeactivatedAfterEdit() && !is_readonly && strcmp(g.InputTextDeactivatedState.TextA.Data, buf) != 0)
        {
            apply_new_text = g.InputTextDeactivatedState.TextA.Data;
            apply_new_text_length = g.InputTextDeactivatedState.TextA.Size - 1;
            value_changed = true;
            //IMGUI_DEBUG_LOG("InputText(): apply Deactivated data for 0x%08X: \"%.*s\".\n", id, apply_new_text_length, apply_new_text);
        }
        g.InputTextDeactivatedState.ID = 0;
    }

    // Copy result to user buffer. This can currently only happen when (g.ActiveId == id)
    if (apply_new_text != NULL)
    {
        //// We cannot test for 'backup_current_text_length != apply_new_text_length' here because we have no guarantee that the size
        //// of our owned buffer matches the size of the string object held by the user, and by design we allow InputText() to be used
        //// without any storage on user's side.
        IM_ASSERT(apply_new_text_length >= 0);
        if (is_resizable)
        {
            UxImGuiInputTextCallbackData callback_data;
            callback_data.Ctx = &g;
            callback_data.EventFlag = UxImGuiInputTextFlags_CallbackResize;
            callback_data.Flags = flags;
            callback_data.Buf = buf;
            callback_data.BufTextLen = apply_new_text_length;
            callback_data.BufSize = UxImMax(buf_size, apply_new_text_length + 1);
            callback_data.UserData = callback_user_data;
            callback(&callback_data);
            buf = callback_data.Buf;
            buf_size = callback_data.BufSize;
            apply_new_text_length = UxImMin(callback_data.BufTextLen, buf_size - 1);
            IM_ASSERT(apply_new_text_length <= buf_size);
        }
        //IMGUI_DEBUG_PRINT("InputText(\"%s\"): apply_new_text length %d\n", label, apply_new_text_length);

        // If the underlying buffer resize was denied or not carried to the next frame, apply_new_text_length+1 may be >= buf_size.
        UxImStrncpy(buf, apply_new_text, UxImMin(apply_new_text_length + 1, buf_size));
    }

    // Release active ID at the end of the function (so e.g. pressing Return still does a final application of the value)
    // Otherwise request text input ahead for next frame.
    if (g.ActiveId == id && clear_active_id)
        ClearActiveID();

    // Render frame
    if (!is_multiline)
    {
        RenderNavCursor(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(UxImGuiCol_FrameBg), true, style.FrameRounding);
    }

    const UxImVec4 clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + inner_size.x, frame_bb.Min.y + inner_size.y); // Not using frame_bb.Max because we have adjusted size
    UxImVec2 draw_pos = is_multiline ? draw_window->DC.CursorPos : frame_bb.Min + style.FramePadding;
    UxImVec2 text_size(0.0f, 0.0f);

    // Set upper limit of single-line InputTextEx() at 2 million characters strings. The current pathological worst case is a long line
    // without any carriage return, which would makes UxImFont::RenderText() reserve too many vertices and probably crash. Avoid it altogether.
    // Note that we only use this limit on single-line InputText(), so a pathologically large line on a InputTextMultiline() would still crash.
    const int buf_display_max_length = 2 * 1024 * 1024;
    const char* buf_display = buf_display_from_state ? state->TextA.Data : buf; //-V595
    const char* buf_display_end = NULL; // We have specialized paths below for setting the length

    // Display hint when contents is empty
    // At this point we need to handle the possibility that a callback could have modified the underlying buffer (#8368)
    const bool new_is_displaying_hint = (hint != NULL && (buf_display_from_state ? state->TextA.Data : buf)[0] == 0);
    if (new_is_displaying_hint != is_displaying_hint)
    {
        if (is_password && !is_displaying_hint)
            PopPasswordFont();
        is_displaying_hint = new_is_displaying_hint;
        if (is_password && !is_displaying_hint)
            PushPasswordFont();
    }
    if (is_displaying_hint)
    {
        buf_display = hint;
        buf_display_end = hint + UxImStrlen(hint);
    }

    // Render text. We currently only render selection when the widget is active or while scrolling.
    // FIXME: We could remove the '&& render_cursor' to keep rendering selection when inactive.
    if (render_cursor || render_selection)
    {
        IM_ASSERT(state != NULL);
        if (!is_displaying_hint)
            buf_display_end = buf_display + state->TextLen;

        // Render text (with cursor and selection)
        // This is going to be messy. We need to:
        // - Display the text (this alone can be more easily clipped)
        // - Handle scrolling, highlight selection, display cursor (those all requires some form of 1d->2d cursor position calculation)
        // - Measure text height (for scrollbar)
        // We are attempting to do most of that in **one main pass** to minimize the computation cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we could merge them by an extra refactoring effort)
        // FIXME: This should occur on buf_display but we'd need to maintain cursor/select_start/select_end for UTF-8.
        const char* text_begin = buf_display;
        const char* text_end = text_begin + state->TextLen;
        UxImVec2 cursor_offset, select_start_offset;

        {
            // Find lines numbers straddling cursor and selection min position
            int cursor_line_no = render_cursor ? -1 : -1000;
            int selmin_line_no = render_selection ? -1 : -1000;
            const char* cursor_ptr = render_cursor ? text_begin + state->Stb->cursor : NULL;
            const char* selmin_ptr = render_selection ? text_begin + UxImMin(state->Stb->select_start, state->Stb->select_end) : NULL;

            // Count lines and find line number for cursor and selection ends
            int line_count = 1;
            if (is_multiline)
            {
                for (const char* s = text_begin; (s = (const char*)UxImMemchr(s, '\n', (size_t)(text_end - s))) != NULL; s++)
                {
                    if (cursor_line_no == -1 && s >= cursor_ptr) { cursor_line_no = line_count; }
                    if (selmin_line_no == -1 && s >= selmin_ptr) { selmin_line_no = line_count; }
                    line_count++;
                }
            }
            if (cursor_line_no == -1)
                cursor_line_no = line_count;
            if (selmin_line_no == -1)
                selmin_line_no = line_count;

            // Calculate 2d position by finding the beginning of the line and measuring distance
            cursor_offset.x = InputTextCalcTextSize(&g, UxImStrbol(cursor_ptr, text_begin), cursor_ptr).x;
            cursor_offset.y = cursor_line_no * g.FontSize;
            if (selmin_line_no >= 0)
            {
                select_start_offset.x = InputTextCalcTextSize(&g, UxImStrbol(selmin_ptr, text_begin), selmin_ptr).x;
                select_start_offset.y = selmin_line_no * g.FontSize;
            }

            // Store text height (note that we haven't calculated text width at all, see GitHub issues #383, #1224)
            if (is_multiline)
                text_size = UxImVec2(inner_size.x, line_count * g.FontSize);
        }

        // Scroll
        if (render_cursor && state->CursorFollow)
        {
            // Horizontal scroll in chunks of quarter width
            if (!(flags & UxImGuiInputTextFlags_NoHorizontalScroll))
            {
                const float scroll_increment_x = inner_size.x * 0.25f;
                const float visible_width = inner_size.x - style.FramePadding.x;
                if (cursor_offset.x < state->Scroll.x)
                    state->Scroll.x = IM_TRUNC(UxImMax(0.0f, cursor_offset.x - scroll_increment_x));
                else if (cursor_offset.x - visible_width >= state->Scroll.x)
                    state->Scroll.x = IM_TRUNC(cursor_offset.x - visible_width + scroll_increment_x);
            }
            else
            {
                state->Scroll.y = 0.0f;
            }

            // Vertical scroll
            if (is_multiline)
            {
                // Test if cursor is vertically visible
                if (cursor_offset.y - g.FontSize < scroll_y)
                    scroll_y = UxImMax(0.0f, cursor_offset.y - g.FontSize);
                else if (cursor_offset.y - (inner_size.y - style.FramePadding.y * 2.0f) >= scroll_y)
                    scroll_y = cursor_offset.y - inner_size.y + style.FramePadding.y * 2.0f;
                const float scroll_max_y = UxImMax((text_size.y + style.FramePadding.y * 2.0f) - inner_size.y, 0.0f);
                scroll_y = UxImClamp(scroll_y, 0.0f, scroll_max_y);
                draw_pos.y += (draw_window->Scroll.y - scroll_y);   // Manipulate cursor pos immediately avoid a frame of lag
                draw_window->Scroll.y = scroll_y;
            }

            state->CursorFollow = false;
        }

        // Draw selection
        const UxImVec2 draw_scroll = UxImVec2(state->Scroll.x, 0.0f);
        if (render_selection)
        {
            const char* text_selected_begin = text_begin + UxImMin(state->Stb->select_start, state->Stb->select_end);
            const char* text_selected_end = text_begin + UxImMax(state->Stb->select_start, state->Stb->select_end);

            UxImU32 bg_color = GetColorU32(UxImGuiCol_TextSelectedBg, render_cursor ? 1.0f : 0.6f); // FIXME: current code flow mandate that render_cursor is always true here, we are leaving the transparent one for tests.
            float bg_offy_up = is_multiline ? 0.0f : -1.0f;    // FIXME: those offsets should be part of the style? they don't play so well with multi-line selection.
            float bg_offy_dn = is_multiline ? 0.0f : 2.0f;
            UxImVec2 rect_pos = draw_pos + select_start_offset - draw_scroll;
            for (const char* p = text_selected_begin; p < text_selected_end; )
            {
                if (rect_pos.y > clip_rect.w + g.FontSize)
                    break;
                if (rect_pos.y < clip_rect.y)
                {
                    p = (const char*)UxImMemchr((void*)p, '\n', text_selected_end - p);
                    p = p ? p + 1 : text_selected_end;
                }
                else
                {
                    UxImVec2 rect_size = InputTextCalcTextSize(&g, p, text_selected_end, &p, NULL, true);
                    if (rect_size.x <= 0.0f) rect_size.x = IM_TRUNC(g.FontBaked->GetCharAdvance((UxImWchar)' ') * 0.50f); // So we can see selected empty lines
                    UxImRect rect(rect_pos + UxImVec2(0.0f, bg_offy_up - g.FontSize), rect_pos + UxImVec2(rect_size.x, bg_offy_dn));
                    rect.ClipWith(clip_rect);
                    if (rect.Overlaps(clip_rect))
                        draw_window->DrawList->AddRectFilled(rect.Min, rect.Max, bg_color);
                    rect_pos.x = draw_pos.x - draw_scroll.x;
                }
                rect_pos.y += g.FontSize;
            }
        }

        // We test for 'buf_display_max_length' as a way to avoid some pathological cases (e.g. single-line 1 MB string) which would make UxImDrawList crash.
        // FIXME-OPT: Multiline could submit a smaller amount of contents to AddText() since we already iterated through it.
        if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
        {
            UxImU32 col = GetColorU32(is_displaying_hint ? UxImGuiCol_TextDisabled : UxImGuiCol_Text);
            draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos - draw_scroll, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
        }

        // Draw blinking cursor
        if (render_cursor)
        {
            state->CursorAnim += io.DeltaTime;
            bool cursor_is_visible = (!g.IO.ConfigInputTextCursorBlink) || (state->CursorAnim <= 0.0f) || UxImFmod(state->CursorAnim, 1.20f) <= 0.80f;
            UxImVec2 cursor_screen_pos = UxImTrunc(draw_pos + cursor_offset - draw_scroll);
            UxImRect cursor_screen_rect(cursor_screen_pos.x, cursor_screen_pos.y - g.FontSize + 0.5f, cursor_screen_pos.x + 1.0f, cursor_screen_pos.y - 1.5f);
            if (cursor_is_visible && cursor_screen_rect.Overlaps(clip_rect))
                draw_window->DrawList->AddLine(cursor_screen_rect.Min, cursor_screen_rect.GetBL(), GetColorU32(UxImGuiCol_InputTextCursor), 1.0f); // FIXME-DPI: Cursor thickness (#7031)

            // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME can cover our cursor. Bit of an extra nicety.)
            // This is required for some backends (SDL3) to start emitting character/text inputs.
            // As per #6341, make sure we don't set that on the deactivating frame.
            if (!is_readonly && g.ActiveId == id)
            {
                UxImGuiPlatformImeData* ime_data = &g.PlatformImeData; // (this is a public struct, passed to io.Platform_SetImeDataFn() handler)
                ime_data->WantVisible = true;
                ime_data->WantTextInput = true;
                ime_data->InputPos = UxImVec2(cursor_screen_pos.x - 1.0f, cursor_screen_pos.y - g.FontSize);
                ime_data->InputLineHeight = g.FontSize;
                ime_data->ViewportId = window->Viewport->ID;
            }
        }
    }
    else
    {
        // Render text only (no selection, no cursor)
        if (is_multiline)
            text_size = UxImVec2(inner_size.x, InputTextCalcTextLenAndLineCount(buf_display, &buf_display_end) * g.FontSize); // We don't need width
        else if (!is_displaying_hint && g.ActiveId == id)
            buf_display_end = buf_display + state->TextLen;
        else if (!is_displaying_hint)
            buf_display_end = buf_display + UxImStrlen(buf_display);

        if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
        {
            // Find render position for right alignment
            if (flags & UxImGuiInputTextFlags_ElideLeft)
                draw_pos.x = UxImMin(draw_pos.x, frame_bb.Max.x - CalcTextSize(buf_display, NULL).x - style.FramePadding.x);

            const UxImVec2 draw_scroll = /*state ? UxImVec2(state->Scroll.x, 0.0f) :*/ UxImVec2(0.0f, 0.0f); // Preserve scroll when inactive?
            UxImU32 col = GetColorU32(is_displaying_hint ? UxImGuiCol_TextDisabled : UxImGuiCol_Text);
            draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos - draw_scroll, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
        }
    }

    if (is_password && !is_displaying_hint)
        PopPasswordFont();

    if (is_multiline)
    {
        // For focus requests to work on our multiline we need to ensure our child ItemAdd() call specifies the UxImGuiItemFlags_Inputable (see #4761, #7870)...
        Dummy(UxImVec2(text_size.x, text_size.y + style.FramePadding.y));
        g.NextItemData.ItemFlags |= (UxImGuiItemFlags)UxImGuiItemFlags_Inputable | UxImGuiItemFlags_NoTabStop;
        EndChild();
        item_data_backup.StatusFlags |= (g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_HoveredWindow);

        // ...and then we need to undo the group overriding last item data, which gets a bit messy as EndGroup() tries to forward scrollbar being active...
        // FIXME: This quite messy/tricky, should attempt to get rid of the child window.
        EndGroup();
        if (g.LastItemData.ID == 0 || g.LastItemData.ID != GetWindowScrollbarID(draw_window, UxImGuiAxis_Y))
        {
            g.LastItemData.ID = id;
            g.LastItemData.ItemFlags = item_data_backup.ItemFlags;
            g.LastItemData.StatusFlags = item_data_backup.StatusFlags;
        }
    }
    if (state)
        state->TextSrc = NULL;

    // Log as text
    if (g.LogEnabled && (!is_password || is_displaying_hint))
    {
        LogSetNextTextDecoration("{", "}");
        LogRenderedText(&draw_pos, buf_display, buf_display_end);
    }

    if (label_size.x > 0)
        RenderText(UxImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    if (value_changed)
        MarkItemEdited(id);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | UxImGuiItemStatusFlags_Inputable);
    if ((flags & UxImGuiInputTextFlags_EnterReturnsTrue) != 0)
        return validated;
    else
        return value_changed;
}

void UxImGui::DebugNodeInputTextState(UxImGuiInputTextState* state)
{
#ifndef IMGUI_DISABLE_DEBUG_TOOLS
    UxImGuiContext& g = *GUxImGui;
    UxImStb::STB_TexteditState* stb_state = state->Stb;
    UxImStb::StbUndoState* undo_state = &stb_state->undostate;
    Text("ID: 0x%08X, ActiveID: 0x%08X", state->ID, g.ActiveId);
    DebugLocateItemOnHover(state->ID);
    Text("CurLenA: %d, Cursor: %d, Selection: %d..%d", state->TextLen, stb_state->cursor, stb_state->select_start, stb_state->select_end);
    Text("BufCapacityA: %d", state->BufCapacity);
    Text("(Internal Buffer: TextA Size: %d, Capacity: %d)", state->TextA.Size, state->TextA.Capacity);
    Text("has_preferred_x: %d (%.2f)", stb_state->has_preferred_x, stb_state->preferred_x);
    Text("undo_point: %d, redo_point: %d, undo_char_point: %d, redo_char_point: %d", undo_state->undo_point, undo_state->redo_point, undo_state->undo_char_point, undo_state->redo_char_point);
    if (BeginChild("undopoints", UxImVec2(0.0f, GetTextLineHeight() * 10), UxImGuiChildFlags_Borders | UxImGuiChildFlags_ResizeY)) // Visualize undo state
    {
        PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(0, 0));
        for (int n = 0; n < IMSTB_TEXTEDIT_UNDOSTATECOUNT; n++)
        {
            UxImStb::StbUndoRecord* undo_rec = &undo_state->undo_rec[n];
            const char undo_rec_type = (n < undo_state->undo_point) ? 'u' : (n >= undo_state->redo_point) ? 'r' : ' ';
            if (undo_rec_type == ' ')
                BeginDisabled();
            const int buf_preview_len = (undo_rec_type != ' ' && undo_rec->char_storage != -1) ? undo_rec->insert_length : 0;
            const char* buf_preview_str = undo_state->undo_char + undo_rec->char_storage;
            Text("%c [%02d] where %03d, insert %03d, delete %03d, char_storage %03d \"%.*s\"",
                undo_rec_type, n, undo_rec->where, undo_rec->insert_length, undo_rec->delete_length, undo_rec->char_storage, buf_preview_len, buf_preview_str);
            if (undo_rec_type == ' ')
                EndDisabled();
        }
        PopStyleVar();
    }
    EndChild();
#else
    IM_UNUSED(state);
#endif
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: ColorEdit, ColorPicker, ColorButton, etc.
//-------------------------------------------------------------------------
// - ColorEdit3()
// - ColorEdit4()
// - ColorPicker3()
// - RenderColorRectWithAlphaCheckerboard() [Internal]
// - ColorPicker4()
// - ColorButton()
// - SetColorEditOptions()
// - ColorTooltip() [Internal]
// - ColorEditOptionsPopup() [Internal]
// - ColorPickerOptionsPopup() [Internal]
//-------------------------------------------------------------------------

bool UxImGui::ColorEdit3(const char* label, float col[3], UxImGuiColorEditFlags flags)
{
    return ColorEdit4(label, col, flags | UxImGuiColorEditFlags_NoAlpha);
}

static void ColorEditRestoreH(const float* col, float* H)
{
    UxImGuiContext& g = *GUxImGui;
    IM_ASSERT(g.ColorEditCurrentID != 0);
    if (g.ColorEditSavedID != g.ColorEditCurrentID || g.ColorEditSavedColor != UxImGui::ColorConvertFloat4ToU32(UxImVec4(col[0], col[1], col[2], 0)))
        return;
    *H = g.ColorEditSavedHue;
}

// ColorEdit supports RGB and HSV inputs. In case of RGB input resulting color may have undefined hue and/or saturation.
// Since widget displays both RGB and HSV values we must preserve hue and saturation to prevent these values resetting.
static void ColorEditRestoreHS(const float* col, float* H, float* S, float* V)
{
    UxImGuiContext& g = *GUxImGui;
    IM_ASSERT(g.ColorEditCurrentID != 0);
    if (g.ColorEditSavedID != g.ColorEditCurrentID || g.ColorEditSavedColor != UxImGui::ColorConvertFloat4ToU32(UxImVec4(col[0], col[1], col[2], 0)))
        return;

    // When S == 0, H is undefined.
    // When H == 1 it wraps around to 0.
    if (*S == 0.0f || (*H == 0.0f && g.ColorEditSavedHue == 1))
        *H = g.ColorEditSavedHue;

    // When V == 0, S is undefined.
    if (*V == 0.0f)
        *S = g.ColorEditSavedSat;
}

// Edit colors components (each component in 0.0f..1.0f range).
// See enum UxImGuiColorEditFlags_ for available options. e.g. Only access 3 floats if UxImGuiColorEditFlags_NoAlpha flag is set.
// With typical options: Left-click on color square to open color picker. Right-click to open option menu. CTRL+Click over input fields to edit them and TAB to go to next item.
bool UxImGui::ColorEdit4(const char* label, float col[4], UxImGuiColorEditFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const float square_sz = GetFrameHeight();
    const char* label_display_end = FindRenderedTextEnd(label);
    float w_full = CalcItemWidth();
    g.NextItemData.ClearFlags();

    BeginGroup();
    PushID(label);
    const bool set_current_color_edit_id = (g.ColorEditCurrentID == 0);
    if (set_current_color_edit_id)
        g.ColorEditCurrentID = window->IDStack.back();

    // If we're not showing any slider there's no point in doing any HSV conversions
    const UxImGuiColorEditFlags flags_untouched = flags;
    if (flags & UxImGuiColorEditFlags_NoInputs)
        flags = (flags & (~UxImGuiColorEditFlags_DisplayMask_)) | UxImGuiColorEditFlags_DisplayRGB | UxImGuiColorEditFlags_NoOptions;

    // Context menu: display and modify options (before defaults are applied)
    if (!(flags & UxImGuiColorEditFlags_NoOptions))
        ColorEditOptionsPopup(col, flags);

    // Read stored options
    if (!(flags & UxImGuiColorEditFlags_DisplayMask_))
        flags |= (g.ColorEditOptions & UxImGuiColorEditFlags_DisplayMask_);
    if (!(flags & UxImGuiColorEditFlags_DataTypeMask_))
        flags |= (g.ColorEditOptions & UxImGuiColorEditFlags_DataTypeMask_);
    if (!(flags & UxImGuiColorEditFlags_PickerMask_))
        flags |= (g.ColorEditOptions & UxImGuiColorEditFlags_PickerMask_);
    if (!(flags & UxImGuiColorEditFlags_InputMask_))
        flags |= (g.ColorEditOptions & UxImGuiColorEditFlags_InputMask_);
    flags |= (g.ColorEditOptions & ~(UxImGuiColorEditFlags_DisplayMask_ | UxImGuiColorEditFlags_DataTypeMask_ | UxImGuiColorEditFlags_PickerMask_ | UxImGuiColorEditFlags_InputMask_));
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_DisplayMask_)); // Check that only 1 is selected
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_InputMask_));   // Check that only 1 is selected

    const bool alpha = (flags & UxImGuiColorEditFlags_NoAlpha) == 0;
    const bool hdr = (flags & UxImGuiColorEditFlags_HDR) != 0;
    const int components = alpha ? 4 : 3;
    const float w_button = (flags & UxImGuiColorEditFlags_NoSmallPreview) ? 0.0f : (square_sz + style.ItemInnerSpacing.x);
    const float w_inputs = UxImMax(w_full - w_button, 1.0f);
    w_full = w_inputs + w_button;

    // Convert to the formats we need
    float f[4] = { col[0], col[1], col[2], alpha ? col[3] : 1.0f };
    if ((flags & UxImGuiColorEditFlags_InputHSV) && (flags & UxImGuiColorEditFlags_DisplayRGB))
        ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
    else if ((flags & UxImGuiColorEditFlags_InputRGB) && (flags & UxImGuiColorEditFlags_DisplayHSV))
    {
        // Hue is lost when converting from grayscale rgb (saturation=0). Restore it.
        ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);
        ColorEditRestoreHS(col, &f[0], &f[1], &f[2]);
    }
    int i[4] = { IM_F32_TO_INT8_UNBOUND(f[0]), IM_F32_TO_INT8_UNBOUND(f[1]), IM_F32_TO_INT8_UNBOUND(f[2]), IM_F32_TO_INT8_UNBOUND(f[3]) };

    bool value_changed = false;
    bool value_changed_as_float = false;

    const UxImVec2 pos = window->DC.CursorPos;
    const float inputs_offset_x = (style.ColorButtonPosition == UxImGuiDir_Left) ? w_button : 0.0f;
    window->DC.CursorPos.x = pos.x + inputs_offset_x;

    if ((flags & (UxImGuiColorEditFlags_DisplayRGB | UxImGuiColorEditFlags_DisplayHSV)) != 0 && (flags & UxImGuiColorEditFlags_NoInputs) == 0)
    {
        // RGB/HSV 0..255 Sliders
        const float w_items = w_inputs - style.ItemInnerSpacing.x * (components - 1);

        const bool hide_prefix = (IM_TRUNC(w_items / components) <= CalcTextSize((flags & UxImGuiColorEditFlags_Float) ? "M:0.000" : "M:000").x);
        static const char* ids[4] = { "##X", "##Y", "##Z", "##W" };
        static const char* fmt_table_int[3][4] =
        {
            {   "%3d",   "%3d",   "%3d",   "%3d" }, // Short display
            { "R:%3d", "G:%3d", "B:%3d", "A:%3d" }, // Long display for RGBA
            { "H:%3d", "S:%3d", "V:%3d", "A:%3d" }  // Long display for HSVA
        };
        static const char* fmt_table_float[3][4] =
        {
            {   "%0.3f",   "%0.3f",   "%0.3f",   "%0.3f" }, // Short display
            { "R:%0.3f", "G:%0.3f", "B:%0.3f", "A:%0.3f" }, // Long display for RGBA
            { "H:%0.3f", "S:%0.3f", "V:%0.3f", "A:%0.3f" }  // Long display for HSVA
        };
        const int fmt_idx = hide_prefix ? 0 : (flags & UxImGuiColorEditFlags_DisplayHSV) ? 2 : 1;

        float prev_split = 0.0f;
        for (int n = 0; n < components; n++)
        {
            if (n > 0)
                SameLine(0, style.ItemInnerSpacing.x);
            float next_split = IM_TRUNC(w_items * (n + 1) / components);
            SetNextItemWidth(UxImMax(next_split - prev_split, 1.0f));
            prev_split = next_split;

            // FIXME: When UxImGuiColorEditFlags_HDR flag is passed HS values snap in weird ways when SV values go below 0.
            if (flags & UxImGuiColorEditFlags_Float)
            {
                value_changed |= DragFloat(ids[n], &f[n], 1.0f / 255.0f, 0.0f, hdr ? 0.0f : 1.0f, fmt_table_float[fmt_idx][n]);
                value_changed_as_float |= value_changed;
            }
            else
            {
                value_changed |= DragInt(ids[n], &i[n], 1.0f, 0, hdr ? 0 : 255, fmt_table_int[fmt_idx][n]);
            }
            if (!(flags & UxImGuiColorEditFlags_NoOptions))
                OpenPopupOnItemClick("context", UxImGuiPopupFlags_MouseButtonRight);
        }
    }
    else if ((flags & UxImGuiColorEditFlags_DisplayHex) != 0 && (flags & UxImGuiColorEditFlags_NoInputs) == 0)
    {
        // RGB Hexadecimal Input
        char buf[64];
        if (alpha)
            UxImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X%02X", UxImClamp(i[0], 0, 255), UxImClamp(i[1], 0, 255), UxImClamp(i[2], 0, 255), UxImClamp(i[3], 0, 255));
        else
            UxImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", UxImClamp(i[0], 0, 255), UxImClamp(i[1], 0, 255), UxImClamp(i[2], 0, 255));
        SetNextItemWidth(w_inputs);
        if (InputText("##Text", buf, IM_ARRAYSIZE(buf), UxImGuiInputTextFlags_CharsUppercase))
        {
            value_changed = true;
            char* p = buf;
            while (*p == '#' || UxImCharIsBlankA(*p))
                p++;
            i[0] = i[1] = i[2] = 0;
            i[3] = 0xFF; // alpha default to 255 is not parsed by scanf (e.g. inputting #FFFFFF omitting alpha)
            int r;
            if (alpha)
                r = sscanf(p, "%02X%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2], (unsigned int*)&i[3]); // Treat at unsigned (%X is unsigned)
            else
                r = sscanf(p, "%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2]);
            IM_UNUSED(r); // Fixes C6031: Return value ignored: 'sscanf'.
        }
        if (!(flags & UxImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context", UxImGuiPopupFlags_MouseButtonRight);
    }

    UxImGuiWindow* picker_active_window = NULL;
    if (!(flags & UxImGuiColorEditFlags_NoSmallPreview))
    {
        const float button_offset_x = ((flags & UxImGuiColorEditFlags_NoInputs) || (style.ColorButtonPosition == UxImGuiDir_Left)) ? 0.0f : w_inputs + style.ItemInnerSpacing.x;
        window->DC.CursorPos = UxImVec2(pos.x + button_offset_x, pos.y);

        const UxImVec4 col_v4(col[0], col[1], col[2], alpha ? col[3] : 1.0f);
        if (ColorButton("##ColorButton", col_v4, flags))
        {
            if (!(flags & UxImGuiColorEditFlags_NoPicker))
            {
                // Store current color and open a picker
                g.ColorPickerRef = col_v4;
                OpenPopup("picker");
                SetNextWindowPos(g.LastItemData.Rect.GetBL() + UxImVec2(0.0f, style.ItemSpacing.y));
            }
        }
        if (!(flags & UxImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context", UxImGuiPopupFlags_MouseButtonRight);

        if (BeginPopup("picker"))
        {
            if (g.CurrentWindow->BeginCount == 1)
            {
                picker_active_window = g.CurrentWindow;
                if (label != label_display_end)
                {
                    TextEx(label, label_display_end);
                    Spacing();
                }
                UxImGuiColorEditFlags picker_flags_to_forward = UxImGuiColorEditFlags_DataTypeMask_ | UxImGuiColorEditFlags_PickerMask_ | UxImGuiColorEditFlags_InputMask_ | UxImGuiColorEditFlags_HDR | UxImGuiColorEditFlags_NoAlpha | UxImGuiColorEditFlags_AlphaBar;
                UxImGuiColorEditFlags picker_flags = (flags_untouched & picker_flags_to_forward) | UxImGuiColorEditFlags_DisplayMask_ | UxImGuiColorEditFlags_NoLabel | UxImGuiColorEditFlags_AlphaPreviewHalf;
                SetNextItemWidth(square_sz * 12.0f); // Use 256 + bar sizes?
                value_changed |= ColorPicker4("##picker", col, picker_flags, &g.ColorPickerRef.x);
            }
            EndPopup();
        }
    }

    if (label != label_display_end && !(flags & UxImGuiColorEditFlags_NoLabel))
    {
        // Position not necessarily next to last submitted button (e.g. if style.ColorButtonPosition == UxImGuiDir_Left),
        // but we need to use SameLine() to setup baseline correctly. Might want to refactor SameLine() to simplify this.
        SameLine(0.0f, style.ItemInnerSpacing.x);
        window->DC.CursorPos.x = pos.x + ((flags & UxImGuiColorEditFlags_NoInputs) ? w_button : w_full + style.ItemInnerSpacing.x);
        TextEx(label, label_display_end);
    }

    // Convert back
    if (value_changed && picker_active_window == NULL)
    {
        if (!value_changed_as_float)
            for (int n = 0; n < 4; n++)
                f[n] = i[n] / 255.0f;
        if ((flags & UxImGuiColorEditFlags_DisplayHSV) && (flags & UxImGuiColorEditFlags_InputRGB))
        {
            g.ColorEditSavedHue = f[0];
            g.ColorEditSavedSat = f[1];
            ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
            g.ColorEditSavedID = g.ColorEditCurrentID;
            g.ColorEditSavedColor = ColorConvertFloat4ToU32(UxImVec4(f[0], f[1], f[2], 0));
        }
        if ((flags & UxImGuiColorEditFlags_DisplayRGB) && (flags & UxImGuiColorEditFlags_InputHSV))
            ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);

        col[0] = f[0];
        col[1] = f[1];
        col[2] = f[2];
        if (alpha)
            col[3] = f[3];
    }

    if (set_current_color_edit_id)
        g.ColorEditCurrentID = 0;
    PopID();
    EndGroup();

    // Drag and Drop Target
    // NB: The flag test is merely an optional micro-optimization, BeginDragDropTarget() does the same test.
    if ((g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_HoveredRect) && !(g.LastItemData.ItemFlags & UxImGuiItemFlags_ReadOnly) && !(flags & UxImGuiColorEditFlags_NoDragDrop) && BeginDragDropTarget())
    {
        bool accepted_drag_drop = false;
        if (const UxImGuiPayload* payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
        {
            memcpy((float*)col, payload->Data, sizeof(float) * 3); // Preserve alpha if any //-V512 //-V1086
            value_changed = accepted_drag_drop = true;
        }
        if (const UxImGuiPayload* payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
        {
            memcpy((float*)col, payload->Data, sizeof(float) * components);
            value_changed = accepted_drag_drop = true;
        }

        // Drag-drop payloads are always RGB
        if (accepted_drag_drop && (flags & UxImGuiColorEditFlags_InputHSV))
            ColorConvertRGBtoHSV(col[0], col[1], col[2], col[0], col[1], col[2]);
        EndDragDropTarget();
    }

    // When picker is being actively used, use its active id so IsItemActive() will function on ColorEdit4().
    if (picker_active_window && g.ActiveId != 0 && g.ActiveIdWindow == picker_active_window)
        g.LastItemData.ID = g.ActiveId;

    if (value_changed && g.LastItemData.ID != 0) // In case of ID collision, the second EndGroup() won't catch g.ActiveId
        MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

bool UxImGui::ColorPicker3(const char* label, float col[3], UxImGuiColorEditFlags flags)
{
    float col4[4] = { col[0], col[1], col[2], 1.0f };
    if (!ColorPicker4(label, col4, flags | UxImGuiColorEditFlags_NoAlpha))
        return false;
    col[0] = col4[0]; col[1] = col4[1]; col[2] = col4[2];
    return true;
}

// Helper for ColorPicker4()
static void RenderArrowsForVerticalBar(UxImDrawList* draw_list, UxImVec2 pos, UxImVec2 half_sz, float bar_w, float alpha)
{
    UxImU32 alpha8 = IM_F32_TO_INT8_SAT(alpha);
    UxImGui::RenderArrowPointingAt(draw_list, UxImVec2(pos.x + half_sz.x + 1,         pos.y), UxImVec2(half_sz.x + 2, half_sz.y + 1), UxImGuiDir_Right, IM_COL32(0,0,0,alpha8));
    UxImGui::RenderArrowPointingAt(draw_list, UxImVec2(pos.x + half_sz.x,             pos.y), half_sz,                              UxImGuiDir_Right, IM_COL32(255,255,255,alpha8));
    UxImGui::RenderArrowPointingAt(draw_list, UxImVec2(pos.x + bar_w - half_sz.x - 1, pos.y), UxImVec2(half_sz.x + 2, half_sz.y + 1), UxImGuiDir_Left,  IM_COL32(0,0,0,alpha8));
    UxImGui::RenderArrowPointingAt(draw_list, UxImVec2(pos.x + bar_w - half_sz.x,     pos.y), half_sz,                              UxImGuiDir_Left,  IM_COL32(255,255,255,alpha8));
}

// Note: ColorPicker4() only accesses 3 floats if UxImGuiColorEditFlags_NoAlpha flag is set.
// (In C++ the 'float col[4]' notation for a function argument is equivalent to 'float* col', we only specify a size to facilitate understanding of the code.)
// FIXME: we adjust the big color square height based on item width, which may cause a flickering feedback loop (if automatic height makes a vertical scrollbar appears, affecting automatic width..)
// FIXME: this is trying to be aware of style.Alpha but not fully correct. Also, the color wheel will have overlapping glitches with (style.Alpha < 1.0)
bool UxImGui::ColorPicker4(const char* label, float col[4], UxImGuiColorEditFlags flags, const float* ref_col)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImDrawList* draw_list = window->DrawList;
    UxImGuiStyle& style = g.Style;
    UxImGuiIO& io = g.IO;

    const float width = CalcItemWidth();
    const bool is_readonly = ((g.NextItemData.ItemFlags | g.CurrentItemFlags) & UxImGuiItemFlags_ReadOnly) != 0;
    g.NextItemData.ClearFlags();

    PushID(label);
    const bool set_current_color_edit_id = (g.ColorEditCurrentID == 0);
    if (set_current_color_edit_id)
        g.ColorEditCurrentID = window->IDStack.back();
    BeginGroup();

    if (!(flags & UxImGuiColorEditFlags_NoSidePreview))
        flags |= UxImGuiColorEditFlags_NoSmallPreview;

    // Context menu: display and store options.
    if (!(flags & UxImGuiColorEditFlags_NoOptions))
        ColorPickerOptionsPopup(col, flags);

    // Read stored options
    if (!(flags & UxImGuiColorEditFlags_PickerMask_))
        flags |= ((g.ColorEditOptions & UxImGuiColorEditFlags_PickerMask_) ? g.ColorEditOptions : UxImGuiColorEditFlags_DefaultOptions_) & UxImGuiColorEditFlags_PickerMask_;
    if (!(flags & UxImGuiColorEditFlags_InputMask_))
        flags |= ((g.ColorEditOptions & UxImGuiColorEditFlags_InputMask_) ? g.ColorEditOptions : UxImGuiColorEditFlags_DefaultOptions_) & UxImGuiColorEditFlags_InputMask_;
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_PickerMask_)); // Check that only 1 is selected
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_InputMask_));  // Check that only 1 is selected
    if (!(flags & UxImGuiColorEditFlags_NoOptions))
        flags |= (g.ColorEditOptions & UxImGuiColorEditFlags_AlphaBar);

    // Setup
    int components = (flags & UxImGuiColorEditFlags_NoAlpha) ? 3 : 4;
    bool alpha_bar = (flags & UxImGuiColorEditFlags_AlphaBar) && !(flags & UxImGuiColorEditFlags_NoAlpha);
    UxImVec2 picker_pos = window->DC.CursorPos;
    float square_sz = GetFrameHeight();
    float bars_width = square_sz; // Arbitrary smallish width of Hue/Alpha picking bars
    float sv_picker_size = UxImMax(bars_width * 1, width - (alpha_bar ? 2 : 1) * (bars_width + style.ItemInnerSpacing.x)); // Saturation/Value picking box
    float bar0_pos_x = picker_pos.x + sv_picker_size + style.ItemInnerSpacing.x;
    float bar1_pos_x = bar0_pos_x + bars_width + style.ItemInnerSpacing.x;
    float bars_triangles_half_sz = IM_TRUNC(bars_width * 0.20f);

    float backup_initial_col[4];
    memcpy(backup_initial_col, col, components * sizeof(float));

    float wheel_thickness = sv_picker_size * 0.08f;
    float wheel_r_outer = sv_picker_size * 0.50f;
    float wheel_r_inner = wheel_r_outer - wheel_thickness;
    UxImVec2 wheel_center(picker_pos.x + (sv_picker_size + bars_width)*0.5f, picker_pos.y + sv_picker_size * 0.5f);

    // Note: the triangle is displayed rotated with triangle_pa pointing to Hue, but most coordinates stays unrotated for logic.
    float triangle_r = wheel_r_inner - (int)(sv_picker_size * 0.027f);
    UxImVec2 triangle_pa = UxImVec2(triangle_r, 0.0f); // Hue point.
    UxImVec2 triangle_pb = UxImVec2(triangle_r * -0.5f, triangle_r * -0.866025f); // Black point.
    UxImVec2 triangle_pc = UxImVec2(triangle_r * -0.5f, triangle_r * +0.866025f); // White point.

    float H = col[0], S = col[1], V = col[2];
    float R = col[0], G = col[1], B = col[2];
    if (flags & UxImGuiColorEditFlags_InputRGB)
    {
        // Hue is lost when converting from grayscale rgb (saturation=0). Restore it.
        ColorConvertRGBtoHSV(R, G, B, H, S, V);
        ColorEditRestoreHS(col, &H, &S, &V);
    }
    else if (flags & UxImGuiColorEditFlags_InputHSV)
    {
        ColorConvertHSVtoRGB(H, S, V, R, G, B);
    }

    bool value_changed = false, value_changed_h = false, value_changed_sv = false;

    PushItemFlag(UxImGuiItemFlags_NoNav, true);
    if (flags & UxImGuiColorEditFlags_PickerHueWheel)
    {
        // Hue wheel + SV triangle logic
        InvisibleButton("hsv", UxImVec2(sv_picker_size + style.ItemInnerSpacing.x + bars_width, sv_picker_size));
        if (IsItemActive() && !is_readonly)
        {
            UxImVec2 initial_off = g.IO.MouseClickedPos[0] - wheel_center;
            UxImVec2 current_off = g.IO.MousePos - wheel_center;
            float initial_dist2 = UxImLengthSqr(initial_off);
            if (initial_dist2 >= (wheel_r_inner - 1) * (wheel_r_inner - 1) && initial_dist2 <= (wheel_r_outer + 1) * (wheel_r_outer + 1))
            {
                // Interactive with Hue wheel
                H = UxImAtan2(current_off.y, current_off.x) / IM_PI * 0.5f;
                if (H < 0.0f)
                    H += 1.0f;
                value_changed = value_changed_h = true;
            }
            float cos_hue_angle = UxImCos(-H * 2.0f * IM_PI);
            float sin_hue_angle = UxImSin(-H * 2.0f * IM_PI);
            if (UxImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, UxImRotate(initial_off, cos_hue_angle, sin_hue_angle)))
            {
                // Interacting with SV triangle
                UxImVec2 current_off_unrotated = UxImRotate(current_off, cos_hue_angle, sin_hue_angle);
                if (!UxImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated))
                    current_off_unrotated = UxImTriangleClosestPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated);
                float uu, vv, ww;
                UxImTriangleBarycentricCoords(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated, uu, vv, ww);
                V = UxImClamp(1.0f - vv, 0.0001f, 1.0f);
                S = UxImClamp(uu / V, 0.0001f, 1.0f);
                value_changed = value_changed_sv = true;
            }
        }
        if (!(flags & UxImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context", UxImGuiPopupFlags_MouseButtonRight);
    }
    else if (flags & UxImGuiColorEditFlags_PickerHueBar)
    {
        // SV rectangle logic
        InvisibleButton("sv", UxImVec2(sv_picker_size, sv_picker_size));
        if (IsItemActive() && !is_readonly)
        {
            S = UxImSaturate((io.MousePos.x - picker_pos.x) / (sv_picker_size - 1));
            V = 1.0f - UxImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
            ColorEditRestoreH(col, &H); // Greatly reduces hue jitter and reset to 0 when hue == 255 and color is rapidly modified using SV square.
            value_changed = value_changed_sv = true;
        }
        if (!(flags & UxImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context", UxImGuiPopupFlags_MouseButtonRight);

        // Hue bar logic
        SetCursorScreenPos(UxImVec2(bar0_pos_x, picker_pos.y));
        InvisibleButton("hue", UxImVec2(bars_width, sv_picker_size));
        if (IsItemActive() && !is_readonly)
        {
            H = UxImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
            value_changed = value_changed_h = true;
        }
    }

    // Alpha bar logic
    if (alpha_bar)
    {
        SetCursorScreenPos(UxImVec2(bar1_pos_x, picker_pos.y));
        InvisibleButton("alpha", UxImVec2(bars_width, sv_picker_size));
        if (IsItemActive())
        {
            col[3] = 1.0f - UxImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
            value_changed = true;
        }
    }
    PopItemFlag(); // UxImGuiItemFlags_NoNav

    if (!(flags & UxImGuiColorEditFlags_NoSidePreview))
    {
        SameLine(0, style.ItemInnerSpacing.x);
        BeginGroup();
    }

    if (!(flags & UxImGuiColorEditFlags_NoLabel))
    {
        const char* label_display_end = FindRenderedTextEnd(label);
        if (label != label_display_end)
        {
            if ((flags & UxImGuiColorEditFlags_NoSidePreview))
                SameLine(0, style.ItemInnerSpacing.x);
            TextEx(label, label_display_end);
        }
    }

    if (!(flags & UxImGuiColorEditFlags_NoSidePreview))
    {
        PushItemFlag(UxImGuiItemFlags_NoNavDefaultFocus, true);
        UxImVec4 col_v4(col[0], col[1], col[2], (flags & UxImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
        if ((flags & UxImGuiColorEditFlags_NoLabel))
            Text("Current");

        UxImGuiColorEditFlags sub_flags_to_forward = UxImGuiColorEditFlags_InputMask_ | UxImGuiColorEditFlags_HDR | UxImGuiColorEditFlags_AlphaMask_ | UxImGuiColorEditFlags_NoTooltip;
        ColorButton("##current", col_v4, (flags & sub_flags_to_forward), UxImVec2(square_sz * 3, square_sz * 2));
        if (ref_col != NULL)
        {
            Text("Original");
            UxImVec4 ref_col_v4(ref_col[0], ref_col[1], ref_col[2], (flags & UxImGuiColorEditFlags_NoAlpha) ? 1.0f : ref_col[3]);
            if (ColorButton("##original", ref_col_v4, (flags & sub_flags_to_forward), UxImVec2(square_sz * 3, square_sz * 2)))
            {
                memcpy(col, ref_col, components * sizeof(float));
                value_changed = true;
            }
        }
        PopItemFlag();
        EndGroup();
    }

    // Convert back color to RGB
    if (value_changed_h || value_changed_sv)
    {
        if (flags & UxImGuiColorEditFlags_InputRGB)
        {
            ColorConvertHSVtoRGB(H, S, V, col[0], col[1], col[2]);
            g.ColorEditSavedHue = H;
            g.ColorEditSavedSat = S;
            g.ColorEditSavedID = g.ColorEditCurrentID;
            g.ColorEditSavedColor = ColorConvertFloat4ToU32(UxImVec4(col[0], col[1], col[2], 0));
        }
        else if (flags & UxImGuiColorEditFlags_InputHSV)
        {
            col[0] = H;
            col[1] = S;
            col[2] = V;
        }
    }

    // R,G,B and H,S,V slider color editor
    bool value_changed_fix_hue_wrap = false;
    if ((flags & UxImGuiColorEditFlags_NoInputs) == 0)
    {
        PushItemWidth((alpha_bar ? bar1_pos_x : bar0_pos_x) + bars_width - picker_pos.x);
        UxImGuiColorEditFlags sub_flags_to_forward = UxImGuiColorEditFlags_DataTypeMask_ | UxImGuiColorEditFlags_InputMask_ | UxImGuiColorEditFlags_HDR | UxImGuiColorEditFlags_AlphaMask_ | UxImGuiColorEditFlags_NoOptions | UxImGuiColorEditFlags_NoTooltip | UxImGuiColorEditFlags_NoSmallPreview;
        UxImGuiColorEditFlags sub_flags = (flags & sub_flags_to_forward) | UxImGuiColorEditFlags_NoPicker;
        if (flags & UxImGuiColorEditFlags_DisplayRGB || (flags & UxImGuiColorEditFlags_DisplayMask_) == 0)
            if (ColorEdit4("##rgb", col, sub_flags | UxImGuiColorEditFlags_DisplayRGB))
            {
                // FIXME: Hackily differentiating using the DragInt (ActiveId != 0 && !ActiveIdAllowOverlap) vs. using the InputText or DropTarget.
                // For the later we don't want to run the hue-wrap canceling code. If you are well versed in HSV picker please provide your input! (See #2050)
                value_changed_fix_hue_wrap = (g.ActiveId != 0 && !g.ActiveIdAllowOverlap);
                value_changed = true;
            }
        if (flags & UxImGuiColorEditFlags_DisplayHSV || (flags & UxImGuiColorEditFlags_DisplayMask_) == 0)
            value_changed |= ColorEdit4("##hsv", col, sub_flags | UxImGuiColorEditFlags_DisplayHSV);
        if (flags & UxImGuiColorEditFlags_DisplayHex || (flags & UxImGuiColorEditFlags_DisplayMask_) == 0)
            value_changed |= ColorEdit4("##hex", col, sub_flags | UxImGuiColorEditFlags_DisplayHex);
        PopItemWidth();
    }

    // Try to cancel hue wrap (after ColorEdit4 call), if any
    if (value_changed_fix_hue_wrap && (flags & UxImGuiColorEditFlags_InputRGB))
    {
        float new_H, new_S, new_V;
        ColorConvertRGBtoHSV(col[0], col[1], col[2], new_H, new_S, new_V);
        if (new_H <= 0 && H > 0)
        {
            if (new_V <= 0 && V != new_V)
                ColorConvertHSVtoRGB(H, S, new_V <= 0 ? V * 0.5f : new_V, col[0], col[1], col[2]);
            else if (new_S <= 0)
                ColorConvertHSVtoRGB(H, new_S <= 0 ? S * 0.5f : new_S, new_V, col[0], col[1], col[2]);
        }
    }

    if (value_changed)
    {
        if (flags & UxImGuiColorEditFlags_InputRGB)
        {
            R = col[0];
            G = col[1];
            B = col[2];
            ColorConvertRGBtoHSV(R, G, B, H, S, V);
            ColorEditRestoreHS(col, &H, &S, &V);   // Fix local Hue as display below will use it immediately.
        }
        else if (flags & UxImGuiColorEditFlags_InputHSV)
        {
            H = col[0];
            S = col[1];
            V = col[2];
            ColorConvertHSVtoRGB(H, S, V, R, G, B);
        }
    }

    const int style_alpha8 = IM_F32_TO_INT8_SAT(style.Alpha);
    const UxImU32 col_black = IM_COL32(0,0,0,style_alpha8);
    const UxImU32 col_white = IM_COL32(255,255,255,style_alpha8);
    const UxImU32 col_midgrey = IM_COL32(128,128,128,style_alpha8);
    const UxImU32 col_hues[6 + 1] = { IM_COL32(255,0,0,style_alpha8), IM_COL32(255,255,0,style_alpha8), IM_COL32(0,255,0,style_alpha8), IM_COL32(0,255,255,style_alpha8), IM_COL32(0,0,255,style_alpha8), IM_COL32(255,0,255,style_alpha8), IM_COL32(255,0,0,style_alpha8) };

    UxImVec4 hue_color_f(1, 1, 1, style.Alpha); ColorConvertHSVtoRGB(H, 1, 1, hue_color_f.x, hue_color_f.y, hue_color_f.z);
    UxImU32 hue_color32 = ColorConvertFloat4ToU32(hue_color_f);
    UxImU32 user_col32_striped_of_alpha = ColorConvertFloat4ToU32(UxImVec4(R, G, B, style.Alpha)); // Important: this is still including the main rendering/style alpha!!

    UxImVec2 sv_cursor_pos;

    if (flags & UxImGuiColorEditFlags_PickerHueWheel)
    {
        // Render Hue Wheel
        const float aeps = 0.5f / wheel_r_outer; // Half a pixel arc length in radians (2pi cancels out).
        const int segment_per_arc = UxImMax(4, (int)wheel_r_outer / 12);
        for (int n = 0; n < 6; n++)
        {
            const float a0 = (n)     /6.0f * 2.0f * IM_PI - aeps;
            const float a1 = (n+1.0f)/6.0f * 2.0f * IM_PI + aeps;
            const int vert_start_idx = draw_list->VtxBuffer.Size;
            draw_list->PathArcTo(wheel_center, (wheel_r_inner + wheel_r_outer)*0.5f, a0, a1, segment_per_arc);
            draw_list->PathStroke(col_white, 0, wheel_thickness);
            const int vert_end_idx = draw_list->VtxBuffer.Size;

            // Paint colors over existing vertices
            UxImVec2 gradient_p0(wheel_center.x + UxImCos(a0) * wheel_r_inner, wheel_center.y + UxImSin(a0) * wheel_r_inner);
            UxImVec2 gradient_p1(wheel_center.x + UxImCos(a1) * wheel_r_inner, wheel_center.y + UxImSin(a1) * wheel_r_inner);
            ShadeVertsLinearColorGradientKeepAlpha(draw_list, vert_start_idx, vert_end_idx, gradient_p0, gradient_p1, col_hues[n], col_hues[n + 1]);
        }

        // Render Cursor + preview on Hue Wheel
        float cos_hue_angle = UxImCos(H * 2.0f * IM_PI);
        float sin_hue_angle = UxImSin(H * 2.0f * IM_PI);
        UxImVec2 hue_cursor_pos(wheel_center.x + cos_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f, wheel_center.y + sin_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f);
        float hue_cursor_rad = value_changed_h ? wheel_thickness * 0.65f : wheel_thickness * 0.55f;
        int hue_cursor_segments = draw_list->_CalcCircleAutoSegmentCount(hue_cursor_rad); // Lock segment count so the +1 one matches others.
        draw_list->AddCircleFilled(hue_cursor_pos, hue_cursor_rad, hue_color32, hue_cursor_segments);
        draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad + 1, col_midgrey, hue_cursor_segments);
        draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad, col_white, hue_cursor_segments);

        // Render SV triangle (rotated according to hue)
        UxImVec2 tra = wheel_center + UxImRotate(triangle_pa, cos_hue_angle, sin_hue_angle);
        UxImVec2 trb = wheel_center + UxImRotate(triangle_pb, cos_hue_angle, sin_hue_angle);
        UxImVec2 trc = wheel_center + UxImRotate(triangle_pc, cos_hue_angle, sin_hue_angle);
        UxImVec2 uv_white = GetFontTexUvWhitePixel();
        draw_list->PrimReserve(3, 3);
        draw_list->PrimVtx(tra, uv_white, hue_color32);
        draw_list->PrimVtx(trb, uv_white, col_black);
        draw_list->PrimVtx(trc, uv_white, col_white);
        draw_list->AddTriangle(tra, trb, trc, col_midgrey, 1.5f);
        sv_cursor_pos = UxImLerp(UxImLerp(trc, tra, UxImSaturate(S)), trb, UxImSaturate(1 - V));
    }
    else if (flags & UxImGuiColorEditFlags_PickerHueBar)
    {
        // Render SV Square
        draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + UxImVec2(sv_picker_size, sv_picker_size), col_white, hue_color32, hue_color32, col_white);
        draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + UxImVec2(sv_picker_size, sv_picker_size), 0, 0, col_black, col_black);
        RenderFrameBorder(picker_pos, picker_pos + UxImVec2(sv_picker_size, sv_picker_size), 0.0f);
        sv_cursor_pos.x = UxImClamp(IM_ROUND(picker_pos.x + UxImSaturate(S)     * sv_picker_size), picker_pos.x + 2, picker_pos.x + sv_picker_size - 2); // Sneakily prevent the circle to stick out too much
        sv_cursor_pos.y = UxImClamp(IM_ROUND(picker_pos.y + UxImSaturate(1 - V) * sv_picker_size), picker_pos.y + 2, picker_pos.y + sv_picker_size - 2);

        // Render Hue Bar
        for (int i = 0; i < 6; ++i)
            draw_list->AddRectFilledMultiColor(UxImVec2(bar0_pos_x, picker_pos.y + i * (sv_picker_size / 6)), UxImVec2(bar0_pos_x + bars_width, picker_pos.y + (i + 1) * (sv_picker_size / 6)), col_hues[i], col_hues[i], col_hues[i + 1], col_hues[i + 1]);
        float bar0_line_y = IM_ROUND(picker_pos.y + H * sv_picker_size);
        RenderFrameBorder(UxImVec2(bar0_pos_x, picker_pos.y), UxImVec2(bar0_pos_x + bars_width, picker_pos.y + sv_picker_size), 0.0f);
        RenderArrowsForVerticalBar(draw_list, UxImVec2(bar0_pos_x - 1, bar0_line_y), UxImVec2(bars_triangles_half_sz + 1, bars_triangles_half_sz), bars_width + 2.0f, style.Alpha);
    }

    // Render cursor/preview circle (clamp S/V within 0..1 range because floating points colors may lead HSV values to be out of range)
    float sv_cursor_rad = value_changed_sv ? wheel_thickness * 0.55f : wheel_thickness * 0.40f;
    int sv_cursor_segments = draw_list->_CalcCircleAutoSegmentCount(sv_cursor_rad); // Lock segment count so the +1 one matches others.
    draw_list->AddCircleFilled(sv_cursor_pos, sv_cursor_rad, user_col32_striped_of_alpha, sv_cursor_segments);
    draw_list->AddCircle(sv_cursor_pos, sv_cursor_rad + 1, col_midgrey, sv_cursor_segments);
    draw_list->AddCircle(sv_cursor_pos, sv_cursor_rad, col_white, sv_cursor_segments);

    // Render alpha bar
    if (alpha_bar)
    {
        float alpha = UxImSaturate(col[3]);
        UxImRect bar1_bb(bar1_pos_x, picker_pos.y, bar1_pos_x + bars_width, picker_pos.y + sv_picker_size);
        RenderColorRectWithAlphaCheckerboard(draw_list, bar1_bb.Min, bar1_bb.Max, 0, bar1_bb.GetWidth() / 2.0f, UxImVec2(0.0f, 0.0f));
        draw_list->AddRectFilledMultiColor(bar1_bb.Min, bar1_bb.Max, user_col32_striped_of_alpha, user_col32_striped_of_alpha, user_col32_striped_of_alpha & ~IM_COL32_A_MASK, user_col32_striped_of_alpha & ~IM_COL32_A_MASK);
        float bar1_line_y = IM_ROUND(picker_pos.y + (1.0f - alpha) * sv_picker_size);
        RenderFrameBorder(bar1_bb.Min, bar1_bb.Max, 0.0f);
        RenderArrowsForVerticalBar(draw_list, UxImVec2(bar1_pos_x - 1, bar1_line_y), UxImVec2(bars_triangles_half_sz + 1, bars_triangles_half_sz), bars_width + 2.0f, style.Alpha);
    }

    EndGroup();

    if (value_changed && memcmp(backup_initial_col, col, components * sizeof(float)) == 0)
        value_changed = false;
    if (value_changed && g.LastItemData.ID != 0) // In case of ID collision, the second EndGroup() won't catch g.ActiveId
        MarkItemEdited(g.LastItemData.ID);

    if (set_current_color_edit_id)
        g.ColorEditCurrentID = 0;
    PopID();

    return value_changed;
}

// A little color square. Return true when clicked.
// FIXME: May want to display/ignore the alpha component in the color display? Yet show it in the tooltip.
// 'desc_id' is not called 'label' because we don't display it next to the button, but only in the tooltip.
// Note that 'col' may be encoded in HSV if UxImGuiColorEditFlags_InputHSV is set.
bool UxImGui::ColorButton(const char* desc_id, const UxImVec4& col, UxImGuiColorEditFlags flags, const UxImVec2& size_arg)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiID id = window->GetID(desc_id);
    const float default_size = GetFrameHeight();
    const UxImVec2 size(size_arg.x == 0.0f ? default_size : size_arg.x, size_arg.y == 0.0f ? default_size : size_arg.y);
    const UxImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(bb, (size.y >= default_size) ? g.Style.FramePadding.y : 0.0f);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    if (flags & (UxImGuiColorEditFlags_NoAlpha | UxImGuiColorEditFlags_AlphaOpaque))
        flags &= ~(UxImGuiColorEditFlags_AlphaNoBg | UxImGuiColorEditFlags_AlphaPreviewHalf);

    UxImVec4 col_rgb = col;
    if (flags & UxImGuiColorEditFlags_InputHSV)
        ColorConvertHSVtoRGB(col_rgb.x, col_rgb.y, col_rgb.z, col_rgb.x, col_rgb.y, col_rgb.z);

    UxImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);
    float grid_step = UxImMin(size.x, size.y) / 2.99f;
    float rounding = UxImMin(g.Style.FrameRounding, grid_step * 0.5f);
    UxImRect bb_inner = bb;
    float off = 0.0f;
    if ((flags & UxImGuiColorEditFlags_NoBorder) == 0)
    {
        off = -0.75f; // The border (using Col_FrameBg) tends to look off when color is near-opaque and rounding is enabled. This offset seemed like a good middle ground to reduce those artifacts.
        bb_inner.Expand(off);
    }
    if ((flags & UxImGuiColorEditFlags_AlphaPreviewHalf) && col_rgb.w < 1.0f)
    {
        float mid_x = IM_ROUND((bb_inner.Min.x + bb_inner.Max.x) * 0.5f);
        if ((flags & UxImGuiColorEditFlags_AlphaNoBg) == 0)
            RenderColorRectWithAlphaCheckerboard(window->DrawList, UxImVec2(bb_inner.Min.x + grid_step, bb_inner.Min.y), bb_inner.Max, GetColorU32(col_rgb), grid_step, UxImVec2(-grid_step + off, off), rounding, UxImDrawFlags_RoundCornersRight);
        else
            window->DrawList->AddRectFilled(UxImVec2(bb_inner.Min.x + grid_step, bb_inner.Min.y), bb_inner.Max, GetColorU32(col_rgb), rounding, UxImDrawFlags_RoundCornersRight);
        window->DrawList->AddRectFilled(bb_inner.Min, UxImVec2(mid_x, bb_inner.Max.y), GetColorU32(col_rgb_without_alpha), rounding, UxImDrawFlags_RoundCornersLeft);
    }
    else
    {
        // Because GetColorU32() multiplies by the global style Alpha and we don't want to display a checkerboard if the source code had no alpha
        UxImVec4 col_source = (flags & UxImGuiColorEditFlags_AlphaOpaque) ? col_rgb_without_alpha : col_rgb;
        if (col_source.w < 1.0f && (flags & UxImGuiColorEditFlags_AlphaNoBg) == 0)
            RenderColorRectWithAlphaCheckerboard(window->DrawList, bb_inner.Min, bb_inner.Max, GetColorU32(col_source), grid_step, UxImVec2(off, off), rounding);
        else
            window->DrawList->AddRectFilled(bb_inner.Min, bb_inner.Max, GetColorU32(col_source), rounding);
    }
    RenderNavCursor(bb, id);
    if ((flags & UxImGuiColorEditFlags_NoBorder) == 0)
    {
        if (g.Style.FrameBorderSize > 0.0f)
            RenderFrameBorder(bb.Min, bb.Max, rounding);
        else
            window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(UxImGuiCol_FrameBg), rounding); // Color buttons are often in need of some sort of border // FIXME-DPI
    }

    // Drag and Drop Source
    // NB: The ActiveId test is merely an optional micro-optimization, BeginDragDropSource() does the same test.
    if (g.ActiveId == id && !(flags & UxImGuiColorEditFlags_NoDragDrop) && BeginDragDropSource())
    {
        if (flags & UxImGuiColorEditFlags_NoAlpha)
            SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, &col_rgb, sizeof(float) * 3, UxImGuiCond_Once);
        else
            SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, &col_rgb, sizeof(float) * 4, UxImGuiCond_Once);
        ColorButton(desc_id, col, flags);
        SameLine();
        TextEx("Color");
        EndDragDropSource();
    }

    // Tooltip
    if (!(flags & UxImGuiColorEditFlags_NoTooltip) && hovered && IsItemHovered(UxImGuiHoveredFlags_ForTooltip))
        ColorTooltip(desc_id, &col.x, flags & (UxImGuiColorEditFlags_InputMask_ | UxImGuiColorEditFlags_AlphaMask_));

    return pressed;
}

// Initialize/override default color options
void UxImGui::SetColorEditOptions(UxImGuiColorEditFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    if ((flags & UxImGuiColorEditFlags_DisplayMask_) == 0)
        flags |= UxImGuiColorEditFlags_DefaultOptions_ & UxImGuiColorEditFlags_DisplayMask_;
    if ((flags & UxImGuiColorEditFlags_DataTypeMask_) == 0)
        flags |= UxImGuiColorEditFlags_DefaultOptions_ & UxImGuiColorEditFlags_DataTypeMask_;
    if ((flags & UxImGuiColorEditFlags_PickerMask_) == 0)
        flags |= UxImGuiColorEditFlags_DefaultOptions_ & UxImGuiColorEditFlags_PickerMask_;
    if ((flags & UxImGuiColorEditFlags_InputMask_) == 0)
        flags |= UxImGuiColorEditFlags_DefaultOptions_ & UxImGuiColorEditFlags_InputMask_;
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_DisplayMask_));    // Check only 1 option is selected
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_DataTypeMask_));   // Check only 1 option is selected
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_PickerMask_));     // Check only 1 option is selected
    IM_ASSERT(UxImIsPowerOfTwo(flags & UxImGuiColorEditFlags_InputMask_));      // Check only 1 option is selected
    g.ColorEditOptions = flags;
}

// Note: only access 3 floats if UxImGuiColorEditFlags_NoAlpha flag is set.
void UxImGui::ColorTooltip(const char* text, const float* col, UxImGuiColorEditFlags flags)
{
    UxImGuiContext& g = *GUxImGui;

    if (!BeginTooltipEx(UxImGuiTooltipFlags_OverridePrevious, UxImGuiWindowFlags_None))
        return;
    const char* text_end = text ? FindRenderedTextEnd(text, NULL) : text;
    if (text_end > text)
    {
        TextEx(text, text_end);
        Separator();
    }

    UxImVec2 sz(g.FontSize * 3 + g.Style.FramePadding.y * 2, g.FontSize * 3 + g.Style.FramePadding.y * 2);
    UxImVec4 cf(col[0], col[1], col[2], (flags & UxImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
    int cr = IM_F32_TO_INT8_SAT(col[0]), cg = IM_F32_TO_INT8_SAT(col[1]), cb = IM_F32_TO_INT8_SAT(col[2]), ca = (flags & UxImGuiColorEditFlags_NoAlpha) ? 255 : IM_F32_TO_INT8_SAT(col[3]);
    UxImGuiColorEditFlags flags_to_forward = UxImGuiColorEditFlags_InputMask_ | UxImGuiColorEditFlags_AlphaMask_;
    ColorButton("##preview", cf, (flags & flags_to_forward) | UxImGuiColorEditFlags_NoTooltip, sz);
    SameLine();
    if ((flags & UxImGuiColorEditFlags_InputRGB) || !(flags & UxImGuiColorEditFlags_InputMask_))
    {
        if (flags & UxImGuiColorEditFlags_NoAlpha)
            Text("#%02X%02X%02X\nR: %d, G: %d, B: %d\n(%.3f, %.3f, %.3f)", cr, cg, cb, cr, cg, cb, col[0], col[1], col[2]);
        else
            Text("#%02X%02X%02X%02X\nR:%d, G:%d, B:%d, A:%d\n(%.3f, %.3f, %.3f, %.3f)", cr, cg, cb, ca, cr, cg, cb, ca, col[0], col[1], col[2], col[3]);
    }
    else if (flags & UxImGuiColorEditFlags_InputHSV)
    {
        if (flags & UxImGuiColorEditFlags_NoAlpha)
            Text("H: %.3f, S: %.3f, V: %.3f", col[0], col[1], col[2]);
        else
            Text("H: %.3f, S: %.3f, V: %.3f, A: %.3f", col[0], col[1], col[2], col[3]);
    }
    EndTooltip();
}

void UxImGui::ColorEditOptionsPopup(const float* col, UxImGuiColorEditFlags flags)
{
    bool allow_opt_inputs = !(flags & UxImGuiColorEditFlags_DisplayMask_);
    bool allow_opt_datatype = !(flags & UxImGuiColorEditFlags_DataTypeMask_);
    if ((!allow_opt_inputs && !allow_opt_datatype) || !BeginPopup("context"))
        return;

    UxImGuiContext& g = *GUxImGui;
    PushItemFlag(UxImGuiItemFlags_NoMarkEdited, true);
    UxImGuiColorEditFlags opts = g.ColorEditOptions;
    if (allow_opt_inputs)
    {
        if (RadioButton("RGB", (opts & UxImGuiColorEditFlags_DisplayRGB) != 0)) opts = (opts & ~UxImGuiColorEditFlags_DisplayMask_) | UxImGuiColorEditFlags_DisplayRGB;
        if (RadioButton("HSV", (opts & UxImGuiColorEditFlags_DisplayHSV) != 0)) opts = (opts & ~UxImGuiColorEditFlags_DisplayMask_) | UxImGuiColorEditFlags_DisplayHSV;
        if (RadioButton("Hex", (opts & UxImGuiColorEditFlags_DisplayHex) != 0)) opts = (opts & ~UxImGuiColorEditFlags_DisplayMask_) | UxImGuiColorEditFlags_DisplayHex;
    }
    if (allow_opt_datatype)
    {
        if (allow_opt_inputs) Separator();
        if (RadioButton("0..255",     (opts & UxImGuiColorEditFlags_Uint8) != 0)) opts = (opts & ~UxImGuiColorEditFlags_DataTypeMask_) | UxImGuiColorEditFlags_Uint8;
        if (RadioButton("0.00..1.00", (opts & UxImGuiColorEditFlags_Float) != 0)) opts = (opts & ~UxImGuiColorEditFlags_DataTypeMask_) | UxImGuiColorEditFlags_Float;
    }

    if (allow_opt_inputs || allow_opt_datatype)
        Separator();
    if (Button("Copy as..", UxImVec2(-1, 0)))
        OpenPopup("Copy");
    if (BeginPopup("Copy"))
    {
        int cr = IM_F32_TO_INT8_SAT(col[0]), cg = IM_F32_TO_INT8_SAT(col[1]), cb = IM_F32_TO_INT8_SAT(col[2]), ca = (flags & UxImGuiColorEditFlags_NoAlpha) ? 255 : IM_F32_TO_INT8_SAT(col[3]);
        char buf[64];
        UxImFormatString(buf, IM_ARRAYSIZE(buf), "(%.3ff, %.3ff, %.3ff, %.3ff)", col[0], col[1], col[2], (flags & UxImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
        if (Selectable(buf))
            SetClipboardText(buf);
        UxImFormatString(buf, IM_ARRAYSIZE(buf), "(%d,%d,%d,%d)", cr, cg, cb, ca);
        if (Selectable(buf))
            SetClipboardText(buf);
        UxImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", cr, cg, cb);
        if (Selectable(buf))
            SetClipboardText(buf);
        if (!(flags & UxImGuiColorEditFlags_NoAlpha))
        {
            UxImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X%02X", cr, cg, cb, ca);
            if (Selectable(buf))
                SetClipboardText(buf);
        }
        EndPopup();
    }

    g.ColorEditOptions = opts;
    PopItemFlag();
    EndPopup();
}

void UxImGui::ColorPickerOptionsPopup(const float* ref_col, UxImGuiColorEditFlags flags)
{
    bool allow_opt_picker = !(flags & UxImGuiColorEditFlags_PickerMask_);
    bool allow_opt_alpha_bar = !(flags & UxImGuiColorEditFlags_NoAlpha) && !(flags & UxImGuiColorEditFlags_AlphaBar);
    if ((!allow_opt_picker && !allow_opt_alpha_bar) || !BeginPopup("context"))
        return;

    UxImGuiContext& g = *GUxImGui;
    PushItemFlag(UxImGuiItemFlags_NoMarkEdited, true);
    if (allow_opt_picker)
    {
        UxImVec2 picker_size(g.FontSize * 8, UxImMax(g.FontSize * 8 - (GetFrameHeight() + g.Style.ItemInnerSpacing.x), 1.0f)); // FIXME: Picker size copied from main picker function
        PushItemWidth(picker_size.x);
        for (int picker_type = 0; picker_type < 2; picker_type++)
        {
            // Draw small/thumbnail version of each picker type (over an invisible button for selection)
            if (picker_type > 0) Separator();
            PushID(picker_type);
            UxImGuiColorEditFlags picker_flags = UxImGuiColorEditFlags_NoInputs | UxImGuiColorEditFlags_NoOptions | UxImGuiColorEditFlags_NoLabel | UxImGuiColorEditFlags_NoSidePreview | (flags & UxImGuiColorEditFlags_NoAlpha);
            if (picker_type == 0) picker_flags |= UxImGuiColorEditFlags_PickerHueBar;
            if (picker_type == 1) picker_flags |= UxImGuiColorEditFlags_PickerHueWheel;
            UxImVec2 backup_pos = GetCursorScreenPos();
            if (Selectable("##selectable", false, 0, picker_size)) // By default, Selectable() is closing popup
                g.ColorEditOptions = (g.ColorEditOptions & ~UxImGuiColorEditFlags_PickerMask_) | (picker_flags & UxImGuiColorEditFlags_PickerMask_);
            SetCursorScreenPos(backup_pos);
            UxImVec4 previewing_ref_col;
            memcpy(&previewing_ref_col, ref_col, sizeof(float) * ((picker_flags & UxImGuiColorEditFlags_NoAlpha) ? 3 : 4));
            ColorPicker4("##previewing_picker", &previewing_ref_col.x, picker_flags);
            PopID();
        }
        PopItemWidth();
    }
    if (allow_opt_alpha_bar)
    {
        if (allow_opt_picker) Separator();
        CheckboxFlags("Alpha Bar", &g.ColorEditOptions, UxImGuiColorEditFlags_AlphaBar);
    }
    PopItemFlag();
    EndPopup();
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: TreeNode, CollapsingHeader, etc.
//-------------------------------------------------------------------------
// - TreeNode()
// - TreeNodeV()
// - TreeNodeEx()
// - TreeNodeExV()
// - TreeNodeStoreStackData() [Internal]
// - TreeNodeBehavior() [Internal]
// - TreePush()
// - TreePop()
// - GetTreeNodeToLabelSpacing()
// - SetNextItemOpen()
// - CollapsingHeader()
//-------------------------------------------------------------------------

bool UxImGui::TreeNode(const char* str_id, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(str_id, 0, fmt, args);
    va_end(args);
    return is_open;
}

bool UxImGui::TreeNode(const void* ptr_id, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(ptr_id, 0, fmt, args);
    va_end(args);
    return is_open;
}

bool UxImGui::TreeNode(const char* label)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    UxImGuiID id = window->GetID(label);
    return TreeNodeBehavior(id, UxImGuiTreeNodeFlags_None, label, NULL);
}

bool UxImGui::TreeNodeV(const char* str_id, const char* fmt, va_list args)
{
    return TreeNodeExV(str_id, 0, fmt, args);
}

bool UxImGui::TreeNodeV(const void* ptr_id, const char* fmt, va_list args)
{
    return TreeNodeExV(ptr_id, 0, fmt, args);
}

bool UxImGui::TreeNodeEx(const char* label, UxImGuiTreeNodeFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    UxImGuiID id = window->GetID(label);
    return TreeNodeBehavior(id, flags, label, NULL);
}

bool UxImGui::TreeNodeEx(const char* str_id, UxImGuiTreeNodeFlags flags, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(str_id, flags, fmt, args);
    va_end(args);
    return is_open;
}

bool UxImGui::TreeNodeEx(const void* ptr_id, UxImGuiTreeNodeFlags flags, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(ptr_id, flags, fmt, args);
    va_end(args);
    return is_open;
}

bool UxImGui::TreeNodeExV(const char* str_id, UxImGuiTreeNodeFlags flags, const char* fmt, va_list args)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiID id = window->GetID(str_id);
    const char* label, *label_end;
    UxImFormatStringToTempBufferV(&label, &label_end, fmt, args);
    return TreeNodeBehavior(id, flags, label, label_end);
}

bool UxImGui::TreeNodeExV(const void* ptr_id, UxImGuiTreeNodeFlags flags, const char* fmt, va_list args)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiID id = window->GetID(ptr_id);
    const char* label, *label_end;
    UxImFormatStringToTempBufferV(&label, &label_end, fmt, args);
    return TreeNodeBehavior(id, flags, label, label_end);
}

bool UxImGui::TreeNodeGetOpen(UxImGuiID storage_id)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiStorage* storage = g.CurrentWindow->DC.StateStorage;
    return storage->GetInt(storage_id, 0) != 0;
}

void UxImGui::TreeNodeSetOpen(UxImGuiID storage_id, bool open)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiStorage* storage = g.CurrentWindow->DC.StateStorage;
    storage->SetInt(storage_id, open ? 1 : 0);
}

bool UxImGui::TreeNodeUpdateNextOpen(UxImGuiID storage_id, UxImGuiTreeNodeFlags flags)
{
    if (flags & UxImGuiTreeNodeFlags_Leaf)
        return true;

    // We only write to the tree storage if the user clicks, or explicitly use the SetNextItemOpen function
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    UxImGuiStorage* storage = window->DC.StateStorage;

    bool is_open;
    if (g.NextItemData.HasFlags & UxImGuiNextItemDataFlags_HasOpen)
    {
        if (g.NextItemData.OpenCond & UxImGuiCond_Always)
        {
            is_open = g.NextItemData.OpenVal;
            TreeNodeSetOpen(storage_id, is_open);
        }
        else
        {
            // We treat UxImGuiCond_Once and UxImGuiCond_FirstUseEver the same because tree node state are not saved persistently.
            const int stored_value = storage->GetInt(storage_id, -1);
            if (stored_value == -1)
            {
                is_open = g.NextItemData.OpenVal;
                TreeNodeSetOpen(storage_id, is_open);
            }
            else
            {
                is_open = stored_value != 0;
            }
        }
    }
    else
    {
        is_open = storage->GetInt(storage_id, (flags & UxImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0) != 0;
    }

    // When logging is enabled, we automatically expand tree nodes (but *NOT* collapsing headers.. seems like sensible behavior).
    // NB- If we are above max depth we still allow manually opened nodes to be logged.
    if (g.LogEnabled && !(flags & UxImGuiTreeNodeFlags_NoAutoOpenOnLog) && (window->DC.TreeDepth - g.LogDepthRef) < g.LogDepthToExpand)
        is_open = true;

    return is_open;
}

// Store UxImGuiTreeNodeStackData for just submitted node.
// Currently only supports 32 level deep and we are fine with (1 << Depth) overflowing into a zero, easy to increase.
static void TreeNodeStoreStackData(UxImGuiTreeNodeFlags flags, float x1)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    g.TreeNodeStack.resize(g.TreeNodeStack.Size + 1);
    UxImGuiTreeNodeStackData* tree_node_data = &g.TreeNodeStack.Data[g.TreeNodeStack.Size - 1];
    tree_node_data->ID = g.LastItemData.ID;
    tree_node_data->TreeFlags = flags;
    tree_node_data->ItemFlags = g.LastItemData.ItemFlags;
    tree_node_data->NavRect = g.LastItemData.NavRect;

    // Initially I tried to latch value for GetColorU32(UxImGuiCol_TreeLines) but it's not a good trade-off for very large trees.
    const bool draw_lines = (flags & (UxImGuiTreeNodeFlags_DrawLinesFull | UxImGuiTreeNodeFlags_DrawLinesToNodes)) != 0;
    tree_node_data->DrawLinesX1 = draw_lines ? (x1 + g.FontSize * 0.5f + g.Style.FramePadding.x) : +FLT_MAX;
    tree_node_data->DrawLinesTableColumn = (draw_lines && g.CurrentTable) ? (UxImGuiTableColumnIdx)g.CurrentTable->CurrentColumn : -1;
    tree_node_data->DrawLinesToNodesY2 = -FLT_MAX;
    window->DC.TreeHasStackDataDepthMask |= (1 << window->DC.TreeDepth);
    if (flags & UxImGuiTreeNodeFlags_DrawLinesToNodes)
        window->DC.TreeRecordsClippedNodesY2Mask |= (1 << window->DC.TreeDepth);
}

// When using public API, currently 'id == storage_id' is always true, but we separate the values to facilitate advanced user code doing storage queries outside of UI loop.
bool UxImGui::TreeNodeBehavior(UxImGuiID id, UxImGuiTreeNodeFlags flags, const char* label, const char* label_end)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const bool display_frame = (flags & UxImGuiTreeNodeFlags_Framed) != 0;
    const UxImVec2 padding = (display_frame || (flags & UxImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : UxImVec2(style.FramePadding.x, UxImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));

    if (!label_end)
        label_end = FindRenderedTextEnd(label);
    const UxImVec2 label_size = CalcTextSize(label, label_end, false);

    const float text_offset_x = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);   // Collapsing arrow width + Spacing
    const float text_offset_y = UxImMax(padding.y, window->DC.CurrLineTextBaseOffset);            // Latch before ItemSize changes it
    const float text_width = g.FontSize + label_size.x + padding.x * 2;                         // Include collapsing arrow

    // We vertically grow up to current line height up the typical widget height.
    const float frame_height = UxImMax(UxImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), label_size.y + padding.y * 2);
    const bool span_all_columns = (flags & UxImGuiTreeNodeFlags_SpanAllColumns) != 0 && (g.CurrentTable != NULL);
    const bool span_all_columns_label = (flags & UxImGuiTreeNodeFlags_LabelSpanAllColumns) != 0 && (g.CurrentTable != NULL);
    UxImRect frame_bb;
    frame_bb.Min.x = span_all_columns ? window->ParentWorkRect.Min.x : (flags & UxImGuiTreeNodeFlags_SpanFullWidth) ? window->WorkRect.Min.x : window->DC.CursorPos.x;
    frame_bb.Min.y = window->DC.CursorPos.y;
    frame_bb.Max.x = span_all_columns ? window->ParentWorkRect.Max.x : (flags & UxImGuiTreeNodeFlags_SpanLabelWidth) ? window->DC.CursorPos.x + text_width + padding.x : window->WorkRect.Max.x;
    frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
    if (display_frame)
    {
        const float outer_extend = IM_TRUNC(window->WindowPadding.x * 0.5f); // Framed header expand a little outside of current limits
        frame_bb.Min.x -= outer_extend;
        frame_bb.Max.x += outer_extend;
    }

    UxImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
    ItemSize(UxImVec2(text_width, frame_height), padding.y);

    // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
    UxImRect interact_bb = frame_bb;
    if ((flags & (UxImGuiTreeNodeFlags_Framed | UxImGuiTreeNodeFlags_SpanAvailWidth | UxImGuiTreeNodeFlags_SpanFullWidth | UxImGuiTreeNodeFlags_SpanLabelWidth | UxImGuiTreeNodeFlags_SpanAllColumns)) == 0)
        interact_bb.Max.x = frame_bb.Min.x + text_width + (label_size.x > 0.0f ? style.ItemSpacing.x * 2.0f : 0.0f);

    // Compute open and multi-select states before ItemAdd() as it clear NextItem data.
    UxImGuiID storage_id = (g.NextItemData.HasFlags & UxImGuiNextItemDataFlags_HasStorageID) ? g.NextItemData.StorageId : id;
    bool is_open = TreeNodeUpdateNextOpen(storage_id, flags);

    bool is_visible;
    if (span_all_columns || span_all_columns_label)
    {
        // Modify ClipRect for the ItemAdd(), faster than doing a PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
        const float backup_clip_rect_min_x = window->ClipRect.Min.x;
        const float backup_clip_rect_max_x = window->ClipRect.Max.x;
        window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
        window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
        is_visible = ItemAdd(interact_bb, id);
        window->ClipRect.Min.x = backup_clip_rect_min_x;
        window->ClipRect.Max.x = backup_clip_rect_max_x;
    }
    else
    {
        is_visible = ItemAdd(interact_bb, id);
    }
    g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_HasDisplayRect;
    g.LastItemData.DisplayRect = frame_bb;

    // If a NavLeft request is happening and UxImGuiTreeNodeFlags_NavLeftJumpsToParent enabled:
    // Store data for the current depth to allow returning to this node from any child item.
    // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode() and TreePop().
    // It will become tempting to enable UxImGuiTreeNodeFlags_NavLeftJumpsToParent by default or move it to UxImGuiStyle.
    bool store_tree_node_stack_data = false;
    if ((flags & UxImGuiTreeNodeFlags_DrawLinesMask_) == 0)
        flags |= g.Style.TreeLinesFlags;
    const bool draw_tree_lines = (flags & (UxImGuiTreeNodeFlags_DrawLinesFull | UxImGuiTreeNodeFlags_DrawLinesToNodes)) && (frame_bb.Min.y < window->ClipRect.Max.y) && (g.Style.TreeLinesSize > 0.0f);
    if (is_open && !(flags & UxImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        if ((flags & UxImGuiTreeNodeFlags_NavLeftJumpsToParent) && !g.NavIdIsAlive)
            if (g.NavMoveDir == UxImGuiDir_Left && g.NavWindow == window && NavMoveRequestButNoResultYet())
                store_tree_node_stack_data = true;
        if (draw_tree_lines)
            store_tree_node_stack_data = true;
    }

    const bool is_leaf = (flags & UxImGuiTreeNodeFlags_Leaf) != 0;
    if (!is_visible)
    {
        if ((flags & UxImGuiTreeNodeFlags_DrawLinesToNodes) && (window->DC.TreeRecordsClippedNodesY2Mask & (1 << (window->DC.TreeDepth - 1))))
        {
            UxImGuiTreeNodeStackData* parent_data = &g.TreeNodeStack.Data[g.TreeNodeStack.Size - 1];
            parent_data->DrawLinesToNodesY2 = UxImMax(parent_data->DrawLinesToNodesY2, window->DC.CursorPos.y); // Don't need to aim to mid Y position as we are clipped anyway.
            if (frame_bb.Min.y >= window->ClipRect.Max.y)
                window->DC.TreeRecordsClippedNodesY2Mask &= ~(1 << (window->DC.TreeDepth - 1)); // Done
        }
        if (is_open && store_tree_node_stack_data)
            TreeNodeStoreStackData(flags, text_pos.x - text_offset_x); // Call before TreePushOverrideID()
        if (is_open && !(flags & UxImGuiTreeNodeFlags_NoTreePushOnOpen))
            TreePushOverrideID(id);
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | (is_leaf ? 0 : UxImGuiItemStatusFlags_Openable) | (is_open ? UxImGuiItemStatusFlags_Opened : 0));
        return is_open;
    }

    if (span_all_columns || span_all_columns_label)
    {
        TablePushBackgroundChannel();
        g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_HasClipRect;
        g.LastItemData.ClipRect = window->ClipRect;
    }

    UxImGuiButtonFlags button_flags = UxImGuiTreeNodeFlags_None;
    if ((flags & UxImGuiTreeNodeFlags_AllowOverlap) || (g.LastItemData.ItemFlags & UxImGuiItemFlags_AllowOverlap))
        button_flags |= UxImGuiButtonFlags_AllowOverlap;
    if (!is_leaf)
        button_flags |= UxImGuiButtonFlags_PressedOnDragDropHold;

    // We allow clicking on the arrow section with keyboard modifiers held, in order to easily
    // allow browsing a tree while preserving selection with code implementing multi-selection patterns.
    // When clicking on the rest of the tree node we always disallow keyboard modifiers.
    const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
    const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
    const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);

    const bool is_multi_select = (g.LastItemData.ItemFlags & UxImGuiItemFlags_IsMultiSelect) != 0;
    if (is_multi_select) // We absolutely need to distinguish open vs select so _OpenOnArrow comes by default
        flags |= (flags & UxImGuiTreeNodeFlags_OpenOnMask_) == 0 ? UxImGuiTreeNodeFlags_OpenOnArrow | UxImGuiTreeNodeFlags_OpenOnDoubleClick : UxImGuiTreeNodeFlags_OpenOnArrow;

    // Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
    // Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to requirements for multi-selection and drag and drop support.
    // - Single-click on label = Toggle on MouseUp (default, when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
    // - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
    // - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and _OpenOnArrow=0)
    // It is rather standard that arrow click react on Down rather than Up.
    // We set UxImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be active on the initial MouseDown in order for drag and drop to work.
    if (is_mouse_x_over_arrow)
        button_flags |= UxImGuiButtonFlags_PressedOnClick;
    else if (flags & UxImGuiTreeNodeFlags_OpenOnDoubleClick)
        button_flags |= UxImGuiButtonFlags_PressedOnClickRelease | UxImGuiButtonFlags_PressedOnDoubleClick;
    else
        button_flags |= UxImGuiButtonFlags_PressedOnClickRelease;
    if (flags & UxImGuiTreeNodeFlags_NoNavFocus)
        button_flags |= UxImGuiButtonFlags_NoNavFocus;

    bool selected = (flags & UxImGuiTreeNodeFlags_Selected) != 0;
    const bool was_selected = selected;

    // Multi-selection support (header)
    if (is_multi_select)
    {
        // Handle multi-select + alter button flags for it
        MultiSelectItemHeader(id, &selected, &button_flags);
        if (is_mouse_x_over_arrow)
            button_flags = (button_flags | UxImGuiButtonFlags_PressedOnClick) & ~UxImGuiButtonFlags_PressedOnClickRelease;
    }
    else
    {
        if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
            button_flags |= UxImGuiButtonFlags_NoKeyModsAllowed;
    }

    bool hovered, held;
    bool pressed = ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
    bool toggled = false;
    if (!is_leaf)
    {
        if (pressed && g.DragDropHoldJustPressedId != id)
        {
            if ((flags & UxImGuiTreeNodeFlags_OpenOnMask_) == 0 || (g.NavActivateId == id && !is_multi_select))
                toggled = true; // Single click
            if (flags & UxImGuiTreeNodeFlags_OpenOnArrow)
                toggled |= is_mouse_x_over_arrow && !g.NavHighlightItemUnderNav; // Lightweight equivalent of IsMouseHoveringRect() since ButtonBehavior() already did the job
            if ((flags & UxImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseClickedCount[0] == 2)
                toggled = true; // Double click
        }
        else if (pressed && g.DragDropHoldJustPressedId == id)
        {
            IM_ASSERT(button_flags & UxImGuiButtonFlags_PressedOnDragDropHold);
            if (!is_open) // When using Drag and Drop "hold to open" we keep the node highlighted after opening, but never close it again.
                toggled = true;
            else
                pressed = false; // Cancel press so it doesn't trigger selection.
        }

        if (g.NavId == id && g.NavMoveDir == UxImGuiDir_Left && is_open)
        {
            toggled = true;
            NavClearPreferredPosForAxis(UxImGuiAxis_X);
            NavMoveRequestCancel();
        }
        if (g.NavId == id && g.NavMoveDir == UxImGuiDir_Right && !is_open) // If there's something upcoming on the line we may want to give it the priority?
        {
            toggled = true;
            NavClearPreferredPosForAxis(UxImGuiAxis_X);
            NavMoveRequestCancel();
        }

        if (toggled)
        {
            is_open = !is_open;
            window->DC.StateStorage->SetInt(storage_id, is_open);
            g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_ToggledOpen;
        }
    }

    // Multi-selection support (footer)
    if (is_multi_select)
    {
        bool pressed_copy = pressed && !toggled;
        MultiSelectItemFooter(id, &selected, &pressed_copy);
        if (pressed)
            SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, interact_bb);
    }

    if (selected != was_selected)
        g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_ToggledSelection;

    // Render
    {
        const UxImU32 text_col = GetColorU32(UxImGuiCol_Text);
        UxImGuiNavRenderCursorFlags nav_render_cursor_flags = UxImGuiNavRenderCursorFlags_Compact;
        if (is_multi_select)
            nav_render_cursor_flags |= UxImGuiNavRenderCursorFlags_AlwaysDraw; // Always show the nav rectangle
        if (display_frame)
        {
            // Framed type
            const UxImU32 bg_col = GetColorU32((held && hovered) ? UxImGuiCol_HeaderActive : hovered ? UxImGuiCol_HeaderHovered : UxImGuiCol_Header);
            RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
            RenderNavCursor(frame_bb, id, nav_render_cursor_flags);
            if (span_all_columns && !span_all_columns_label)
                TablePopBackgroundChannel();
            if (flags & UxImGuiTreeNodeFlags_Bullet)
                RenderBullet(window->DrawList, UxImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f), text_col);
            else if (!is_leaf)
                RenderArrow(window->DrawList, UxImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y), text_col, is_open ? ((flags & UxImGuiTreeNodeFlags_UpsideDownArrow) ? UxImGuiDir_Up : UxImGuiDir_Down) : UxImGuiDir_Right, 1.0f);
            else // Leaf without bullet, left-adjusted text
                text_pos.x -= text_offset_x - padding.x;
            if (flags & UxImGuiTreeNodeFlags_ClipLabelForTrailingButton)
                frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
            if (g.LogEnabled)
                LogSetNextTextDecoration("###", "###");
        }
        else
        {
            // Unframed typed for tree nodes
            if (hovered || selected)
            {
                const UxImU32 bg_col = GetColorU32((held && hovered) ? UxImGuiCol_HeaderActive : hovered ? UxImGuiCol_HeaderHovered : UxImGuiCol_Header);
                RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
            }
            RenderNavCursor(frame_bb, id, nav_render_cursor_flags);
            if (span_all_columns && !span_all_columns_label)
                TablePopBackgroundChannel();
            if (flags & UxImGuiTreeNodeFlags_Bullet)
                RenderBullet(window->DrawList, UxImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f), text_col);
            else if (!is_leaf)
                RenderArrow(window->DrawList, UxImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f), text_col, is_open ? ((flags & UxImGuiTreeNodeFlags_UpsideDownArrow) ? UxImGuiDir_Up : UxImGuiDir_Down) : UxImGuiDir_Right, 0.70f);
            if (g.LogEnabled)
                LogSetNextTextDecoration(">", NULL);
        }

        if (draw_tree_lines)
            TreeNodeDrawLineToChildNode(UxImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.5f));

        // Label
        if (display_frame)
            RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
        else
            RenderText(text_pos, label, label_end, false);

        if (span_all_columns_label)
            TablePopBackgroundChannel();
    }

    if (store_tree_node_stack_data && is_open)
        TreeNodeStoreStackData(flags, text_pos.x - text_offset_x); // Call before TreePushOverrideID()
    if (is_open && !(flags & UxImGuiTreeNodeFlags_NoTreePushOnOpen))
        TreePushOverrideID(id); // Could use TreePush(label) but this avoid computing twice

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (is_leaf ? 0 : UxImGuiItemStatusFlags_Openable) | (is_open ? UxImGuiItemStatusFlags_Opened : 0));
    return is_open;
}

// Draw horizontal line from our parent node
// This is only called for visible child nodes so we are not too fussy anymore about performances
void UxImGui::TreeNodeDrawLineToChildNode(const UxImVec2& target_pos)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if ((window->DC.TreeHasStackDataDepthMask & (1 << (window->DC.TreeDepth - 1))) == 0)
        return;

    UxImGuiTreeNodeStackData* parent_data = &g.TreeNodeStack.Data[g.TreeNodeStack.Size - 1];
    float x1 = UxImTrunc(parent_data->DrawLinesX1);
    float x2 = UxImTrunc(target_pos.x - g.Style.ItemInnerSpacing.x);
    float y = UxImTrunc(target_pos.y);
    float rounding = (g.Style.TreeLinesRounding > 0.0f) ? UxImMin(x2 - x1, g.Style.TreeLinesRounding) : 0.0f;
    parent_data->DrawLinesToNodesY2 = UxImMax(parent_data->DrawLinesToNodesY2, y - rounding);
    if (x1 >= x2)
        return;
    if (rounding > 0.0f)
    {
        x1 += 0.5f + rounding;
        window->DrawList->PathArcToFast(UxImVec2(x1, y - rounding), rounding, 6, 3);
        if (x1 < x2)
            window->DrawList->PathLineTo(UxImVec2(x2, y));
        window->DrawList->PathStroke(GetColorU32(UxImGuiCol_TreeLines), UxImDrawFlags_None, g.Style.TreeLinesSize);
    }
    else
    {
        window->DrawList->AddLine(UxImVec2(x1, y), UxImVec2(x2, y), GetColorU32(UxImGuiCol_TreeLines), g.Style.TreeLinesSize);
    }
}

// Draw vertical line of the hierarchy
void UxImGui::TreeNodeDrawLineToTreePop(const UxImGuiTreeNodeStackData* data)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    float y1 = UxImMax(data->NavRect.Max.y, window->ClipRect.Min.y);
    float y2 = data->DrawLinesToNodesY2;
    if (data->TreeFlags & UxImGuiTreeNodeFlags_DrawLinesFull)
    {
        float y2_full = window->DC.CursorPos.y;
        if (g.CurrentTable)
            y2_full = UxImMax(g.CurrentTable->RowPosY2, y2_full);
        y2_full = UxImTrunc(y2_full - g.Style.ItemSpacing.y - g.FontSize * 0.5f);
        if (y2 + (g.Style.ItemSpacing.y + g.Style.TreeLinesRounding) < y2_full) // FIXME: threshold to use ToNodes Y2 instead of Full Y2 when close by ItemSpacing.y
            y2 = y2_full;
    }
    y2 = UxImMin(y2, window->ClipRect.Max.y);
    if (y2 <= y1)
        return;
    float x = UxImTrunc(data->DrawLinesX1);
    if (data->DrawLinesTableColumn != -1)
        TablePushColumnChannel(data->DrawLinesTableColumn);
    window->DrawList->AddLine(UxImVec2(x, y1), UxImVec2(x, y2), GetColorU32(UxImGuiCol_TreeLines), g.Style.TreeLinesSize);
    if (data->DrawLinesTableColumn != -1)
        TablePopColumnChannel();
}

void UxImGui::TreePush(const char* str_id)
{
    UxImGuiWindow* window = GetCurrentWindow();
    Indent();
    window->DC.TreeDepth++;
    PushID(str_id);
}

void UxImGui::TreePush(const void* ptr_id)
{
    UxImGuiWindow* window = GetCurrentWindow();
    Indent();
    window->DC.TreeDepth++;
    PushID(ptr_id);
}

void UxImGui::TreePushOverrideID(UxImGuiID id)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    Indent();
    window->DC.TreeDepth++;
    PushOverrideID(id);
}

void UxImGui::TreePop()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    Unindent();

    window->DC.TreeDepth--;
    UxImU32 tree_depth_mask = (1 << window->DC.TreeDepth);

    if (window->DC.TreeHasStackDataDepthMask & tree_depth_mask) // Only set during request
    {
        const UxImGuiTreeNodeStackData* data = &g.TreeNodeStack.Data[g.TreeNodeStack.Size - 1];
        IM_ASSERT(data->ID == window->IDStack.back());

        // Handle Left arrow to move to parent tree node (when UxImGuiTreeNodeFlags_NavLeftJumpsToParent is enabled)
        if (data->TreeFlags & UxImGuiTreeNodeFlags_NavLeftJumpsToParent)
            if (g.NavIdIsAlive && g.NavMoveDir == UxImGuiDir_Left && g.NavWindow == window && NavMoveRequestButNoResultYet())
                NavMoveRequestResolveWithPastTreeNode(&g.NavMoveResultLocal, data);

        // Draw hierarchy lines
        if (data->DrawLinesX1 != +FLT_MAX && window->DC.CursorPos.y >= window->ClipRect.Min.y)
            TreeNodeDrawLineToTreePop(data);

        g.TreeNodeStack.pop_back();
        window->DC.TreeHasStackDataDepthMask &= ~tree_depth_mask;
    }

    IM_ASSERT(window->IDStack.Size > 1); // There should always be 1 element in the IDStack (pushed during window creation). If this triggers you called TreePop/PopID too much.
    PopID();
}

// Horizontal distance preceding label when using TreeNode() or Bullet()
float UxImGui::GetTreeNodeToLabelSpacing()
{
    UxImGuiContext& g = *GUxImGui;
    return g.FontSize + (g.Style.FramePadding.x * 2.0f);
}

// Set next TreeNode/CollapsingHeader open state.
void UxImGui::SetNextItemOpen(bool is_open, UxImGuiCond cond)
{
    UxImGuiContext& g = *GUxImGui;
    if (g.CurrentWindow->SkipItems)
        return;
    g.NextItemData.HasFlags |= UxImGuiNextItemDataFlags_HasOpen;
    g.NextItemData.OpenVal = is_open;
    g.NextItemData.OpenCond = (UxImU8)(cond ? cond : UxImGuiCond_Always);
}

// Set next TreeNode/CollapsingHeader storage id.
void UxImGui::SetNextItemStorageID(UxImGuiID storage_id)
{
    UxImGuiContext& g = *GUxImGui;
    if (g.CurrentWindow->SkipItems)
        return;
    g.NextItemData.HasFlags |= UxImGuiNextItemDataFlags_HasStorageID;
    g.NextItemData.StorageId = storage_id;
}

// CollapsingHeader returns true when opened but do not indent nor push into the ID stack (because of the UxImGuiTreeNodeFlags_NoTreePushOnOpen flag).
// This is basically the same as calling TreeNodeEx(label, UxImGuiTreeNodeFlags_CollapsingHeader). You can remove the _NoTreePushOnOpen flag if you want behavior closer to normal TreeNode().
bool UxImGui::CollapsingHeader(const char* label, UxImGuiTreeNodeFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    UxImGuiID id = window->GetID(label);
    return TreeNodeBehavior(id, flags | UxImGuiTreeNodeFlags_CollapsingHeader, label);
}

// p_visible == NULL                        : regular collapsing header
// p_visible != NULL && *p_visible == true  : show a small close button on the corner of the header, clicking the button will set *p_visible = false
// p_visible != NULL && *p_visible == false : do not show the header at all
// Do not mistake this with the Open state of the header itself, which you can adjust with SetNextItemOpen() or UxImGuiTreeNodeFlags_DefaultOpen.
bool UxImGui::CollapsingHeader(const char* label, bool* p_visible, UxImGuiTreeNodeFlags flags)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    if (p_visible && !*p_visible)
        return false;

    UxImGuiID id = window->GetID(label);
    flags |= UxImGuiTreeNodeFlags_CollapsingHeader;
    if (p_visible)
        flags |= UxImGuiTreeNodeFlags_AllowOverlap | (UxImGuiTreeNodeFlags)UxImGuiTreeNodeFlags_ClipLabelForTrailingButton;
    bool is_open = TreeNodeBehavior(id, flags, label);
    if (p_visible != NULL)
    {
        // Create a small overlapping close button
        // FIXME: We can evolve this into user accessible helpers to add extra buttons on title bars, headers, etc.
        // FIXME: CloseButton can overlap into text, need find a way to clip the text somehow.
        UxImGuiContext& g = *GUxImGui;
        UxImGuiLastItemData last_item_backup = g.LastItemData;
        float button_size = g.FontSize;
        float button_x = UxImMax(g.LastItemData.Rect.Min.x, g.LastItemData.Rect.Max.x - g.Style.FramePadding.x - button_size);
        float button_y = g.LastItemData.Rect.Min.y + g.Style.FramePadding.y;
        UxImGuiID close_button_id = GetIDWithSeed("#CLOSE", NULL, id);
        if (CloseButton(close_button_id, UxImVec2(button_x, button_y)))
            *p_visible = false;
        g.LastItemData = last_item_backup;
    }

    return is_open;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Selectable
//-------------------------------------------------------------------------
// - Selectable()
//-------------------------------------------------------------------------

// Tip: pass a non-visible label (e.g. "##hello") then you can use the space to draw other text or image.
// But you need to make sure the ID is unique, e.g. enclose calls in PushID/PopID or use ##unique_id.
// With this scheme, UxImGuiSelectableFlags_SpanAllColumns and UxImGuiSelectableFlags_AllowOverlap are also frequently used flags.
// FIXME: Selectable() with (size.x == 0.0f) and (SelectableTextAlign.x > 0.0f) followed by SameLine() is currently not supported.
bool UxImGui::Selectable(const char* label, bool selected, UxImGuiSelectableFlags flags, const UxImVec2& size_arg)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;

    // Submit label or explicit size to ItemSize(), whereas ItemAdd() will submit a larger/spanning rectangle.
    UxImGuiID id = window->GetID(label);
    UxImVec2 label_size = CalcTextSize(label, NULL, true);
    UxImVec2 size(size_arg.x != 0.0f ? size_arg.x : label_size.x, size_arg.y != 0.0f ? size_arg.y : label_size.y);
    UxImVec2 pos = window->DC.CursorPos;
    pos.y += window->DC.CurrLineTextBaseOffset;
    ItemSize(size, 0.0f);

    // Fill horizontal space
    // We don't support (size < 0.0f) in Selectable() because the ItemSpacing extension would make explicitly right-aligned sizes not visibly match other widgets.
    const bool span_all_columns = (flags & UxImGuiSelectableFlags_SpanAllColumns) != 0;
    const float min_x = span_all_columns ? window->ParentWorkRect.Min.x : pos.x;
    const float max_x = span_all_columns ? window->ParentWorkRect.Max.x : window->WorkRect.Max.x;
    if (size_arg.x == 0.0f || (flags & UxImGuiSelectableFlags_SpanAvailWidth))
        size.x = UxImMax(label_size.x, max_x - min_x);

    // Selectables are meant to be tightly packed together with no click-gap, so we extend their box to cover spacing between selectable.
    // FIXME: Not part of layout so not included in clipper calculation, but ItemSize currently doesn't allow offsetting CursorPos.
    UxImRect bb(min_x, pos.y, min_x + size.x, pos.y + size.y);
    if ((flags & UxImGuiSelectableFlags_NoPadWithHalfSpacing) == 0)
    {
        const float spacing_x = span_all_columns ? 0.0f : style.ItemSpacing.x;
        const float spacing_y = style.ItemSpacing.y;
        const float spacing_L = IM_TRUNC(spacing_x * 0.50f);
        const float spacing_U = IM_TRUNC(spacing_y * 0.50f);
        bb.Min.x -= spacing_L;
        bb.Min.y -= spacing_U;
        bb.Max.x += (spacing_x - spacing_L);
        bb.Max.y += (spacing_y - spacing_U);
    }
    //if (g.IO.KeyCtrl) { GetForegroundDrawList()->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 0, 255)); }

    const bool disabled_item = (flags & UxImGuiSelectableFlags_Disabled) != 0;
    const UxImGuiItemFlags extra_item_flags = disabled_item ? (UxImGuiItemFlags)UxImGuiItemFlags_Disabled : UxImGuiItemFlags_None;
    bool is_visible;
    if (span_all_columns)
    {
        // Modify ClipRect for the ItemAdd(), faster than doing a PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
        const float backup_clip_rect_min_x = window->ClipRect.Min.x;
        const float backup_clip_rect_max_x = window->ClipRect.Max.x;
        window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
        window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
        is_visible = ItemAdd(bb, id, NULL, extra_item_flags);
        window->ClipRect.Min.x = backup_clip_rect_min_x;
        window->ClipRect.Max.x = backup_clip_rect_max_x;
    }
    else
    {
        is_visible = ItemAdd(bb, id, NULL, extra_item_flags);
    }

    const bool is_multi_select = (g.LastItemData.ItemFlags & UxImGuiItemFlags_IsMultiSelect) != 0;
    if (!is_visible)
        if (!is_multi_select || !g.BoxSelectState.UnclipMode || !g.BoxSelectState.UnclipRect.Overlaps(bb)) // Extra layer of "no logic clip" for box-select support (would be more overhead to add to ItemAdd)
            return false;

    const bool disabled_global = (g.CurrentItemFlags & UxImGuiItemFlags_Disabled) != 0;
    if (disabled_item && !disabled_global) // Only testing this as an optimization
        BeginDisabled();

    // FIXME: We can standardize the behavior of those two, we could also keep the fast path of override ClipRect + full push on render only,
    // which would be advantageous since most selectable are not selected.
    if (span_all_columns)
    {
        if (g.CurrentTable)
            TablePushBackgroundChannel();
        else if (window->DC.CurrentColumns)
            PushColumnsBackground();
        g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_HasClipRect;
        g.LastItemData.ClipRect = window->ClipRect;
    }

    // We use NoHoldingActiveID on menus so user can click and _hold_ on a menu then drag to browse child entries
    UxImGuiButtonFlags button_flags = 0;
    if (flags & UxImGuiSelectableFlags_NoHoldingActiveID) { button_flags |= UxImGuiButtonFlags_NoHoldingActiveId; }
    if (flags & UxImGuiSelectableFlags_NoSetKeyOwner)     { button_flags |= UxImGuiButtonFlags_NoSetKeyOwner; }
    if (flags & UxImGuiSelectableFlags_SelectOnClick)     { button_flags |= UxImGuiButtonFlags_PressedOnClick; }
    if (flags & UxImGuiSelectableFlags_SelectOnRelease)   { button_flags |= UxImGuiButtonFlags_PressedOnRelease; }
    if (flags & UxImGuiSelectableFlags_AllowDoubleClick)  { button_flags |= UxImGuiButtonFlags_PressedOnClickRelease | UxImGuiButtonFlags_PressedOnDoubleClick; }
    if ((flags & UxImGuiSelectableFlags_AllowOverlap) || (g.LastItemData.ItemFlags & UxImGuiItemFlags_AllowOverlap)) { button_flags |= UxImGuiButtonFlags_AllowOverlap; }

    // Multi-selection support (header)
    const bool was_selected = selected;
    if (is_multi_select)
    {
        // Handle multi-select + alter button flags for it
        MultiSelectItemHeader(id, &selected, &button_flags);
    }

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);

    // Multi-selection support (footer)
    if (is_multi_select)
    {
        MultiSelectItemFooter(id, &selected, &pressed);
    }
    else
    {
        // Auto-select when moved into
        // - This will be more fully fleshed in the range-select branch
        // - This is not exposed as it won't nicely work with some user side handling of shift/control
        // - We cannot do 'if (g.NavJustMovedToId != id) { selected = false; pressed = was_selected; }' for two reasons
        //   - (1) it would require focus scope to be set, need exposing PushFocusScope() or equivalent (e.g. BeginSelection() calling PushFocusScope())
        //   - (2) usage will fail with clipped items
        //   The multi-select API aim to fix those issues, e.g. may be replaced with a BeginSelection() API.
        if ((flags & UxImGuiSelectableFlags_SelectOnNav) && g.NavJustMovedToId != 0 && g.NavJustMovedToFocusScopeId == g.CurrentFocusScopeId)
            if (g.NavJustMovedToId == id)
                selected = pressed = true;
    }

    // Update NavId when clicking or when Hovering (this doesn't happen on most widgets), so navigation can be resumed with keyboard/gamepad
    if (pressed || (hovered && (flags & UxImGuiSelectableFlags_SetNavIdOnHover)))
    {
        if (!g.NavHighlightItemUnderNav && g.NavWindow == window && g.NavLayer == window->DC.NavLayerCurrent)
        {
            SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, WindowRectAbsToRel(window, bb)); // (bb == NavRect)
            if (g.IO.ConfigNavCursorVisibleAuto)
                g.NavCursorVisible = false;
        }
    }
    if (pressed)
        MarkItemEdited(id);

    if (selected != was_selected)
        g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_ToggledSelection;

    // Render
    if (is_visible)
    {
        const bool highlighted = hovered || (flags & UxImGuiSelectableFlags_Highlight);
        if (highlighted || selected)
        {
            // Between 1.91.0 and 1.91.4 we made selected Selectable use an arbitrary lerp between _Header and _HeaderHovered. Removed that now. (#8106)
            UxImU32 col = GetColorU32((held && highlighted) ? UxImGuiCol_HeaderActive : highlighted ? UxImGuiCol_HeaderHovered : UxImGuiCol_Header);
            RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
        }
        if (g.NavId == id)
        {
            UxImGuiNavRenderCursorFlags nav_render_cursor_flags = UxImGuiNavRenderCursorFlags_Compact | UxImGuiNavRenderCursorFlags_NoRounding;
            if (is_multi_select)
                nav_render_cursor_flags |= UxImGuiNavRenderCursorFlags_AlwaysDraw; // Always show the nav rectangle
            RenderNavCursor(bb, id, nav_render_cursor_flags);
        }
    }

    if (span_all_columns)
    {
        if (g.CurrentTable)
            TablePopBackgroundChannel();
        else if (window->DC.CurrentColumns)
            PopColumnsBackground();
    }

    // Text stays at the submission position. Alignment/clipping extents ignore SpanAllColumns.
    if (is_visible)
        RenderTextClipped(pos, UxImVec2(UxImMin(pos.x + size.x, window->WorkRect.Max.x), pos.y + size.y), label, NULL, &label_size, style.SelectableTextAlign, &bb);

    // Automatically close popups
    if (pressed && (window->Flags & UxImGuiWindowFlags_Popup) && !(flags & UxImGuiSelectableFlags_NoAutoClosePopups) && (g.LastItemData.ItemFlags & UxImGuiItemFlags_AutoClosePopups))
        CloseCurrentPopup();

    if (disabled_item && !disabled_global)
        EndDisabled();

    // Selectable() always returns a pressed state!
    // Users of BeginMultiSelect()/EndMultiSelect() scope: you may call UxImGui::IsItemToggledSelection() to retrieve
    // selection toggle, only useful if you need that state updated (e.g. for rendering purpose) before reaching EndMultiSelect().
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed; //-V1020
}

bool UxImGui::Selectable(const char* label, bool* p_selected, UxImGuiSelectableFlags flags, const UxImVec2& size_arg)
{
    if (Selectable(label, *p_selected, flags, size_arg))
    {
        *p_selected = !*p_selected;
        return true;
    }
    return false;
}


//-------------------------------------------------------------------------
// [SECTION] Widgets: Typing-Select support
//-------------------------------------------------------------------------

// [Experimental] Currently not exposed in public API.
// Consume character inputs and return search request, if any.
// This would typically only be called on the focused window or location you want to grab inputs for, e.g.
//   if (UxImGui::IsWindowFocused(...))
//       if (UxImGuiTypingSelectRequest* req = UxImGui::GetTypingSelectRequest())
//           focus_idx = UxImGui::TypingSelectFindMatch(req, my_items.size(), [](void*, int n) { return my_items[n]->Name; }, &my_items, -1);
// However the code is written in a way where calling it from multiple locations is safe (e.g. to obtain buffer).
UxImGuiTypingSelectRequest* UxImGui::GetTypingSelectRequest(UxImGuiTypingSelectFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiTypingSelectState* data = &g.TypingSelectState;
    UxImGuiTypingSelectRequest* out_request = &data->Request;

    // Clear buffer
    const float TYPING_SELECT_RESET_TIMER = 1.80f;          // FIXME: Potentially move to IO config.
    const int TYPING_SELECT_SINGLE_CHAR_COUNT_FOR_LOCK = 4; // Lock single char matching when repeating same char 4 times
    if (data->SearchBuffer[0] != 0)
    {
        bool clear_buffer = false;
        clear_buffer |= (g.NavFocusScopeId != data->FocusScope);
        clear_buffer |= (data->LastRequestTime + TYPING_SELECT_RESET_TIMER < g.Time);
        clear_buffer |= g.NavAnyRequest;
        clear_buffer |= g.ActiveId != 0 && g.NavActivateId == 0; // Allow temporary SPACE activation to not interfere
        clear_buffer |= IsKeyPressed(UxImGuiKey_Escape) || IsKeyPressed(UxImGuiKey_Enter);
        clear_buffer |= IsKeyPressed(UxImGuiKey_Backspace) && (flags & UxImGuiTypingSelectFlags_AllowBackspace) == 0;
        //if (clear_buffer) { IMGUI_DEBUG_LOG("GetTypingSelectRequest(): Clear SearchBuffer.\n"); }
        if (clear_buffer)
            data->Clear();
    }

    // Append to buffer
    const int buffer_max_len = IM_ARRAYSIZE(data->SearchBuffer) - 1;
    int buffer_len = (int)UxImStrlen(data->SearchBuffer);
    bool select_request = false;
    for (UxImWchar w : g.IO.InputQueueCharacters)
    {
        const int w_len = UxImTextCountUtf8BytesFromStr(&w, &w + 1);
        if (w < 32 || (buffer_len == 0 && UxImCharIsBlankW(w)) || (buffer_len + w_len > buffer_max_len)) // Ignore leading blanks
            continue;
        char w_buf[5];
        UxImTextCharToUtf8(w_buf, (unsigned int)w);
        if (data->SingleCharModeLock && w_len == out_request->SingleCharSize && memcmp(w_buf, data->SearchBuffer, w_len) == 0)
        {
            select_request = true; // Same character: don't need to append to buffer.
            continue;
        }
        if (data->SingleCharModeLock)
        {
            data->Clear(); // Different character: clear
            buffer_len = 0;
        }
        memcpy(data->SearchBuffer + buffer_len, w_buf, w_len + 1); // Append
        buffer_len += w_len;
        select_request = true;
    }
    g.IO.InputQueueCharacters.resize(0);

    // Handle backspace
    if ((flags & UxImGuiTypingSelectFlags_AllowBackspace) && IsKeyPressed(UxImGuiKey_Backspace, UxImGuiInputFlags_Repeat))
    {
        char* p = (char*)(void*)UxImTextFindPreviousUtf8Codepoint(data->SearchBuffer, data->SearchBuffer + buffer_len);
        *p = 0;
        buffer_len = (int)(p - data->SearchBuffer);
    }

    // Return request if any
    if (buffer_len == 0)
        return NULL;
    if (select_request)
    {
        data->FocusScope = g.NavFocusScopeId;
        data->LastRequestFrame = g.FrameCount;
        data->LastRequestTime = (float)g.Time;
    }
    out_request->Flags = flags;
    out_request->SearchBufferLen = buffer_len;
    out_request->SearchBuffer = data->SearchBuffer;
    out_request->SelectRequest = (data->LastRequestFrame == g.FrameCount);
    out_request->SingleCharMode = false;
    out_request->SingleCharSize = 0;

    // Calculate if buffer contains the same character repeated.
    // - This can be used to implement a special search mode on first character.
    // - Performed on UTF-8 codepoint for correctness.
    // - SingleCharMode is always set for first input character, because it usually leads to a "next".
    if (flags & UxImGuiTypingSelectFlags_AllowSingleCharMode)
    {
        const char* buf_begin = out_request->SearchBuffer;
        const char* buf_end = out_request->SearchBuffer + out_request->SearchBufferLen;
        const int c0_len = UxImTextCountUtf8BytesFromChar(buf_begin, buf_end);
        const char* p = buf_begin + c0_len;
        for (; p < buf_end; p += c0_len)
            if (memcmp(buf_begin, p, (size_t)c0_len) != 0)
                break;
        const int single_char_count = (p == buf_end) ? (out_request->SearchBufferLen / c0_len) : 0;
        out_request->SingleCharMode = (single_char_count > 0 || data->SingleCharModeLock);
        out_request->SingleCharSize = (UxImS8)c0_len;
        data->SingleCharModeLock |= (single_char_count >= TYPING_SELECT_SINGLE_CHAR_COUNT_FOR_LOCK); // From now on we stop search matching to lock to single char mode.
    }

    return out_request;
}

static int UxImStrimatchlen(const char* s1, const char* s1_end, const char* s2)
{
    int match_len = 0;
    while (s1 < s1_end && UxImToUpper(*s1++) == UxImToUpper(*s2++))
        match_len++;
    return match_len;
}

// Default handler for finding a result for typing-select. You may implement your own.
// You might want to display a tooltip to visualize the current request SearchBuffer
// When SingleCharMode is set:
// - it is better to NOT display a tooltip of other on-screen display indicator.
// - the index of the currently focused item is required.
//   if your SetNextItemSelectionUserData() values are indices, you can obtain it from UxImGuiMultiSelectIO::NavIdItem, otherwise from g.NavLastValidSelectionUserData.
int UxImGui::TypingSelectFindMatch(UxImGuiTypingSelectRequest* req, int items_count, const char* (*get_item_name_func)(void*, int), void* user_data, int nav_item_idx)
{
    if (req == NULL || req->SelectRequest == false) // Support NULL parameter so both calls can be done from same spot.
        return -1;
    int idx = -1;
    if (req->SingleCharMode && (req->Flags & UxImGuiTypingSelectFlags_AllowSingleCharMode))
        idx = TypingSelectFindNextSingleCharMatch(req, items_count, get_item_name_func, user_data, nav_item_idx);
    else
        idx = TypingSelectFindBestLeadingMatch(req, items_count, get_item_name_func, user_data);
    if (idx != -1)
        SetNavCursorVisibleAfterMove();
    return idx;
}

// Special handling when a single character is repeated: perform search on a single letter and goes to next.
int UxImGui::TypingSelectFindNextSingleCharMatch(UxImGuiTypingSelectRequest* req, int items_count, const char* (*get_item_name_func)(void*, int), void* user_data, int nav_item_idx)
{
    // FIXME: Assume selection user data is index. Would be extremely practical.
    //if (nav_item_idx == -1)
    //    nav_item_idx = (int)g.NavLastValidSelectionUserData;

    int first_match_idx = -1;
    bool return_next_match = false;
    for (int idx = 0; idx < items_count; idx++)
    {
        const char* item_name = get_item_name_func(user_data, idx);
        if (UxImStrimatchlen(req->SearchBuffer, req->SearchBuffer + req->SingleCharSize, item_name) < req->SingleCharSize)
            continue;
        if (return_next_match)                           // Return next matching item after current item.
            return idx;
        if (first_match_idx == -1 && nav_item_idx == -1) // Return first match immediately if we don't have a nav_item_idx value.
            return idx;
        if (first_match_idx == -1)                       // Record first match for wrapping.
            first_match_idx = idx;
        if (nav_item_idx == idx)                         // Record that we encountering nav_item so we can return next match.
            return_next_match = true;
    }
    return first_match_idx; // First result
}

int UxImGui::TypingSelectFindBestLeadingMatch(UxImGuiTypingSelectRequest* req, int items_count, const char* (*get_item_name_func)(void*, int), void* user_data)
{
    int longest_match_idx = -1;
    int longest_match_len = 0;
    for (int idx = 0; idx < items_count; idx++)
    {
        const char* item_name = get_item_name_func(user_data, idx);
        const int match_len = UxImStrimatchlen(req->SearchBuffer, req->SearchBuffer + req->SearchBufferLen, item_name);
        if (match_len <= longest_match_len)
            continue;
        longest_match_idx = idx;
        longest_match_len = match_len;
        if (match_len == req->SearchBufferLen)
            break;
    }
    return longest_match_idx;
}

void UxImGui::DebugNodeTypingSelectState(UxImGuiTypingSelectState* data)
{
#ifndef IMGUI_DISABLE_DEBUG_TOOLS
    Text("SearchBuffer = \"%s\"", data->SearchBuffer);
    Text("SingleCharMode = %d, Size = %d, Lock = %d", data->Request.SingleCharMode, data->Request.SingleCharSize, data->SingleCharModeLock);
    Text("LastRequest = time: %.2f, frame: %d", data->LastRequestTime, data->LastRequestFrame);
#else
    IM_UNUSED(data);
#endif
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Box-Select support
// This has been extracted away from Multi-Select logic in the hope that it could eventually be used elsewhere, but hasn't been yet.
//-------------------------------------------------------------------------
// Extra logic in MultiSelectItemFooter() and UxImGuiListClipper::Step()
//-------------------------------------------------------------------------
// - BoxSelectPreStartDrag() [Internal]
// - BoxSelectActivateDrag() [Internal]
// - BoxSelectDeactivateDrag() [Internal]
// - BoxSelectScrollWithMouseDrag() [Internal]
// - BeginBoxSelect() [Internal]
// - EndBoxSelect() [Internal]
//-------------------------------------------------------------------------

// Call on the initial click.
static void BoxSelectPreStartDrag(UxImGuiID id, UxImGuiSelectionUserData clicked_item)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiBoxSelectState* bs = &g.BoxSelectState;
    bs->ID = id;
    bs->IsStarting = true; // Consider starting box-select.
    bs->IsStartedFromVoid = (clicked_item == UxImGuiSelectionUserData_Invalid);
    bs->IsStartedSetNavIdOnce = bs->IsStartedFromVoid;
    bs->KeyMods = g.IO.KeyMods;
    bs->StartPosRel = bs->EndPosRel = UxImGui::WindowPosAbsToRel(g.CurrentWindow, g.IO.MousePos);
    bs->ScrollAccum = UxImVec2(0.0f, 0.0f);
}

static void BoxSelectActivateDrag(UxImGuiBoxSelectState* bs, UxImGuiWindow* window)
{
    UxImGuiContext& g = *GUxImGui;
    IMGUI_DEBUG_LOG_SELECTION("[selection] BeginBoxSelect() 0X%08X: Activate\n", bs->ID);
    bs->IsActive = true;
    bs->Window = window;
    bs->IsStarting = false;
    UxImGui::SetActiveID(bs->ID, window);
    UxImGui::SetActiveIdUsingAllKeyboardKeys();
    if (bs->IsStartedFromVoid && (bs->KeyMods & (UxImGuiMod_Ctrl | UxImGuiMod_Shift)) == 0)
        bs->RequestClear = true;
}

static void BoxSelectDeactivateDrag(UxImGuiBoxSelectState* bs)
{
    UxImGuiContext& g = *GUxImGui;
    bs->IsActive = bs->IsStarting = false;
    if (g.ActiveId == bs->ID)
    {
        IMGUI_DEBUG_LOG_SELECTION("[selection] BeginBoxSelect() 0X%08X: Deactivate\n", bs->ID);
        UxImGui::ClearActiveID();
    }
    bs->ID = 0;
}

static void BoxSelectScrollWithMouseDrag(UxImGuiBoxSelectState* bs, UxImGuiWindow* window, const UxImRect& inner_r)
{
    UxImGuiContext& g = *GUxImGui;
    IM_ASSERT(bs->Window == window);
    for (int n = 0; n < 2; n++) // each axis
    {
        const float mouse_pos = g.IO.MousePos[n];
        const float dist = (mouse_pos > inner_r.Max[n]) ? mouse_pos - inner_r.Max[n] : (mouse_pos < inner_r.Min[n]) ? mouse_pos - inner_r.Min[n] : 0.0f;
        const float scroll_curr = window->Scroll[n];
        if (dist == 0.0f || (dist < 0.0f && scroll_curr < 0.0f) || (dist > 0.0f && scroll_curr >= window->ScrollMax[n]))
            continue;

        const float speed_multiplier = UxImLinearRemapClamp(g.FontSize, g.FontSize * 5.0f, 1.0f, 4.0f, UxImAbs(dist)); // x1 to x4 depending on distance
        const float scroll_step = g.FontSize * 35.0f * speed_multiplier * UxImSign(dist) * g.IO.DeltaTime;
        bs->ScrollAccum[n] += scroll_step;

        // Accumulate into a stored value so we can handle high-framerate
        const float scroll_step_i = UxImFloor(bs->ScrollAccum[n]);
        if (scroll_step_i == 0.0f)
            continue;
        if (n == 0)
            UxImGui::SetScrollX(window, scroll_curr + scroll_step_i);
        else
            UxImGui::SetScrollY(window, scroll_curr + scroll_step_i);
        bs->ScrollAccum[n] -= scroll_step_i;
    }
}

bool UxImGui::BeginBoxSelect(const UxImRect& scope_rect, UxImGuiWindow* window, UxImGuiID box_select_id, UxImGuiMultiSelectFlags ms_flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiBoxSelectState* bs = &g.BoxSelectState;
    KeepAliveID(box_select_id);
    if (bs->ID != box_select_id)
        return false;

    // IsStarting is set by MultiSelectItemFooter() when considering a possible box-select. We validate it here and lock geometry.
    bs->UnclipMode = false;
    bs->RequestClear = false;
    if (bs->IsStarting && IsMouseDragPastThreshold(0))
        BoxSelectActivateDrag(bs, window);
    else if ((bs->IsStarting || bs->IsActive) && g.IO.MouseDown[0] == false)
        BoxSelectDeactivateDrag(bs);
    if (!bs->IsActive)
        return false;

    // Current frame absolute prev/current rectangles are used to toggle selection.
    // They are derived from positions relative to scrolling space.
    UxImVec2 start_pos_abs = WindowPosRelToAbs(window, bs->StartPosRel);
    UxImVec2 prev_end_pos_abs = WindowPosRelToAbs(window, bs->EndPosRel); // Clamped already
    UxImVec2 curr_end_pos_abs = g.IO.MousePos;
    if (ms_flags & UxImGuiMultiSelectFlags_ScopeWindow) // Box-select scrolling only happens with ScopeWindow
        curr_end_pos_abs = UxImClamp(curr_end_pos_abs, scope_rect.Min, scope_rect.Max);
    bs->BoxSelectRectPrev.Min = UxImMin(start_pos_abs, prev_end_pos_abs);
    bs->BoxSelectRectPrev.Max = UxImMax(start_pos_abs, prev_end_pos_abs);
    bs->BoxSelectRectCurr.Min = UxImMin(start_pos_abs, curr_end_pos_abs);
    bs->BoxSelectRectCurr.Max = UxImMax(start_pos_abs, curr_end_pos_abs);

    // Box-select 2D mode detects horizontal changes (vertical ones are already picked by Clipper)
    // Storing an extra rect used by widgets supporting box-select.
    if (ms_flags & UxImGuiMultiSelectFlags_BoxSelect2d)
        if (bs->BoxSelectRectPrev.Min.x != bs->BoxSelectRectCurr.Min.x || bs->BoxSelectRectPrev.Max.x != bs->BoxSelectRectCurr.Max.x)
        {
            bs->UnclipMode = true;
            bs->UnclipRect = bs->BoxSelectRectPrev; // FIXME-OPT: UnclipRect x coordinates could be intersection of Prev and Curr rect on X axis.
            bs->UnclipRect.Add(bs->BoxSelectRectCurr);
        }

    //GetForegroundDrawList()->AddRect(bs->UnclipRect.Min, bs->UnclipRect.Max, IM_COL32(255,0,0,200), 0.0f, 0, 3.0f);
    //GetForegroundDrawList()->AddRect(bs->BoxSelectRectPrev.Min, bs->BoxSelectRectPrev.Max, IM_COL32(255,0,0,200), 0.0f, 0, 3.0f);
    //GetForegroundDrawList()->AddRect(bs->BoxSelectRectCurr.Min, bs->BoxSelectRectCurr.Max, IM_COL32(0,255,0,200), 0.0f, 0, 1.0f);
    return true;
}

void UxImGui::EndBoxSelect(const UxImRect& scope_rect, UxImGuiMultiSelectFlags ms_flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    UxImGuiBoxSelectState* bs = &g.BoxSelectState;
    IM_ASSERT(bs->IsActive);
    bs->UnclipMode = false;

    // Render selection rectangle
    bs->EndPosRel = WindowPosAbsToRel(window, UxImClamp(g.IO.MousePos, scope_rect.Min, scope_rect.Max)); // Clamp stored position according to current scrolling view
    UxImRect box_select_r = bs->BoxSelectRectCurr;
    box_select_r.ClipWith(scope_rect);
    window->DrawList->AddRectFilled(box_select_r.Min, box_select_r.Max, GetColorU32(UxImGuiCol_SeparatorHovered, 0.30f)); // FIXME-MULTISELECT: Styling
    window->DrawList->AddRect(box_select_r.Min, box_select_r.Max, GetColorU32(UxImGuiCol_NavCursor)); // FIXME-MULTISELECT FIXME-DPI: Styling

    // Scroll
    const bool enable_scroll = (ms_flags & UxImGuiMultiSelectFlags_ScopeWindow) && (ms_flags & UxImGuiMultiSelectFlags_BoxSelectNoScroll) == 0;
    if (enable_scroll)
    {
        UxImRect scroll_r = scope_rect;
        scroll_r.Expand(-g.FontSize);
        //GetForegroundDrawList()->AddRect(scroll_r.Min, scroll_r.Max, IM_COL32(0, 255, 0, 255));
        if (!scroll_r.Contains(g.IO.MousePos))
            BoxSelectScrollWithMouseDrag(bs, window, scroll_r);
    }
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Multi-Select support
//-------------------------------------------------------------------------
// - DebugLogMultiSelectRequests() [Internal]
// - CalcScopeRect() [Internal]
// - BeginMultiSelect()
// - EndMultiSelect()
// - SetNextItemSelectionUserData()
// - MultiSelectItemHeader() [Internal]
// - MultiSelectItemFooter() [Internal]
// - DebugNodeMultiSelectState() [Internal]
//-------------------------------------------------------------------------

static void DebugLogMultiSelectRequests(const char* function, const UxImGuiMultiSelectIO* io)
{
    UxImGuiContext& g = *GUxImGui;
    IM_UNUSED(function);
    for (const UxImGuiSelectionRequest& req : io->Requests)
    {
        if (req.Type == UxImGuiSelectionRequestType_SetAll)    IMGUI_DEBUG_LOG_SELECTION("[selection] %s: Request: SetAll %d (= %s)\n", function, req.Selected, req.Selected ? "SelectAll" : "Clear");
        if (req.Type == UxImGuiSelectionRequestType_SetRange)  IMGUI_DEBUG_LOG_SELECTION("[selection] %s: Request: SetRange %" IM_PRId64 "..%" IM_PRId64 " (0x%" IM_PRIX64 "..0x%" IM_PRIX64 ") = %d (dir %d)\n", function, req.RangeFirstItem, req.RangeLastItem, req.RangeFirstItem, req.RangeLastItem, req.Selected, req.RangeDirection);
    }
}

static UxImRect CalcScopeRect(UxImGuiMultiSelectTempData* ms, UxImGuiWindow* window)
{
    UxImGuiContext& g = *GUxImGui;
    if (ms->Flags & UxImGuiMultiSelectFlags_ScopeRect)
    {
        // Warning: this depends on CursorMaxPos so it means to be called by EndMultiSelect() only
        return UxImRect(ms->ScopeRectMin, UxImMax(window->DC.CursorMaxPos, ms->ScopeRectMin));
    }
    else
    {
        // When a table, pull HostClipRect, which allows us to predict ClipRect before first row/layout is performed. (#7970)
        UxImRect scope_rect = window->InnerClipRect;
        if (g.CurrentTable != NULL)
            scope_rect = g.CurrentTable->HostClipRect;

        // Add inner table decoration (#7821) // FIXME: Why not baking in InnerClipRect?
        scope_rect.Min = UxImMin(scope_rect.Min + UxImVec2(window->DecoInnerSizeX1, window->DecoInnerSizeY1), scope_rect.Max);
        return scope_rect;
    }
}

// Return UxImGuiMultiSelectIO structure.
// Lifetime: don't hold on UxImGuiMultiSelectIO* pointers over multiple frames or past any subsequent call to BeginMultiSelect() or EndMultiSelect().
// Passing 'selection_size' and 'items_count' parameters is currently optional.
// - 'selection_size' is useful to disable some shortcut routing: e.g. UxImGuiMultiSelectFlags_ClearOnEscape won't claim Escape key when selection_size 0,
//    allowing a first press to clear selection THEN the second press to leave child window and return to parent.
// - 'items_count' is stored in UxImGuiMultiSelectIO which makes it a convenient way to pass the information to your ApplyRequest() handler (but you may pass it differently).
// - If they are costly for you to compute (e.g. external intrusive selection without maintaining size), you may avoid them and pass -1.
//   - If you can easily tell if your selection is empty or not, you may pass 0/1, or you may enable UxImGuiMultiSelectFlags_ClearOnEscape flag dynamically.
UxImGuiMultiSelectIO* UxImGui::BeginMultiSelect(UxImGuiMultiSelectFlags flags, int selection_size, int items_count)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    if (++g.MultiSelectTempDataStacked > g.MultiSelectTempData.Size)
        g.MultiSelectTempData.resize(g.MultiSelectTempDataStacked, UxImGuiMultiSelectTempData());
    UxImGuiMultiSelectTempData* ms = &g.MultiSelectTempData[g.MultiSelectTempDataStacked - 1];
    IM_STATIC_ASSERT(offsetof(UxImGuiMultiSelectTempData, IO) == 0); // Clear() relies on that.
    g.CurrentMultiSelect = ms;
    if ((flags & (UxImGuiMultiSelectFlags_ScopeWindow | UxImGuiMultiSelectFlags_ScopeRect)) == 0)
        flags |= UxImGuiMultiSelectFlags_ScopeWindow;
    if (flags & UxImGuiMultiSelectFlags_SingleSelect)
        flags &= ~(UxImGuiMultiSelectFlags_BoxSelect2d | UxImGuiMultiSelectFlags_BoxSelect1d);
    if (flags & UxImGuiMultiSelectFlags_BoxSelect2d)
        flags &= ~UxImGuiMultiSelectFlags_BoxSelect1d;

    // FIXME: Workaround to the fact we override CursorMaxPos, meaning size measurement are lost. (#8250)
    // They should perhaps be stacked properly?
    if (UxImGuiTable* table = g.CurrentTable)
        if (table->CurrentColumn != -1)
            TableEndCell(table); // This is currently safe to call multiple time. If that properly is lost we can extract the "save measurement" part of it.

    // FIXME: BeginFocusScope()
    const UxImGuiID id = window->IDStack.back();
    ms->Clear();
    ms->FocusScopeId = id;
    ms->Flags = flags;
    ms->IsFocused = (ms->FocusScopeId == g.NavFocusScopeId);
    ms->BackupCursorMaxPos = window->DC.CursorMaxPos;
    ms->ScopeRectMin = window->DC.CursorMaxPos = window->DC.CursorPos;
    PushFocusScope(ms->FocusScopeId);
    if (flags & UxImGuiMultiSelectFlags_ScopeWindow) // Mark parent child window as navigable into, with highlight. Assume user will always submit interactive items.
        window->DC.NavLayersActiveMask |= 1 << UxImGuiNavLayer_Main;

    // Use copy of keyboard mods at the time of the request, otherwise we would requires mods to be held for an extra frame.
    ms->KeyMods = g.NavJustMovedToId ? (g.NavJustMovedToIsTabbing ? 0 : g.NavJustMovedToKeyMods) : g.IO.KeyMods;
    if (flags & UxImGuiMultiSelectFlags_NoRangeSelect)
        ms->KeyMods &= ~UxImGuiMod_Shift;

    // Bind storage
    UxImGuiMultiSelectState* storage = g.MultiSelectStorage.GetOrAddByKey(id);
    storage->ID = id;
    storage->LastFrameActive = g.FrameCount;
    storage->LastSelectionSize = selection_size;
    storage->Window = window;
    ms->Storage = storage;

    // Output to user
    ms->IO.Requests.resize(0);
    ms->IO.RangeSrcItem = storage->RangeSrcItem;
    ms->IO.NavIdItem = storage->NavIdItem;
    ms->IO.NavIdSelected = (storage->NavIdSelected == 1) ? true : false;
    ms->IO.ItemsCount = items_count;

    // Clear when using Navigation to move within the scope
    // (we compare FocusScopeId so it possible to use multiple selections inside a same window)
    bool request_clear = false;
    bool request_select_all = false;
    if (g.NavJustMovedToId != 0 && g.NavJustMovedToFocusScopeId == ms->FocusScopeId && g.NavJustMovedToHasSelectionData)
    {
        if (ms->KeyMods & UxImGuiMod_Shift)
            ms->IsKeyboardSetRange = true;
        if (ms->IsKeyboardSetRange)
            IM_ASSERT(storage->RangeSrcItem != UxImGuiSelectionUserData_Invalid); // Not ready -> could clear?
        if ((ms->KeyMods & (UxImGuiMod_Ctrl | UxImGuiMod_Shift)) == 0 && (flags & (UxImGuiMultiSelectFlags_NoAutoClear | UxImGuiMultiSelectFlags_NoAutoSelect)) == 0)
            request_clear = true;
    }
    else if (g.NavJustMovedFromFocusScopeId == ms->FocusScopeId)
    {
        // Also clear on leaving scope (may be optional?)
        if ((ms->KeyMods & (UxImGuiMod_Ctrl | UxImGuiMod_Shift)) == 0 && (flags & (UxImGuiMultiSelectFlags_NoAutoClear | UxImGuiMultiSelectFlags_NoAutoSelect)) == 0)
            request_clear = true;
    }

    // Box-select handling: update active state.
    UxImGuiBoxSelectState* bs = &g.BoxSelectState;
    if (flags & (UxImGuiMultiSelectFlags_BoxSelect1d | UxImGuiMultiSelectFlags_BoxSelect2d))
    {
        ms->BoxSelectId = GetID("##BoxSelect");
        if (BeginBoxSelect(CalcScopeRect(ms, window), window, ms->BoxSelectId, flags))
            request_clear |= bs->RequestClear;
    }

    if (ms->IsFocused)
    {
        // Shortcut: Clear selection (Escape)
        // - Only claim shortcut if selection is not empty, allowing further presses on Escape to e.g. leave current child window.
        // - Box select also handle Escape and needs to pass an id to bypass ActiveIdUsingAllKeyboardKeys lock.
        if (flags & UxImGuiMultiSelectFlags_ClearOnEscape)
        {
            if (selection_size != 0 || bs->IsActive)
                if (Shortcut(UxImGuiKey_Escape, UxImGuiInputFlags_None, bs->IsActive ? bs->ID : 0))
                {
                    request_clear = true;
                    if (bs->IsActive)
                        BoxSelectDeactivateDrag(bs);
                }
        }

        // Shortcut: Select all (CTRL+A)
        if (!(flags & UxImGuiMultiSelectFlags_SingleSelect) && !(flags & UxImGuiMultiSelectFlags_NoSelectAll))
            if (Shortcut(UxImGuiMod_Ctrl | UxImGuiKey_A))
                request_select_all = true;
    }

    if (request_clear || request_select_all)
    {
        MultiSelectAddSetAll(ms, request_select_all);
        if (!request_select_all)
            storage->LastSelectionSize = 0;
    }
    ms->LoopRequestSetAll = request_select_all ? 1 : request_clear ? 0 : -1;
    ms->LastSubmittedItem = UxImGuiSelectionUserData_Invalid;

    if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventSelection)
        DebugLogMultiSelectRequests("BeginMultiSelect", &ms->IO);

    return &ms->IO;
}

// Return updated UxImGuiMultiSelectIO structure.
// Lifetime: don't hold on UxImGuiMultiSelectIO* pointers over multiple frames or past any subsequent call to BeginMultiSelect() or EndMultiSelect().
UxImGuiMultiSelectIO* UxImGui::EndMultiSelect()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiMultiSelectTempData* ms = g.CurrentMultiSelect;
    UxImGuiMultiSelectState* storage = ms->Storage;
    UxImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT_USER_ERROR(ms->FocusScopeId == g.CurrentFocusScopeId, "EndMultiSelect() FocusScope mismatch!");
    IM_ASSERT(g.CurrentMultiSelect != NULL && storage->Window == g.CurrentWindow);
    IM_ASSERT(g.MultiSelectTempDataStacked > 0 && &g.MultiSelectTempData[g.MultiSelectTempDataStacked - 1] == g.CurrentMultiSelect);

    UxImRect scope_rect = CalcScopeRect(ms, window);
    if (ms->IsFocused)
    {
        // We currently don't allow user code to modify RangeSrcItem by writing to BeginIO's version, but that would be an easy change here.
        if (ms->IO.RangeSrcReset || (ms->RangeSrcPassedBy == false && ms->IO.RangeSrcItem != UxImGuiSelectionUserData_Invalid)) // Can't read storage->RangeSrcItem here -> we want the state at beginning of the scope (see tests for easy failure)
        {
            IMGUI_DEBUG_LOG_SELECTION("[selection] EndMultiSelect: Reset RangeSrcItem.\n"); // Will set be to NavId.
            storage->RangeSrcItem = UxImGuiSelectionUserData_Invalid;
        }
        if (ms->NavIdPassedBy == false && storage->NavIdItem != UxImGuiSelectionUserData_Invalid)
        {
            IMGUI_DEBUG_LOG_SELECTION("[selection] EndMultiSelect: Reset NavIdItem.\n");
            storage->NavIdItem = UxImGuiSelectionUserData_Invalid;
            storage->NavIdSelected = -1;
        }

        if ((ms->Flags & (UxImGuiMultiSelectFlags_BoxSelect1d | UxImGuiMultiSelectFlags_BoxSelect2d)) && GetBoxSelectState(ms->BoxSelectId))
            EndBoxSelect(scope_rect, ms->Flags);
    }

    if (ms->IsEndIO == false)
        ms->IO.Requests.resize(0);

    // Clear selection when clicking void?
    // We specifically test for IsMouseDragPastThreshold(0) == false to allow box-selection!
    // The InnerRect test is necessary for non-child/decorated windows.
    bool scope_hovered = IsWindowHovered() && window->InnerRect.Contains(g.IO.MousePos);
    if (scope_hovered && (ms->Flags & UxImGuiMultiSelectFlags_ScopeRect))
        scope_hovered &= scope_rect.Contains(g.IO.MousePos);
    if (scope_hovered && g.HoveredId == 0 && g.ActiveId == 0)
    {
        if (ms->Flags & (UxImGuiMultiSelectFlags_BoxSelect1d | UxImGuiMultiSelectFlags_BoxSelect2d))
        {
            if (!g.BoxSelectState.IsActive && !g.BoxSelectState.IsStarting && g.IO.MouseClickedCount[0] == 1)
            {
                BoxSelectPreStartDrag(ms->BoxSelectId, UxImGuiSelectionUserData_Invalid);
                FocusWindow(window, UxImGuiFocusRequestFlags_UnlessBelowModal);
                SetHoveredID(ms->BoxSelectId);
                if (ms->Flags & UxImGuiMultiSelectFlags_ScopeRect)
                    SetNavID(0, UxImGuiNavLayer_Main, ms->FocusScopeId, UxImRect(g.IO.MousePos, g.IO.MousePos)); // Automatically switch FocusScope for initial click from void to box-select.
            }
        }

        if (ms->Flags & UxImGuiMultiSelectFlags_ClearOnClickVoid)
            if (IsMouseReleased(0) && IsMouseDragPastThreshold(0) == false && g.IO.KeyMods == UxImGuiMod_None)
                MultiSelectAddSetAll(ms, false);
    }

    // Courtesy nav wrapping helper flag
    if (ms->Flags & UxImGuiMultiSelectFlags_NavWrapX)
    {
        IM_ASSERT(ms->Flags & UxImGuiMultiSelectFlags_ScopeWindow); // Only supported at window scope
        UxImGui::NavMoveRequestTryWrapping(UxImGui::GetCurrentWindow(), UxImGuiNavMoveFlags_WrapX);
    }

    // Unwind
    window->DC.CursorMaxPos = UxImMax(ms->BackupCursorMaxPos, window->DC.CursorMaxPos);
    PopFocusScope();

    if (g.DebugLogFlags & UxImGuiDebugLogFlags_EventSelection)
        DebugLogMultiSelectRequests("EndMultiSelect", &ms->IO);

    ms->FocusScopeId = 0;
    ms->Flags = UxImGuiMultiSelectFlags_None;
    g.CurrentMultiSelect = (--g.MultiSelectTempDataStacked > 0) ? &g.MultiSelectTempData[g.MultiSelectTempDataStacked - 1] : NULL;

    return &ms->IO;
}

void UxImGui::SetNextItemSelectionUserData(UxImGuiSelectionUserData selection_user_data)
{
    // Note that flags will be cleared by ItemAdd(), so it's only useful for Navigation code!
    // This designed so widgets can also cheaply set this before calling ItemAdd(), so we are not tied to MultiSelect api.
    UxImGuiContext& g = *GUxImGui;
    g.NextItemData.SelectionUserData = selection_user_data;
    g.NextItemData.FocusScopeId = g.CurrentFocusScopeId;

    if (UxImGuiMultiSelectTempData* ms = g.CurrentMultiSelect)
    {
        // Auto updating RangeSrcPassedBy for cases were clipper is not used (done before ItemAdd() clipping)
        g.NextItemData.ItemFlags |= UxImGuiItemFlags_HasSelectionUserData | UxImGuiItemFlags_IsMultiSelect;
        if (ms->IO.RangeSrcItem == selection_user_data)
            ms->RangeSrcPassedBy = true;
    }
    else
    {
        g.NextItemData.ItemFlags |= UxImGuiItemFlags_HasSelectionUserData;
    }
}

// In charge of:
// - Applying SetAll for submitted items.
// - Applying SetRange for submitted items and record end points.
// - Altering button behavior flags to facilitate use with drag and drop.
void UxImGui::MultiSelectItemHeader(UxImGuiID id, bool* p_selected, UxImGuiButtonFlags* p_button_flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiMultiSelectTempData* ms = g.CurrentMultiSelect;

    bool selected = *p_selected;
    if (ms->IsFocused)
    {
        UxImGuiMultiSelectState* storage = ms->Storage;
        UxImGuiSelectionUserData item_data = g.NextItemData.SelectionUserData;
        IM_ASSERT(g.NextItemData.FocusScopeId == g.CurrentFocusScopeId && "Forgot to call SetNextItemSelectionUserData() prior to item, required in BeginMultiSelect()/EndMultiSelect() scope");

        // Apply SetAll (Clear/SelectAll) requests requested by BeginMultiSelect().
        // This is only useful if the user hasn't processed them already, and this only works if the user isn't using the clipper.
        // If you are using a clipper you need to process the SetAll request after calling BeginMultiSelect()
        if (ms->LoopRequestSetAll != -1)
            selected = (ms->LoopRequestSetAll == 1);

        // When using SHIFT+Nav: because it can incur scrolling we cannot afford a frame of lag with the selection highlight (otherwise scrolling would happen before selection)
        // For this to work, we need someone to set 'RangeSrcPassedBy = true' at some point (either clipper either SetNextItemSelectionUserData() function)
        if (ms->IsKeyboardSetRange)
        {
            IM_ASSERT(id != 0 && (ms->KeyMods & UxImGuiMod_Shift) != 0);
            const bool is_range_dst = (ms->RangeDstPassedBy == false) && g.NavJustMovedToId == id;     // Assume that g.NavJustMovedToId is not clipped.
            if (is_range_dst)
                ms->RangeDstPassedBy = true;
            if (is_range_dst && storage->RangeSrcItem == UxImGuiSelectionUserData_Invalid) // If we don't have RangeSrc, assign RangeSrc = RangeDst
            {
                storage->RangeSrcItem = item_data;
                storage->RangeSelected = selected ? 1 : 0;
            }
            const bool is_range_src = storage->RangeSrcItem == item_data;
            if (is_range_src || is_range_dst || ms->RangeSrcPassedBy != ms->RangeDstPassedBy)
            {
                // Apply range-select value to visible items
                IM_ASSERT(storage->RangeSrcItem != UxImGuiSelectionUserData_Invalid && storage->RangeSelected != -1);
                selected = (storage->RangeSelected != 0);
            }
            else if ((ms->KeyMods & UxImGuiMod_Ctrl) == 0 && (ms->Flags & UxImGuiMultiSelectFlags_NoAutoClear) == 0)
            {
                // Clear other items
                selected = false;
            }
        }
        *p_selected = selected;
    }

    // Alter button behavior flags
    // To handle drag and drop of multiple items we need to avoid clearing selection on click.
    // Enabling this test makes actions using CTRL+SHIFT delay their effect on MouseUp which is annoying, but it allows drag and drop of multiple items.
    if (p_button_flags != NULL)
    {
        UxImGuiButtonFlags button_flags = *p_button_flags;
        button_flags |= UxImGuiButtonFlags_NoHoveredOnFocus;
        if ((!selected || (g.ActiveId == id && g.ActiveIdHasBeenPressedBefore)) && !(ms->Flags & UxImGuiMultiSelectFlags_SelectOnClickRelease))
            button_flags = (button_flags | UxImGuiButtonFlags_PressedOnClick) & ~UxImGuiButtonFlags_PressedOnClickRelease;
        else
            button_flags |= UxImGuiButtonFlags_PressedOnClickRelease;
        *p_button_flags = button_flags;
    }
}

// In charge of:
// - Auto-select on navigation.
// - Box-select toggle handling.
// - Right-click handling.
// - Altering selection based on Ctrl/Shift modifiers, both for keyboard and mouse.
// - Record current selection state for RangeSrc
// This is all rather complex, best to run and refer to "widgets_multiselect_xxx" tests in imgui_test_suite.
void UxImGui::MultiSelectItemFooter(UxImGuiID id, bool* p_selected, bool* p_pressed)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    bool selected = *p_selected;
    bool pressed = *p_pressed;
    UxImGuiMultiSelectTempData* ms = g.CurrentMultiSelect;
    UxImGuiMultiSelectState* storage = ms->Storage;
    if (pressed)
        ms->IsFocused = true;

    bool hovered = false;
    if (g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_HoveredRect)
        hovered = IsItemHovered(UxImGuiHoveredFlags_AllowWhenBlockedByPopup);
    if (!ms->IsFocused && !hovered)
        return;

    UxImGuiSelectionUserData item_data = g.NextItemData.SelectionUserData;

    UxImGuiMultiSelectFlags flags = ms->Flags;
    const bool is_singleselect = (flags & UxImGuiMultiSelectFlags_SingleSelect) != 0;
    bool is_ctrl = (ms->KeyMods & UxImGuiMod_Ctrl) != 0;
    bool is_shift = (ms->KeyMods & UxImGuiMod_Shift) != 0;

    bool apply_to_range_src = false;

    if (g.NavId == id && storage->RangeSrcItem == UxImGuiSelectionUserData_Invalid)
        apply_to_range_src = true;
    if (ms->IsEndIO == false)
    {
        ms->IO.Requests.resize(0);
        ms->IsEndIO = true;
    }

    // Auto-select as you navigate a list
    if (g.NavJustMovedToId == id)
    {
        if ((flags & UxImGuiMultiSelectFlags_NoAutoSelect) == 0)
        {
            if (is_ctrl && is_shift)
                pressed = true;
            else if (!is_ctrl)
                selected = pressed = true;
        }
        else
        {
            // With NoAutoSelect, using Shift+keyboard performs a write/copy
            if (is_shift)
                pressed = true;
            else if (!is_ctrl)
                apply_to_range_src = true; // Since if (pressed) {} main block is not running we update this
        }
    }

    if (apply_to_range_src)
    {
        storage->RangeSrcItem = item_data;
        storage->RangeSelected = selected; // Will be updated at the end of this function anyway.
    }

    // Box-select toggle handling
    if (ms->BoxSelectId != 0)
        if (UxImGuiBoxSelectState* bs = GetBoxSelectState(ms->BoxSelectId))
        {
            const bool rect_overlap_curr = bs->BoxSelectRectCurr.Overlaps(g.LastItemData.Rect);
            const bool rect_overlap_prev = bs->BoxSelectRectPrev.Overlaps(g.LastItemData.Rect);
            if ((rect_overlap_curr && !rect_overlap_prev && !selected) || (rect_overlap_prev && !rect_overlap_curr))
            {
                if (storage->LastSelectionSize <= 0 && bs->IsStartedSetNavIdOnce)
                {
                    pressed = true; // First item act as a pressed: code below will emit selection request and set NavId (whatever we emit here will be overridden anyway)
                    bs->IsStartedSetNavIdOnce = false;
                }
                else
                {
                    selected = !selected;
                    MultiSelectAddSetRange(ms, selected, +1, item_data, item_data);
                }
                storage->LastSelectionSize = UxImMax(storage->LastSelectionSize + 1, 1);
            }
        }

    // Right-click handling.
    // FIXME-MULTISELECT: Currently filtered out by UxImGuiMultiSelectFlags_NoAutoSelect but maybe should be moved to Selectable(). See https://github.com/ocornut/imgui/pull/5816
    if (hovered && IsMouseClicked(1) && (flags & UxImGuiMultiSelectFlags_NoAutoSelect) == 0)
    {
        if (g.ActiveId != 0 && g.ActiveId != id)
            ClearActiveID();
        SetFocusID(id, window);
        if (!pressed && !selected)
        {
            pressed = true;
            is_ctrl = is_shift = false;
        }
    }

    // Unlike Space, Enter doesn't alter selection (but can still return a press) unless current item is not selected.
    // The later, "unless current item is not select", may become optional? It seems like a better default if Enter doesn't necessarily open something
    // (unlike e.g. Windows explorer). For use case where Enter always open something, we might decide to make this optional?
    const bool enter_pressed = pressed && (g.NavActivateId == id) && (g.NavActivateFlags & UxImGuiActivateFlags_PreferInput);

    // Alter selection
    if (pressed && (!enter_pressed || !selected))
    {
        // Box-select
        UxImGuiInputSource input_source = (g.NavJustMovedToId == id || g.NavActivateId == id) ? g.NavInputSource : UxImGuiInputSource_Mouse;
        if (flags & (UxImGuiMultiSelectFlags_BoxSelect1d | UxImGuiMultiSelectFlags_BoxSelect2d))
            if (selected == false && !g.BoxSelectState.IsActive && !g.BoxSelectState.IsStarting && input_source == UxImGuiInputSource_Mouse && g.IO.MouseClickedCount[0] == 1)
                BoxSelectPreStartDrag(ms->BoxSelectId, item_data);

        //----------------------------------------------------------------------------------------
        // ACTION                      | Begin  | Pressed/Activated  | End
        //----------------------------------------------------------------------------------------
        // Keys Navigated:             | Clear  | Src=item, Sel=1               SetRange 1
        // Keys Navigated: Ctrl        | n/a    | n/a
        // Keys Navigated:      Shift  | n/a    | Dst=item, Sel=1,   => Clear + SetRange 1
        // Keys Navigated: Ctrl+Shift  | n/a    | Dst=item, Sel=Src  => Clear + SetRange Src-Dst
        // Keys Activated:             | n/a    | Src=item, Sel=1    => Clear + SetRange 1
        // Keys Activated: Ctrl        | n/a    | Src=item, Sel=!Sel =>         SetSange 1
        // Keys Activated:      Shift  | n/a    | Dst=item, Sel=1    => Clear + SetSange 1
        //----------------------------------------------------------------------------------------
        // Mouse Pressed:              | n/a    | Src=item, Sel=1,   => Clear + SetRange 1
        // Mouse Pressed:  Ctrl        | n/a    | Src=item, Sel=!Sel =>         SetRange 1
        // Mouse Pressed:       Shift  | n/a    | Dst=item, Sel=1,   => Clear + SetRange 1
        // Mouse Pressed:  Ctrl+Shift  | n/a    | Dst=item, Sel=!Sel =>         SetRange Src-Dst
        //----------------------------------------------------------------------------------------

        if ((flags & UxImGuiMultiSelectFlags_NoAutoClear) == 0)
        {
            bool request_clear = false;
            if (is_singleselect)
                request_clear = true;
            else if ((input_source == UxImGuiInputSource_Mouse || g.NavActivateId == id) && !is_ctrl)
                request_clear = (flags & UxImGuiMultiSelectFlags_NoAutoClearOnReselect) ? !selected : true;
            else if ((input_source == UxImGuiInputSource_Keyboard || input_source == UxImGuiInputSource_Gamepad) && is_shift && !is_ctrl)
                request_clear = true; // With is_shift==false the RequestClear was done in BeginIO, not necessary to do again.
            if (request_clear)
                MultiSelectAddSetAll(ms, false);
        }

        int range_direction;
        bool range_selected;
        if (is_shift && !is_singleselect)
        {
            //IM_ASSERT(storage->HasRangeSrc && storage->HasRangeValue);
            if (storage->RangeSrcItem == UxImGuiSelectionUserData_Invalid)
                storage->RangeSrcItem = item_data;
            if ((flags & UxImGuiMultiSelectFlags_NoAutoSelect) == 0)
            {
                // Shift+Arrow always select
                // Ctrl+Shift+Arrow copy source selection state (already stored by BeginMultiSelect() in storage->RangeSelected)
                range_selected = (is_ctrl && storage->RangeSelected != -1) ? (storage->RangeSelected != 0) : true;
            }
            else
            {
                // Shift+Arrow copy source selection state
                // Shift+Click always copy from target selection state
                if (ms->IsKeyboardSetRange)
                    range_selected = (storage->RangeSelected != -1) ? (storage->RangeSelected != 0) : true;
                else
                    range_selected = !selected;
            }
            range_direction = ms->RangeSrcPassedBy ? +1 : -1;
        }
        else
        {
            // Ctrl inverts selection, otherwise always select
            if ((flags & UxImGuiMultiSelectFlags_NoAutoSelect) == 0)
                selected = is_ctrl ? !selected : true;
            else
                selected = !selected;
            storage->RangeSrcItem = item_data;
            range_selected = selected;
            range_direction = +1;
        }
        MultiSelectAddSetRange(ms, range_selected, range_direction, storage->RangeSrcItem, item_data);
    }

    // Update/store the selection state of the Source item (used by CTRL+SHIFT, when Source is unselected we perform a range unselect)
    if (storage->RangeSrcItem == item_data)
        storage->RangeSelected = selected ? 1 : 0;

    // Update/store the selection state of focused item
    if (g.NavId == id)
    {
        storage->NavIdItem = item_data;
        storage->NavIdSelected = selected ? 1 : 0;
    }
    if (storage->NavIdItem == item_data)
        ms->NavIdPassedBy = true;
    ms->LastSubmittedItem = item_data;

    *p_selected = selected;
    *p_pressed = pressed;
}

void UxImGui::MultiSelectAddSetAll(UxImGuiMultiSelectTempData* ms, bool selected)
{
    UxImGuiSelectionRequest req = { UxImGuiSelectionRequestType_SetAll, selected, 0, UxImGuiSelectionUserData_Invalid, UxImGuiSelectionUserData_Invalid };
    ms->IO.Requests.resize(0);      // Can always clear previous requests
    ms->IO.Requests.push_back(req); // Add new request
}

void UxImGui::MultiSelectAddSetRange(UxImGuiMultiSelectTempData* ms, bool selected, int range_dir, UxImGuiSelectionUserData first_item, UxImGuiSelectionUserData last_item)
{
    // Merge contiguous spans into same request (unless NoRangeSelect is set which guarantees single-item ranges)
    if (ms->IO.Requests.Size > 0 && first_item == last_item && (ms->Flags & UxImGuiMultiSelectFlags_NoRangeSelect) == 0)
    {
        UxImGuiSelectionRequest* prev = &ms->IO.Requests.Data[ms->IO.Requests.Size - 1];
        if (prev->Type == UxImGuiSelectionRequestType_SetRange && prev->RangeLastItem == ms->LastSubmittedItem && prev->Selected == selected)
        {
            prev->RangeLastItem = last_item;
            return;
        }
    }

    UxImGuiSelectionRequest req = { UxImGuiSelectionRequestType_SetRange, selected, (UxImS8)range_dir, (range_dir > 0) ? first_item : last_item, (range_dir > 0) ? last_item : first_item };
    ms->IO.Requests.push_back(req); // Add new request
}

void UxImGui::DebugNodeMultiSelectState(UxImGuiMultiSelectState* storage)
{
#ifndef IMGUI_DISABLE_DEBUG_TOOLS
    const bool is_active = (storage->LastFrameActive >= GetFrameCount() - 2); // Note that fully clipped early out scrolling tables will appear as inactive here.
    if (!is_active) { PushStyleColor(UxImGuiCol_Text, GetStyleColorVec4(UxImGuiCol_TextDisabled)); }
    bool open = TreeNode((void*)(intptr_t)storage->ID, "MultiSelect 0x%08X in '%s'%s", storage->ID, storage->Window ? storage->Window->Name : "N/A", is_active ? "" : " *Inactive*");
    if (!is_active) { PopStyleColor(); }
    if (!open)
        return;
    Text("RangeSrcItem = %" IM_PRId64 " (0x%" IM_PRIX64 "), RangeSelected = %d", storage->RangeSrcItem, storage->RangeSrcItem, storage->RangeSelected);
    Text("NavIdItem = %" IM_PRId64 " (0x%" IM_PRIX64 "), NavIdSelected = %d", storage->NavIdItem, storage->NavIdItem, storage->NavIdSelected);
    Text("LastSelectionSize = %d", storage->LastSelectionSize); // Provided by user
    TreePop();
#else
    IM_UNUSED(storage);
#endif
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Multi-Select helpers
//-------------------------------------------------------------------------
// - UxImGuiSelectionBasicStorage
// - UxImGuiSelectionExternalStorage
//-------------------------------------------------------------------------

UxImGuiSelectionBasicStorage::UxImGuiSelectionBasicStorage()
{
    Size = 0;
    PreserveOrder = false;
    UserData = NULL;
    AdapterIndexToStorageId = [](UxImGuiSelectionBasicStorage*, int idx) { return (UxImGuiID)idx; };
    _SelectionOrder = 1; // Always >0
}

void UxImGuiSelectionBasicStorage::Clear()
{
    Size = 0;
    _SelectionOrder = 1; // Always >0
    _Storage.Data.resize(0);
}

void UxImGuiSelectionBasicStorage::Swap(UxImGuiSelectionBasicStorage& r)
{
    UxImSwap(Size, r.Size);
    UxImSwap(_SelectionOrder, r._SelectionOrder);
    _Storage.Data.swap(r._Storage.Data);
}

bool UxImGuiSelectionBasicStorage::Contains(UxImGuiID id) const
{
    return _Storage.GetInt(id, 0) != 0;
}

static int IMGUI_CDECL PairComparerByValueInt(const void* lhs, const void* rhs)
{
    int lhs_v = ((const UxImGuiStoragePair*)lhs)->val_i;
    int rhs_v = ((const UxImGuiStoragePair*)rhs)->val_i;
    return (lhs_v > rhs_v ? +1 : lhs_v < rhs_v ? -1 : 0);
}

// GetNextSelectedItem() is an abstraction allowing us to change our underlying actual storage system without impacting user.
// (e.g. store unselected vs compact down, compact down on demand, use raw UxImVector<UxImGuiID> instead of UxImGuiStorage...)
bool UxImGuiSelectionBasicStorage::GetNextSelectedItem(void** opaque_it, UxImGuiID* out_id)
{
    UxImGuiStoragePair* it = (UxImGuiStoragePair*)*opaque_it;
    UxImGuiStoragePair* it_end = _Storage.Data.Data + _Storage.Data.Size;
    if (PreserveOrder && it == NULL && it_end != NULL)
        UxImQsort(_Storage.Data.Data, (size_t)_Storage.Data.Size, sizeof(UxImGuiStoragePair), PairComparerByValueInt); // ~UxImGuiStorage::BuildSortByValueInt()
    if (it == NULL)
        it = _Storage.Data.Data;
    IM_ASSERT(it >= _Storage.Data.Data && it <= it_end);
    if (it != it_end)
        while (it->val_i == 0 && it < it_end)
            it++;
    const bool has_more = (it != it_end);
    *opaque_it = has_more ? (void**)(it + 1) : (void**)(it);
    *out_id = has_more ? it->key : 0;
    if (PreserveOrder && !has_more)
        _Storage.BuildSortByKey();
    return has_more;
}

void UxImGuiSelectionBasicStorage::SetItemSelected(UxImGuiID id, bool selected)
{
    int* p_int = _Storage.GetIntRef(id, 0);
    if (selected && *p_int == 0) { *p_int = _SelectionOrder++; Size++; }
    else if (!selected && *p_int != 0) { *p_int = 0; Size--; }
}

// Optimized for batch edits (with same value of 'selected')
static void UxImGuiSelectionBasicStorage_BatchSetItemSelected(UxImGuiSelectionBasicStorage* selection, UxImGuiID id, bool selected, int size_before_amends, int selection_order)
{
    UxImGuiStorage* storage = &selection->_Storage;
    UxImGuiStoragePair* it = UxImLowerBound(storage->Data.Data, storage->Data.Data + size_before_amends, id);
    const bool is_contained = (it != storage->Data.Data + size_before_amends) && (it->key == id);
    if (selected == (is_contained && it->val_i != 0))
        return;
    if (selected && !is_contained)
        storage->Data.push_back(UxImGuiStoragePair(id, selection_order)); // Push unsorted at end of vector, will be sorted in SelectionMultiAmendsFinish()
    else if (is_contained)
        it->val_i = selected ? selection_order : 0; // Modify in-place.
    selection->Size += selected ? +1 : -1;
}

static void UxImGuiSelectionBasicStorage_BatchFinish(UxImGuiSelectionBasicStorage* selection, bool selected, int size_before_amends)
{
    UxImGuiStorage* storage = &selection->_Storage;
    if (selected && selection->Size != size_before_amends)
        storage->BuildSortByKey(); // When done selecting: sort everything
}

// Apply requests coming from BeginMultiSelect() and EndMultiSelect().
// - Enable 'Demo->Tools->Debug Log->Selection' to see selection requests as they happen.
// - Honoring SetRange requests requires that you can iterate/interpolate between RangeFirstItem and RangeLastItem.
//   - In this demo we often submit indices to SetNextItemSelectionUserData() + store the same indices in persistent selection.
//   - Your code may do differently. If you store pointers or objects ID in UxImGuiSelectionUserData you may need to perform
//     a lookup in order to have some way to iterate/interpolate between two items.
// - A full-featured application is likely to allow search/filtering which is likely to lead to using indices
//   and constructing a view index <> object id/ptr data structure anyway.
// WHEN YOUR APPLICATION SETTLES ON A CHOICE, YOU WILL PROBABLY PREFER TO GET RID OF THIS UNNECESSARY 'UxImGuiSelectionBasicStorage' INDIRECTION LOGIC.
// Notice that with the simplest adapter (using indices everywhere), all functions return their parameters.
// The most simple implementation (using indices everywhere) would look like:
//   for (UxImGuiSelectionRequest& req : ms_io->Requests)
//   {
//      if (req.Type == UxImGuiSelectionRequestType_SetAll)    { Clear(); if (req.Selected) { for (int n = 0; n < items_count; n++) { SetItemSelected(n, true); } }
//      if (req.Type == UxImGuiSelectionRequestType_SetRange)  { for (int n = (int)ms_io->RangeFirstItem; n <= (int)ms_io->RangeLastItem; n++) { SetItemSelected(n, ms_io->Selected); } }
//   }
void UxImGuiSelectionBasicStorage::ApplyRequests(UxImGuiMultiSelectIO* ms_io)
{
    // For convenience we obtain ItemsCount as passed to BeginMultiSelect(), which is optional.
    // It makes sense when using UxImGuiSelectionBasicStorage to simply pass your items count to BeginMultiSelect().
    // Other scheme may handle SetAll differently.
    IM_ASSERT(ms_io->ItemsCount != -1 && "Missing value for items_count in BeginMultiSelect() call!");
    IM_ASSERT(AdapterIndexToStorageId != NULL);

    // This is optimized/specialized to cope with very large selections (e.g. 100k+ items)
    // - A simpler version could call SetItemSelected() directly instead of UxImGuiSelectionBasicStorage_BatchSetItemSelected() + UxImGuiSelectionBasicStorage_BatchFinish().
    // - Optimized select can append unsorted, then sort in a second pass. Optimized unselect can clear in-place then compact in a second pass.
    // - A more optimal version wouldn't even use UxImGuiStorage but directly a UxImVector<UxImGuiID> to reduce bandwidth, but this is a reasonable trade off to reuse code.
    // - There are many ways this could be better optimized. The worse case scenario being: using BoxSelect2d in a grid, box-select scrolling down while wiggling
    //   left and right: it affects coarse clipping + can emit multiple SetRange with 1 item each.)
    // FIXME-OPT: For each block of consecutive SetRange request:
    // - add all requests to a sorted list, store ID, selected, offset in UxImGuiStorage.
    // - rewrite sorted storage a single time.
    for (UxImGuiSelectionRequest& req : ms_io->Requests)
    {
        if (req.Type == UxImGuiSelectionRequestType_SetAll)
        {
            Clear();
            if (req.Selected)
            {
                _Storage.Data.reserve(ms_io->ItemsCount);
                const int size_before_amends = _Storage.Data.Size;
                for (int idx = 0; idx < ms_io->ItemsCount; idx++, _SelectionOrder++)
                    UxImGuiSelectionBasicStorage_BatchSetItemSelected(this, GetStorageIdFromIndex(idx), req.Selected, size_before_amends, _SelectionOrder);
                UxImGuiSelectionBasicStorage_BatchFinish(this, req.Selected, size_before_amends);
            }
        }
        else if (req.Type == UxImGuiSelectionRequestType_SetRange)
        {
            const int selection_changes = (int)req.RangeLastItem - (int)req.RangeFirstItem + 1;
            //UxImGuiContext& g = *GUxImGui; IMGUI_DEBUG_LOG_SELECTION("Req %d/%d: set %d to %d\n", ms_io->Requests.index_from_ptr(&req), ms_io->Requests.Size, selection_changes, req.Selected);
            if (selection_changes == 1 || (selection_changes < Size / 100))
            {
                // Multiple sorted insertion + copy likely to be faster.
                // Technically we could do a single copy with a little more work (sort sequential SetRange requests)
                for (int idx = (int)req.RangeFirstItem; idx <= (int)req.RangeLastItem; idx++)
                    SetItemSelected(GetStorageIdFromIndex(idx), req.Selected);
            }
            else
            {
                // Append insertion + single sort likely be faster.
                // Use req.RangeDirection to set order field so that shift+clicking from 1 to 5 is different than shift+clicking from 5 to 1
                const int size_before_amends = _Storage.Data.Size;
                int selection_order = _SelectionOrder + ((req.RangeDirection < 0) ? selection_changes - 1 : 0);
                for (int idx = (int)req.RangeFirstItem; idx <= (int)req.RangeLastItem; idx++, selection_order += req.RangeDirection)
                    UxImGuiSelectionBasicStorage_BatchSetItemSelected(this, GetStorageIdFromIndex(idx), req.Selected, size_before_amends, selection_order);
                if (req.Selected)
                    _SelectionOrder += selection_changes;
                UxImGuiSelectionBasicStorage_BatchFinish(this, req.Selected, size_before_amends);
            }
        }
    }
}

//-------------------------------------------------------------------------

UxImGuiSelectionExternalStorage::UxImGuiSelectionExternalStorage()
{
    UserData = NULL;
    AdapterSetItemSelected = NULL;
}

// Apply requests coming from BeginMultiSelect() and EndMultiSelect().
// We also pull 'ms_io->ItemsCount' as passed for BeginMultiSelect() for consistency with UxImGuiSelectionBasicStorage
// This makes no assumption about underlying storage.
void UxImGuiSelectionExternalStorage::ApplyRequests(UxImGuiMultiSelectIO* ms_io)
{
    IM_ASSERT(AdapterSetItemSelected);
    for (UxImGuiSelectionRequest& req : ms_io->Requests)
    {
        if (req.Type == UxImGuiSelectionRequestType_SetAll)
            for (int idx = 0; idx < ms_io->ItemsCount; idx++)
                AdapterSetItemSelected(this, idx, req.Selected);
        if (req.Type == UxImGuiSelectionRequestType_SetRange)
            for (int idx = (int)req.RangeFirstItem; idx <= (int)req.RangeLastItem; idx++)
                AdapterSetItemSelected(this, idx, req.Selected);
    }
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: ListBox
//-------------------------------------------------------------------------
// - BeginListBox()
// - EndListBox()
// - ListBox()
//-------------------------------------------------------------------------

// This is essentially a thin wrapper to using BeginChild/EndChild with the UxImGuiChildFlags_FrameStyle flag for stylistic changes + displaying a label.
// This handle some subtleties with capturing info from the label.
// If you don't need a label you can pretty much directly use UxImGui::BeginChild() with UxImGuiChildFlags_FrameStyle.
// Tip: To have a list filling the entire window width, use size.x = -FLT_MIN and pass an non-visible label e.g. "##empty"
// Tip: If your vertical size is calculated from an item count (e.g. 10 * item_height) consider adding a fractional part to facilitate seeing scrolling boundaries (e.g. 10.5f * item_height).
bool UxImGui::BeginListBox(const char* label, const UxImVec2& size_arg)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = GetID(label);
    const UxImVec2 label_size = CalcTextSize(label, NULL, true);

    // Size default to hold ~7.25 items.
    // Fractional number of items helps seeing that we can scroll down/up without looking at scrollbar.
    UxImVec2 size = UxImTrunc(CalcItemSize(size_arg, CalcItemWidth(), GetTextLineHeightWithSpacing() * 7.25f + style.FramePadding.y * 2.0f));
    UxImVec2 frame_size = UxImVec2(size.x, UxImMax(size.y, label_size.y));
    UxImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    UxImRect bb(frame_bb.Min, frame_bb.Max + UxImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    g.NextItemData.ClearFlags();

    if (!IsRectVisible(bb.Min, bb.Max))
    {
        ItemSize(bb.GetSize(), style.FramePadding.y);
        ItemAdd(bb, 0, &frame_bb);
        g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
        return false;
    }

    // FIXME-OPT: We could omit the BeginGroup() if label_size.x == 0.0f but would need to omit the EndGroup() as well.
    BeginGroup();
    if (label_size.x > 0.0f)
    {
        UxImVec2 label_pos = UxImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y);
        RenderText(label_pos, label);
        window->DC.CursorMaxPos = UxImMax(window->DC.CursorMaxPos, label_pos + label_size);
        AlignTextToFramePadding();
    }

    BeginChild(id, frame_bb.GetSize(), UxImGuiChildFlags_FrameStyle);
    return true;
}

void UxImGui::EndListBox()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT((window->Flags & UxImGuiWindowFlags_ChildWindow) && "Mismatched BeginListBox/EndListBox calls. Did you test the return value of BeginListBox?");
    IM_UNUSED(window);

    EndChild();
    EndGroup(); // This is only required to be able to do IsItemXXX query on the whole ListBox including label
}

bool UxImGui::ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_items)
{
    const bool value_changed = ListBox(label, current_item, Items_ArrayGetter, (void*)items, items_count, height_items);
    return value_changed;
}

// This is merely a helper around BeginListBox(), EndListBox().
// Considering using those directly to submit custom data or store selection differently.
bool UxImGui::ListBox(const char* label, int* current_item, const char* (*getter)(void* user_data, int idx), void* user_data, int items_count, int height_in_items)
{
    UxImGuiContext& g = *GUxImGui;

    // Calculate size from "height_in_items"
    if (height_in_items < 0)
        height_in_items = UxImMin(items_count, 7);
    float height_in_items_f = height_in_items + 0.25f;
    UxImVec2 size(0.0f, UxImTrunc(GetTextLineHeightWithSpacing() * height_in_items_f + g.Style.FramePadding.y * 2.0f));

    if (!BeginListBox(label, size))
        return false;

    // Assume all items have even height (= 1 line of text). If you need items of different height,
    // you can create a custom version of ListBox() in your code without using the clipper.
    bool value_changed = false;
    UxImGuiListClipper clipper;
    clipper.Begin(items_count, GetTextLineHeightWithSpacing()); // We know exactly our line height here so we pass it as a minor optimization, but generally you don't need to.
    clipper.IncludeItemByIndex(*current_item);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const char* item_text = getter(user_data, i);
            if (item_text == NULL)
                item_text = "*Unknown item*";

            PushID(i);
            const bool item_selected = (i == *current_item);
            if (Selectable(item_text, item_selected))
            {
                *current_item = i;
                value_changed = true;
            }
            if (item_selected)
                SetItemDefaultFocus();
            PopID();
        }
    EndListBox();

    if (value_changed)
        MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: PlotLines, PlotHistogram
//-------------------------------------------------------------------------
// - PlotEx() [Internal]
// - PlotLines()
// - PlotHistogram()
//-------------------------------------------------------------------------
// Plot/Graph widgets are not very good.
// Consider writing your own, or using a third-party one, see:
// - UxImPlot https://github.com/epezent/implot
// - others https://github.com/ocornut/imgui/wiki/Useful-Extensions
//-------------------------------------------------------------------------

int UxImGui::PlotEx(UxImGuiPlotType plot_type, const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, const UxImVec2& size_arg)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return -1;

    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);

    const UxImVec2 label_size = CalcTextSize(label, NULL, true);
    const UxImVec2 frame_size = CalcItemSize(size_arg, CalcItemWidth(), label_size.y + style.FramePadding.y * 2.0f);

    const UxImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const UxImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const UxImRect total_bb(frame_bb.Min, frame_bb.Max + UxImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb, UxImGuiItemFlags_NoNav))
        return -1;
    bool hovered;
    ButtonBehavior(frame_bb, id, &hovered, NULL);

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX)
    {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int i = 0; i < values_count; i++)
        {
            const float v = values_getter(data, i);
            if (v != v) // Ignore NaN values
                continue;
            v_min = UxImMin(v_min, v);
            v_max = UxImMax(v_max, v);
        }
        if (scale_min == FLT_MAX)
            scale_min = v_min;
        if (scale_max == FLT_MAX)
            scale_max = v_max;
    }

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(UxImGuiCol_FrameBg), true, style.FrameRounding);

    const int values_count_min = (plot_type == UxImGuiPlotType_Lines) ? 2 : 1;
    int idx_hovered = -1;
    if (values_count >= values_count_min)
    {
        int res_w = UxImMin((int)frame_size.x, values_count) + ((plot_type == UxImGuiPlotType_Lines) ? -1 : 0);
        int item_count = values_count + ((plot_type == UxImGuiPlotType_Lines) ? -1 : 0);

        // Tooltip on hover
        if (hovered && inner_bb.Contains(g.IO.MousePos))
        {
            const float t = UxImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
            const int v_idx = (int)(t * item_count);
            IM_ASSERT(v_idx >= 0 && v_idx < values_count);

            const float v0 = values_getter(data, (v_idx + values_offset) % values_count);
            const float v1 = values_getter(data, (v_idx + 1 + values_offset) % values_count);
            if (plot_type == UxImGuiPlotType_Lines)
                SetTooltip("%d: %8.4g\n%d: %8.4g", v_idx, v0, v_idx + 1, v1);
            else if (plot_type == UxImGuiPlotType_Histogram)
                SetTooltip("%d: %8.4g", v_idx, v0);
            idx_hovered = v_idx;
        }

        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

        float v0 = values_getter(data, (0 + values_offset) % values_count);
        float t0 = 0.0f;
        UxImVec2 tp0 = UxImVec2( t0, 1.0f - UxImSaturate((v0 - scale_min) * inv_scale) );                       // Point in the normalized space of our target rectangle
        float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (1 + scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);   // Where does the zero line stands

        const UxImU32 col_base = GetColorU32((plot_type == UxImGuiPlotType_Lines) ? UxImGuiCol_PlotLines : UxImGuiCol_PlotHistogram);
        const UxImU32 col_hovered = GetColorU32((plot_type == UxImGuiPlotType_Lines) ? UxImGuiCol_PlotLinesHovered : UxImGuiCol_PlotHistogramHovered);

        for (int n = 0; n < res_w; n++)
        {
            const float t1 = t0 + t_step;
            const int v1_idx = (int)(t0 * item_count + 0.5f);
            IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
            const float v1 = values_getter(data, (v1_idx + values_offset + 1) % values_count);
            const UxImVec2 tp1 = UxImVec2( t1, 1.0f - UxImSaturate((v1 - scale_min) * inv_scale) );

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            UxImVec2 pos0 = UxImLerp(inner_bb.Min, inner_bb.Max, tp0);
            UxImVec2 pos1 = UxImLerp(inner_bb.Min, inner_bb.Max, (plot_type == UxImGuiPlotType_Lines) ? tp1 : UxImVec2(tp1.x, histogram_zero_line_t));
            if (plot_type == UxImGuiPlotType_Lines)
            {
                window->DrawList->AddLine(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }
            else if (plot_type == UxImGuiPlotType_Histogram)
            {
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                window->DrawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }

            t0 = t1;
            tp0 = tp1;
        }
    }

    // Text overlay
    if (overlay_text)
        RenderTextClipped(UxImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, UxImVec2(0.5f, 0.0f));

    if (label_size.x > 0.0f)
        RenderText(UxImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    // Return hovered index or -1 if none are hovered.
    // This is currently not exposed in the public API because we need a larger redesign of the whole thing, but in the short-term we are making it available in PlotEx().
    return idx_hovered;
}

struct UxImGuiPlotArrayGetterData
{
    const float* Values;
    int Stride;

    UxImGuiPlotArrayGetterData(const float* values, int stride) { Values = values; Stride = stride; }
};

static float Plot_ArrayGetter(void* data, int idx)
{
    UxImGuiPlotArrayGetterData* plot_data = (UxImGuiPlotArrayGetterData*)data;
    const float v = *(const float*)(const void*)((const unsigned char*)plot_data->Values + (size_t)idx * plot_data->Stride);
    return v;
}

void UxImGui::PlotLines(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, UxImVec2 graph_size, int stride)
{
    UxImGuiPlotArrayGetterData data(values, stride);
    PlotEx(UxImGuiPlotType_Lines, label, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

void UxImGui::PlotLines(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, UxImVec2 graph_size)
{
    PlotEx(UxImGuiPlotType_Lines, label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

void UxImGui::PlotHistogram(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, UxImVec2 graph_size, int stride)
{
    UxImGuiPlotArrayGetterData data(values, stride);
    PlotEx(UxImGuiPlotType_Histogram, label, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

void UxImGui::PlotHistogram(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, UxImVec2 graph_size)
{
    PlotEx(UxImGuiPlotType_Histogram, label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Value helpers
// Those is not very useful, legacy API.
//-------------------------------------------------------------------------
// - Value()
//-------------------------------------------------------------------------

void UxImGui::Value(const char* prefix, bool b)
{
    Text("%s: %s", prefix, (b ? "true" : "false"));
}

void UxImGui::Value(const char* prefix, int v)
{
    Text("%s: %d", prefix, v);
}

void UxImGui::Value(const char* prefix, unsigned int v)
{
    Text("%s: %d", prefix, v);
}

void UxImGui::Value(const char* prefix, float v, const char* float_format)
{
    if (float_format)
    {
        char fmt[64];
        UxImFormatString(fmt, IM_ARRAYSIZE(fmt), "%%s: %s", float_format);
        Text(fmt, prefix, v);
    }
    else
    {
        Text("%s: %.3f", prefix, v);
    }
}

//-------------------------------------------------------------------------
// [SECTION] MenuItem, BeginMenu, EndMenu, etc.
//-------------------------------------------------------------------------
// - UxImGuiMenuColumns [Internal]
// - BeginMenuBar()
// - EndMenuBar()
// - BeginMainMenuBar()
// - EndMainMenuBar()
// - BeginMenu()
// - EndMenu()
// - MenuItemEx() [Internal]
// - MenuItem()
//-------------------------------------------------------------------------

// Helpers for internal use
void UxImGuiMenuColumns::Update(float spacing, bool window_reappearing)
{
    if (window_reappearing)
        memset(Widths, 0, sizeof(Widths));
    Spacing = (UxImU16)spacing;
    CalcNextTotalWidth(true);
    memset(Widths, 0, sizeof(Widths));
    TotalWidth = NextTotalWidth;
    NextTotalWidth = 0;
}

void UxImGuiMenuColumns::CalcNextTotalWidth(bool update_offsets)
{
    UxImU16 offset = 0;
    bool want_spacing = false;
    for (int i = 0; i < IM_ARRAYSIZE(Widths); i++)
    {
        UxImU16 width = Widths[i];
        if (want_spacing && width > 0)
            offset += Spacing;
        want_spacing |= (width > 0);
        if (update_offsets)
        {
            if (i == 1) { OffsetLabel = offset; }
            if (i == 2) { OffsetShortcut = offset; }
            if (i == 3) { OffsetMark = offset; }
        }
        offset += width;
    }
    NextTotalWidth = offset;
}

float UxImGuiMenuColumns::DeclColumns(float w_icon, float w_label, float w_shortcut, float w_mark)
{
    Widths[0] = UxImMax(Widths[0], (UxImU16)w_icon);
    Widths[1] = UxImMax(Widths[1], (UxImU16)w_label);
    Widths[2] = UxImMax(Widths[2], (UxImU16)w_shortcut);
    Widths[3] = UxImMax(Widths[3], (UxImU16)w_mark);
    CalcNextTotalWidth(false);
    return (float)UxImMax(TotalWidth, NextTotalWidth);
}

// FIXME: Provided a rectangle perhaps e.g. a BeginMenuBarEx() could be used anywhere..
// Currently the main responsibility of this function being to setup clip-rect + horizontal layout + menu navigation layer.
// Ideally we also want this to be responsible for claiming space out of the main window scrolling rectangle, in which case UxImGuiWindowFlags_MenuBar will become unnecessary.
// Then later the same system could be used for multiple menu-bars, scrollbars, side-bars.
bool UxImGui::BeginMenuBar()
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    if (!(window->Flags & UxImGuiWindowFlags_MenuBar))
        return false;

    IM_ASSERT(!window->DC.MenuBarAppending);
    BeginGroup(); // Backup position on layer 0 // FIXME: Misleading to use a group for that backup/restore
    PushID("##MenuBar");

    // We don't clip with current window clipping rectangle as it is already set to the area below. However we clip with window full rect.
    // We remove 1 worth of rounding to Max.x to that text in long menus and small windows don't tend to display over the lower-right rounded area, which looks particularly glitchy.
    const float border_top = UxImMax(IM_ROUND(window->WindowBorderSize * 0.5f - window->TitleBarHeight), 0.0f);
    const float border_half = IM_ROUND(window->WindowBorderSize * 0.5f);
    UxImRect bar_rect = window->MenuBarRect();
    UxImRect clip_rect(UxImFloor(bar_rect.Min.x + border_half), UxImFloor(bar_rect.Min.y + border_top), UxImFloor(UxImMax(bar_rect.Min.x, bar_rect.Max.x - UxImMax(window->WindowRounding, border_half))), UxImFloor(bar_rect.Max.y));
    clip_rect.ClipWith(window->OuterRectClipped);
    PushClipRect(clip_rect.Min, clip_rect.Max, false);

    // We overwrite CursorMaxPos because BeginGroup sets it to CursorPos (essentially the .EmitItem hack in EndMenuBar() would need something analogous here, maybe a BeginGroupEx() with flags).
    window->DC.CursorPos = window->DC.CursorMaxPos = UxImVec2(bar_rect.Min.x + window->DC.MenuBarOffset.x, bar_rect.Min.y + window->DC.MenuBarOffset.y);
    window->DC.LayoutType = UxImGuiLayoutType_Horizontal;
    window->DC.IsSameLine = false;
    window->DC.NavLayerCurrent = UxImGuiNavLayer_Menu;
    window->DC.MenuBarAppending = true;
    AlignTextToFramePadding();
    return true;
}

void UxImGui::EndMenuBar()
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    UxImGuiContext& g = *GUxImGui;

    IM_MSVC_WARNING_SUPPRESS(6011); // Static Analysis false positive "warning C6011: Dereferencing NULL pointer 'window'"
    IM_ASSERT(window->Flags & UxImGuiWindowFlags_MenuBar);
    IM_ASSERT(window->DC.MenuBarAppending);

    // Nav: When a move request within one of our child menu failed, capture the request to navigate among our siblings.
    if (NavMoveRequestButNoResultYet() && (g.NavMoveDir == UxImGuiDir_Left || g.NavMoveDir == UxImGuiDir_Right) && (g.NavWindow->Flags & UxImGuiWindowFlags_ChildMenu))
    {
        // Try to find out if the request is for one of our child menu
        UxImGuiWindow* nav_earliest_child = g.NavWindow;
        while (nav_earliest_child->ParentWindow && (nav_earliest_child->ParentWindow->Flags & UxImGuiWindowFlags_ChildMenu))
            nav_earliest_child = nav_earliest_child->ParentWindow;
        if (nav_earliest_child->ParentWindow == window && nav_earliest_child->DC.ParentLayoutType == UxImGuiLayoutType_Horizontal && (g.NavMoveFlags & UxImGuiNavMoveFlags_Forwarded) == 0)
        {
            // To do so we claim focus back, restore NavId and then process the movement request for yet another frame.
            // This involve a one-frame delay which isn't very problematic in this situation. We could remove it by scoring in advance for multiple window (probably not worth bothering)
            const UxImGuiNavLayer layer = UxImGuiNavLayer_Menu;
            IM_ASSERT(window->DC.NavLayersActiveMaskNext & (1 << layer)); // Sanity check (FIXME: Seems unnecessary)
            FocusWindow(window);
            SetNavID(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);
            // FIXME-NAV: How to deal with this when not using g.IO.ConfigNavCursorVisibleAuto?
            if (g.NavCursorVisible)
            {
                g.NavCursorVisible = false; // Hide nav cursor for the current frame so we don't see the intermediary selection. Will be set again
                g.NavCursorHideFrames = 2;
            }
            g.NavHighlightItemUnderNav = g.NavMousePosDirty = true;
            NavMoveRequestForward(g.NavMoveDir, g.NavMoveClipDir, g.NavMoveFlags, g.NavMoveScrollFlags); // Repeat
        }
    }

    PopClipRect();
    PopID();
    window->DC.MenuBarOffset.x = window->DC.CursorPos.x - window->Pos.x; // Save horizontal position so next append can reuse it. This is kinda equivalent to a per-layer CursorPos.

    // FIXME: Extremely confusing, cleanup by (a) working on WorkRect stack system (b) not using a Group confusingly here.
    UxImGuiGroupData& group_data = g.GroupStack.back();
    group_data.EmitItem = false;
    UxImVec2 restore_cursor_max_pos = group_data.BackupCursorMaxPos;
    window->DC.IdealMaxPos.x = UxImMax(window->DC.IdealMaxPos.x, window->DC.CursorMaxPos.x - window->Scroll.x); // Convert ideal extents for scrolling layer equivalent.
    EndGroup(); // Restore position on layer 0 // FIXME: Misleading to use a group for that backup/restore
    window->DC.LayoutType = UxImGuiLayoutType_Vertical;
    window->DC.IsSameLine = false;
    window->DC.NavLayerCurrent = UxImGuiNavLayer_Main;
    window->DC.MenuBarAppending = false;
    window->DC.CursorMaxPos = restore_cursor_max_pos;
}

// Important: calling order matters!
// FIXME: Somehow overlapping with docking tech.
// FIXME: The "rect-cut" aspect of this could be formalized into a lower-level helper (rect-cut: https://halt.software/dead-simple-layouts)
bool UxImGui::BeginViewportSideBar(const char* name, UxImGuiViewport* viewport_p, UxImGuiDir dir, float axis_size, UxImGuiWindowFlags window_flags)
{
    IM_ASSERT(dir != UxImGuiDir_None);

    UxImGuiWindow* bar_window = FindWindowByName(name);
    UxImGuiViewportP* viewport = (UxImGuiViewportP*)(void*)(viewport_p ? viewport_p : GetMainViewport());
    if (bar_window == NULL || bar_window->BeginCount == 0)
    {
        // Calculate and set window size/position
        UxImRect avail_rect = viewport->GetBuildWorkRect();
        UxImGuiAxis axis = (dir == UxImGuiDir_Up || dir == UxImGuiDir_Down) ? UxImGuiAxis_Y : UxImGuiAxis_X;
        UxImVec2 pos = avail_rect.Min;
        if (dir == UxImGuiDir_Right || dir == UxImGuiDir_Down)
            pos[axis] = avail_rect.Max[axis] - axis_size;
        UxImVec2 size = avail_rect.GetSize();
        size[axis] = axis_size;
        SetNextWindowPos(pos);
        SetNextWindowSize(size);

        // Report our size into work area (for next frame) using actual window size
        if (dir == UxImGuiDir_Up || dir == UxImGuiDir_Left)
            viewport->BuildWorkInsetMin[axis] += axis_size;
        else if (dir == UxImGuiDir_Down || dir == UxImGuiDir_Right)
            viewport->BuildWorkInsetMax[axis] += axis_size;
    }

    window_flags |= UxImGuiWindowFlags_NoTitleBar | UxImGuiWindowFlags_NoResize | UxImGuiWindowFlags_NoMove | UxImGuiWindowFlags_NoDocking;
    SetNextWindowViewport(viewport->ID); // Enforce viewport so we don't create our own viewport when UxImGuiConfigFlags_ViewportsNoMerge is set.
    PushStyleVar(UxImGuiStyleVar_WindowRounding, 0.0f);
    PushStyleVar(UxImGuiStyleVar_WindowMinSize, UxImVec2(0, 0)); // Lift normal size constraint
    bool is_open = Begin(name, NULL, window_flags);
    PopStyleVar(2);

    return is_open;
}

bool UxImGui::BeginMainMenuBar()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiViewportP* viewport = (UxImGuiViewportP*)(void*)GetMainViewport();

    // Notify of viewport change so GetFrameHeight() can be accurate in case of DPI change
    SetCurrentViewport(NULL, viewport);

    // For the main menu bar, which cannot be moved, we honor g.Style.DisplaySafeAreaPadding to ensure text can be visible on a TV set.
    // FIXME: This could be generalized as an opt-in way to clamp window->DC.CursorStartPos to avoid SafeArea?
    // FIXME: Consider removing support for safe area down the line... it's messy. Nowadays consoles have support for TV calibration in OS settings.
    g.NextWindowData.MenuBarOffsetMinVal = UxImVec2(g.Style.DisplaySafeAreaPadding.x, UxImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
    UxImGuiWindowFlags window_flags = UxImGuiWindowFlags_NoScrollbar | UxImGuiWindowFlags_NoSavedSettings | UxImGuiWindowFlags_MenuBar;
    float height = GetFrameHeight();
    bool is_open = BeginViewportSideBar("##MainMenuBar", viewport, UxImGuiDir_Up, height, window_flags);
    g.NextWindowData.MenuBarOffsetMinVal = UxImVec2(0.0f, 0.0f);
    if (!is_open)
    {
        End();
        return false;
    }

    // Temporarily disable _NoSavedSettings, in the off-chance that tables or child windows submitted within the menu-bar may want to use settings. (#8356)
    g.CurrentWindow->Flags &= ~UxImGuiWindowFlags_NoSavedSettings;
    BeginMenuBar();
    return is_open;
}

void UxImGui::EndMainMenuBar()
{
    UxImGuiContext& g = *GUxImGui;
    if (!g.CurrentWindow->DC.MenuBarAppending)
    {
        IM_ASSERT_USER_ERROR(0, "Calling EndMainMenuBar() not from a menu-bar!"); // Not technically testing that it is the main menu bar
        return;
    }

    EndMenuBar();
    g.CurrentWindow->Flags |= UxImGuiWindowFlags_NoSavedSettings; // Restore _NoSavedSettings (#8356)

    // When the user has left the menu layer (typically: closed menus through activation of an item), we restore focus to the previous window
    // FIXME: With this strategy we won't be able to restore a NULL focus.
    if (g.CurrentWindow == g.NavWindow && g.NavLayer == UxImGuiNavLayer_Main && !g.NavAnyRequest && g.ActiveId == 0)
        FocusTopMostWindowUnderOne(g.NavWindow, NULL, NULL, UxImGuiFocusRequestFlags_UnlessBelowModal | UxImGuiFocusRequestFlags_RestoreFocusedChild);

    End();
}

static bool IsRootOfOpenMenuSet()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if ((g.OpenPopupStack.Size <= g.BeginPopupStack.Size) || (window->Flags & UxImGuiWindowFlags_ChildMenu))
        return false;

    // Initially we used 'upper_popup->OpenParentId == window->IDStack.back()' to differentiate multiple menu sets from each others
    // (e.g. inside menu bar vs loose menu items) based on parent ID.
    // This would however prevent the use of e.g. PushID() user code submitting menus.
    // Previously this worked between popup and a first child menu because the first child menu always had the _ChildWindow flag,
    // making hovering on parent popup possible while first child menu was focused - but this was generally a bug with other side effects.
    // Instead we don't treat Popup specifically (in order to consistently support menu features in them), maybe the first child menu of a Popup
    // doesn't have the _ChildWindow flag, and we rely on this IsRootOfOpenMenuSet() check to allow hovering between root window/popup and first child menu.
    // In the end, lack of ID check made it so we could no longer differentiate between separate menu sets. To compensate for that, we at least check parent window nav layer.
    // This fixes the most common case of menu opening on hover when moving between window content and menu bar. Multiple different menu sets in same nav layer would still
    // open on hover, but that should be a lesser problem, because if such menus are close in proximity in window content then it won't feel weird and if they are far apart
    // it likely won't be a problem anyone runs into.
    const UxImGuiPopupData* upper_popup = &g.OpenPopupStack[g.BeginPopupStack.Size];
    if (window->DC.NavLayerCurrent != upper_popup->ParentNavLayer)
        return false;
    return upper_popup->Window && (upper_popup->Window->Flags & UxImGuiWindowFlags_ChildMenu) && UxImGui::IsWindowChildOf(upper_popup->Window, window, true, false);
}

bool UxImGui::BeginMenuEx(const char* label, const char* icon, bool enabled)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = window->GetID(label);
    bool menu_is_open = IsPopupOpen(id, UxImGuiPopupFlags_None);

    // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would steal focus and not allow hovering on parent menu)
    // The first menu in a hierarchy isn't so hovering doesn't get across (otherwise e.g. resizing borders with UxImGuiButtonFlags_FlattenChildren would react), but top-most BeginMenu() will bypass that limitation.
    UxImGuiWindowFlags window_flags = UxImGuiWindowFlags_ChildMenu | UxImGuiWindowFlags_AlwaysAutoResize | UxImGuiWindowFlags_NoMove | UxImGuiWindowFlags_NoTitleBar | UxImGuiWindowFlags_NoSavedSettings | UxImGuiWindowFlags_NoNavFocus;
    if (window->Flags & UxImGuiWindowFlags_ChildMenu)
        window_flags |= UxImGuiWindowFlags_ChildWindow;

    // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
    // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the expected small amount of BeginMenu() calls per frame.
    // If somehow this is ever becoming a problem we can switch to use e.g. UxImGuiStorage mapping key to last frame used.
    if (g.MenusIdSubmittedThisFrame.contains(id))
    {
        if (menu_is_open)
            menu_is_open = BeginPopupMenuEx(id, label, window_flags); // menu_is_open can be 'false' when the popup is completely clipped (e.g. zero size display)
        else
            g.NextWindowData.ClearFlags();          // we behave like Begin() and need to consume those values
        return menu_is_open;
    }

    // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
    g.MenusIdSubmittedThisFrame.push_back(id);

    UxImVec2 label_size = CalcTextSize(label, NULL, true);

    // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent without always being a Child window)
    // This is only done for items for the menu set and not the full parent window.
    const bool menuset_is_open = IsRootOfOpenMenuSet();
    if (menuset_is_open)
        PushItemFlag(UxImGuiItemFlags_NoWindowHoverableCheck, true);

    // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child menu,
    // However the final position is going to be different! It is chosen by FindBestWindowPosForPopup().
    // e.g. Menus tend to overlap each other horizontally to amplify relative Z-ordering.
    UxImVec2 popup_pos, pos = window->DC.CursorPos;
    PushID(label);
    if (!enabled)
        BeginDisabled();
    const UxImGuiMenuColumns* offsets = &window->DC.MenuColumns;
    bool pressed;

    // We use UxImGuiSelectableFlags_NoSetKeyOwner to allow down on one menu item, move, up on another.
    const UxImGuiSelectableFlags selectable_flags = UxImGuiSelectableFlags_NoHoldingActiveID | UxImGuiSelectableFlags_NoSetKeyOwner | UxImGuiSelectableFlags_SelectOnClick | UxImGuiSelectableFlags_NoAutoClosePopups;
    if (window->DC.LayoutType == UxImGuiLayoutType_Horizontal)
    {
        // Menu inside a horizontal menu bar
        // Selectable extend their highlight by half ItemSpacing in each direction.
        // For ChildMenu, the popup position will be overwritten by the call to FindBestWindowPosForPopup() in Begin()
        popup_pos = UxImVec2(pos.x - 1.0f - IM_TRUNC(style.ItemSpacing.x * 0.5f), pos.y - style.FramePadding.y + window->MenuBarHeight);
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
        PushStyleVarX(UxImGuiStyleVar_ItemSpacing, style.ItemSpacing.x * 2.0f);
        float w = label_size.x;
        UxImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        pressed = Selectable("", menu_is_open, selectable_flags, UxImVec2(w, label_size.y));
        LogSetNextTextDecoration("[", "]");
        RenderText(text_pos, label);
        PopStyleVar();
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
    }
    else
    {
        // Menu inside a regular/vertical menu
        // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
        //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.
        popup_pos = UxImVec2(pos.x, pos.y - style.WindowPadding.y);
        float icon_w = (icon && icon[0]) ? CalcTextSize(icon, NULL).x : 0.0f;
        float checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
        float min_w = window->DC.MenuColumns.DeclColumns(icon_w, label_size.x, 0.0f, checkmark_w); // Feedback to next frame
        float extra_w = UxImMax(0.0f, GetContentRegionAvail().x - min_w);
        UxImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        pressed = Selectable("", menu_is_open, selectable_flags | UxImGuiSelectableFlags_SpanAvailWidth, UxImVec2(min_w, label_size.y));
        LogSetNextTextDecoration("", ">");
        RenderText(text_pos, label);
        if (icon_w > 0.0f)
            RenderText(pos + UxImVec2(offsets->OffsetIcon, 0.0f), icon);
        RenderArrow(window->DrawList, pos + UxImVec2(offsets->OffsetMark + extra_w + g.FontSize * 0.30f, 0.0f), GetColorU32(UxImGuiCol_Text), UxImGuiDir_Right);
    }
    if (!enabled)
        EndDisabled();

    const bool hovered = (g.HoveredId == id) && enabled && !g.NavHighlightItemUnderNav;
    if (menuset_is_open)
        PopItemFlag();

    bool want_open = false;
    bool want_open_nav_init = false;
    bool want_close = false;
    if (window->DC.LayoutType == UxImGuiLayoutType_Vertical) // (window->Flags & (UxImGuiWindowFlags_Popup|UxImGuiWindowFlags_ChildMenu))
    {
        // Close menu when not hovering it anymore unless we are moving roughly in the direction of the menu
        // Implement http://bjk5.com/post/44698559168/breaking-down-amazons-mega-dropdown to avoid using timers, so menus feels more reactive.
        bool moving_toward_child_menu = false;
        UxImGuiPopupData* child_popup = (g.BeginPopupStack.Size < g.OpenPopupStack.Size) ? &g.OpenPopupStack[g.BeginPopupStack.Size] : NULL; // Popup candidate (testing below)
        UxImGuiWindow* child_menu_window = (child_popup && child_popup->Window && child_popup->Window->ParentWindow == window) ? child_popup->Window : NULL;
        if (g.HoveredWindow == window && child_menu_window != NULL)
        {
            const float ref_unit = g.FontSize; // FIXME-DPI
            const float child_dir = (window->Pos.x < child_menu_window->Pos.x) ? 1.0f : -1.0f;
            const UxImRect next_window_rect = child_menu_window->Rect();
            UxImVec2 ta = (g.IO.MousePos - g.IO.MouseDelta);
            UxImVec2 tb = (child_dir > 0.0f) ? next_window_rect.GetTL() : next_window_rect.GetTR();
            UxImVec2 tc = (child_dir > 0.0f) ? next_window_rect.GetBL() : next_window_rect.GetBR();
            const float pad_farmost_h = UxImClamp(UxImFabs(ta.x - tb.x) * 0.30f, ref_unit * 0.5f, ref_unit * 2.5f); // Add a bit of extra slack.
            ta.x += child_dir * -0.5f;
            tb.x += child_dir * ref_unit;
            tc.x += child_dir * ref_unit;
            tb.y = ta.y + UxImMax((tb.y - pad_farmost_h) - ta.y, -ref_unit * 8.0f); // Triangle has maximum height to limit the slope and the bias toward large sub-menus
            tc.y = ta.y + UxImMin((tc.y + pad_farmost_h) - ta.y, +ref_unit * 8.0f);
            moving_toward_child_menu = UxImTriangleContainsPoint(ta, tb, tc, g.IO.MousePos);
            //GetForegroundDrawList()->AddTriangleFilled(ta, tb, tc, moving_toward_child_menu ? IM_COL32(0,128,0,128) : IM_COL32(128,0,0,128)); // [DEBUG]
        }

        // The 'HovereWindow == window' check creates an inconsistency (e.g. moving away from menu slowly tends to hit same window, whereas moving away fast does not)
        // But we also need to not close the top-menu menu when moving over void. Perhaps we should extend the triangle check to a larger polygon.
        // (Remember to test this on BeginPopup("A")->BeginMenu("B") sequence which behaves slightly differently as B isn't a Child of A and hovering isn't shared.)
        if (menu_is_open && !hovered && g.HoveredWindow == window && !moving_toward_child_menu && !g.NavHighlightItemUnderNav && g.ActiveId == 0)
            want_close = true;

        // Open
        // (note: at this point 'hovered' actually includes the NavDisableMouseHover == false test)
        if (!menu_is_open && pressed) // Click/activate to open
            want_open = true;
        else if (!menu_is_open && hovered && !moving_toward_child_menu) // Hover to open
            want_open = true;
        else if (!menu_is_open && hovered && g.HoveredIdTimer >= 0.30f && g.MouseStationaryTimer >= 0.30f) // Hover to open (timer fallback)
            want_open = true;
        if (g.NavId == id && g.NavMoveDir == UxImGuiDir_Right) // Nav-Right to open
        {
            want_open = want_open_nav_init = true;
            NavMoveRequestCancel();
            SetNavCursorVisibleAfterMove();
        }
    }
    else
    {
        // Menu bar
        if (menu_is_open && pressed && menuset_is_open) // Click an open menu again to close it
        {
            want_close = true;
            want_open = menu_is_open = false;
        }
        else if (pressed || (hovered && menuset_is_open && !menu_is_open)) // First click to open, then hover to open others
        {
            want_open = true;
        }
        else if (g.NavId == id && g.NavMoveDir == UxImGuiDir_Down) // Nav-Down to open
        {
            want_open = true;
            NavMoveRequestCancel();
        }
    }

    if (!enabled) // explicitly close if an open menu becomes disabled, facilitate users code a lot in pattern such as 'if (BeginMenu("options", has_object)) { ..use object.. }'
        want_close = true;
    if (want_close && IsPopupOpen(id, UxImGuiPopupFlags_None))
        ClosePopupToLevel(g.BeginPopupStack.Size, true);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | UxImGuiItemStatusFlags_Openable | (menu_is_open ? UxImGuiItemStatusFlags_Opened : 0));
    PopID();

    if (want_open && !menu_is_open && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
    {
        // Don't reopen/recycle same menu level in the same frame if it is a different menu ID, first close the other menu and yield for a frame.
        OpenPopup(label);
    }
    else if (want_open)
    {
        menu_is_open = true;
        OpenPopup(label, UxImGuiPopupFlags_NoReopen);// | (want_open_nav_init ? UxImGuiPopupFlags_NoReopenAlwaysNavInit : 0));
    }

    if (menu_is_open)
    {
        UxImGuiLastItemData last_item_in_parent = g.LastItemData;
        SetNextWindowPos(popup_pos, UxImGuiCond_Always);                  // Note: misleading: the value will serve as reference for FindBestWindowPosForPopup(), not actual pos.
        PushStyleVar(UxImGuiStyleVar_ChildRounding, style.PopupRounding); // First level will use _PopupRounding, subsequent will use _ChildRounding
        menu_is_open = BeginPopupMenuEx(id, label, window_flags); // menu_is_open may be 'false' when the popup is completely clipped (e.g. zero size display)
        PopStyleVar();
        if (menu_is_open)
        {
            // Implement what UxImGuiPopupFlags_NoReopenAlwaysNavInit would do:
            // Perform an init request in the case the popup was already open (via a previous mouse hover)
            if (want_open && want_open_nav_init && !g.NavInitRequest)
            {
                FocusWindow(g.CurrentWindow, UxImGuiFocusRequestFlags_UnlessBelowModal);
                NavInitWindow(g.CurrentWindow, false);
            }

            // Restore LastItemData so IsItemXXXX functions can work after BeginMenu()/EndMenu()
            // (This fixes using IsItemClicked() and IsItemHovered(), but IsItemHovered() also relies on its support for UxImGuiItemFlags_NoWindowHoverableCheck)
            g.LastItemData = last_item_in_parent;
            if (g.HoveredWindow == window)
                g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_HoveredWindow;
        }
    }
    else
    {
        g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
    }

    return menu_is_open;
}

bool UxImGui::BeginMenu(const char* label, bool enabled)
{
    return BeginMenuEx(label, NULL, enabled);
}

void UxImGui::EndMenu()
{
    // Nav: When a left move request our menu failed, close ourselves.
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT(window->Flags & UxImGuiWindowFlags_Popup);  // Mismatched BeginMenu()/EndMenu() calls
    UxImGuiWindow* parent_window = window->ParentWindow;  // Should always be != NULL is we passed assert.
    if (window->BeginCount == window->BeginCountPreviousFrame)
        if (g.NavMoveDir == UxImGuiDir_Left && NavMoveRequestButNoResultYet())
            if (g.NavWindow && (g.NavWindow->RootWindowForNav == window) && parent_window->DC.LayoutType == UxImGuiLayoutType_Vertical)
            {
                ClosePopupToLevel(g.BeginPopupStack.Size - 1, true);
                NavMoveRequestCancel();
            }

    EndPopup();
}

bool UxImGui::MenuItemEx(const char* label, const char* icon, const char* shortcut, bool selected, bool enabled)
{
    UxImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    UxImGuiContext& g = *GUxImGui;
    UxImGuiStyle& style = g.Style;
    UxImVec2 pos = window->DC.CursorPos;
    UxImVec2 label_size = CalcTextSize(label, NULL, true);

    // See BeginMenuEx() for comments about this.
    const bool menuset_is_open = IsRootOfOpenMenuSet();
    if (menuset_is_open)
        PushItemFlag(UxImGuiItemFlags_NoWindowHoverableCheck, true);

    // We've been using the equivalent of UxImGuiSelectableFlags_SetNavIdOnHover on all Selectable() since early Nav system days (commit 43ee5d73),
    // but I am unsure whether this should be kept at all. For now moved it to be an opt-in feature used by menus only.
    bool pressed;
    PushID(label);
    if (!enabled)
        BeginDisabled();

    // We use UxImGuiSelectableFlags_NoSetKeyOwner to allow down on one menu item, move, up on another.
    const UxImGuiSelectableFlags selectable_flags = UxImGuiSelectableFlags_SelectOnRelease | UxImGuiSelectableFlags_NoSetKeyOwner | UxImGuiSelectableFlags_SetNavIdOnHover;
    const UxImGuiMenuColumns* offsets = &window->DC.MenuColumns;
    if (window->DC.LayoutType == UxImGuiLayoutType_Horizontal)
    {
        // Mimic the exact layout spacing of BeginMenu() to allow MenuItem() inside a menu bar, which is a little misleading but may be useful
        // Note that in this situation: we don't render the shortcut, we render a highlight instead of the selected tick mark.
        float w = label_size.x;
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
        UxImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        PushStyleVarX(UxImGuiStyleVar_ItemSpacing, style.ItemSpacing.x * 2.0f);
        pressed = Selectable("", selected, selectable_flags, UxImVec2(w, 0.0f));
        PopStyleVar();
        if (g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_Visible)
            RenderText(text_pos, label);
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
    }
    else
    {
        // Menu item inside a vertical menu
        // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
        //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.
        float icon_w = (icon && icon[0]) ? CalcTextSize(icon, NULL).x : 0.0f;
        float shortcut_w = (shortcut && shortcut[0]) ? CalcTextSize(shortcut, NULL).x : 0.0f;
        float checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
        float min_w = window->DC.MenuColumns.DeclColumns(icon_w, label_size.x, shortcut_w, checkmark_w); // Feedback for next frame
        float stretch_w = UxImMax(0.0f, GetContentRegionAvail().x - min_w);
        pressed = Selectable("", false, selectable_flags | UxImGuiSelectableFlags_SpanAvailWidth, UxImVec2(min_w, label_size.y));
        if (g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_Visible)
        {
            RenderText(pos + UxImVec2(offsets->OffsetLabel, 0.0f), label);
            if (icon_w > 0.0f)
                RenderText(pos + UxImVec2(offsets->OffsetIcon, 0.0f), icon);
            if (shortcut_w > 0.0f)
            {
                PushStyleColor(UxImGuiCol_Text, style.Colors[UxImGuiCol_TextDisabled]);
                LogSetNextTextDecoration("(", ")");
                RenderText(pos + UxImVec2(offsets->OffsetShortcut + stretch_w, 0.0f), shortcut, NULL, false);
                PopStyleColor();
            }
            if (selected)
                RenderCheckMark(window->DrawList, pos + UxImVec2(offsets->OffsetMark + stretch_w + g.FontSize * 0.40f, g.FontSize * 0.134f * 0.5f), GetColorU32(UxImGuiCol_Text), g.FontSize * 0.866f);
        }
    }
    IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | UxImGuiItemStatusFlags_Checkable | (selected ? UxImGuiItemStatusFlags_Checked : 0));
    if (!enabled)
        EndDisabled();
    PopID();
    if (menuset_is_open)
        PopItemFlag();

    return pressed;
}

bool UxImGui::MenuItem(const char* label, const char* shortcut, bool selected, bool enabled)
{
    return MenuItemEx(label, NULL, shortcut, selected, enabled);
}

bool UxImGui::MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled)
{
    if (MenuItemEx(label, NULL, shortcut, p_selected ? *p_selected : false, enabled))
    {
        if (p_selected)
            *p_selected = !*p_selected;
        return true;
    }
    return false;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: BeginTabBar, EndTabBar, etc.
//-------------------------------------------------------------------------
// - BeginTabBar()
// - BeginTabBarEx() [Internal]
// - EndTabBar()
// - TabBarLayout() [Internal]
// - TabBarCalcTabID() [Internal]
// - TabBarCalcMaxTabWidth() [Internal]
// - TabBarFindTabById() [Internal]
// - TabBarFindTabByOrder() [Internal]
// - TabBarFindMostRecentlySelectedTabForActiveWindow() [Internal]
// - TabBarGetCurrentTab() [Internal]
// - TabBarGetTabName() [Internal]
// - TabBarAddTab() [Internal]
// - TabBarRemoveTab() [Internal]
// - TabBarCloseTab() [Internal]
// - TabBarScrollClamp() [Internal]
// - TabBarScrollToTab() [Internal]
// - TabBarQueueFocus() [Internal]
// - TabBarQueueReorder() [Internal]
// - TabBarProcessReorderFromMousePos() [Internal]
// - TabBarProcessReorder() [Internal]
// - TabBarScrollingButtons() [Internal]
// - TabBarTabListPopupButton() [Internal]
//-------------------------------------------------------------------------

struct UxImGuiTabBarSection
{
    int                 TabCount;               // Number of tabs in this section.
    float               Width;                  // Sum of width of tabs in this section (after shrinking down)
    float               Spacing;                // Horizontal spacing at the end of the section.

    UxImGuiTabBarSection() { memset(this, 0, sizeof(*this)); }
};

namespace UxImGui
{
    static void             TabBarLayout(UxImGuiTabBar* tab_bar);
    static UxImU32            TabBarCalcTabID(UxImGuiTabBar* tab_bar, const char* label, UxImGuiWindow* docked_window);
    static float            TabBarCalcMaxTabWidth();
    static float            TabBarScrollClamp(UxImGuiTabBar* tab_bar, float scrolling);
    static void             TabBarScrollToTab(UxImGuiTabBar* tab_bar, UxImGuiID tab_id, UxImGuiTabBarSection* sections);
    static UxImGuiTabItem*    TabBarScrollingButtons(UxImGuiTabBar* tab_bar);
    static UxImGuiTabItem*    TabBarTabListPopupButton(UxImGuiTabBar* tab_bar);
}

UxImGuiTabBar::UxImGuiTabBar()
{
    memset(this, 0, sizeof(*this));
    CurrFrameVisible = PrevFrameVisible = -1;
    LastTabItemIdx = -1;
}

static inline int TabItemGetSectionIdx(const UxImGuiTabItem* tab)
{
    return (tab->Flags & UxImGuiTabItemFlags_Leading) ? 0 : (tab->Flags & UxImGuiTabItemFlags_Trailing) ? 2 : 1;
}

static int IMGUI_CDECL TabItemComparerBySection(const void* lhs, const void* rhs)
{
    const UxImGuiTabItem* a = (const UxImGuiTabItem*)lhs;
    const UxImGuiTabItem* b = (const UxImGuiTabItem*)rhs;
    const int a_section = TabItemGetSectionIdx(a);
    const int b_section = TabItemGetSectionIdx(b);
    if (a_section != b_section)
        return a_section - b_section;
    return (int)(a->IndexDuringLayout - b->IndexDuringLayout);
}

static int IMGUI_CDECL TabItemComparerByBeginOrder(const void* lhs, const void* rhs)
{
    const UxImGuiTabItem* a = (const UxImGuiTabItem*)lhs;
    const UxImGuiTabItem* b = (const UxImGuiTabItem*)rhs;
    return (int)(a->BeginOrder - b->BeginOrder);
}

static UxImGuiTabBar* GetTabBarFromTabBarRef(const UxImGuiPtrOrIndex& ref)
{
    UxImGuiContext& g = *GUxImGui;
    return ref.Ptr ? (UxImGuiTabBar*)ref.Ptr : g.TabBars.GetByIndex(ref.Index);
}

static UxImGuiPtrOrIndex GetTabBarRefFromTabBar(UxImGuiTabBar* tab_bar)
{
    UxImGuiContext& g = *GUxImGui;
    if (g.TabBars.Contains(tab_bar))
        return UxImGuiPtrOrIndex(g.TabBars.GetIndex(tab_bar));
    return UxImGuiPtrOrIndex(tab_bar);
}

bool    UxImGui::BeginTabBar(const char* str_id, UxImGuiTabBarFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    UxImGuiID id = window->GetID(str_id);
    UxImGuiTabBar* tab_bar = g.TabBars.GetOrAddByKey(id);
    UxImRect tab_bar_bb = UxImRect(window->DC.CursorPos.x, window->DC.CursorPos.y, window->WorkRect.Max.x, window->DC.CursorPos.y + g.FontSize + g.Style.FramePadding.y * 2);
    tab_bar->ID = id;
    tab_bar->SeparatorMinX = tab_bar->BarRect.Min.x - IM_TRUNC(window->WindowPadding.x * 0.5f);
    tab_bar->SeparatorMaxX = tab_bar->BarRect.Max.x + IM_TRUNC(window->WindowPadding.x * 0.5f);
    //if (g.NavWindow && IsWindowChildOf(g.NavWindow, window, false, false))
    flags |= UxImGuiTabBarFlags_IsFocused;
    return BeginTabBarEx(tab_bar, tab_bar_bb, flags);
}

bool    UxImGui::BeginTabBarEx(UxImGuiTabBar* tab_bar, const UxImRect& tab_bar_bb, UxImGuiTabBarFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    IM_ASSERT(tab_bar->ID != 0);
    if ((flags & UxImGuiTabBarFlags_DockNode) == 0)
        PushOverrideID(tab_bar->ID);

    // Add to stack
    g.CurrentTabBarStack.push_back(GetTabBarRefFromTabBar(tab_bar));
    g.CurrentTabBar = tab_bar;
    tab_bar->Window = window;

    // Append with multiple BeginTabBar()/EndTabBar() pairs.
    tab_bar->BackupCursorPos = window->DC.CursorPos;
    if (tab_bar->CurrFrameVisible == g.FrameCount)
    {
        window->DC.CursorPos = UxImVec2(tab_bar->BarRect.Min.x, tab_bar->BarRect.Max.y + tab_bar->ItemSpacingY);
        tab_bar->BeginCount++;
        return true;
    }

    // Ensure correct ordering when toggling UxImGuiTabBarFlags_Reorderable flag, or when a new tab was added while being not reorderable
    if ((flags & UxImGuiTabBarFlags_Reorderable) != (tab_bar->Flags & UxImGuiTabBarFlags_Reorderable) || (tab_bar->TabsAddedNew && !(flags & UxImGuiTabBarFlags_Reorderable)))
        if ((flags & UxImGuiTabBarFlags_DockNode) == 0) // FIXME: TabBar with DockNode can now be hybrid
            UxImQsort(tab_bar->Tabs.Data, tab_bar->Tabs.Size, sizeof(UxImGuiTabItem), TabItemComparerByBeginOrder);
    tab_bar->TabsAddedNew = false;

    // Flags
    if ((flags & UxImGuiTabBarFlags_FittingPolicyMask_) == 0)
        flags |= UxImGuiTabBarFlags_FittingPolicyDefault_;

    tab_bar->Flags = flags;
    tab_bar->BarRect = tab_bar_bb;
    tab_bar->WantLayout = true; // Layout will be done on the first call to ItemTab()
    tab_bar->PrevFrameVisible = tab_bar->CurrFrameVisible;
    tab_bar->CurrFrameVisible = g.FrameCount;
    tab_bar->PrevTabsContentsHeight = tab_bar->CurrTabsContentsHeight;
    tab_bar->CurrTabsContentsHeight = 0.0f;
    tab_bar->ItemSpacingY = g.Style.ItemSpacing.y;
    tab_bar->FramePadding = g.Style.FramePadding;
    tab_bar->TabsActiveCount = 0;
    tab_bar->LastTabItemIdx = -1;
    tab_bar->BeginCount = 1;

    // Set cursor pos in a way which only be used in the off-chance the user erroneously submits item before BeginTabItem(): items will overlap
    window->DC.CursorPos = UxImVec2(tab_bar->BarRect.Min.x, tab_bar->BarRect.Max.y + tab_bar->ItemSpacingY);

    // Draw separator
    // (it would be misleading to draw this in EndTabBar() suggesting that it may be drawn over tabs, as tab bar are appendable)
    const UxImU32 col = GetColorU32((flags & UxImGuiTabBarFlags_IsFocused) ? UxImGuiCol_TabSelected : UxImGuiCol_TabDimmedSelected);
    if (g.Style.TabBarBorderSize > 0.0f)
    {
        const float y = tab_bar->BarRect.Max.y;
        window->DrawList->AddRectFilled(UxImVec2(tab_bar->SeparatorMinX, y - g.Style.TabBarBorderSize), UxImVec2(tab_bar->SeparatorMaxX, y), col);
    }
    return true;
}

void    UxImGui::EndTabBar()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    UxImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Mismatched BeginTabBar()/EndTabBar()!");
        return;
    }

    // Fallback in case no TabItem have been submitted
    if (tab_bar->WantLayout)
        TabBarLayout(tab_bar);

    // Restore the last visible height if no tab is visible, this reduce vertical flicker/movement when a tabs gets removed without calling SetTabItemClosed().
    const bool tab_bar_appearing = (tab_bar->PrevFrameVisible + 1 < g.FrameCount);
    if (tab_bar->VisibleTabWasSubmitted || tab_bar->VisibleTabId == 0 || tab_bar_appearing)
    {
        tab_bar->CurrTabsContentsHeight = UxImMax(window->DC.CursorPos.y - tab_bar->BarRect.Max.y, tab_bar->CurrTabsContentsHeight);
        window->DC.CursorPos.y = tab_bar->BarRect.Max.y + tab_bar->CurrTabsContentsHeight;
    }
    else
    {
        window->DC.CursorPos.y = tab_bar->BarRect.Max.y + tab_bar->PrevTabsContentsHeight;
    }
    if (tab_bar->BeginCount > 1)
        window->DC.CursorPos = tab_bar->BackupCursorPos;

    tab_bar->LastTabItemIdx = -1;
    if ((tab_bar->Flags & UxImGuiTabBarFlags_DockNode) == 0)
        PopID();

    g.CurrentTabBarStack.pop_back();
    g.CurrentTabBar = g.CurrentTabBarStack.empty() ? NULL : GetTabBarFromTabBarRef(g.CurrentTabBarStack.back());
}

// Scrolling happens only in the central section (leading/trailing sections are not scrolling)
static float TabBarCalcScrollableWidth(UxImGuiTabBar* tab_bar, UxImGuiTabBarSection* sections)
{
    return tab_bar->BarRect.GetWidth() - sections[0].Width - sections[2].Width - sections[1].Spacing;
}

// This is called only once a frame before by the first call to ItemTab()
// The reason we're not calling it in BeginTabBar() is to leave a chance to the user to call the SetTabItemClosed() functions.
static void UxImGui::TabBarLayout(UxImGuiTabBar* tab_bar)
{
    UxImGuiContext& g = *GUxImGui;
    tab_bar->WantLayout = false;

    // Garbage collect by compacting list
    // Detect if we need to sort out tab list (e.g. in rare case where a tab changed section)
    int tab_dst_n = 0;
    bool need_sort_by_section = false;
    UxImGuiTabBarSection sections[3]; // Layout sections: Leading, Central, Trailing
    for (int tab_src_n = 0; tab_src_n < tab_bar->Tabs.Size; tab_src_n++)
    {
        UxImGuiTabItem* tab = &tab_bar->Tabs[tab_src_n];
        if (tab->LastFrameVisible < tab_bar->PrevFrameVisible || tab->WantClose)
        {
            // Remove tab
            if (tab_bar->VisibleTabId == tab->ID) { tab_bar->VisibleTabId = 0; }
            if (tab_bar->SelectedTabId == tab->ID) { tab_bar->SelectedTabId = 0; }
            if (tab_bar->NextSelectedTabId == tab->ID) { tab_bar->NextSelectedTabId = 0; }
            continue;
        }
        if (tab_dst_n != tab_src_n)
            tab_bar->Tabs[tab_dst_n] = tab_bar->Tabs[tab_src_n];

        tab = &tab_bar->Tabs[tab_dst_n];
        tab->IndexDuringLayout = (UxImS16)tab_dst_n;

        // We will need sorting if tabs have changed section (e.g. moved from one of Leading/Central/Trailing to another)
        int curr_tab_section_n = TabItemGetSectionIdx(tab);
        if (tab_dst_n > 0)
        {
            UxImGuiTabItem* prev_tab = &tab_bar->Tabs[tab_dst_n - 1];
            int prev_tab_section_n = TabItemGetSectionIdx(prev_tab);
            if (curr_tab_section_n == 0 && prev_tab_section_n != 0)
                need_sort_by_section = true;
            if (prev_tab_section_n == 2 && curr_tab_section_n != 2)
                need_sort_by_section = true;
        }

        sections[curr_tab_section_n].TabCount++;
        tab_dst_n++;
    }
    if (tab_bar->Tabs.Size != tab_dst_n)
        tab_bar->Tabs.resize(tab_dst_n);

    if (need_sort_by_section)
        UxImQsort(tab_bar->Tabs.Data, tab_bar->Tabs.Size, sizeof(UxImGuiTabItem), TabItemComparerBySection);

    // Calculate spacing between sections
    sections[0].Spacing = sections[0].TabCount > 0 && (sections[1].TabCount + sections[2].TabCount) > 0 ? g.Style.ItemInnerSpacing.x : 0.0f;
    sections[1].Spacing = sections[1].TabCount > 0 && sections[2].TabCount > 0 ? g.Style.ItemInnerSpacing.x : 0.0f;

    // Setup next selected tab
    UxImGuiID scroll_to_tab_id = 0;
    if (tab_bar->NextSelectedTabId)
    {
        tab_bar->SelectedTabId = tab_bar->NextSelectedTabId;
        tab_bar->NextSelectedTabId = 0;
        scroll_to_tab_id = tab_bar->SelectedTabId;
    }

    // Process order change request (we could probably process it when requested but it's just saner to do it in a single spot).
    if (tab_bar->ReorderRequestTabId != 0)
    {
        if (TabBarProcessReorder(tab_bar))
            if (tab_bar->ReorderRequestTabId == tab_bar->SelectedTabId)
                scroll_to_tab_id = tab_bar->ReorderRequestTabId;
        tab_bar->ReorderRequestTabId = 0;
    }

    // Tab List Popup (will alter tab_bar->BarRect and therefore the available width!)
    const bool tab_list_popup_button = (tab_bar->Flags & UxImGuiTabBarFlags_TabListPopupButton) != 0;
    if (tab_list_popup_button)
        if (UxImGuiTabItem* tab_to_select = TabBarTabListPopupButton(tab_bar)) // NB: Will alter BarRect.Min.x!
            scroll_to_tab_id = tab_bar->SelectedTabId = tab_to_select->ID;

    // Leading/Trailing tabs will be shrink only if central one aren't visible anymore, so layout the shrink data as: leading, trailing, central
    // (whereas our tabs are stored as: leading, central, trailing)
    int shrink_buffer_indexes[3] = { 0, sections[0].TabCount + sections[2].TabCount, sections[0].TabCount };
    g.ShrinkWidthBuffer.resize(tab_bar->Tabs.Size);

    // Compute ideal tabs widths + store them into shrink buffer
    UxImGuiTabItem* most_recently_selected_tab = NULL;
    int curr_section_n = -1;
    bool found_selected_tab_id = false;
    for (int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
    {
        UxImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
        IM_ASSERT(tab->LastFrameVisible >= tab_bar->PrevFrameVisible);

        if ((most_recently_selected_tab == NULL || most_recently_selected_tab->LastFrameSelected < tab->LastFrameSelected) && !(tab->Flags & UxImGuiTabItemFlags_Button))
            most_recently_selected_tab = tab;
        if (tab->ID == tab_bar->SelectedTabId)
            found_selected_tab_id = true;
        if (scroll_to_tab_id == 0 && g.NavJustMovedToId == tab->ID)
            scroll_to_tab_id = tab->ID;

        // Refresh tab width immediately, otherwise changes of style e.g. style.FramePadding.x would noticeably lag in the tab bar.
        // Additionally, when using TabBarAddTab() to manipulate tab bar order we occasionally insert new tabs that don't have a width yet,
        // and we cannot wait for the next BeginTabItem() call. We cannot compute this width within TabBarAddTab() because font size depends on the active window.
        const char* tab_name = TabBarGetTabName(tab_bar, tab);
        const bool has_close_button_or_unsaved_marker = (tab->Flags & UxImGuiTabItemFlags_NoCloseButton) == 0 || (tab->Flags & UxImGuiTabItemFlags_UnsavedDocument);
        tab->ContentWidth = (tab->RequestedWidth >= 0.0f) ? tab->RequestedWidth : TabItemCalcSize(tab_name, has_close_button_or_unsaved_marker).x;

        int section_n = TabItemGetSectionIdx(tab);
        UxImGuiTabBarSection* section = &sections[section_n];
        section->Width += tab->ContentWidth + (section_n == curr_section_n ? g.Style.ItemInnerSpacing.x : 0.0f);
        curr_section_n = section_n;

        // Store data so we can build an array sorted by width if we need to shrink tabs down
        IM_MSVC_WARNING_SUPPRESS(6385);
        UxImGuiShrinkWidthItem* shrink_width_item = &g.ShrinkWidthBuffer[shrink_buffer_indexes[section_n]++];
        shrink_width_item->Index = tab_n;
        shrink_width_item->Width = shrink_width_item->InitialWidth = tab->ContentWidth;
        tab->Width = UxImMax(tab->ContentWidth, 1.0f);
    }

    // Compute total ideal width (used for e.g. auto-resizing a window)
    tab_bar->WidthAllTabsIdeal = 0.0f;
    for (int section_n = 0; section_n < 3; section_n++)
        tab_bar->WidthAllTabsIdeal += sections[section_n].Width + sections[section_n].Spacing;

    // Horizontal scrolling buttons
    // (note that TabBarScrollButtons() will alter BarRect.Max.x)
    if ((tab_bar->WidthAllTabsIdeal > tab_bar->BarRect.GetWidth() && tab_bar->Tabs.Size > 1) && !(tab_bar->Flags & UxImGuiTabBarFlags_NoTabListScrollingButtons) && (tab_bar->Flags & UxImGuiTabBarFlags_FittingPolicyScroll))
        if (UxImGuiTabItem* scroll_and_select_tab = TabBarScrollingButtons(tab_bar))
        {
            scroll_to_tab_id = scroll_and_select_tab->ID;
            if ((scroll_and_select_tab->Flags & UxImGuiTabItemFlags_Button) == 0)
                tab_bar->SelectedTabId = scroll_to_tab_id;
        }

    // Shrink widths if full tabs don't fit in their allocated space
    float section_0_w = sections[0].Width + sections[0].Spacing;
    float section_1_w = sections[1].Width + sections[1].Spacing;
    float section_2_w = sections[2].Width + sections[2].Spacing;
    bool central_section_is_visible = (section_0_w + section_2_w) < tab_bar->BarRect.GetWidth();
    float width_excess;
    if (central_section_is_visible)
        width_excess = UxImMax(section_1_w - (tab_bar->BarRect.GetWidth() - section_0_w - section_2_w), 0.0f); // Excess used to shrink central section
    else
        width_excess = (section_0_w + section_2_w) - tab_bar->BarRect.GetWidth(); // Excess used to shrink leading/trailing section

    // With UxImGuiTabBarFlags_FittingPolicyScroll policy, we will only shrink leading/trailing if the central section is not visible anymore
    if (width_excess >= 1.0f && ((tab_bar->Flags & UxImGuiTabBarFlags_FittingPolicyResizeDown) || !central_section_is_visible))
    {
        int shrink_data_count = (central_section_is_visible ? sections[1].TabCount : sections[0].TabCount + sections[2].TabCount);
        int shrink_data_offset = (central_section_is_visible ? sections[0].TabCount + sections[2].TabCount : 0);
        ShrinkWidths(g.ShrinkWidthBuffer.Data + shrink_data_offset, shrink_data_count, width_excess);

        // Apply shrunk values into tabs and sections
        for (int tab_n = shrink_data_offset; tab_n < shrink_data_offset + shrink_data_count; tab_n++)
        {
            UxImGuiTabItem* tab = &tab_bar->Tabs[g.ShrinkWidthBuffer[tab_n].Index];
            float shrinked_width = IM_TRUNC(g.ShrinkWidthBuffer[tab_n].Width);
            if (shrinked_width < 0.0f)
                continue;

            shrinked_width = UxImMax(1.0f, shrinked_width);
            int section_n = TabItemGetSectionIdx(tab);
            sections[section_n].Width -= (tab->Width - shrinked_width);
            tab->Width = shrinked_width;
        }
    }

    // Layout all active tabs
    int section_tab_index = 0;
    float tab_offset = 0.0f;
    tab_bar->WidthAllTabs = 0.0f;
    for (int section_n = 0; section_n < 3; section_n++)
    {
        UxImGuiTabBarSection* section = &sections[section_n];
        if (section_n == 2)
            tab_offset = UxImMin(UxImMax(0.0f, tab_bar->BarRect.GetWidth() - section->Width), tab_offset);

        for (int tab_n = 0; tab_n < section->TabCount; tab_n++)
        {
            UxImGuiTabItem* tab = &tab_bar->Tabs[section_tab_index + tab_n];
            tab->Offset = tab_offset;
            tab->NameOffset = -1;
            tab_offset += tab->Width + (tab_n < section->TabCount - 1 ? g.Style.ItemInnerSpacing.x : 0.0f);
        }
        tab_bar->WidthAllTabs += UxImMax(section->Width + section->Spacing, 0.0f);
        tab_offset += section->Spacing;
        section_tab_index += section->TabCount;
    }

    // Clear name buffers
    tab_bar->TabsNames.Buf.resize(0);

    // If we have lost the selected tab, select the next most recently active one
    if (found_selected_tab_id == false)
        tab_bar->SelectedTabId = 0;
    if (tab_bar->SelectedTabId == 0 && tab_bar->NextSelectedTabId == 0 && most_recently_selected_tab != NULL)
        scroll_to_tab_id = tab_bar->SelectedTabId = most_recently_selected_tab->ID;

    // Lock in visible tab
    tab_bar->VisibleTabId = tab_bar->SelectedTabId;
    tab_bar->VisibleTabWasSubmitted = false;

    // CTRL+TAB can override visible tab temporarily
    if (g.NavWindowingTarget != NULL && g.NavWindowingTarget->DockNode && g.NavWindowingTarget->DockNode->TabBar == tab_bar)
        tab_bar->VisibleTabId = scroll_to_tab_id = g.NavWindowingTarget->TabId;

    // Apply request requests
    if (scroll_to_tab_id != 0)
        TabBarScrollToTab(tab_bar, scroll_to_tab_id, sections);
    else if ((tab_bar->Flags & UxImGuiTabBarFlags_FittingPolicyScroll) && IsMouseHoveringRect(tab_bar->BarRect.Min, tab_bar->BarRect.Max, true) && IsWindowContentHoverable(g.CurrentWindow))
    {
        const float wheel = g.IO.MouseWheelRequestAxisSwap ? g.IO.MouseWheel : g.IO.MouseWheelH;
        const UxImGuiKey wheel_key = g.IO.MouseWheelRequestAxisSwap ? UxImGuiKey_MouseWheelY : UxImGuiKey_MouseWheelX;
        if (TestKeyOwner(wheel_key, tab_bar->ID) && wheel != 0.0f)
        {
            const float scroll_step = wheel * TabBarCalcScrollableWidth(tab_bar, sections) / 3.0f;
            tab_bar->ScrollingTargetDistToVisibility = 0.0f;
            tab_bar->ScrollingTarget = TabBarScrollClamp(tab_bar, tab_bar->ScrollingTarget - scroll_step);
        }
        SetKeyOwner(wheel_key, tab_bar->ID);
    }

    // Update scrolling
    tab_bar->ScrollingAnim = TabBarScrollClamp(tab_bar, tab_bar->ScrollingAnim);
    tab_bar->ScrollingTarget = TabBarScrollClamp(tab_bar, tab_bar->ScrollingTarget);
    if (tab_bar->ScrollingAnim != tab_bar->ScrollingTarget)
    {
        // Scrolling speed adjust itself so we can always reach our target in 1/3 seconds.
        // Teleport if we are aiming far off the visible line
        tab_bar->ScrollingSpeed = UxImMax(tab_bar->ScrollingSpeed, 70.0f * g.FontSize);
        tab_bar->ScrollingSpeed = UxImMax(tab_bar->ScrollingSpeed, UxImFabs(tab_bar->ScrollingTarget - tab_bar->ScrollingAnim) / 0.3f);
        const bool teleport = (tab_bar->PrevFrameVisible + 1 < g.FrameCount) || (tab_bar->ScrollingTargetDistToVisibility > 10.0f * g.FontSize);
        tab_bar->ScrollingAnim = teleport ? tab_bar->ScrollingTarget : UxImLinearSweep(tab_bar->ScrollingAnim, tab_bar->ScrollingTarget, g.IO.DeltaTime * tab_bar->ScrollingSpeed);
    }
    else
    {
        tab_bar->ScrollingSpeed = 0.0f;
    }
    tab_bar->ScrollingRectMinX = tab_bar->BarRect.Min.x + sections[0].Width + sections[0].Spacing;
    tab_bar->ScrollingRectMaxX = tab_bar->BarRect.Max.x - sections[2].Width - sections[1].Spacing;

    // Actual layout in host window (we don't do it in BeginTabBar() so as not to waste an extra frame)
    UxImGuiWindow* window = g.CurrentWindow;
    window->DC.CursorPos = tab_bar->BarRect.Min;
    ItemSize(UxImVec2(tab_bar->WidthAllTabs, tab_bar->BarRect.GetHeight()), tab_bar->FramePadding.y);
    window->DC.IdealMaxPos.x = UxImMax(window->DC.IdealMaxPos.x, tab_bar->BarRect.Min.x + tab_bar->WidthAllTabsIdeal);
}

// Dockable windows uses Name/ID in the global namespace. Non-dockable items use the ID stack.
static UxImU32   UxImGui::TabBarCalcTabID(UxImGuiTabBar* tab_bar, const char* label, UxImGuiWindow* docked_window)
{
    if (docked_window != NULL)
    {
        IM_UNUSED(tab_bar);
        IM_ASSERT(tab_bar->Flags & UxImGuiTabBarFlags_DockNode);
        UxImGuiID id = docked_window->TabId;
        KeepAliveID(id);
        return id;
    }
    else
    {
        UxImGuiWindow* window = GUxImGui->CurrentWindow;
        return window->GetID(label);
    }
}

static float UxImGui::TabBarCalcMaxTabWidth()
{
    UxImGuiContext& g = *GUxImGui;
    return g.FontSize * 20.0f;
}

UxImGuiTabItem* UxImGui::TabBarFindTabByID(UxImGuiTabBar* tab_bar, UxImGuiID tab_id)
{
    if (tab_id != 0)
        for (int n = 0; n < tab_bar->Tabs.Size; n++)
            if (tab_bar->Tabs[n].ID == tab_id)
                return &tab_bar->Tabs[n];
    return NULL;
}

// Order = visible order, not submission order! (which is tab->BeginOrder)
UxImGuiTabItem* UxImGui::TabBarFindTabByOrder(UxImGuiTabBar* tab_bar, int order)
{
    if (order < 0 || order >= tab_bar->Tabs.Size)
        return NULL;
    return &tab_bar->Tabs[order];
}

// FIXME: See references to #2304 in TODO.txt
UxImGuiTabItem* UxImGui::TabBarFindMostRecentlySelectedTabForActiveWindow(UxImGuiTabBar* tab_bar)
{
    UxImGuiTabItem* most_recently_selected_tab = NULL;
    for (int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
    {
        UxImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
        if (most_recently_selected_tab == NULL || most_recently_selected_tab->LastFrameSelected < tab->LastFrameSelected)
            if (tab->Window && tab->Window->WasActive)
                most_recently_selected_tab = tab;
    }
    return most_recently_selected_tab;
}

UxImGuiTabItem* UxImGui::TabBarGetCurrentTab(UxImGuiTabBar* tab_bar)
{
    if (tab_bar->LastTabItemIdx < 0 || tab_bar->LastTabItemIdx >= tab_bar->Tabs.Size)
        return NULL;
    return &tab_bar->Tabs[tab_bar->LastTabItemIdx];
}

const char* UxImGui::TabBarGetTabName(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab)
{
    if (tab->Window)
        return tab->Window->Name;
    if (tab->NameOffset == -1)
        return "N/A";
    IM_ASSERT(tab->NameOffset < tab_bar->TabsNames.Buf.Size);
    return tab_bar->TabsNames.Buf.Data + tab->NameOffset;
}

// The purpose of this call is to register tab in advance so we can control their order at the time they appear.
// Otherwise calling this is unnecessary as tabs are appending as needed by the BeginTabItem() function.
void UxImGui::TabBarAddTab(UxImGuiTabBar* tab_bar, UxImGuiTabItemFlags tab_flags, UxImGuiWindow* window)
{
    UxImGuiContext& g = *GUxImGui;
    IM_ASSERT(TabBarFindTabByID(tab_bar, window->TabId) == NULL);
    IM_ASSERT(g.CurrentTabBar != tab_bar);  // Can't work while the tab bar is active as our tab doesn't have an X offset yet, in theory we could/should test something like (tab_bar->CurrFrameVisible < g.FrameCount) but we'd need to solve why triggers the commented early-out assert in BeginTabBarEx() (probably dock node going from implicit to explicit in same frame)

    if (!window->HasCloseButton)
        tab_flags |= UxImGuiTabItemFlags_NoCloseButton;       // Set _NoCloseButton immediately because it will be used for first-frame width calculation.

    UxImGuiTabItem new_tab;
    new_tab.ID = window->TabId;
    new_tab.Flags = tab_flags;
    new_tab.LastFrameVisible = tab_bar->CurrFrameVisible;   // Required so BeginTabBar() doesn't ditch the tab
    if (new_tab.LastFrameVisible == -1)
        new_tab.LastFrameVisible = g.FrameCount - 1;
    new_tab.Window = window;                                // Required so tab bar layout can compute the tab width before tab submission
    tab_bar->Tabs.push_back(new_tab);
}

// The *TabId fields are already set by the docking system _before_ the actual TabItem was created, so we clear them regardless.
void UxImGui::TabBarRemoveTab(UxImGuiTabBar* tab_bar, UxImGuiID tab_id)
{
    if (UxImGuiTabItem* tab = TabBarFindTabByID(tab_bar, tab_id))
        tab_bar->Tabs.erase(tab);
    if (tab_bar->VisibleTabId == tab_id)      { tab_bar->VisibleTabId = 0; }
    if (tab_bar->SelectedTabId == tab_id)     { tab_bar->SelectedTabId = 0; }
    if (tab_bar->NextSelectedTabId == tab_id) { tab_bar->NextSelectedTabId = 0; }
}

// Called on manual closure attempt
void UxImGui::TabBarCloseTab(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab)
{
    if (tab->Flags & UxImGuiTabItemFlags_Button)
        return; // A button appended with TabItemButton().

    if ((tab->Flags & (UxImGuiTabItemFlags_UnsavedDocument | UxImGuiTabItemFlags_NoAssumedClosure)) == 0)
    {
        // This will remove a frame of lag for selecting another tab on closure.
        // However we don't run it in the case where the 'Unsaved' flag is set, so user gets a chance to fully undo the closure
        tab->WantClose = true;
        if (tab_bar->VisibleTabId == tab->ID)
        {
            tab->LastFrameVisible = -1;
            tab_bar->SelectedTabId = tab_bar->NextSelectedTabId = 0;
        }
    }
    else
    {
        // Actually select before expecting closure attempt (on an UnsavedDocument tab user is expect to e.g. show a popup)
        if (tab_bar->VisibleTabId != tab->ID)
            TabBarQueueFocus(tab_bar, tab);
    }
}

static float UxImGui::TabBarScrollClamp(UxImGuiTabBar* tab_bar, float scrolling)
{
    scrolling = UxImMin(scrolling, tab_bar->WidthAllTabs - tab_bar->BarRect.GetWidth());
    return UxImMax(scrolling, 0.0f);
}

// Note: we may scroll to tab that are not selected! e.g. using keyboard arrow keys
static void UxImGui::TabBarScrollToTab(UxImGuiTabBar* tab_bar, UxImGuiID tab_id, UxImGuiTabBarSection* sections)
{
    UxImGuiTabItem* tab = TabBarFindTabByID(tab_bar, tab_id);
    if (tab == NULL)
        return;
    if (tab->Flags & UxImGuiTabItemFlags_SectionMask_)
        return;

    UxImGuiContext& g = *GUxImGui;
    float margin = g.FontSize * 1.0f; // When to scroll to make Tab N+1 visible always make a bit of N visible to suggest more scrolling area (since we don't have a scrollbar)
    int order = TabBarGetTabOrder(tab_bar, tab);

    // Scrolling happens only in the central section (leading/trailing sections are not scrolling)
    float scrollable_width = TabBarCalcScrollableWidth(tab_bar, sections);

    // We make all tabs positions all relative Sections[0].Width to make code simpler
    float tab_x1 = tab->Offset - sections[0].Width + (order > sections[0].TabCount - 1 ? -margin : 0.0f);
    float tab_x2 = tab->Offset - sections[0].Width + tab->Width + (order + 1 < tab_bar->Tabs.Size - sections[2].TabCount ? margin : 1.0f);
    tab_bar->ScrollingTargetDistToVisibility = 0.0f;
    if (tab_bar->ScrollingTarget > tab_x1 || (tab_x2 - tab_x1 >= scrollable_width))
    {
        // Scroll to the left
        tab_bar->ScrollingTargetDistToVisibility = UxImMax(tab_bar->ScrollingAnim - tab_x2, 0.0f);
        tab_bar->ScrollingTarget = tab_x1;
    }
    else if (tab_bar->ScrollingTarget < tab_x2 - scrollable_width)
    {
        // Scroll to the right
        tab_bar->ScrollingTargetDistToVisibility = UxImMax((tab_x1 - scrollable_width) - tab_bar->ScrollingAnim, 0.0f);
        tab_bar->ScrollingTarget = tab_x2 - scrollable_width;
    }
}

void UxImGui::TabBarQueueFocus(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab)
{
    tab_bar->NextSelectedTabId = tab->ID;
}

void UxImGui::TabBarQueueFocus(UxImGuiTabBar* tab_bar, const char* tab_name)
{
    IM_ASSERT((tab_bar->Flags & UxImGuiTabBarFlags_DockNode) == 0); // Only supported for manual/explicit tab bars
    UxImGuiID tab_id = TabBarCalcTabID(tab_bar, tab_name, NULL);
    tab_bar->NextSelectedTabId = tab_id;
}

void UxImGui::TabBarQueueReorder(UxImGuiTabBar* tab_bar, UxImGuiTabItem* tab, int offset)
{
    IM_ASSERT(offset != 0);
    IM_ASSERT(tab_bar->ReorderRequestTabId == 0);
    tab_bar->ReorderRequestTabId = tab->ID;
    tab_bar->ReorderRequestOffset = (UxImS16)offset;
}

void UxImGui::TabBarQueueReorderFromMousePos(UxImGuiTabBar* tab_bar, UxImGuiTabItem* src_tab, UxImVec2 mouse_pos)
{
    UxImGuiContext& g = *GUxImGui;
    IM_ASSERT(tab_bar->ReorderRequestTabId == 0);
    if ((tab_bar->Flags & UxImGuiTabBarFlags_Reorderable) == 0)
        return;

    const bool is_central_section = (src_tab->Flags & UxImGuiTabItemFlags_SectionMask_) == 0;
    const float bar_offset = tab_bar->BarRect.Min.x - (is_central_section ? tab_bar->ScrollingTarget : 0);

    // Count number of contiguous tabs we are crossing over
    const int dir = (bar_offset + src_tab->Offset) > mouse_pos.x ? -1 : +1;
    const int src_idx = tab_bar->Tabs.index_from_ptr(src_tab);
    int dst_idx = src_idx;
    for (int i = src_idx; i >= 0 && i < tab_bar->Tabs.Size; i += dir)
    {
        // Reordered tabs must share the same section
        const UxImGuiTabItem* dst_tab = &tab_bar->Tabs[i];
        if (dst_tab->Flags & UxImGuiTabItemFlags_NoReorder)
            break;
        if ((dst_tab->Flags & UxImGuiTabItemFlags_SectionMask_) != (src_tab->Flags & UxImGuiTabItemFlags_SectionMask_))
            break;
        dst_idx = i;

        // Include spacing after tab, so when mouse cursor is between tabs we would not continue checking further tabs that are not hovered.
        const float x1 = bar_offset + dst_tab->Offset - g.Style.ItemInnerSpacing.x;
        const float x2 = bar_offset + dst_tab->Offset + dst_tab->Width + g.Style.ItemInnerSpacing.x;
        //GetForegroundDrawList()->AddRect(UxImVec2(x1, tab_bar->BarRect.Min.y), UxImVec2(x2, tab_bar->BarRect.Max.y), IM_COL32(255, 0, 0, 255));
        if ((dir < 0 && mouse_pos.x > x1) || (dir > 0 && mouse_pos.x < x2))
            break;
    }

    if (dst_idx != src_idx)
        TabBarQueueReorder(tab_bar, src_tab, dst_idx - src_idx);
}

bool UxImGui::TabBarProcessReorder(UxImGuiTabBar* tab_bar)
{
    UxImGuiTabItem* tab1 = TabBarFindTabByID(tab_bar, tab_bar->ReorderRequestTabId);
    if (tab1 == NULL || (tab1->Flags & UxImGuiTabItemFlags_NoReorder))
        return false;

    //IM_ASSERT(tab_bar->Flags & UxImGuiTabBarFlags_Reorderable); // <- this may happen when using debug tools
    int tab2_order = TabBarGetTabOrder(tab_bar, tab1) + tab_bar->ReorderRequestOffset;
    if (tab2_order < 0 || tab2_order >= tab_bar->Tabs.Size)
        return false;

    // Reordered tabs must share the same section
    // (Note: TabBarQueueReorderFromMousePos() also has a similar test but since we allow direct calls to TabBarQueueReorder() we do it here too)
    UxImGuiTabItem* tab2 = &tab_bar->Tabs[tab2_order];
    if (tab2->Flags & UxImGuiTabItemFlags_NoReorder)
        return false;
    if ((tab1->Flags & UxImGuiTabItemFlags_SectionMask_) != (tab2->Flags & UxImGuiTabItemFlags_SectionMask_))
        return false;

    UxImGuiTabItem item_tmp = *tab1;
    UxImGuiTabItem* src_tab = (tab_bar->ReorderRequestOffset > 0) ? tab1 + 1 : tab2;
    UxImGuiTabItem* dst_tab = (tab_bar->ReorderRequestOffset > 0) ? tab1 : tab2 + 1;
    const int move_count = (tab_bar->ReorderRequestOffset > 0) ? tab_bar->ReorderRequestOffset : -tab_bar->ReorderRequestOffset;
    memmove(dst_tab, src_tab, move_count * sizeof(UxImGuiTabItem));
    *tab2 = item_tmp;

    if (tab_bar->Flags & UxImGuiTabBarFlags_SaveSettings)
        MarkIniSettingsDirty();
    return true;
}

static UxImGuiTabItem* UxImGui::TabBarScrollingButtons(UxImGuiTabBar* tab_bar)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    const UxImVec2 arrow_button_size(g.FontSize - 2.0f, g.FontSize + g.Style.FramePadding.y * 2.0f);
    const float scrolling_buttons_width = arrow_button_size.x * 2.0f;

    const UxImVec2 backup_cursor_pos = window->DC.CursorPos;
    //window->DrawList->AddRect(UxImVec2(tab_bar->BarRect.Max.x - scrolling_buttons_width, tab_bar->BarRect.Min.y), UxImVec2(tab_bar->BarRect.Max.x, tab_bar->BarRect.Max.y), IM_COL32(255,0,0,255));

    int select_dir = 0;
    UxImVec4 arrow_col = g.Style.Colors[UxImGuiCol_Text];
    arrow_col.w *= 0.5f;

    PushStyleColor(UxImGuiCol_Text, arrow_col);
    PushStyleColor(UxImGuiCol_Button, UxImVec4(0, 0, 0, 0));
    PushItemFlag(UxImGuiItemFlags_ButtonRepeat, true);
    const float backup_repeat_delay = g.IO.KeyRepeatDelay;
    const float backup_repeat_rate = g.IO.KeyRepeatRate;
    g.IO.KeyRepeatDelay = 0.250f;
    g.IO.KeyRepeatRate = 0.200f;
    float x = UxImMax(tab_bar->BarRect.Min.x, tab_bar->BarRect.Max.x - scrolling_buttons_width);
    window->DC.CursorPos = UxImVec2(x, tab_bar->BarRect.Min.y);
    if (ArrowButtonEx("##<", UxImGuiDir_Left, arrow_button_size, UxImGuiButtonFlags_PressedOnClick))
        select_dir = -1;
    window->DC.CursorPos = UxImVec2(x + arrow_button_size.x, tab_bar->BarRect.Min.y);
    if (ArrowButtonEx("##>", UxImGuiDir_Right, arrow_button_size, UxImGuiButtonFlags_PressedOnClick))
        select_dir = +1;
    PopItemFlag();
    PopStyleColor(2);
    g.IO.KeyRepeatRate = backup_repeat_rate;
    g.IO.KeyRepeatDelay = backup_repeat_delay;

    UxImGuiTabItem* tab_to_scroll_to = NULL;
    if (select_dir != 0)
        if (UxImGuiTabItem* tab_item = TabBarFindTabByID(tab_bar, tab_bar->SelectedTabId))
        {
            int selected_order = TabBarGetTabOrder(tab_bar, tab_item);
            int target_order = selected_order + select_dir;

            // Skip tab item buttons until another tab item is found or end is reached
            while (tab_to_scroll_to == NULL)
            {
                // If we are at the end of the list, still scroll to make our tab visible
                tab_to_scroll_to = &tab_bar->Tabs[(target_order >= 0 && target_order < tab_bar->Tabs.Size) ? target_order : selected_order];

                // Cross through buttons
                // (even if first/last item is a button, return it so we can update the scroll)
                if (tab_to_scroll_to->Flags & UxImGuiTabItemFlags_Button)
                {
                    target_order += select_dir;
                    selected_order += select_dir;
                    tab_to_scroll_to = (target_order < 0 || target_order >= tab_bar->Tabs.Size) ? tab_to_scroll_to : NULL;
                }
            }
        }
    window->DC.CursorPos = backup_cursor_pos;
    tab_bar->BarRect.Max.x -= scrolling_buttons_width + 1.0f;

    return tab_to_scroll_to;
}

static UxImGuiTabItem* UxImGui::TabBarTabListPopupButton(UxImGuiTabBar* tab_bar)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;

    // We use g.Style.FramePadding.y to match the square ArrowButton size
    const float tab_list_popup_button_width = g.FontSize + g.Style.FramePadding.y;
    const UxImVec2 backup_cursor_pos = window->DC.CursorPos;
    window->DC.CursorPos = UxImVec2(tab_bar->BarRect.Min.x - g.Style.FramePadding.y, tab_bar->BarRect.Min.y);
    tab_bar->BarRect.Min.x += tab_list_popup_button_width;

    UxImVec4 arrow_col = g.Style.Colors[UxImGuiCol_Text];
    arrow_col.w *= 0.5f;
    PushStyleColor(UxImGuiCol_Text, arrow_col);
    PushStyleColor(UxImGuiCol_Button, UxImVec4(0, 0, 0, 0));
    bool open = BeginCombo("##v", NULL, UxImGuiComboFlags_NoPreview | UxImGuiComboFlags_HeightLargest);
    PopStyleColor(2);

    UxImGuiTabItem* tab_to_select = NULL;
    if (open)
    {
        for (int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
        {
            UxImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
            if (tab->Flags & UxImGuiTabItemFlags_Button)
                continue;

            const char* tab_name = TabBarGetTabName(tab_bar, tab);
            if (Selectable(tab_name, tab_bar->SelectedTabId == tab->ID))
                tab_to_select = tab;
        }
        EndCombo();
    }

    window->DC.CursorPos = backup_cursor_pos;
    return tab_to_select;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: BeginTabItem, EndTabItem, etc.
//-------------------------------------------------------------------------
// - BeginTabItem()
// - EndTabItem()
// - TabItemButton()
// - TabItemEx() [Internal]
// - SetTabItemClosed()
// - TabItemCalcSize() [Internal]
// - TabItemBackground() [Internal]
// - TabItemLabelAndCloseButton() [Internal]
//-------------------------------------------------------------------------

bool    UxImGui::BeginTabItem(const char* label, bool* p_open, UxImGuiTabItemFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    UxImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return false;
    }
    IM_ASSERT((flags & UxImGuiTabItemFlags_Button) == 0); // BeginTabItem() Can't be used with button flags, use TabItemButton() instead!

    bool ret = TabItemEx(tab_bar, label, p_open, flags, NULL);
    if (ret && !(flags & UxImGuiTabItemFlags_NoPushId))
    {
        UxImGuiTabItem* tab = &tab_bar->Tabs[tab_bar->LastTabItemIdx];
        PushOverrideID(tab->ID); // We already hashed 'label' so push into the ID stack directly instead of doing another hash through PushID(label)
    }
    return ret;
}

void    UxImGui::EndTabItem()
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    UxImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return;
    }
    IM_ASSERT(tab_bar->LastTabItemIdx >= 0);
    UxImGuiTabItem* tab = &tab_bar->Tabs[tab_bar->LastTabItemIdx];
    if (!(tab->Flags & UxImGuiTabItemFlags_NoPushId))
        PopID();
}

bool    UxImGui::TabItemButton(const char* label, UxImGuiTabItemFlags flags)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    UxImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return false;
    }
    return TabItemEx(tab_bar, label, NULL, flags | UxImGuiTabItemFlags_Button | UxImGuiTabItemFlags_NoReorder, NULL);
}

void    UxImGui::TabItemSpacing(const char* str_id, UxImGuiTabItemFlags flags, float width)
{
    UxImGuiContext& g = *GUxImGui;
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    UxImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return;
    }
    SetNextItemWidth(width);
    TabItemEx(tab_bar, str_id, NULL, flags | UxImGuiTabItemFlags_Button | UxImGuiTabItemFlags_NoReorder | UxImGuiTabItemFlags_Invisible, NULL);
}

bool    UxImGui::TabItemEx(UxImGuiTabBar* tab_bar, const char* label, bool* p_open, UxImGuiTabItemFlags flags, UxImGuiWindow* docked_window)
{
    // Layout whole tab bar if not already done
    UxImGuiContext& g = *GUxImGui;
    if (tab_bar->WantLayout)
    {
        UxImGuiNextItemData backup_next_item_data = g.NextItemData;
        TabBarLayout(tab_bar);
        g.NextItemData = backup_next_item_data;
    }
    UxImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    const UxImGuiStyle& style = g.Style;
    const UxImGuiID id = TabBarCalcTabID(tab_bar, label, docked_window);

    // If the user called us with *p_open == false, we early out and don't render.
    // We make a call to ItemAdd() so that attempts to use a contextual popup menu with an implicit ID won't use an older ID.
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    if (p_open && !*p_open)
    {
        ItemAdd(UxImRect(), id, NULL, UxImGuiItemFlags_NoNav);
        return false;
    }

    IM_ASSERT(!p_open || !(flags & UxImGuiTabItemFlags_Button));
    IM_ASSERT((flags & (UxImGuiTabItemFlags_Leading | UxImGuiTabItemFlags_Trailing)) != (UxImGuiTabItemFlags_Leading | UxImGuiTabItemFlags_Trailing)); // Can't use both Leading and Trailing

    // Store into UxImGuiTabItemFlags_NoCloseButton, also honor UxImGuiTabItemFlags_NoCloseButton passed by user (although not documented)
    if (flags & UxImGuiTabItemFlags_NoCloseButton)
        p_open = NULL;
    else if (p_open == NULL)
        flags |= UxImGuiTabItemFlags_NoCloseButton;

    // Acquire tab data
    UxImGuiTabItem* tab = TabBarFindTabByID(tab_bar, id);
    bool tab_is_new = false;
    if (tab == NULL)
    {
        tab_bar->Tabs.push_back(UxImGuiTabItem());
        tab = &tab_bar->Tabs.back();
        tab->ID = id;
        tab_bar->TabsAddedNew = tab_is_new = true;
    }
    tab_bar->LastTabItemIdx = (UxImS16)tab_bar->Tabs.index_from_ptr(tab);

    // Calculate tab contents size
    UxImVec2 size = TabItemCalcSize(label, (p_open != NULL) || (flags & UxImGuiTabItemFlags_UnsavedDocument));
    tab->RequestedWidth = -1.0f;
    if (g.NextItemData.HasFlags & UxImGuiNextItemDataFlags_HasWidth)
        size.x = tab->RequestedWidth = g.NextItemData.Width;
    if (tab_is_new)
        tab->Width = UxImMax(1.0f, size.x);
    tab->ContentWidth = size.x;
    tab->BeginOrder = tab_bar->TabsActiveCount++;

    const bool tab_bar_appearing = (tab_bar->PrevFrameVisible + 1 < g.FrameCount);
    const bool tab_bar_focused = (tab_bar->Flags & UxImGuiTabBarFlags_IsFocused) != 0;
    const bool tab_appearing = (tab->LastFrameVisible + 1 < g.FrameCount);
    const bool tab_just_unsaved = (flags & UxImGuiTabItemFlags_UnsavedDocument) && !(tab->Flags & UxImGuiTabItemFlags_UnsavedDocument);
    const bool is_tab_button = (flags & UxImGuiTabItemFlags_Button) != 0;
    tab->LastFrameVisible = g.FrameCount;
    tab->Flags = flags;
    tab->Window = docked_window;

    // Append name _WITH_ the zero-terminator
    // (regular tabs are permitted in a DockNode tab bar, but window tabs not permitted in a non-DockNode tab bar)
    if (docked_window != NULL)
    {
        IM_ASSERT(tab_bar->Flags & UxImGuiTabBarFlags_DockNode);
        tab->NameOffset = -1;
    }
    else
    {
        tab->NameOffset = (UxImS32)tab_bar->TabsNames.size();
        tab_bar->TabsNames.append(label, label + UxImStrlen(label) + 1);
    }

    // Update selected tab
    if (!is_tab_button)
    {
        if (tab_appearing && (tab_bar->Flags & UxImGuiTabBarFlags_AutoSelectNewTabs) && tab_bar->NextSelectedTabId == 0)
            if (!tab_bar_appearing || tab_bar->SelectedTabId == 0)
                TabBarQueueFocus(tab_bar, tab); // New tabs gets activated
        if ((flags & UxImGuiTabItemFlags_SetSelected) && (tab_bar->SelectedTabId != id)) // _SetSelected can only be passed on explicit tab bar
            TabBarQueueFocus(tab_bar, tab);
    }

    // Lock visibility
    // (Note: tab_contents_visible != tab_selected... because CTRL+TAB operations may preview some tabs without selecting them!)
    bool tab_contents_visible = (tab_bar->VisibleTabId == id);
    if (tab_contents_visible)
        tab_bar->VisibleTabWasSubmitted = true;

    // On the very first frame of a tab bar we let first tab contents be visible to minimize appearing glitches
    if (!tab_contents_visible && tab_bar->SelectedTabId == 0 && tab_bar_appearing && docked_window == NULL)
        if (tab_bar->Tabs.Size == 1 && !(tab_bar->Flags & UxImGuiTabBarFlags_AutoSelectNewTabs))
            tab_contents_visible = true;

    // Note that tab_is_new is not necessarily the same as tab_appearing! When a tab bar stops being submitted
    // and then gets submitted again, the tabs will have 'tab_appearing=true' but 'tab_is_new=false'.
    if (tab_appearing && (!tab_bar_appearing || tab_is_new))
    {
        ItemAdd(UxImRect(), id, NULL, UxImGuiItemFlags_NoNav);
        if (is_tab_button)
            return false;
        return tab_contents_visible;
    }

    if (tab_bar->SelectedTabId == id)
        tab->LastFrameSelected = g.FrameCount;

    // Backup current layout position
    const UxImVec2 backup_main_cursor_pos = window->DC.CursorPos;

    // Layout
    const bool is_central_section = (tab->Flags & UxImGuiTabItemFlags_SectionMask_) == 0;
    size.x = tab->Width;
    if (is_central_section)
        window->DC.CursorPos = tab_bar->BarRect.Min + UxImVec2(IM_TRUNC(tab->Offset - tab_bar->ScrollingAnim), 0.0f);
    else
        window->DC.CursorPos = tab_bar->BarRect.Min + UxImVec2(tab->Offset, 0.0f);
    UxImVec2 pos = window->DC.CursorPos;
    UxImRect bb(pos, pos + size);

    // We don't have CPU clipping primitives to clip the CloseButton (until it becomes a texture), so need to add an extra draw call (temporary in the case of vertical animation)
    const bool want_clip_rect = is_central_section && (bb.Min.x < tab_bar->ScrollingRectMinX || bb.Max.x > tab_bar->ScrollingRectMaxX);
    if (want_clip_rect)
        PushClipRect(UxImVec2(UxImMax(bb.Min.x, tab_bar->ScrollingRectMinX), bb.Min.y - 1), UxImVec2(tab_bar->ScrollingRectMaxX, bb.Max.y), true);

    UxImVec2 backup_cursor_max_pos = window->DC.CursorMaxPos;
    ItemSize(bb.GetSize(), style.FramePadding.y);
    window->DC.CursorMaxPos = backup_cursor_max_pos;

    if (!ItemAdd(bb, id))
    {
        if (want_clip_rect)
            PopClipRect();
        window->DC.CursorPos = backup_main_cursor_pos;
        return tab_contents_visible;
    }

    // Click to Select a tab
    UxImGuiButtonFlags button_flags = ((is_tab_button ? UxImGuiButtonFlags_PressedOnClickRelease : UxImGuiButtonFlags_PressedOnClick) | UxImGuiButtonFlags_AllowOverlap);
    if (g.DragDropActive && !g.DragDropPayload.IsDataType(IMGUI_PAYLOAD_TYPE_WINDOW)) // FIXME: May be an opt-in property of the payload to disable this
        button_flags |= UxImGuiButtonFlags_PressedOnDragDropHold;
    bool hovered, held, pressed;
    if (flags & UxImGuiTabItemFlags_Invisible)
        hovered = held = pressed = false;
    else
        pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);
    if (pressed && !is_tab_button)
        TabBarQueueFocus(tab_bar, tab);

    // Transfer active id window so the active id is not owned by the dock host (as StartMouseMovingWindow()
    // will only do it on the drag). This allows FocusWindow() to be more conservative in how it clears active id.
    if (held && docked_window && g.ActiveId == id && g.ActiveIdIsJustActivated)
        g.ActiveIdWindow = docked_window;

    // Drag and drop a single floating window node moves it
    UxImGuiDockNode* node = docked_window ? docked_window->DockNode : NULL;
    const bool single_floating_window_node = node && node->IsFloatingNode() && (node->Windows.Size == 1);
    if (held && single_floating_window_node && IsMouseDragging(0, 0.0f))
    {
        // Move
        StartMouseMovingWindow(docked_window);
    }
    else if (held && !tab_appearing && IsMouseDragging(0))
    {
        // Drag and drop: re-order tabs
        int drag_dir = 0;
        float drag_distance_from_edge_x = 0.0f;
        if (!g.DragDropActive && ((tab_bar->Flags & UxImGuiTabBarFlags_Reorderable) || (docked_window != NULL)))
        {
            // While moving a tab it will jump on the other side of the mouse, so we also test for MouseDelta.x
            if (g.IO.MouseDelta.x < 0.0f && g.IO.MousePos.x < bb.Min.x)
            {
                drag_dir = -1;
                drag_distance_from_edge_x = bb.Min.x - g.IO.MousePos.x;
                TabBarQueueReorderFromMousePos(tab_bar, tab, g.IO.MousePos);
            }
            else if (g.IO.MouseDelta.x > 0.0f && g.IO.MousePos.x > bb.Max.x)
            {
                drag_dir = +1;
                drag_distance_from_edge_x = g.IO.MousePos.x - bb.Max.x;
                TabBarQueueReorderFromMousePos(tab_bar, tab, g.IO.MousePos);
            }
        }

        // Extract a Dockable window out of it's tab bar
        const bool can_undock = docked_window != NULL && !(docked_window->Flags & UxImGuiWindowFlags_NoMove) && !(node->MergedFlags & UxImGuiDockNodeFlags_NoUndocking);
        if (can_undock)
        {
            // We use a variable threshold to distinguish dragging tabs within a tab bar and extracting them out of the tab bar
            bool undocking_tab = (g.DragDropActive && g.DragDropPayload.SourceId == id);
            if (!undocking_tab) //&& (!g.IO.ConfigDockingWithShift || g.IO.KeyShift)
            {
                float threshold_base = g.FontSize;
                float threshold_x = (threshold_base * 2.2f);
                float threshold_y = (threshold_base * 1.5f) + UxImClamp((UxImFabs(g.IO.MouseDragMaxDistanceAbs[0].x) - threshold_base * 2.0f) * 0.20f, 0.0f, threshold_base * 4.0f);
                //GetForegroundDrawList()->AddRect(UxImVec2(bb.Min.x - threshold_x, bb.Min.y - threshold_y), UxImVec2(bb.Max.x + threshold_x, bb.Max.y + threshold_y), IM_COL32_WHITE); // [DEBUG]

                float distance_from_edge_y = UxImMax(bb.Min.y - g.IO.MousePos.y, g.IO.MousePos.y - bb.Max.y);
                if (distance_from_edge_y >= threshold_y)
                    undocking_tab = true;
                if (drag_distance_from_edge_x > threshold_x)
                    if ((drag_dir < 0 && TabBarGetTabOrder(tab_bar, tab) == 0) || (drag_dir > 0 && TabBarGetTabOrder(tab_bar, tab) == tab_bar->Tabs.Size - 1))
                        undocking_tab = true;
            }

            if (undocking_tab)
            {
                // Undock
                // FIXME: refactor to share more code with e.g. StartMouseMovingWindow
                DockContextQueueUndockWindow(&g, docked_window);
                g.MovingWindow = docked_window;
                SetActiveID(g.MovingWindow->MoveId, g.MovingWindow);
                g.ActiveIdClickOffset -= g.MovingWindow->Pos - bb.Min;
                g.ActiveIdNoClearOnFocusLoss = true;
                SetActiveIdUsingAllKeyboardKeys();
            }
        }
    }

#if 0
    if (hovered && g.HoveredIdNotActiveTimer > TOOLTIP_DELAY && bb.GetWidth() < tab->ContentWidth)
    {
        // Enlarge tab display when hovering
        bb.Max.x = bb.Min.x + IM_TRUNC(UxImLerp(bb.GetWidth(), tab->ContentWidth, UxImSaturate((g.HoveredIdNotActiveTimer - 0.40f) * 6.0f)));
        display_draw_list = GetForegroundDrawList(window);
        TabItemBackground(display_draw_list, bb, flags, GetColorU32(UxImGuiCol_TitleBgActive));
    }
#endif

    // Render tab shape
    const bool is_visible = (g.LastItemData.StatusFlags & UxImGuiItemStatusFlags_Visible) && !(flags & UxImGuiTabItemFlags_Invisible);
    if (is_visible)
    {
        UxImDrawList* display_draw_list = window->DrawList;
        const UxImU32 tab_col = GetColorU32((held || hovered) ? UxImGuiCol_TabHovered : tab_contents_visible ? (tab_bar_focused ? UxImGuiCol_TabSelected : UxImGuiCol_TabDimmedSelected) : (tab_bar_focused ? UxImGuiCol_Tab : UxImGuiCol_TabDimmed));
        TabItemBackground(display_draw_list, bb, flags, tab_col);
        if (tab_contents_visible && (tab_bar->Flags & UxImGuiTabBarFlags_DrawSelectedOverline) && style.TabBarOverlineSize > 0.0f)
        {
            // Might be moved to TabItemBackground() ?
            UxImVec2 tl = bb.GetTL() + UxImVec2(0, 1.0f * g.CurrentDpiScale);
            UxImVec2 tr = bb.GetTR() + UxImVec2(0, 1.0f * g.CurrentDpiScale);
            UxImU32 overline_col = GetColorU32(tab_bar_focused ? UxImGuiCol_TabSelectedOverline : UxImGuiCol_TabDimmedSelectedOverline);
            if (style.TabRounding > 0.0f)
            {
                float rounding = style.TabRounding;
                display_draw_list->PathArcToFast(tl + UxImVec2(+rounding, +rounding), rounding, 7, 9);
                display_draw_list->PathArcToFast(tr + UxImVec2(-rounding, +rounding), rounding, 9, 11);
                display_draw_list->PathStroke(overline_col, 0, style.TabBarOverlineSize);
            }
            else
            {
                display_draw_list->AddLine(tl - UxImVec2(0.5f, 0.5f), tr - UxImVec2(0.5f, 0.5f), overline_col, style.TabBarOverlineSize);
            }
        }
        RenderNavCursor(bb, id);

        // Select with right mouse button. This is so the common idiom for context menu automatically highlight the current widget.
        const bool hovered_unblocked = IsItemHovered(UxImGuiHoveredFlags_AllowWhenBlockedByPopup);
        if (tab_bar->SelectedTabId != tab->ID && hovered_unblocked && (IsMouseClicked(1) || IsMouseReleased(1)) && !is_tab_button)
            TabBarQueueFocus(tab_bar, tab);

        if (tab_bar->Flags & UxImGuiTabBarFlags_NoCloseWithMiddleMouseButton)
            flags |= UxImGuiTabItemFlags_NoCloseWithMiddleMouseButton;

        // Render tab label, process close button
        const UxImGuiID close_button_id = p_open ? GetIDWithSeed("#CLOSE", NULL, docked_window ? docked_window->ID : id) : 0;
        bool just_closed;
        bool text_clipped;
        TabItemLabelAndCloseButton(display_draw_list, bb, tab_just_unsaved ? (flags & ~UxImGuiTabItemFlags_UnsavedDocument) : flags, tab_bar->FramePadding, label, id, close_button_id, tab_contents_visible, &just_closed, &text_clipped);
        if (just_closed && p_open != NULL)
        {
            *p_open = false;
            TabBarCloseTab(tab_bar, tab);
        }

        // Forward Hovered state so IsItemHovered() after Begin() can work (even though we are technically hovering our parent)
        // That state is copied to window->DockTabItemStatusFlags by our caller.
        if (docked_window && (hovered || g.HoveredId == close_button_id))
            g.LastItemData.StatusFlags |= UxImGuiItemStatusFlags_HoveredWindow;

        // Tooltip
        // (Won't work over the close button because ItemOverlap systems messes up with HoveredIdTimer-> seems ok)
        // (We test IsItemHovered() to discard e.g. when another item is active or drag and drop over the tab bar, which g.HoveredId ignores)
        // FIXME: This is a mess.
        // FIXME: We may want disabled tab to still display the tooltip?
        if (text_clipped && g.HoveredId == id && !held)
            if (!(tab_bar->Flags & UxImGuiTabBarFlags_NoTooltip) && !(tab->Flags & UxImGuiTabItemFlags_NoTooltip))
                SetItemTooltip("%.*s", (int)(FindRenderedTextEnd(label) - label), label);
    }

    // Restore main window position so user can draw there
    if (want_clip_rect)
        PopClipRect();
    window->DC.CursorPos = backup_main_cursor_pos;

    IM_ASSERT(!is_tab_button || !(tab_bar->SelectedTabId == tab->ID && is_tab_button)); // TabItemButton should not be selected
    if (is_tab_button)
        return pressed;
    return tab_contents_visible;
}

// [Public] This is call is 100% optional but it allows to remove some one-frame glitches when a tab has been unexpectedly removed.
// To use it to need to call the function SetTabItemClosed() between BeginTabBar() and EndTabBar().
// Tabs closed by the close button will automatically be flagged to avoid this issue.
void    UxImGui::SetTabItemClosed(const char* label)
{
    UxImGuiContext& g = *GUxImGui;
    bool is_within_manual_tab_bar = g.CurrentTabBar && !(g.CurrentTabBar->Flags & UxImGuiTabBarFlags_DockNode);
    if (is_within_manual_tab_bar)
    {
        UxImGuiTabBar* tab_bar = g.CurrentTabBar;
        UxImGuiID tab_id = TabBarCalcTabID(tab_bar, label, NULL);
        if (UxImGuiTabItem* tab = TabBarFindTabByID(tab_bar, tab_id))
            tab->WantClose = true; // Will be processed by next call to TabBarLayout()
    }
    else if (UxImGuiWindow* window = FindWindowByName(label))
    {
        if (window->DockIsActive)
            if (UxImGuiDockNode* node = window->DockNode)
            {
                UxImGuiID tab_id = TabBarCalcTabID(node->TabBar, label, window);
                TabBarRemoveTab(node->TabBar, tab_id);
                window->DockTabWantClose = true;
            }
    }
}

UxImVec2 UxImGui::TabItemCalcSize(const char* label, bool has_close_button_or_unsaved_marker)
{
    UxImGuiContext& g = *GUxImGui;
    UxImVec2 label_size = CalcTextSize(label, NULL, true);
    UxImVec2 size = UxImVec2(label_size.x + g.Style.FramePadding.x, label_size.y + g.Style.FramePadding.y * 2.0f);
    if (has_close_button_or_unsaved_marker)
        size.x += g.Style.FramePadding.x + (g.Style.ItemInnerSpacing.x + g.FontSize); // We use Y intentionally to fit the close button circle.
    else
        size.x += g.Style.FramePadding.x + 1.0f;
    return UxImVec2(UxImMin(size.x, TabBarCalcMaxTabWidth()), size.y);
}

UxImVec2 UxImGui::TabItemCalcSize(UxImGuiWindow* window)
{
    return TabItemCalcSize(window->Name, window->HasCloseButton || (window->Flags & UxImGuiWindowFlags_UnsavedDocument));
}

void UxImGui::TabItemBackground(UxImDrawList* draw_list, const UxImRect& bb, UxImGuiTabItemFlags flags, UxImU32 col)
{
    // While rendering tabs, we trim 1 pixel off the top of our bounding box so they can fit within a regular frame height while looking "detached" from it.
    UxImGuiContext& g = *GUxImGui;
    const float width = bb.GetWidth();
    IM_UNUSED(flags);
    IM_ASSERT(width > 0.0f);
    const float rounding = UxImMax(0.0f, UxImMin((flags & UxImGuiTabItemFlags_Button) ? g.Style.FrameRounding : g.Style.TabRounding, width * 0.5f - 1.0f));
    const float y1 = bb.Min.y + 1.0f;
    const float y2 = bb.Max.y - g.Style.TabBarBorderSize;
    draw_list->PathLineTo(UxImVec2(bb.Min.x, y2));
    draw_list->PathArcToFast(UxImVec2(bb.Min.x + rounding, y1 + rounding), rounding, 6, 9);
    draw_list->PathArcToFast(UxImVec2(bb.Max.x - rounding, y1 + rounding), rounding, 9, 12);
    draw_list->PathLineTo(UxImVec2(bb.Max.x, y2));
    draw_list->PathFillConvex(col);
    if (g.Style.TabBorderSize > 0.0f)
    {
        draw_list->PathLineTo(UxImVec2(bb.Min.x + 0.5f, y2));
        draw_list->PathArcToFast(UxImVec2(bb.Min.x + rounding + 0.5f, y1 + rounding + 0.5f), rounding, 6, 9);
        draw_list->PathArcToFast(UxImVec2(bb.Max.x - rounding - 0.5f, y1 + rounding + 0.5f), rounding, 9, 12);
        draw_list->PathLineTo(UxImVec2(bb.Max.x - 0.5f, y2));
        draw_list->PathStroke(GetColorU32(UxImGuiCol_Border), 0, g.Style.TabBorderSize);
    }
}

// Render text label (with custom clipping) + Unsaved Document marker + Close Button logic
// We tend to lock style.FramePadding for a given tab-bar, hence the 'frame_padding' parameter.
void UxImGui::TabItemLabelAndCloseButton(UxImDrawList* draw_list, const UxImRect& bb, UxImGuiTabItemFlags flags, UxImVec2 frame_padding, const char* label, UxImGuiID tab_id, UxImGuiID close_button_id, bool is_contents_visible, bool* out_just_closed, bool* out_text_clipped)
{
    UxImGuiContext& g = *GUxImGui;
    UxImVec2 label_size = CalcTextSize(label, NULL, true);

    if (out_just_closed)
        *out_just_closed = false;
    if (out_text_clipped)
        *out_text_clipped = false;

    if (bb.GetWidth() <= 1.0f)
        return;

    // In Style V2 we'll have full override of all colors per state (e.g. focused, selected)
    // But right now if you want to alter text color of tabs this is what you need to do.
#if 0
    const float backup_alpha = g.Style.Alpha;
    if (!is_contents_visible)
        g.Style.Alpha *= 0.7f;
#endif

    // Render text label (with clipping + alpha gradient) + unsaved marker
    UxImRect text_ellipsis_clip_bb(bb.Min.x + frame_padding.x, bb.Min.y + frame_padding.y, bb.Max.x - frame_padding.x, bb.Max.y);

    // Return clipped state ignoring the close button
    if (out_text_clipped)
    {
        *out_text_clipped = (text_ellipsis_clip_bb.Min.x + label_size.x) > text_ellipsis_clip_bb.Max.x;
        //draw_list->AddCircle(text_ellipsis_clip_bb.Min, 3.0f, *out_text_clipped ? IM_COL32(255, 0, 0, 255) : IM_COL32(0, 255, 0, 255));
    }

    const float button_sz = g.FontSize;
    const UxImVec2 button_pos(UxImMax(bb.Min.x, bb.Max.x - frame_padding.x - button_sz), bb.Min.y + frame_padding.y);

    // Close Button & Unsaved Marker
    // We are relying on a subtle and confusing distinction between 'hovered' and 'g.HoveredId' which happens because we are using UxImGuiButtonFlags_AllowOverlapMode + SetItemAllowOverlap()
    //  'hovered' will be true when hovering the Tab but NOT when hovering the close button
    //  'g.HoveredId==id' will be true when hovering the Tab including when hovering the close button
    //  'g.ActiveId==close_button_id' will be true when we are holding on the close button, in which case both hovered booleans are false
    bool close_button_pressed = false;
    bool close_button_visible = false;
    bool is_hovered = g.HoveredId == tab_id || g.HoveredId == close_button_id || g.ActiveId == tab_id || g.ActiveId == close_button_id; // Any interaction account for this too.

    if (close_button_id != 0)
    {
        if (is_contents_visible)
            close_button_visible = (g.Style.TabCloseButtonMinWidthSelected < 0.0f) ? true : (is_hovered && bb.GetWidth() >= UxImMax(button_sz, g.Style.TabCloseButtonMinWidthSelected));
        else
            close_button_visible = (g.Style.TabCloseButtonMinWidthUnselected < 0.0f) ? true : (is_hovered && bb.GetWidth() >= UxImMax(button_sz, g.Style.TabCloseButtonMinWidthUnselected));
    }

    // When tabs/document is unsaved, the unsaved marker takes priority over the close button.
    const bool unsaved_marker_visible = (flags & UxImGuiTabItemFlags_UnsavedDocument) != 0 && (button_pos.x + button_sz <= bb.Max.x) && (!close_button_visible || !is_hovered);
    if (unsaved_marker_visible)
    {
        const UxImRect bullet_bb(button_pos, button_pos + UxImVec2(button_sz, button_sz));
        RenderBullet(draw_list, bullet_bb.GetCenter(), GetColorU32(UxImGuiCol_Text));
    }
    else if (close_button_visible)
    {
        UxImGuiLastItemData last_item_backup = g.LastItemData;
        if (CloseButton(close_button_id, button_pos))
            close_button_pressed = true;
        g.LastItemData = last_item_backup;

        // Close with middle mouse button
        if (is_hovered && !(flags & UxImGuiTabItemFlags_NoCloseWithMiddleMouseButton) && IsMouseClicked(2))
            close_button_pressed = true;
    }

    // This is all rather complicated
    // (the main idea is that because the close button only appears on hover, we don't want it to alter the ellipsis position)
    // FIXME: if FramePadding is noticeably large, ellipsis_max_x will be wrong here (e.g. #3497), maybe for consistency that parameter of RenderTextEllipsis() shouldn't exist..
    float ellipsis_max_x = text_ellipsis_clip_bb.Max.x;
    if (close_button_visible || unsaved_marker_visible)
    {
        const bool visible_without_hover = unsaved_marker_visible || (is_contents_visible ? g.Style.TabCloseButtonMinWidthSelected : g.Style.TabCloseButtonMinWidthUnselected) < 0.0f;
        if (visible_without_hover)
        {
            text_ellipsis_clip_bb.Max.x -= button_sz * 0.90f;
            ellipsis_max_x -= button_sz * 0.90f;
        }
        else
        {
            text_ellipsis_clip_bb.Max.x -= button_sz * 1.00f;
        }
    }
    LogSetNextTextDecoration("/", "\\");
    RenderTextEllipsis(draw_list, text_ellipsis_clip_bb.Min, text_ellipsis_clip_bb.Max, ellipsis_max_x, label, NULL, &label_size);

#if 0
    if (!is_contents_visible)
        g.Style.Alpha = backup_alpha;
#endif

    if (out_just_closed)
        *out_just_closed = close_button_pressed;
}


#endif // #ifndef IMGUI_DISABLE


