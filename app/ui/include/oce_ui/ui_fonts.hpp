#pragma once
// Shared font faces loaded once at startup. Null until load_fonts() runs; the
// renderer falls back to the default font when a face is unavailable.

struct ImFont;

namespace oce::ui {

extern ImFont* g_body_font;       // serif body text
extern ImFont* g_bold_font;       // serif bold (markdown **bold**)
extern ImFont* g_italic_font;     // serif italic (markdown *italic*)
extern ImFont* g_bolditalic_font; // serif bold-italic (***both***)
extern ImFont* g_heading_font;    // larger serif for titles / the stat-bar name

// Loads the font faces into the ImGui atlas. Call once after the ImGui context
// exists and before the first frame.
void load_fonts();

} // namespace oce::ui
