#include <pebble.h>

static Window* g_window = NULL;

static TextLayer* g_hour_layer;
static TextLayer* g_minute_layer;
static TextLayer* g_date_layer = NULL;
static TextLayer* g_battery_layer = NULL;

static AppTimer* g_info_timer = NULL;

#define ALWAYS_SHOW_INFO false

////////////////////////////////////////

const char* hour_str(struct tm* time) {
    int h = time->tm_hour;
    if (!clock_is_24h_style()) {
        if (h == 0) {
            h = 12;
        }
        else if (h > 12) {
            h -= 12;
        }
    }

    static char buf[32];
    snprintf(buf, sizeof(buf), "%02d", h);
    return buf;
}

const char* minute_str(struct tm* time) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%02d", time->tm_min);
    return buf;
}

const char* date_str(struct tm* time) {
    static char buf[32];
    int len = strftime(buf, sizeof(buf), "%b\n", time);
    snprintf(buf + len, sizeof(buf) - len, "%d", time->tm_mday);
    return buf;
}

const char* battery_str(BatteryChargeState charge) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%d\n%%", charge.charge_percent);
    return buf;
}


////////////////////////////////////////

void update_battery_visibility(BatteryChargeState charge) {
#if ALWAYS_SHOW_INFO
    bool show_battery = true;
#else
    bool show_battery = g_info_timer || charge.is_charging || charge.charge_percent <= 30;
#endif
    layer_set_hidden(text_layer_get_layer(g_battery_layer), !show_battery);
}

void update_date_visibility() {
#if ALWAYS_SHOW_INFO
    bool show_date = true;
#else
    bool show_date = g_info_timer;
#endif
    layer_set_hidden(text_layer_get_layer(g_date_layer), !show_date);
}

void info_timer_handler(void* data) {
    g_info_timer = NULL;

    update_date_visibility();
    update_battery_visibility(battery_state_service_peek());
}

void show_info() {
    if (g_info_timer) {
        app_timer_cancel(g_info_timer);
    }
    g_info_timer = app_timer_register(2000, info_timer_handler, NULL);

    update_date_visibility();
    update_battery_visibility(battery_state_service_peek());
}

void update_bounds() {
    Layer* window_layer = window_get_root_layer(g_window);
    GRect bounds = layer_get_unobstructed_bounds(window_layer);

    const int time_h = 76;
    const int time_w = bounds.size.w/2 + 20;
    const int time_y = bounds.size.h/2 - time_h + 10;

    GRect hour_rect = GRect(0, time_y, time_w, time_h);
    layer_set_frame(text_layer_get_layer(g_hour_layer), hour_rect);
    GRect minute_rect = GRect(bounds.size.w - time_w, bounds.size.h - time_h - time_y, time_w, time_h);
    layer_set_frame(text_layer_get_layer(g_minute_layer), minute_rect);

    const int info_h = 52;
#if PBL_PLATFORM_CHALK
    const int info_x = 58;
#else
    const int info_x = 5;
#endif
    const int info_y = 5;

    GRect date_rect = GRect(info_x, bounds.size.h - info_h - info_y, bounds.size.w, info_h);
    layer_set_frame(text_layer_get_layer(g_date_layer), date_rect);
    GRect battery_rect = GRect(0, info_y, bounds.size.w - info_x, info_h);
    layer_set_frame(text_layer_get_layer(g_battery_layer), battery_rect);
}


////////////////////////////////////////

void tick_timer_handler(struct tm* tick_time, TimeUnits units_changed) {
    text_layer_set_text(g_hour_layer, hour_str(tick_time));
    text_layer_set_text(g_minute_layer, minute_str(tick_time));
    text_layer_set_text(g_date_layer, date_str(tick_time));
}

void battery_state_handler(BatteryChargeState charge) {
    text_layer_set_text(g_battery_layer, battery_str(charge));
    update_battery_visibility(charge);
}

void accel_data_handler(AccelData* data, uint32_t n) {
    for (int i = 0; i < (int) n; ++i) {
        static AccelData prev_accel = { 0 };
        AccelData accel = data[i];

        if (prev_accel.timestamp) {
            int dy = accel.y - prev_accel.y;
            if (dy > 2500 || dy < -2500) {
                show_info();
            }
        }

        prev_accel = accel;
    }
}

void unobstructed_area_handler(void* context) {
    update_bounds();
}

////////////////////////////////////////

void init() {
    // window
    g_window = window_create();
    window_set_background_color(g_window, GColorBlack );
    window_stack_push(g_window, true);

    Layer* window_layer = window_get_root_layer(g_window);
    GRect bounds = layer_get_frame(window_layer);

    GFont time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_62));

    // hour layer
    g_hour_layer = text_layer_create(GRectZero);
    text_layer_set_text_color(g_hour_layer, GColorWhite);
    text_layer_set_background_color(g_hour_layer, GColorClear);
    text_layer_set_font(g_hour_layer, time_font);
    text_layer_set_text_alignment(g_hour_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(g_hour_layer));

    // minute layer
    g_minute_layer = text_layer_create(GRectZero);
    text_layer_set_text_color(g_minute_layer, GColorVividCerulean);
    text_layer_set_background_color(g_minute_layer, GColorClear);
    text_layer_set_font(g_minute_layer, time_font);
    text_layer_set_text_alignment(g_minute_layer, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(g_minute_layer));


    GFont info_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_24));
    const GColor info_color = GColorCyan;

    // date layer
    g_date_layer = text_layer_create(GRectZero);
    text_layer_set_text_color(g_date_layer, info_color);
    text_layer_set_background_color(g_date_layer, GColorClear);
    text_layer_set_font(g_date_layer, info_font);
    text_layer_set_text_alignment(g_date_layer, GTextAlignmentLeft);
    update_date_visibility();
    layer_add_child(window_layer, text_layer_get_layer(g_date_layer));

    // battery charge layer
    g_battery_layer = text_layer_create(GRectZero);
    text_layer_set_text_color(g_battery_layer, info_color);
    text_layer_set_background_color(g_battery_layer, GColorClear);
    text_layer_set_font(g_battery_layer, info_font);
    text_layer_set_text_alignment(g_battery_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(g_battery_layer));

    // initialize layer text
    time_t tt = time(NULL);
    tick_timer_handler(localtime(&tt), MINUTE_UNIT);
    battery_state_handler(battery_state_service_peek());

    // subscribe to services
    tick_timer_service_subscribe(MINUTE_UNIT, &tick_timer_handler);
    battery_state_service_subscribe(battery_state_handler);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
    accel_data_service_subscribe(1, accel_data_handler);
    UnobstructedAreaHandlers handlers = {
        .did_change = unobstructed_area_handler,
    };
    unobstructed_area_service_subscribe(handlers, NULL);

    update_bounds();
}

void deinit() {
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    accel_data_service_unsubscribe();

    text_layer_destroy(g_hour_layer);
    text_layer_destroy(g_minute_layer);
    text_layer_destroy(g_battery_layer);

    window_destroy(g_window);
}

int main() {
    init();
    app_event_loop();
    deinit();
}
