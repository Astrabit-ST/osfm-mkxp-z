#include <vector>
#include <cstring>

#include "binding-util.h"
#include "eventthread.h"
#include "sharedstate.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
static HKEY hkey = HKEY_CURRENT_USER;
#else
#include <gio/gio.h>
static GSettingsSchemaSource *source = nullptr;
static GSettingsSchema *schema = nullptr;
static GSettings *settings = nullptr;
#endif

#ifndef _WIN32
static void verify_key(const char *key, const void *variant_type) {
  if (!g_settings_schema_has_key(schema, key)) {
    throw Exception(Exception::MKXPError, "Registry key '%s' is not in the schema", key);
  }

  if (variant_type != nullptr) {
    GSettingsSchemaKey *schema_key = g_settings_schema_get_key(schema, key);
    if (schema_key == nullptr) {
      throw Exception(Exception::MKXPError, "Registry key '%s' is not in the schema", key);
    }
    if (!g_variant_type_equal(g_settings_schema_key_get_value_type(schema_key), variant_type)) {
      g_free(schema_key);
      throw Exception(Exception::MKXPError, "Registry key '%s' has mismatching type", key);
    }
    g_free(schema_key);
  }
}
#endif

RB_METHOD_GUARD(registry_get_boolean) {
  RB_UNUSED_PARAM;

  const char *key;
  rb_get_args(argc, argv, "z", &key RB_ARG_END);

#ifdef _WIN32
  DWORD value;
  DWORD size = sizeof value;
  LSTATUS error = RegGetValueA(
    hkey,
    nullptr,
    key,
    RRF_RT_REG_DWORD,
    nullptr,
    &value,
    &size
  );
  if (error != ERROR_SUCCESS) {
    value = 0;
  }
#else
  verify_key(key, G_VARIANT_TYPE_BOOLEAN);
  bool value = g_settings_get_boolean(settings, key);
#endif

  return value ? Qtrue : Qfalse;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(registry_set_boolean) {
  RB_UNUSED_PARAM;

  const char *key;
  bool value;
  rb_get_args(argc, argv, "zb", &key, &value RB_ARG_END);

#ifdef _WIN32
  DWORD value_dword = value;
  RegSetKeyValueA(
    hkey,
    nullptr,
    key,
    REG_DWORD,
    &value_dword,
    sizeof value_dword
  );
#else
  verify_key(key, G_VARIANT_TYPE_BOOLEAN);
  g_settings_set_boolean(settings, key, value);
#endif

  return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(registry_get_integer) {
  RB_UNUSED_PARAM;

  const char *key;
  rb_get_args(argc, argv, "z", &key RB_ARG_END);

#ifdef _WIN32
  DWORD value;
  DWORD size = sizeof value;
  LSTATUS error = RegGetValueA(
    hkey,
    nullptr,
    key,
    RRF_RT_REG_DWORD,
    nullptr,
    &value,
    &size
  );
  if (error != ERROR_SUCCESS) {
    value = 0;
  }
#else
  verify_key(key, G_VARIANT_TYPE_INT32);
  int value = g_settings_get_int(settings, key);
#endif

  return RB_INT2FIX((int)value);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(registry_set_integer) {
  RB_UNUSED_PARAM;

  const char *key;
  int value;
  rb_get_args(argc, argv, "zi", &key, &value RB_ARG_END);

#ifdef _WIN32
  DWORD value_dword = (DWORD)value;
  RegSetKeyValueA(
    hkey,
    nullptr,
    key,
    REG_DWORD,
    &value_dword,
    sizeof value_dword
  );
#else
  verify_key(key, G_VARIANT_TYPE_INT32);
  g_settings_set_int(settings, key, value);
#endif

  return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(registry_get_string) {
  RB_UNUSED_PARAM;

  const char *key;
  rb_get_args(argc, argv, "z", &key RB_ARG_END);

#ifdef _WIN32
  std::vector<char> buffer;
  DWORD size;
  LSTATUS error = RegGetValueA(
    hkey,
    nullptr,
    key,
    RRF_RT_REG_SZ,
    nullptr,
    nullptr,
    &size
  );
  if (error != ERROR_SUCCESS) {
    buffer.clear();
  } else {
    do {
      buffer.resize(size);
      error = RegGetValueA(
        hkey,
        nullptr,
        key,
        RRF_RT_REG_SZ,
        nullptr,
        buffer.data(),
        &size
      );
    } while (error == ERROR_MORE_DATA);
    if (error != ERROR_SUCCESS) {
      buffer.clear();
    }
  }
  buffer.push_back(0);
  VALUE value_obj = rb_utf8_str_new_cstr(buffer.data());
#else
  verify_key(key, G_VARIANT_TYPE_STRING);
  gchar *buffer = g_settings_get_string(settings, key);
  VALUE value_obj = rb_utf8_str_new_cstr(buffer);
  g_free(buffer);
#endif

  return value_obj;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(registry_set_string) {
  RB_UNUSED_PARAM;

  const char *key;
  const char *value;
  rb_get_args(argc, argv, "zz", &key, &value RB_ARG_END);

#ifdef _WIN32
  RegSetKeyValueA(
    hkey,
    nullptr,
    key,
    REG_SZ,
    value,
    std::strlen(value) + 1
  );
#else
  verify_key(key, G_VARIANT_TYPE_STRING);
  g_settings_set_string(settings, key, value);
#endif

  return Qnil;
}
RB_METHOD_GUARD_END

void osfmRegistryBindingTerminate() {
#ifdef _WIN32
  if (hkey != HKEY_CURRENT_USER) {
    RegCloseKey(hkey);
    hkey = HKEY_CURRENT_USER;
  }
#else
  if (settings != nullptr) {
    g_free(settings);
    settings = nullptr;
  }
  if (schema != nullptr) {
    g_free(schema);
    schema = nullptr;
  }
  if (source != nullptr) {
    g_free(source);
    source = nullptr;
  }
#endif
}

void osfmRegistryBindingInit() {
  osfmRegistryBindingTerminate();

#ifdef _WIN32
  LSTATUS error = RegCreateKeyExA(
    HKEY_CURRENT_USER,
    "Software\\astrabit\\TWO",
    0,
    nullptr,
    REG_OPTION_NON_VOLATILE,
    KEY_QUERY_VALUE | KEY_SET_VALUE,
    nullptr,
    &hkey,
    nullptr
  );
  if (error != ERROR_SUCCESS) {
    shState->eThread().showMessageBox("Failed to open the registry");
    std::exit(1);
  }
#else
  GError *error = nullptr;
  source = g_settings_schema_source_new_from_directory(".", nullptr, false, &error);
  if (source == nullptr) {
    if (error != nullptr) {
      shState->eThread().showMessageBox((std::string("Failed to load gschemas.compiled: ") + error->message).c_str());
    } else {
      shState->eThread().showMessageBox("Failed to load gschemas.compiled");
    }
    std::exit(1);
  }
  schema = g_settings_schema_source_lookup(source, "astrabit.TWO", false);
  if (schema == nullptr) {
    g_free(source);
    source = nullptr;
    shState->eThread().showMessageBox("Failed to look up schema in gschemas.compiled");
    std::exit(1);
  }
  settings = g_settings_new_full(schema, nullptr, nullptr);
  if (settings == nullptr) {
    g_free(schema);
    schema = nullptr;
    g_free(source);
    source = nullptr;
    shState->eThread().showMessageBox("Failed to construct settings from gschemas.compiled");
    std::exit(1);
  }
#endif

  VALUE module = rb_define_module("Registry");

  _rb_define_module_function(module, "get_boolean", registry_get_boolean);
  _rb_define_module_function(module, "set_boolean", registry_set_boolean);

  _rb_define_module_function(module, "get_integer", registry_get_integer);
  _rb_define_module_function(module, "set_integer", registry_set_integer);

  _rb_define_module_function(module, "get_string", registry_get_string);
  _rb_define_module_function(module, "set_string", registry_set_string);
}
