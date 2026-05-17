#include <Arduino.h>
#include <ESP_Panel_Library.h>
#include <lvgl.h>
#include "lvgl_port_v8.h"
#include "BUNGEE_140.c"
#include "TKMC_LOGO.c"
#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"

typedef struct {
  int speed;
  int rpm;
} Datapacket;

Datapacket incomingData;
volatile bool newDataAvailable = false;

//ESPNOW
extern "C" void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {

  if (len == sizeof(incomingData)) {
    memcpy(&incomingData, data, len);
    newDataAvailable = true;
    Serial.print("\nSpeed: ");
    Serial.println(incomingData.speed);

    Serial.print("   RPM: ");
    Serial.println(incomingData.rpm);
  } 
  else {
    Serial.print("Invalid packet size: ");
    Serial.println(len);
  }
}

ESP_Panel *panel;
lv_obj_t *rpm_arc;
lv_obj_t *rpm_arc_border;
lv_obj_t *rpm_label;
lv_obj_t *rpm_text_label;
lv_obj_t *kph_label;
lv_obj_t *kph_text_label;

// Colors
lv_color_t palette_amber, palette_black, palette_cyan, palette_red, palette_white;

// Attributes / configuration
const int dimension     = 480;
const int rpm_max       = 14000;
const int rpm_redline   = 11000;
const int rpm_arc_width = 24;
const int rpm_line_width = 4;
const int rpm_arc_size  = dimension - 50;
const int arc_start     = 140;
const int arc_end       = 400;

// GUI timing
const uint32_t UPDATE_MS      = 500;
const uint32_t RPM_ANIM_MS    = 80;

// Derived values
const int rpm_marker_count = (rpm_max / 2000) + 1;
const float marker_gap = (float)(arc_end - arc_start) / (float)(rpm_marker_count - 1);

// State machine values
int logical_speed = 0;
int logical_rpm   = 0;

// Displayed RPM animation
int displayed_rpm = 0;
bool rpm_anim_active = false;

// LVGL animation handle
lv_anim_t rpm_arc_anim;

// ------------------- Helper Functions -------------------

hw_timer_t * lvgl_timer = NULL;
void IRAM_ATTR onTimer() {
    lv_tick_inc(1);
}

void rpm_arc_anim_cb(void * var, int32_t v) {
    lv_arc_set_value((lv_obj_t *)var, v);
    displayed_rpm = v;
}

void rpm_anim_ready_cb(lv_anim_t * a) {
    rpm_anim_active = false;
}

void update_speed(int new_speed_kph) {
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d", new_speed_kph);
    lv_label_set_text(kph_label, buf);
}

void update_rpm(int new_rpm) {
    if (rpm_anim_active) {
        lv_anim_del(rpm_arc, NULL);
    }
    rpm_anim_active = true;

    lv_anim_init(&rpm_arc_anim);
    lv_anim_set_var(&rpm_arc_anim, rpm_arc);
    lv_anim_set_values(&rpm_arc_anim, displayed_rpm, new_rpm);
    lv_anim_set_time(&rpm_arc_anim, RPM_ANIM_MS);
    lv_anim_set_exec_cb(&rpm_arc_anim, rpm_arc_anim_cb);
    lv_anim_set_ready_cb(&rpm_arc_anim, rpm_anim_ready_cb);
    lv_anim_start(&rpm_arc_anim);

    if (new_rpm >= rpm_redline) {
        lv_obj_set_style_arc_color(rpm_arc, palette_red, LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_arc_color(rpm_arc, palette_amber, LV_PART_INDICATOR);
    }

    static char rpm_text[12];
    snprintf(rpm_text, sizeof(rpm_text), "%d", new_rpm);
    lv_label_set_text(rpm_label, rpm_text);

    displayed_rpm = new_rpm;
}

void position_markers(lv_obj_t *marker, int position) {
    lv_obj_set_size(marker, rpm_arc_size + (rpm_line_width * 4), rpm_arc_size + (rpm_line_width * 4));
    lv_obj_align(marker, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_width(marker, rpm_arc_width + (rpm_line_width * 4), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(marker, false, LV_PART_MAIN);
    lv_obj_set_style_arc_color(marker, palette_white, LV_PART_MAIN);
    int a = arc_start + (int)(position * marker_gap + 0.5f);
    lv_arc_set_bg_angles(marker, a, a + 1);
    lv_obj_remove_style(marker, NULL, LV_PART_KNOB);
    lv_obj_remove_style(marker, NULL, LV_PART_INDICATOR);
}

// ------------------- GUI Creation -------------------

void make_rpm_arc(void) {
    rpm_arc = lv_arc_create(lv_scr_act());
    lv_obj_set_size(rpm_arc, rpm_arc_size, rpm_arc_size);
    lv_obj_align(rpm_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(rpm_arc, arc_start, arc_end);
    lv_obj_set_style_arc_color(rpm_arc, palette_black, LV_PART_MAIN);
    lv_obj_set_style_arc_color(rpm_arc, palette_amber, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(rpm_arc, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(rpm_arc, rpm_arc_width, LV_PART_INDICATOR);
    lv_obj_remove_style(rpm_arc, NULL, LV_PART_KNOB);
    lv_arc_set_range(rpm_arc, 0, rpm_max);
    lv_arc_set_value(rpm_arc, 0);
    displayed_rpm = 0;
}

void make_rpm_border(void) {
    rpm_arc_border = lv_arc_create(lv_scr_act());
    int border_size = rpm_arc_size - (rpm_arc_width * 2) - (rpm_line_width * 2);
    lv_obj_set_size(rpm_arc_border, border_size, border_size);
    lv_obj_align(rpm_arc_border, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(rpm_arc_border, arc_start, arc_end);
    lv_obj_set_style_arc_width(rpm_arc_border, rpm_line_width, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(rpm_arc_border, false, LV_PART_MAIN);
    lv_obj_set_style_arc_color(rpm_arc_border, palette_white, LV_PART_MAIN);
    lv_obj_remove_style(rpm_arc_border, NULL, LV_PART_KNOB);
    lv_obj_remove_style(rpm_arc_border, NULL, LV_PART_INDICATOR);

    for (int i = 0; i < rpm_marker_count; i++) {
        lv_obj_t *m = lv_arc_create(lv_scr_act());
        position_markers(m, i);
    }
}

void make_rpm_redline(void) {
    lv_obj_t *rpm_redline_marker = lv_arc_create(lv_scr_act());
    lv_obj_set_size(rpm_redline_marker, dimension, dimension);
    lv_obj_align(rpm_redline_marker, LV_ALIGN_CENTER, 0, 0);
    int red_angle = arc_start + (int)((arc_end - arc_start) * ((float)rpm_redline / (float)rpm_max) + 0.5f);
    lv_arc_set_bg_angles(rpm_redline_marker, red_angle, arc_end);
    lv_obj_set_style_arc_color(rpm_redline_marker, palette_red, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(rpm_redline_marker, false, LV_PART_MAIN);
    lv_obj_set_style_arc_width(rpm_redline_marker, rpm_line_width * 2, LV_PART_MAIN);
    lv_obj_remove_style(rpm_redline_marker, NULL, LV_PART_KNOB);
    lv_obj_remove_style(rpm_redline_marker, NULL, LV_PART_INDICATOR);
}

void make_rpm_digital(void) {
    static lv_style_t style_rpm_text;
    lv_style_init(&style_rpm_text);
    lv_style_set_text_font(&style_rpm_text, &lv_font_montserrat_40);
    lv_style_set_text_color(&style_rpm_text, palette_white);
    rpm_label = lv_label_create(lv_scr_act());
    lv_label_set_text(rpm_label, "0");
    lv_obj_add_style(rpm_label, &style_rpm_text, 0);
    lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, 100);
}

void make_rpm_mini_label(void) {
    static lv_style_t style_rpm_label_text;
    lv_style_init(&style_rpm_label_text);
    lv_style_set_text_font(&style_rpm_label_text, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_rpm_label_text, palette_amber);
    rpm_text_label = lv_label_create(lv_scr_act());
    lv_label_set_text(rpm_text_label, "RPM");
    lv_obj_add_style(rpm_text_label, &style_rpm_label_text, 0);
    lv_obj_align(rpm_text_label, LV_ALIGN_CENTER, 0, 126);
}

void make_kph_mini_label(void) {
    static lv_style_t style_kph_label_text;
    lv_style_init(&style_kph_label_text);
    lv_style_set_text_font(&style_kph_label_text, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_kph_label_text, palette_red);
    kph_text_label = lv_label_create(lv_scr_act());
    lv_label_set_text(kph_text_label, "KPH");
    lv_obj_add_style(kph_text_label, &style_kph_label_text, 0);
    lv_obj_align(kph_text_label, LV_ALIGN_CENTER, 90, 57);
}

void make_speed_digital(void) {
    static lv_style_t style_speed_text;
    lv_style_init(&style_speed_text);
    lv_style_set_text_font(&style_speed_text, &BUNGEE_140);
    lv_style_set_text_color(&style_speed_text, palette_red);
    kph_label = lv_label_create(lv_scr_act());
    lv_label_set_text(kph_label, "0");
    lv_obj_add_style(kph_label, &style_speed_text, 0);
    lv_obj_align(kph_label, LV_ALIGN_CENTER, 0, 0);
}

void make_logo() {
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &TKMC_LOGO);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -125);
}


void setup() {
    Serial.begin(115200);

    //ESPNOW

    WiFi.mode(WIFI_STA);

    // FORCE RECEIVER TO CHANNEL 1
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("Receiver READY on channel 1...");

    //LVGL

    Serial.println("LVGL UI start");

    panel = new ESP_Panel();
    panel->init();
    #if LVGL_PORT_AVOID_TEAR
        // When avoid tearing function is enabled, configure the RGB bus according to the LVGL configuration
        ESP_PanelBus_RGB *rgb_bus = static_cast<ESP_PanelBus_RGB *>(panel->getLcd()->getBus());
        rgb_bus->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
        rgb_bus->configRgbBounceBufferSize(LVGL_PORT_RGB_BOUNCE_BUFFER_SIZE);
    #endif
    panel->begin();
    lvgl_port_init(panel->getLcd(), panel->getTouch());

    // Init colors
    palette_amber = LV_COLOR_MAKE(250, 140, 0);
    palette_black = LV_COLOR_MAKE(0, 0, 0);
    palette_cyan  = LV_COLOR_MAKE(0, 255, 255);
    palette_red   = LV_COLOR_MAKE(255, 0, 0);
    palette_white = LV_COLOR_MAKE(255, 255, 255);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);

    // GUI
    make_rpm_arc();
    make_rpm_border();
    make_rpm_redline();
    make_rpm_digital();
    make_rpm_mini_label();
    make_speed_digital();
    make_kph_mini_label();
    make_logo();

    logical_speed = 0;
    logical_rpm = 0;
    displayed_rpm = 0;

}

void loop() {
    lv_timer_handler();
    if (newDataAvailable) {
        newDataAvailable = false;

        update_speed(incomingData.speed);
        update_rpm(incomingData.rpm);
    }
    delay(180);
}




