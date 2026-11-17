#include "config.h"
#include "keycode.h"

#include <errno.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int client_load_config_api(lua_State *state, client_config *config);

#define KEYBIND_SLED 128
#define KEYBIND_BANK 4 * KEYBIND_SLED
#define KEYBIND_BANK_PREFIX 0 * KEYBIND_BANK
#define KEYBIND_BANK_NORMAL 1 * KEYBIND_BANK
#define KEYBIND_BANK_COUNT 2
struct client_config {
  keycode prefix;
  int *refs;
  lua_State *lua;
};

void client_config_close(client_config *config) {
  if (config) {
    if (config->lua)
      lua_close(config->lua);
    free(config->refs);
    free(config);
  }
}

static int *client_config_keybind(const client_config *config, keycode code) {
  int mult = code.modifiers & 0x3;

  size_t off = (size_t)mult * KEYBIND_SLED + code.ascii;
  return config->refs + off;
}

keycode client_config_prefix(const client_config *config) {
  return config->prefix;
}

client_config *client_config_load(void) {
  client_config *config = calloc(1, sizeof(*config));
  if (!config) {
    fprintf(stderr, "dev-mux: out of memory loading configuration\n");
    return NULL;
  }

  config->refs = malloc(KEYBIND_BANK_COUNT * KEYBIND_BANK * sizeof(int));
  if (!config->refs) {
    fprintf(stderr, "dev-mux: out of memory loading configuration\n");
    client_config_close(config);
    return NULL;
  }

  for (size_t i = 0; i < KEYBIND_BANK_COUNT * KEYBIND_BANK; i++) {
    config->refs[i] = LUA_REFNIL;
  }

  const char *base = getenv("XDG_CONFIG_HOME");
  const char *suffix = "/dev-mux/config.lua";
  if (!base || !*base) {
    base = getenv("HOME");
    suffix = "/.config/dev-mux/config.lua";
  }
  if (!base || !*base)
    return config;

  size_t length = strlen(base) + strlen(suffix) + 1;
  char *path = malloc(length);
  if (!path) {
    fprintf(stderr, "dev-mux: out of memory loading configuration\n");
    client_config_close(config);
    return NULL;
  }
  snprintf(path, length, "%s%s", base, suffix);

  if (access(path, F_OK) != 0 && errno == ENOENT) {
    free(path);
    return config;
  }

  lua_State *lua = luaL_newstate();
  if (!lua) {
    fprintf(stderr, "dev-mux: out of memory loading configuration\n");
    free(path);
    client_config_close(config);
    return NULL;
  }
  luaL_openlibs(lua);
  config->lua = lua;

  client_load_config_api(lua, config);

  if (luaL_loadfile(lua, path) != LUA_OK || lua_pcall(lua, 0, 0, 0) != LUA_OK) {
    fprintf(stderr, "dev-mux: %s: %s\n", path, lua_tostring(lua, -1));
    goto done;
  }

  lua_getglobal(lua, "mux");
  if (!lua_isnil(lua, -1)) {
    if (!lua_istable(lua, -1)) {
      fprintf(stderr, "dev-mux: %s: keymap must be a table\n", path);
      goto done;
    }

    lua_getfield(lua, -1, "keymap");
    if (!lua_isnil(lua, -1)) {
      if (!lua_istable(lua, -1)) {
        fprintf(stderr, "dev-mux: %s: keymap must be a table\n", path);
        goto done;
      }

      lua_getfield(lua, -1, "prefix");

      syslog(LOG_INFO, "loading prefix");

      keycode prefix;
      if (!lua_isnil(lua, -1) &&
          (!lua_isstring(lua, -1) ||
           parse_keycode(lua_tostring(lua, -1), &prefix) != 0)) {
        fprintf(stderr,
                "dev-mux: %s: keymap.prefix must be a control key such as "
                "'C-Space' or 'C-b'\n",
                path);
        goto done;
      }

      if (!lua_isnil(lua, -1)) {
        config->prefix = prefix;
      }
      syslog(LOG_INFO, "setting prefix: %x %#02x", config->prefix.modifiers,
             config->prefix.ascii);
    }
  }

  free(path);
  return config;

done:
  lua_close(lua);
  free(path);
  client_config_close(config);
  return NULL;
}

bool client_config_process(const client_config *self, bool *pending,
                           const char **buf, size_t *len) {
  if (!*len) {
    return false;
  }

  keycode prefix = self->prefix;

  keycode code;
  if (parse_keycode_input(len, buf, &code) < 0) {
    return false;
  }

  if (*pending) {
    *pending = false;
    int *ref = client_config_keybind(self, code) + KEYBIND_BANK_PREFIX;

    if (*ref != LUA_REFNIL) {
      syslog(LOG_INFO, "running pmap: %x %#02x %x", code.modifiers, code.ascii,
             *ref);

      lua_rawgeti(self->lua, LUA_REGISTRYINDEX, *ref);

      if (lua_pcall(self->lua, 0, 0, 0) != LUA_OK) {
        syslog(LOG_INFO, "failed to call pmap handler: %s",
               lua_tostring(self->lua, -1));
        lua_pop(self->lua, 1);
      }
      return false;
    } else {
      return false;
    }
  } else {
    if (code.modifiers == prefix.modifiers && code.ascii == prefix.ascii) {
      *pending = true;

      return false;
    } else {
      int *ref = client_config_keybind(self, code) + KEYBIND_BANK_NORMAL;

      if (*ref != LUA_REFNIL) {
        syslog(LOG_INFO, "running nmap: %x %#02x %x", code.modifiers,
               code.ascii, *ref);

        lua_rawgeti(self->lua, LUA_REGISTRYINDEX, *ref);

        if (lua_pcall(self->lua, 0, 0, 0) != LUA_OK) {
          syslog(LOG_INFO, "failed to call nmap handler: %s",
                 lua_tostring(self->lua, -1));
          lua_pop(self->lua, 1);
        }

        return false;
      } else {
        return true;
      }
    }
  }
}

bool client_prefix_filter(const client_config *config, bool *pending,
                          unsigned char key, unsigned char *output) {
  if (*pending) {
    *pending = false;
    *output = key;

    if (key == config->prefix.ascii)
      return true;

    if (config->refs[key] != LUA_REFNIL) {
      syslog(LOG_INFO, "running pmap: %#02x", key);

      lua_rawgeti(config->lua, LUA_REGISTRYINDEX, config->refs[key]);

      if (lua_pcall(config->lua, 0, 0, 0) != LUA_OK) {
        syslog(LOG_INFO, "failed to call pmap handler: %s",
               lua_tostring(config->lua, -1));
        lua_pop(config->lua, 1);
      }

      return false;
    }

    return true;
  }
  if (key == config->prefix.ascii) {
    *pending = true;
    return false;
  }
  *output = key;
  return true;
}

static int client_config_quit(lua_State *) {
  syslog(LOG_INFO, "trying to quit");
  uv_stop(uv_default_loop());
  return 0;
}

static int client_config_pmap(lua_State *state) {
  syslog(LOG_INFO, "pmap called");
  const char *key = luaL_checkstring(state, 1);
  client_config *config = lua_touserdata(state, lua_upvalueindex(1));

  keycode code;
  if (parse_keycode(key, &code) != 0) {
    // TODO: send warning
    return 0;
  }

  lua_pushvalue(state, 2);
  int new_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  int *ref = client_config_keybind(config, code) + KEYBIND_BANK_PREFIX;

  if (*ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, *ref);
  }

  *ref = new_ref;
  syslog(LOG_INFO, "loaded a function for pmap: %d", code.ascii);

  return 0;
}

static int client_config_nmap(lua_State *state) {
  const char *key = luaL_checkstring(state, 1);
  client_config *config = lua_touserdata(state, lua_upvalueindex(1));

  keycode code;
  if (parse_keycode(key, &code) != 0) {
    // TODO: send warning
    return 0;
  }

  lua_pushvalue(state, 2);
  int new_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  int *ref = client_config_keybind(config, code) + KEYBIND_BANK_NORMAL;
  if (*ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, *ref);
  }

  *ref = new_ref;
  syslog(LOG_INFO, "loaded a function for nmap: %d", code.ascii);

  return 0;
}

static int client_config_punmap(lua_State *state) {
  syslog(LOG_INFO, "punmap called");
  const char *key = luaL_checkstring(state, 1);
  client_config *config = lua_touserdata(state, lua_upvalueindex(1));

  keycode code;
  if (parse_keycode(key, &code) != 0) {
    // TODO: send warning
    return 0;
  }

  int *ref = client_config_keybind(config, code) + KEYBIND_BANK_PREFIX;

  if (*ref != LUA_REFNIL) {
    luaL_unref(config->lua, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_REFNIL;
  }

  return 0;
}

static int client_config_nunmap(lua_State *state) {
  syslog(LOG_INFO, "punmap called");
  const char *key = luaL_checkstring(state, 1);
  client_config *config = lua_touserdata(state, lua_upvalueindex(1));

  keycode code;
  if (parse_keycode(key, &code) != 0) {
    // TODO: send warning
    return 0;
  }

  int *ref = client_config_keybind(config, code) + KEYBIND_BANK_NORMAL;

  if (*ref != LUA_REFNIL) {
    luaL_unref(config->lua, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_REFNIL;
  }

  return 0;
}

int client_load_config_api(lua_State *state, client_config *config) {
  syslog(LOG_INFO, "Loading config api");

  lua_newtable(state);
  lua_newtable(state);
  lua_pushstring(state, "C-Space");
  lua_setfield(state, -2, "prefix");

  lua_pushlightuserdata(state, config);
  lua_pushcclosure(state, client_config_pmap, 1);
  lua_setfield(state, -2, "pmap");

  lua_pushlightuserdata(state, config);
  lua_pushcclosure(state, client_config_nmap, 1);
  lua_setfield(state, -2, "nmap");

  lua_pushlightuserdata(state, config);
  lua_pushcclosure(state, client_config_punmap, 1);
  lua_setfield(state, -2, "punmap");

  lua_pushlightuserdata(state, config);
  lua_pushcclosure(state, client_config_nunmap, 1);
  lua_setfield(state, -2, "nunmap");

  lua_setfield(state, -2, "keymap");

  lua_pushcfunction(state, client_config_quit);
  lua_setfield(state, -2, "quit");

  lua_setglobal(state, "mux");

  lua_pushcfunction(state, client_config_quit);
  int quit_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  *(client_config_keybind(config, (keycode){'d', 0}) + KEYBIND_BANK_PREFIX) =
      quit_ref;

  return 0;
}
