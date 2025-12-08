#include <pebble.h>

static Window* g_window = NULL;

static TextLayer* g_hour_layer;
static TextLayer* g_minute_layer;
static TextLayer* g_date_layer = NULL;
static TextLayer* g_battery_layer = NULL;

static AppTimer* g_date_timer = NULL;


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
    int len = strftime(buf, sizeof(buf), "%b ", time);
    snprintf(buf + len, sizeof(buf) - len, "%d", time->tm_mday);
    return buf;
}

const char* battery_str(BatteryChargeState charge) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%s%d%%", charge.is_charging ? "+" : "", charge.charge_percent);
    return buf;
}


////////////////////////////////////////

static void app_timer_handler(void* data) {
    g_date_timer = NULL;
    layer_set_hidden(text_layer_get_layer(g_date_layer), true);
}

static void show_date() {
    if (g_date_timer) {
        app_timer_cancel(g_date_timer);
    }
    g_date_timer = app_timer_register(2000, app_timer_handler, NULL);
    layer_set_hidden(text_layer_get_layer(g_date_layer), false);
}

////////////////////////////////////////

static void tick_timer_handler(struct tm* tick_time, TimeUnits units_changed) {
    text_layer_set_text(g_hour_layer, hour_str(tick_time));
    text_layer_set_text(g_minute_layer, minute_str(tick_time));
    text_layer_set_text(g_date_layer, date_str(tick_time));
}

static void battery_state_handler(BatteryChargeState charge) {
    bool show_battery = charge.is_charging || charge.charge_percent <= 30;
    layer_set_hidden(text_layer_get_layer(g_battery_layer), !show_battery);
    text_layer_set_text(g_battery_layer, battery_str(charge));
}

static void accel_data_handler(AccelData* data, uint32_t n) {
    for (int i = 0; i < (int) n; ++i) {
        static AccelData prev_accel = { 0 };
        AccelData accel = data[i];

        if (prev_accel.timestamp) {
            int dy = accel.y - prev_accel.y;
            if (dy > 2500 || dy < -2500) {
                show_date();
            }
        }

        prev_accel = accel;
    }
}


////////////////////////////////////////

static void init() {
    // window
    g_window = window_create();
    window_set_background_color(g_window, GColorBlack );
    window_stack_push(g_window, true);

    Layer* window_layer = window_get_root_layer(g_window);
    GRect bounds = layer_get_frame(window_layer);


    GFont time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_62));
    const int time_h = 76;
    const int time_w = bounds.size.w/2 + 20;
    const int time_y = bounds.size.h/2 - time_h + 10;

    // hour layer
    GRect hour_rect = GRect(0, time_y, time_w, time_h);
    g_hour_layer = text_layer_create(hour_rect);
    text_layer_set_text_color(g_hour_layer, GColorWhite);
    text_layer_set_background_color(g_hour_layer, GColorClear);
    text_layer_set_font(g_hour_layer, time_font);
    text_layer_set_text_alignment(g_hour_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(g_hour_layer));

    // minute layer
    GRect minute_rect = GRect(bounds.size.w - time_w, bounds.size.h - time_h - time_y, time_w, time_h);
    g_minute_layer = text_layer_create(minute_rect);
    text_layer_set_text_color(g_minute_layer, GColorVividCerulean);
    text_layer_set_background_color(g_minute_layer, GColorClear);
    text_layer_set_font(g_minute_layer, time_font);
    text_layer_set_text_alignment(g_minute_layer, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(g_minute_layer));


    GFont info_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_20));
    const int info_h = 22;
#if PBL_PLATFORM_CHALK
    const int info_x = 58;
#else
    const int info_x = 5;
#endif
    const int info_y = 5;

    // date layer
    GRect date_rect = GRect(info_x, bounds.size.h - info_h - info_y, bounds.size.w, info_h);
    g_date_layer = text_layer_create(date_rect);
    text_layer_set_text_color(g_date_layer, GColorWhite);
    text_layer_set_background_color(g_date_layer, GColorClear);
    text_layer_set_font(g_date_layer, info_font);
    text_layer_set_text_alignment(g_date_layer, GTextAlignmentLeft);
    layer_set_hidden(text_layer_get_layer(g_date_layer), true);
    layer_add_child(window_layer, text_layer_get_layer(g_date_layer));

    // battery charge layer
    GRect battery_rect = GRect(0, info_y, bounds.size.w - info_x, info_h);
    g_battery_layer = text_layer_create(battery_rect);
    text_layer_set_text_color(g_battery_layer, GColorWhite);
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
}

static void deinit() {
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
