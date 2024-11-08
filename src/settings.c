#include "settings.h"
#include "external/cJSON.h"
#include "utils.h"

#include <glib.h>

// Save settings with cooldown period of 1s.
static void errands_settings_save();

// --- GLOBAL SETTINGS VARIABLES --- //

static const char *settings_path;
static time_t last_save_time = 0;
static bool pending_save = false;
static cJSON *settings;

void errands_settings_load_default() {
  LOG("Settings: Load default configuration");

  settings = cJSON_CreateObject();

  cJSON_AddBoolToObject(settings, "show_completed", true);
  cJSON_AddNumberToObject(settings, "window_width", 800);
  cJSON_AddNumberToObject(settings, "window_height", 600);
  cJSON_AddBoolToObject(settings, "maximized", false);

  errands_settings_save();
}

void errands_settings_load_user() {
  LOG("Settings: Load user configuration");
  char *settings_str = read_file_to_string(settings_path);
  if (!strcmp(settings_str, "")) {
    LOG("Settings: settings.json is empty");
    errands_settings_save();
    free(settings_str);
    return;
  }
  cJSON *json = cJSON_Parse(settings_str);
  free(settings_str);
  if (!json) {
    errands_settings_save();
    return;
  }

  cJSON *show_completed = cJSON_GetObjectItem(json, "show_completed");
  if (show_completed)
    cJSON_ReplaceItemInObject(settings, "show_completed", show_completed);

  cJSON *window_width = cJSON_GetObjectItem(json, "window_width");
  if (window_width)
    cJSON_ReplaceItemInObject(settings, "window_width", window_width);

  cJSON *window_height = cJSON_GetObjectItem(json, "window_height");
  if (window_height)
    cJSON_ReplaceItemInObject(settings, "window_height", window_height);

  cJSON *maximized = cJSON_GetObjectItem(json, "maximized");
  if (maximized)
    cJSON_ReplaceItemInObject(settings, "maximized", maximized);

  errands_settings_save();
}

// Migrate from GSettings to settings.json
void errands_settings_migrate() { LOG("Settings: Migrate to settings.json"); }

// Check if settings has all keys, if not - add default ones.
void errands_settings_upgrade() {}

void errands_settings_init() {
  LOG("Settings: Initialize");
  settings_path = g_build_path("/", g_get_user_data_dir(), "errands", "settings.json", NULL);
  errands_settings_load_default();
  file_exists(settings_path) ? errands_settings_load_user() : errands_settings_migrate();
}

ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type) {
  ErrandsSetting setting;
  cJSON *value = cJSON_GetObjectItem(settings, key);
  if (type == SETTING_TYPE_INT)
    setting.i = value->valueint;
  else if (type == SETTING_TYPE_BOOL)
    setting.b = (bool)value->valueint;
  else if (type == SETTING_TYPE_STRING)
    setting.s = value->valuestring;
  return setting;
}

void errands_settings_set(const char *key, ErrandsSettingType type, void *value) {
  cJSON *val = cJSON_GetObjectItem(settings, key);
  // Create key-value if not exists
  if (!val) {
    if (type == SETTING_TYPE_INT)
      val = cJSON_CreateNumber(*(int *)value);
    else if (type == SETTING_TYPE_BOOL)
      val = cJSON_CreateBool(*(bool *)value);
    else if (type == SETTING_TYPE_STRING)
      val = cJSON_CreateString((const char *)value);
    cJSON_AddItemToObject(settings, key, val);
  }
  // Change existing value
  else {
    if (type == SETTING_TYPE_INT)
      cJSON_SetIntValue(val, *(int *)value);
    else if (type == SETTING_TYPE_BOOL)
      cJSON_SetBoolValue(val, *(bool *)value);
    else if (type == SETTING_TYPE_STRING)
      cJSON_SetValuestring(val, (const char *)value);
  }
  errands_settings_save();
}

// --- SAVING SETTINGS --- //

static void perform_save() {
  LOG("Settings: Save");
  char *json_string = cJSON_PrintUnformatted(settings);
  write_string_to_file(settings_path, json_string);
  free(json_string);
  last_save_time = time(NULL);
  pending_save = false;
}

void errands_settings_save() {
  float diff = difftime(time(NULL), last_save_time);
  if (pending_save)
    return;
  if (diff < 1.0f) {
    pending_save = true;
    g_timeout_add_seconds_once(1, (GSourceOnceFunc)perform_save, NULL);
    return;
  } else {
    pending_save = true;
    perform_save();
    return;
  }
}
