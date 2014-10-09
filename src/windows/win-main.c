#include <pebble.h>
#include <pebble-assist.h>
#include <bitmap-loader.h>
#include "win-timer-add.h"
#include "../timers.h"
#include "../libs/strap/strap.h"

#define MENU_SECTION_TIMERS  0
#define MENU_SECTION_OTHER   1

#define MENU_ROW_COUNT_OTHER 2

#define MENU_ROW_OTHER_ADD_TIMER     0
#define MENU_ROW_OTHER_ADD_STOPWATCH 1

GRect icon_rects[7];
#define ICON_RECT_PLAY      0
#define ICON_RECT_PAUSE     1
#define ICON_RECT_STOP      2
#define ICON_RECT_DONE      3
#define ICON_RECT_TIMER     4
#define ICON_RECT_STOPWATCH 5
#define ICON_RECT_ADD       6

static void window_load(Window* window);
static void window_unload(Window* window);
static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context);
static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context);
static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context);
static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context);
static void menu_draw_row_timers(GContext* ctx, const Layer* cell_layer, uint16_t row_index);
static void menu_draw_row_other(GContext* ctx, const Layer* cell_layer, uint16_t row_index);
static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context);
static void menu_select_timers(uint16_t row_index);
static void menu_select_other(uint16_t row_index);

static void timers_update_handler(void);

static Window*    s_window;
static MenuLayer* s_menu;

void win_main_init(void) {
  icon_rects[ICON_RECT_PLAY] =      GRect( 0,  0, 16, 16);
  icon_rects[ICON_RECT_PAUSE] =     GRect(16,  0, 16, 16);
  icon_rects[ICON_RECT_STOP] =      GRect(32,  0, 16, 16);
  icon_rects[ICON_RECT_DONE] =      GRect(16, 16, 16, 16);
  icon_rects[ICON_RECT_TIMER] =     GRect( 0, 16,  8, 16);
  icon_rects[ICON_RECT_STOPWATCH] = GRect( 8, 16,  8, 16);
  icon_rects[ICON_RECT_ADD] =       GRect(48, 16, 16, 16);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  timers_register_update_handler(timers_update_handler);
  win_timer_add_init();
}

void win_main_show(void) {
  window_stack_push(s_window, false);
}

static void window_load(Window* window) {
  s_menu = menu_layer_create_fullscreen(s_window);
  menu_layer_set_click_config_onto_window(s_menu, s_window);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_num_sections,
    .get_num_rows = menu_num_rows,
    .get_cell_height = menu_cell_height,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
  });
  menu_layer_add_to_window(s_menu, s_window);
}

static void window_unload(Window* window) {

}

static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context) {
  return 2;
}

static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context) {
  switch (section_index) {
    case MENU_SECTION_TIMERS:
      return timers_count();
    case MENU_SECTION_OTHER:
      return MENU_ROW_COUNT_OTHER;
    default:
      return 0;
  }
}

static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context) {
  switch (cell_index->section) {
    case MENU_SECTION_TIMERS: {
      Timer* timer = timers_get(cell_index->row);
      if (! timer) {
        return 36;
      }
      switch (timer->type) {
        case TIMER_TYPE_TIMER:
          return 34;
        case TIMER_TYPE_STOPWATCH:
          return 32;
      }
      break;
    }
  }
  return 36;
}

static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context) {
  switch (cell_index->section) {
    case MENU_SECTION_TIMERS:
      menu_draw_row_timers(ctx, cell_layer, cell_index->row);
      break;
    case MENU_SECTION_OTHER:
      menu_draw_row_other(ctx, cell_layer, cell_index->row);
      break;
  }
}

static void menu_draw_row_timers(GContext* ctx, const Layer* cell_layer, uint16_t row_index) {
  Timer* timer = timers_get(row_index);
  if (! timer) { return; }


  char* time_left = malloc(12);
  timer_time_str(timer, false, time_left, 12);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);

  graphics_draw_text(ctx, time_left,
    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
    GRect(33, -3, PEBBLE_WIDTH - 33, 28), GTextOverflowModeFill,
    GTextAlignmentLeft, NULL);

  GBitmap* bmp_icon = NULL;
  GBitmap* bmp_direction = NULL;

  switch (timer->status) {
    case TIMER_STATUS_STOPPED:
      bmp_icon = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_STOP]);
      break;
    case TIMER_STATUS_RUNNING:
      bmp_icon = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_PLAY]);
      break;
    case TIMER_STATUS_PAUSED:
      bmp_icon = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_PAUSE]);
      break;
    case TIMER_STATUS_DONE:
      bmp_icon = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_DONE]);
      break;
  }

  if (bmp_icon) {
    graphics_draw_bitmap_in_rect(ctx, bmp_icon, GRect(8, 8, 16, 16));
  }

  switch (timer->type) {
    case TIMER_TYPE_TIMER:
      bmp_direction = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_TIMER]);
      break;
    case TIMER_TYPE_STOPWATCH:
      bmp_direction = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_STOPWATCH]);
      break;
  }

  if (bmp_direction) {
    graphics_draw_bitmap_in_rect(ctx, bmp_direction, GRect(PEBBLE_WIDTH - 8 - 8, 9, 8, 16));
  }

  if (timer->type == TIMER_TYPE_TIMER) {
    uint8_t width = (144 * timer->current_time) / timer->length;
    graphics_fill_rect(ctx, GRect(0, 31, width, 2), 0, GCornerNone);
  }

  free(time_left);
}

static void menu_draw_row_other(GContext* ctx, const Layer* cell_layer, uint16_t row_index) {
  graphics_context_set_text_color(ctx, GColorBlack);

  GBitmap* bmp_icon = NULL;
  char title[24];

  switch (row_index) {
    case MENU_ROW_OTHER_ADD_TIMER:
      bmp_icon = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_ADD]);
      snprintf(title, 24, "Add Timer");
      break;
    case MENU_ROW_OTHER_ADD_STOPWATCH:
      bmp_icon = bitmaps_get_sub_bitmap(RESOURCE_ID_ICONS_16, icon_rects[ICON_RECT_ADD]);
      snprintf(title, 24, "Add Stopwatch");
      break;
  }

  if (bmp_icon) {
    graphics_draw_bitmap_in_rect(ctx, bmp_icon, GRect(8, 10, 16, 16));
  }

  graphics_draw_text(ctx, title,
    fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
    GRect(33, 1, PEBBLE_WIDTH - 33, 24), GTextOverflowModeFill,
    GTextAlignmentLeft, NULL);
}

static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context) {
  switch (cell_index->section) {
    case MENU_SECTION_TIMERS:
      menu_select_timers(cell_index->row);
      break;
    case MENU_SECTION_OTHER:
      menu_select_other(cell_index->row);
      break;
  }
}

static void menu_select_timers(uint16_t row_index) {
  Timer* timer = timers_get(row_index);
  if (! timer) { return; }

  switch (timer->status) {
    case TIMER_STATUS_STOPPED: {
      char strap_str[32];
      snprintf(strap_str, 32, "/timer/%s/%d", "start", timer->id);
      strap_log_action(strap_str);
      timer_start(timer);
      break;
    }
    case TIMER_STATUS_RUNNING:
      timer_pause(timer);
      break;
    case TIMER_STATUS_PAUSED:
      timer_resume(timer);
      break;
    case TIMER_STATUS_DONE:
      timer_reset(timer);
      break;
  }
}

static void menu_select_other(uint16_t row_index) {
  switch (row_index) {
    case MENU_ROW_OTHER_ADD_TIMER:
      win_timer_add_show();
    break;
    case MENU_ROW_OTHER_ADD_STOPWATCH:
      // win_stopwatch_add_show();
    break;
  }
}

static void timers_update_handler(void) {
  menu_layer_reload_data(s_menu);
}