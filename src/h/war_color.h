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

#include <stdint.h>

// sets defaults, no need to call during override since it's called at init
static inline void war_color_default(war_color_context* color) {
    color->version = WAR_COLOR_H_VERSION;
    //
    color->top_status_bar = 0;
    color->middle_status_bar = 0;
    color->bottom_status_bar = 0;
    color->top_status_bar_cursor = 0;
    color->middle_status_bar_cursor = 0;
    color->bottom_status_bar_cursor = 0;
    color->top_status_bar_text = 0;
    color->middle_status_bar_text = 0;
    color->bottom_status_bar_text = 0;
    color->top_status_bar_line = 0;
    color->middle_status_bar_line = 0;
    color->bottom_status_bar_line = 0;
    color->top_status_bar_text_foreground = 0;
    color->middle_status_bar_text_foreground = 0;
    color->bottom_status_bar_text_foreground = 0;
    color->explore_header_text = 0;
    color->explore_header_text_foreground = 0;
    color->explore_text = 0;
    color->explore_text_foreground = 0;
    color->explore_editable_text = 0;
    color->explore_editable_text_foreground = 0;
    color->explore_line = 0;
    color->explore_directory_text = 0;
    color->explore_directory_text_foreground = 0;
    color->explore_cursor = 0;
    color->warpoon_text = 0;
    color->warpoon_text_foreground = 0;
    color->warpoon_outline = 0;
    color->warpoon_background = 0;
    color->warpoon_gutter = 0;
    color->warpoon_gutter_text = 0;
    color->warpoon_line = 0;
    color->warpoon_cursor = 0;
    color->preview_outline = 0;
    color->preview_text = 0;
    color->preview_background = 0;
    color->preview_line = 0;
    color->preview_cursor = 0;
    color->mode_text = 0;
    color->error = 0;
    color->error_text = 0;
    color->background = 0;
    color->line = 0;
    color->line_foreground = 0;
    color->line_stressed_1 = 0;
    color->line_stressed_2 = 0;
    color->line_stressed_3 = 0;
    color->line_stressed_4 = 0;
    color->line_stressed_1_foreground = 0;
    color->line_stressed_2_foreground = 0;
    color->line_stressed_3_foreground = 0;
    color->line_stressed_4_foreground = 0;
    color->layer_none = 0;
    color->layer_1 = 0;
    color->layer_2 = 0;
    color->layer_3 = 0;
    color->layer_4 = 0;
    color->layer_5 = 0;
    color->layer_6 = 0;
    color->layer_7 = 0;
    color->layer_8 = 0;
    color->layer_9 = 0;
    color->layer_multiple = 0;
    color->layer_none_foreground = 0;
    color->layer_1_foreground = 0;
    color->layer_2_foreground = 0;
    color->layer_3_foreground = 0;
    color->layer_4_foreground = 0;
    color->layer_5_foreground = 0;
    color->layer_6_foreground = 0;
    color->layer_7_foreground = 0;
    color->layer_8_foreground = 0;
    color->layer_9_foreground = 0;
    color->layer_multiple_foreground = 0;
    color->layer_none_outline = 0;
    color->layer_1_outline = 0;
    color->layer_2_outline = 0;
    color->layer_3_outline = 0;
    color->layer_4_outline = 0;
    color->layer_5_outline = 0;
    color->layer_6_outline = 0;
    color->layer_7_outline = 0;
    color->layer_8_outline = 0;
    color->layer_9_outline = 0;
    color->layer_multiple_outline = 0;
    color->layer_none_outline_foreground = 0;
    color->layer_1_outline_foreground = 0;
    color->layer_2_outline_foreground = 0;
    color->layer_3_outline_foreground = 0;
    color->layer_4_outline_foreground = 0;
    color->layer_5_outline_foreground = 0;
    color->layer_6_outline_foreground = 0;
    color->layer_7_outline_foreground = 0;
    color->layer_8_outline_foreground = 0;
    color->layer_9_outline_foreground = 0;
    color->layer_multiple_outline_foreground = 0;
    color->layer_none_text = 0;
    color->layer_1_text = 0;
    color->layer_2_text = 0;
    color->layer_3_text = 0;
    color->layer_4_text = 0;
    color->layer_5_text = 0;
    color->layer_6_text = 0;
    color->layer_7_text = 0;
    color->layer_8_text = 0;
    color->layer_9_text = 0;
    color->layer_multiple_text = 0;
    color->layer_none_text_foreground = 0;
    color->layer_1_text_foreground = 0;
    color->layer_2_text_foreground = 0;
    color->layer_3_text_foreground = 0;
    color->layer_4_text_foreground = 0;
    color->layer_5_text_foreground = 0;
    color->layer_6_text_foreground = 0;
    color->layer_7_text_foreground = 0;
    color->layer_8_text_foreground = 0;
    color->layer_9_text_foreground = 0;
    color->layer_multiple_text_foreground = 0;
    color->audio_line = 0;
    color->audio_background = 0;
    color->audio_cursor = 0;
    color->video_cursor = 0;
    color->gutter = 0;
    color->gutter_text = 0;
    color->gutter_text_foreground = 0;
    color->gutter_line = 0;
    color->piano_white_key = 0;
    color->piano_white_key_text = 0;
    color->piano_black_key = 0;
    color->piano_line = 0;
    color->playhead = 0;
}

void war_color_override(war_color_context* color);

#endif // WAR_COLOR_H
