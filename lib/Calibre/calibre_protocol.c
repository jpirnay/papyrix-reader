/**
 * @file calibre_protocol.c
 * @brief Protocol message handlers for Calibre Wireless
 *
 * Implements handlers for all Calibre protocol messages:
 * - GET_INITIALIZATION_INFO
 * - SET_LIBRARY_INFO
 * - FREE_SPACE
 * - SEND_BOOK
 * - SEND_BOOKLISTS
 * - DISPLAY_MESSAGE
 * - NOOP
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "calibre_internal.h"
#include "calibre_wireless.h"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#define LOG_TAG "calibre_proto"
#define LOGI(...) ESP_LOGI(LOG_TAG, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(LOG_TAG, __VA_ARGS__)
#define LOGE(...) ESP_LOGE(LOG_TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)             \
  printf("[I] " __VA_ARGS__); \
  printf("\n")
#define LOGW(...)             \
  printf("[W] " __VA_ARGS__); \
  printf("\n")
#define LOGE(...)             \
  printf("[E] " __VA_ARGS__); \
  printf("\n")
#define LOGD(...)             \
  printf("[D] " __VA_ARGS__); \
  printf("\n")
#endif

/* ============================================================================
 * Storage Helper Functions
 * ============================================================================ */

calibre_err_t calibre_get_storage_info(calibre_conn_t* conn, uint64_t* total_bytes, uint64_t* free_bytes) {
  if (!conn) {
    return CALIBRE_ERR_INVALID_ARG;
  }

  /* Return reasonable estimates for storage info.
   * Calibre uses this for display purposes only.
   * Getting actual SD card free space is platform-specific. */
  if (total_bytes) {
    *total_bytes = 16ULL * 1024 * 1024 * 1024; /* 16GB typical SD card */
  }
  if (free_bytes) {
    *free_bytes = 8ULL * 1024 * 1024 * 1024; /* 8GB free estimate */
  }
  return CALIBRE_OK;
}

/**
 * @brief Ensure directory exists (create if necessary)
 */
static int ensure_dir(const char* path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    return S_ISDIR(st.st_mode) ? 0 : -1;
  }
  return mkdir(path, 0755);
}

/**
 * @brief Create parent directories for a file path
 */
static int mkdir_p(const char* path) {
  char tmp[CALIBRE_MAX_PATH_LEN];
  calibre_strlcpy(tmp, path, sizeof(tmp));

  /* Find last slash (parent dir) */
  char* last_slash = strrchr(tmp, '/');
  if (!last_slash || last_slash == tmp) {
    return 0;
  }
  *last_slash = '\0';

  /* Create parent directories */
  char* p = tmp;
  if (*p == '/') p++;

  while (*p) {
    if (*p == '/') {
      *p = '\0';
      if (ensure_dir(tmp) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
    p++;
  }

  return ensure_dir(tmp);
}

/* ============================================================================
 * JSON Response Builders
 * ============================================================================ */

/**
 * @brief Build extensions list as JSON array
 */
static void build_extensions_json(const calibre_device_config_t* config, char* buf, size_t size) {
  int pos = 0;
  pos += snprintf(buf + pos, size - pos, "[");

  for (int i = 0; i < config->extension_count && pos < (int)size - 10; i++) {
    if (i > 0) {
      pos += snprintf(buf + pos, size - pos, ", ");
    }
    pos += snprintf(buf + pos, size - pos, "\"%s\"", config->extensions[i]);
  }

  snprintf(buf + pos, size - pos, "]");
}

/* ============================================================================
 * Protocol Handlers
 * ============================================================================ */

/**
 * @brief Handle GET_INITIALIZATION_INFO - Initial handshake
 *
 * Calibre sends this after TCP connection. We respond with device info.
 */
calibre_err_t calibre_handle_init_info(calibre_conn_t* conn, const char* json) {
  LOGI("Handling GET_INITIALIZATION_INFO");

  json_parser_t p;
  json_parser_init(&p, json, strlen(json));

  /* Extract challenge for password auth */
  size_t challenge_len;
  const char* challenge = json_find_string(&p, "passwordChallenge", &challenge_len);

  /* Extract server protocol version */
  int64_t protocol_version = 0;
  json_find_int(&p, "serverProtocolVersion", &protocol_version);

  if (protocol_version > CALIBRE_PROTOCOL_VERSION) {
    LOGW("Server protocol version %lld > client %d", (long long)protocol_version, CALIBRE_PROTOCOL_VERSION);
  }

  /* Build response */
  char ext_json[256];
  build_extensions_json(&conn->config, ext_json, sizeof(ext_json));

  char response[1024];
  int len = snprintf(response, sizeof(response),
                     "{"
                     "\"appName\": \"Papyrix Reader\","
                     "\"acceptedExtensions\": %s,"
                     "\"cacheUsesLpaths\": %s,"
                     "\"canAcceptLibraryInfo\": true,"
                     "\"canDeleteMultipleBooks\": false,"
                     "\"canReceiveBookBinary\": true,"
                     "\"canSendOkToSendbook\": true,"
                     "\"canStreamBooks\": true,"
                     "\"canStreamMetadata\": true,"
                     "\"canUseCachedMetadata\": true,"
                     "\"ccVersionNumber\": 128,"
                     "\"coverHeight\": 240,"
                     "\"deviceKind\": \"Papyrix E-Ink Reader\","
                     "\"deviceName\": \"%s\","
                     "\"extensionPathLengths\": {},"
                     "\"maxBookContentPacketLen\": %d,"
                     "\"passwordHash\": \"%s\","
                     "\"useUuidFileNames\": false,"
                     "\"versionOK\": true"
                     "}",
                     ext_json, conn->config.cache_uses_lpath ? "true" : "false", conn->config.device_name,
                     CALIBRE_FILE_CHUNK_SIZE, conn->password_hash[0] ? conn->password_hash : "");

  if (len < 0 || len >= (int)sizeof(response)) {
    calibre_set_error(conn, CALIBRE_ERR_NOMEM, "Response too large");
    return CALIBRE_ERR_NOMEM;
  }

  calibre_err_t err = calibre_send_msg(conn, "OK", response);
  if (err == CALIBRE_OK) {
    conn->state = CALIBRE_STATE_CONNECTED;
    conn->connected = 1;
    LOGI("Handshake complete, connected to Calibre");
  }

  return err;
}

/**
 * @brief Handle SET_LIBRARY_INFO - Library metadata from Calibre
 */
calibre_err_t calibre_handle_library_info(calibre_conn_t* conn, const char* json) {
  LOGD("Handling SET_LIBRARY_INFO");

  json_parser_t p;
  json_parser_init(&p, json, strlen(json));

  /* Extract library info */
  size_t len;
  const char* name = json_find_string(&p, "libraryName", &len);
  if (name && len < sizeof(conn->library_name)) {
    memcpy(conn->library_name, name, len);
    conn->library_name[len] = '\0';
  }

  const char* uuid = json_find_string(&p, "libraryUUID", &len);
  if (uuid && len < sizeof(conn->library_uuid)) {
    memcpy(conn->library_uuid, uuid, len);
    conn->library_uuid[len] = '\0';
  }

  LOGI("Library: %s (%s)", conn->library_name, conn->library_uuid);

  /* Acknowledge */
  return calibre_send_msg(conn, "OK", "{}");
}

/**
 * @brief Handle FREE_SPACE - Report available storage
 */
calibre_err_t calibre_handle_free_space(calibre_conn_t* conn, const char* json) {
  LOGD("Handling FREE_SPACE");

  uint64_t total, free_space;
  calibre_get_storage_info(conn, &total, &free_space);

  char response[128];
  snprintf(response, sizeof(response), "{\"free_space_on_device\": %llu}", (unsigned long long)free_space);

  return calibre_send_msg(conn, "OK", response);
}

/**
 * @brief Handle SEND_BOOKLISTS - Return list of books on device
 *
 * For simplicity, we report an empty list. Calibre will then send
 * all books it wants to sync.
 */
calibre_err_t calibre_handle_booklists(calibre_conn_t* conn, const char* json) {
  LOGD("Handling SEND_BOOKLISTS");

  /* Report empty book list - Calibre will send everything */
  char response[128];
  snprintf(response, sizeof(response), "{\"count\": 0, \"willStream\": true, \"willScan\": false}");

  return calibre_send_msg(conn, "OK", response);
}

/**
 * @brief Handle SEND_BOOK - Receive book file from Calibre
 *
 * This is the main book transfer handler. Uses streaming to minimize
 * memory usage - file is written directly to SD card in chunks.
 */
calibre_err_t calibre_handle_send_book(calibre_conn_t* conn, const char* json) {
  LOGI("Handling SEND_BOOK");

  json_parser_t p;
  json_parser_init(&p, json, strlen(json));

  /* Parse book metadata */
  calibre_book_meta_t meta;
  memset(&meta, 0, sizeof(meta));

  size_t len;

  const char* lpath = json_find_string(&p, "lpath", &len);
  if (lpath && len < sizeof(meta.lpath)) {
    memcpy(meta.lpath, lpath, len);
    meta.lpath[len] = '\0';
  }

  const char* title = json_find_string(&p, "title", &len);
  if (title && len < sizeof(meta.title)) {
    memcpy(meta.title, title, len);
    meta.title[len] = '\0';
  }

  const char* authors = json_find_string(&p, "authors", &len);
  if (authors && len < sizeof(meta.authors)) {
    memcpy(meta.authors, authors, len);
    meta.authors[len] = '\0';
  }

  const char* uuid = json_find_string(&p, "uuid", &len);
  if (uuid && len < sizeof(meta.uuid)) {
    memcpy(meta.uuid, uuid, len);
    meta.uuid[len] = '\0';
  }

  int64_t size = 0;
  json_find_int(&p, "length", &size);
  meta.size = (uint64_t)size;

  int64_t id = 0;
  json_find_int(&p, "calibre_id", &id);
  meta.calibre_id = (uint32_t)id;

  LOGI("Receiving book: %s (%s) - %llu bytes", meta.title, meta.lpath, (unsigned long long)meta.size);

  /* Build full path */
  char full_path[CALIBRE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", conn->books_dir, meta.lpath);

  /* Create parent directories */
  if (mkdir_p(full_path) != 0) {
    LOGE("Failed to create directory for %s", full_path);
    char error_resp[128];
    snprintf(error_resp, sizeof(error_resp), "{\"errorMessage\": \"Failed to create directory\"}");
    calibre_send_msg(conn, "ERROR", error_resp);
    return CALIBRE_ERR_WRITE_FILE;
  }

  /* Open file for writing */
  int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    LOGE("Failed to open file %s: %s", full_path, strerror(errno));
    char error_resp[128];
    snprintf(error_resp, sizeof(error_resp), "{\"errorMessage\": \"Failed to open file: %s\"}", strerror(errno));
    calibre_send_msg(conn, "ERROR", error_resp);
    return CALIBRE_ERR_WRITE_FILE;
  }

  /* Confirm we're ready to receive */
  calibre_err_t err = calibre_send_msg(conn, "OK", "{\"willAccept\": true}");
  if (err != CALIBRE_OK) {
    close(fd);
    return err;
  }

  /* Receive file data in chunks */
  uint64_t received = 0;
  uint8_t* chunk_buf = (uint8_t*)malloc(CALIBRE_FILE_CHUNK_SIZE);
  if (!chunk_buf) {
    close(fd);
    return CALIBRE_ERR_NOMEM;
  }

  conn->state = CALIBRE_STATE_RECEIVING_BOOK;

  while (received < meta.size) {
    if (conn->cancelled) {
      err = CALIBRE_ERR_CANCELLED;
      break;
    }

    /* Receive chunk header (length prefix) */
    char opcode[32];
    char* chunk_json = NULL;

    err = calibre_recv_msg(conn, opcode, sizeof(opcode), &chunk_json, CALIBRE_RECV_TIMEOUT_MS);
    if (err != CALIBRE_OK) {
      break;
    }

    /* Check for book data message */
    if (strcmp(opcode, "BOOK_DATA") != 0 && strcmp(opcode, "OK") != 0) {
      LOGW("Unexpected opcode during transfer: %s", opcode);
      err = CALIBRE_ERR_PROTOCOL;
      break;
    }

    /* Parse chunk info */
    json_parser_t cp;
    json_parser_init(&cp, chunk_json, strlen(chunk_json));

    int64_t chunk_len = 0;
    json_find_int(&cp, "length", &chunk_len);

    bool is_last = false;
    json_find_bool(&cp, "isLast", &is_last);

    if (chunk_len <= 0 || chunk_len > CALIBRE_FILE_CHUNK_SIZE) {
      LOGE("Invalid chunk length: %lld", (long long)chunk_len);
      err = CALIBRE_ERR_PROTOCOL;
      break;
    }

    /* Acknowledge chunk header */
    calibre_send_msg(conn, "OK", "{}");

    /* Receive raw binary data */
    size_t to_receive = (size_t)chunk_len;
    size_t buf_pos = 0;

    while (buf_pos < to_receive) {
      ssize_t n = recv(conn->tcp_socket, chunk_buf + buf_pos, to_receive - buf_pos, 0);
      if (n <= 0) {
        if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
          continue;
        }
        err = (n == 0) ? CALIBRE_ERR_DISCONNECTED : CALIBRE_ERR_SOCKET;
        break;
      }
      buf_pos += n;
    }

    if (err != CALIBRE_OK) break;

    /* Write to file */
    ssize_t written = write(fd, chunk_buf, to_receive);
    if (written != (ssize_t)to_receive) {
      LOGE("Write failed: %s", strerror(errno));
      err = CALIBRE_ERR_WRITE_FILE;
      break;
    }

    received += to_receive;

    /* Progress callback */
    if (conn->callbacks.on_progress) {
      if (!conn->callbacks.on_progress(conn->callbacks.user_ctx, received, meta.size)) {
        err = CALIBRE_ERR_CANCELLED;
        break;
      }
    }

    LOGD("Progress: %llu / %llu bytes", (unsigned long long)received, (unsigned long long)meta.size);

    /* Send acknowledgment */
    calibre_send_msg(conn, "OK", "{}");

    if (is_last) break;
  }

  free(chunk_buf);
  close(fd);

  conn->state = CALIBRE_STATE_CONNECTED;

  if (err == CALIBRE_OK && received == meta.size) {
    LOGI("Book received successfully: %s", meta.title);

    /* Notify callback */
    if (conn->callbacks.on_book) {
      conn->callbacks.on_book(conn->callbacks.user_ctx, &meta, full_path);
    }
  } else {
    /* Clean up partial file on error */
    unlink(full_path);
    LOGE("Book transfer failed: %s", calibre_err_str(err));
  }

  return err;
}

/**
 * @brief Handle DISPLAY_MESSAGE - Show message from Calibre
 */
calibre_err_t calibre_handle_message(calibre_conn_t* conn, const char* json) {
  json_parser_t p;
  json_parser_init(&p, json, strlen(json));

  size_t len;
  const char* msg = json_find_string(&p, "message", &len);

  if (msg && len > 0) {
    /* Temporary null-terminate for logging */
    char msg_buf[256];
    size_t copy_len = len < sizeof(msg_buf) - 1 ? len : sizeof(msg_buf) - 1;
    memcpy(msg_buf, msg, copy_len);
    msg_buf[copy_len] = '\0';

    LOGI("Calibre message: %s", msg_buf);

    /* Notify callback */
    if (conn->callbacks.on_message) {
      conn->callbacks.on_message(conn->callbacks.user_ctx, msg_buf);
    }
  }

  return calibre_send_msg(conn, "OK", "{}");
}

/**
 * @brief Handle NOOP - Keep-alive ping
 */
calibre_err_t calibre_handle_noop(calibre_conn_t* conn) {
  LOGD("NOOP received");
  return calibre_send_msg(conn, "OK", "{}");
}
