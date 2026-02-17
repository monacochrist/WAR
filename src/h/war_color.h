//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_color.h
//-----------------------------------------------------------------------------

#ifndef WAR_COLOR_H
#define WAR_COLOR_H

#ifndef WAR_COLOR_H_VERSION
#define WAR_COLOR_H_VERSION 0
#endif // WAR_COLOR_H_VERSION

#include "war_data.h"

// sets defaults, no need to call during override since it's called at init
static inline void war_color_default(war_color_context* color) {
    color->version = WAR_COLOR_H_VERSION;
    //
    color->top_status_bar = 0x504944FF;        // light gray
    color->middle_status_bar = 0x282828FF;     // dark gray
    color->bottom_status_bar = 0xDE0000FF;     // red
    color->top_status_bar_cursor = 0xEBDAB0FF; // tan
    color->middle_status_bar_cursor = 0xEBDAB0FF;
    color->bottom_status_bar_cursor = 0xEBDAB0FF;
    color->top_status_bar_text = 0xEBDAB0FF;
    color->middle_status_bar_text = 0xEBDAB0FF;
    color->bottom_status_bar_text = 0xFFFFFFFF;
    color->top_status_bar_line = 0xEBDAB0FF;
    color->middle_status_bar_line = 0xEBDAB0FF;
    color->bottom_status_bar_line = 0xFFFFFFFF; // full white
    color->top_status_bar_text_foreground = 0x504944FF;
    color->middle_status_bar_text_foreground = 0x282828FF;
    color->bottom_status_bar_text_foreground = 0xDE0000FF;
    color->explore_header_text = 0xEBDAB0FF;
    color->explore_header_text_foreground = 0x282828FF;
    color->explore_text = 0xEBDAB0FF;
    color->explore_text_foreground = 0x282828FF;
    color->explore_editable_text = 0xEBDAB0FF;
    color->explore_editable_text_foreground = 0x282828FF;
    color->explore_line = 0xEBDAB0FF;
    color->explore_directory_text = 0xEBDAB0FF;
    color->explore_directory_text_foreground = 0x282828FF;
    color->explore_cursor = 0xEBDAB0FF;
    color->warpoon_text = 0xEBDAB0FF;
    color->warpoon_text_foreground = 0x504944FF;
    color->warpoon_outline = 0xEBDAB0FF;
    color->warpoon_background = 0x504944FF;
    color->warpoon_gutter = 0x504944FF;
    color->warpoon_gutter_text = 0xEBDAB0FF;
    color->warpoon_line = 0xEBDAB0FF;
    color->warpoon_cursor = 0xEBDAB0FF;
    color->preview_outline = 0xEBDAB0FF;
    color->preview_text = 0xEBDAB0FF;
    color->preview_background = 0x504944FF;
    color->preview_line = 0xEBDAB0FF;
    color->preview_cursor = 0xEBDAB0FF;
    color->mode_text = 0xDE0000FF;
    color->error = 0xDE0000FF;
    color->error_text = 0x282828FF;
    color->background = 0x282828FF;
    color->line = 0xEBDAB0FF;
    color->line_foreground = 0x282828FF;
    color->line_stressed_1 = 0xDE0000FF;
    color->line_stressed_2 = 0xFF8B00FF; // orange
    color->line_stressed_3 = 0xFFDD00FF; // yellow
    color->line_stressed_4 = 0x6FFF00FF; // green
    color->line_stressed_1_foreground = 0x282828FF;
    color->line_stressed_2_foreground = 0x282828FF;
    color->line_stressed_3_foreground = 0x282828FF;
    color->line_stressed_4_foreground = 0x282828FF;
    color->layer_none = 0xEBDAB0FF;
    color->layer_1 = 0xDE0000FF;
    color->layer_2 = 0xFF8B00FF; // orange
    color->layer_3 = 0xFFDD00FF; // yellow
    color->layer_4 = 0x6FFF00FF; // green
    color->layer_5 = 0x006AFFFF; // blue
    color->layer_6 = 0xBB00FFFF; // purple
    color->layer_7 = 0xFF00B7FF; // pink
    color->layer_8 = 0x00FFFBFF; // neon blue
    color->layer_9 = 0xAEFF00FF; // lime
    color->layer_multiple = 0xFFFFFFFF;
    color->layer_none_foreground = 0xFFFDDEFF;     // light tan
    color->layer_1_foreground = 0xFFBFBFFF;        // light red
    color->layer_2_foreground = 0xFFEBBFFF;        // light orange
    color->layer_3_foreground = 0xFFFDBFFF;        // light yellow
    color->layer_4_foreground = 0xC4FFBFFF;        // light green
    color->layer_5_foreground = 0xBFCEFFFF;        // light blue
    color->layer_6_foreground = 0xEABFFFFF;        // light purple
    color->layer_7_foreground = 0xFFBFFCFF;        // light pink
    color->layer_8_foreground = 0xD4F5FFFF;        // light neon blue
    color->layer_9_foreground = 0xF7FFD4FF;        // light lime
    color->layer_multiple_foreground = 0xB8B8B8FF; // light gray
    color->layer_none_outline = 0xEBDAB0FF;
    color->layer_1_outline = 0xEBDAB0FF;
    color->layer_2_outline = 0xEBDAB0FF;
    color->layer_3_outline = 0xEBDAB0FF;
    color->layer_4_outline = 0xEBDAB0FF;
    color->layer_5_outline = 0xEBDAB0FF;
    color->layer_6_outline = 0xEBDAB0FF;
    color->layer_7_outline = 0xEBDAB0FF;
    color->layer_8_outline = 0xEBDAB0FF;
    color->layer_9_outline = 0xEBDAB0FF;
    color->layer_multiple_outline = 0xEBDAB0FF;
    color->layer_none_outline_foreground = 0x282828FF;
    color->layer_1_outline_foreground = 0x282828FF;
    color->layer_2_outline_foreground = 0x282828FF;
    color->layer_3_outline_foreground = 0x282828FF;
    color->layer_4_outline_foreground = 0x282828FF;
    color->layer_5_outline_foreground = 0x282828FF;
    color->layer_6_outline_foreground = 0x282828FF;
    color->layer_7_outline_foreground = 0x282828FF;
    color->layer_8_outline_foreground = 0x282828FF;
    color->layer_9_outline_foreground = 0x282828FF;
    color->layer_multiple_outline_foreground = 0x282828FF;
    color->layer_none_text = 0x000000FF; // black
    color->layer_1_text = 0x000000FF;
    color->layer_2_text = 0x000000FF;
    color->layer_3_text = 0x000000FF;
    color->layer_4_text = 0x000000FF;
    color->layer_5_text = 0x000000FF;
    color->layer_6_text = 0x000000FF;
    color->layer_7_text = 0x000000FF;
    color->layer_8_text = 0x000000FF;
    color->layer_9_text = 0x000000FF;
    color->layer_multiple_text = 0x000000FF;
    color->layer_none_text_foreground = 0x000000FF;
    color->layer_1_text_foreground = 0x000000FF;
    color->layer_2_text_foreground = 0x000000FF;
    color->layer_3_text_foreground = 0x000000FF;
    color->layer_4_text_foreground = 0x000000FF;
    color->layer_5_text_foreground = 0x000000FF;
    color->layer_6_text_foreground = 0x000000FF;
    color->layer_7_text_foreground = 0x000000FF;
    color->layer_8_text_foreground = 0x000000FF;
    color->layer_9_text_foreground = 0x000000FF;
    color->layer_multiple_text_foreground = 0x000000FF;
    color->audio_line = 0xEBDAB0FF;
    color->audio_background = 0x282828FF;
    color->audio_cursor = 0xEBDAB0FF;
    color->video_cursor = 0xEBDAB0FF;
    color->gutter = 0x282828FF;
    color->gutter_text = 0x504944FF;
    color->gutter_text_foreground = 0x000000FF;
    color->gutter_line = 0x504944FF;
    color->piano_white_key = 0xFFFFFFFF;
    color->piano_white_key_text = 0x000000FF;
    color->piano_black_key = 0x000000FF;
    color->piano_line = 0x504944FF;
    color->playhead = 0xDE0000FF;
    color->playhead_outline = 0x000000FF;
}

void war_color_override(war_color_context* color);

#endif // WAR_COLOR_H
