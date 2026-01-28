/**
 * @file calibre_network.c
 * @brief Network handling for Calibre Wireless protocol
 *
 * Implements UDP discovery and TCP connection management
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "calibre_internal.h"
#include "calibre_wireless.h"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#define LOG_TAG "calibre_net"
#define LOGI(...) ESP_LOGI(LOG_TAG, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(LOG_TAG, __VA_ARGS__)
#define LOGE(...) ESP_LOGE(LOG_TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(LOG_TAG, __VA_ARGS__)
#else
#include <netdb.h>
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

/* Broadcast ports for discovery */
static const uint16_t s_broadcast_ports[CALIBRE_BROADCAST_PORT_COUNT] = CALIBRE_BROADCAST_PORTS;

/* ============================================================================
 * Socket Utilities
 * ============================================================================ */

static int socket_set_nonblocking(int sock) {
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) return -1;
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

static int socket_set_blocking(int sock) {
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) return -1;
  return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
}

static int socket_set_timeout(int sock, uint32_t timeout_ms) {
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    return -1;
  }
  return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/* ============================================================================
 * UDP Discovery
 * ============================================================================ */

calibre_err_t calibre_start_discovery(calibre_conn_t* conn, uint16_t port) {
  if (!conn) {
    return CALIBRE_ERR_INVALID_ARG;
  }

  if (conn->discovery_active) {
    return CALIBRE_OK; /* Already running */
  }

  conn->listen_port = port ? port : CALIBRE_DEFAULT_PORT;

  /* Create UDP sockets for each broadcast port */
  for (int i = 0; i < CALIBRE_BROADCAST_PORT_COUNT; i++) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
      LOGE("Failed to create UDP socket: %s", strerror(errno));
      continue;
    }

    /* Allow address reuse */
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind to broadcast port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(s_broadcast_ports[i]);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      LOGW("Failed to bind to port %d: %s", s_broadcast_ports[i], strerror(errno));
      close(sock);
      continue;
    }

    /* Set non-blocking */
    socket_set_nonblocking(sock);

    conn->udp_sockets[i] = sock;
    LOGD("Listening on UDP port %d", s_broadcast_ports[i]);
  }

  /* Check if we bound to at least one port */
  int bound_count = 0;
  for (int i = 0; i < CALIBRE_BROADCAST_PORT_COUNT; i++) {
    if (conn->udp_sockets[i] >= 0) bound_count++;
  }

  if (bound_count == 0) {
    calibre_set_error(conn, CALIBRE_ERR_SOCKET, "Failed to bind to any discovery port");
    return CALIBRE_ERR_SOCKET;
  }

  conn->discovery_active = 1;
  conn->state = CALIBRE_STATE_DISCOVERY;
  LOGI("Discovery started on %d ports, advertising port %d", bound_count, conn->listen_port);

  return CALIBRE_OK;
}

void calibre_stop_discovery(calibre_conn_t* conn) {
  if (!conn) return;

  for (int i = 0; i < CALIBRE_BROADCAST_PORT_COUNT; i++) {
    if (conn->udp_sockets[i] >= 0) {
      close(conn->udp_sockets[i]);
      conn->udp_sockets[i] = -1;
    }
  }

  conn->discovery_active = 0;
  if (conn->state == CALIBRE_STATE_DISCOVERY) {
    conn->state = CALIBRE_STATE_IDLE;
  }
  LOGI("Discovery stopped");
}

/**
 * @brief Process UDP discovery messages
 *
 * Calibre broadcasts "hi there" message, we respond with our TCP port
 */
static calibre_err_t calibre_process_discovery(calibre_conn_t* conn) {
  char buf[64];
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  for (int i = 0; i < CALIBRE_BROADCAST_PORT_COUNT; i++) {
    if (conn->udp_sockets[i] < 0) continue;

    ssize_t len =
        recvfrom(conn->udp_sockets[i], buf, sizeof(buf) - 1, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &addr_len);

    if (len > 0) {
      buf[len] = '\0';
      LOGD("UDP received from %s:%d: %s", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buf);

      /* Check for Calibre discovery message */
      if (strstr(buf, "hi there") || strstr(buf, "calibre")) {
        /* Respond with our TCP port */
        char response[32];
        snprintf(response, sizeof(response), "%d", conn->listen_port);

        sendto(conn->udp_sockets[i], response, strlen(response), 0, (struct sockaddr*)&client_addr, addr_len);

        LOGI("Responded to Calibre discovery with port %d", conn->listen_port);
      }
    }
  }

  return CALIBRE_OK;
}

/* ============================================================================
 * TCP Connection
 * ============================================================================ */

calibre_err_t calibre_connect(calibre_conn_t* conn, const char* host, uint16_t port) {
  if (!conn || !host) {
    return CALIBRE_ERR_INVALID_ARG;
  }

  if (conn->connected) {
    calibre_disconnect(conn);
  }

  LOGI("Connecting to %s:%d", host, port);

  /* Create TCP socket */
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    calibre_set_error(conn, CALIBRE_ERR_SOCKET, strerror(errno));
    return CALIBRE_ERR_SOCKET;
  }

  /* Set timeouts */
  socket_set_timeout(sock, CALIBRE_CONNECT_TIMEOUT_MS);

  /* Resolve hostname */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
    /* Try DNS resolution */
    struct hostent* he = gethostbyname(host);
    if (!he) {
      close(sock);
      calibre_set_error(conn, CALIBRE_ERR_CONNECT, "DNS resolution failed");
      return CALIBRE_ERR_CONNECT;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
  }

  /* Connect */
  if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(sock);
    calibre_set_error(conn, CALIBRE_ERR_CONNECT, strerror(errno));
    return CALIBRE_ERR_CONNECT;
  }

  /* Set receive timeout for normal operation */
  socket_set_timeout(sock, CALIBRE_RECV_TIMEOUT_MS);

  conn->tcp_socket = sock;
  conn->server_addr = addr;
  conn->state = CALIBRE_STATE_HANDSHAKE;

  LOGI("TCP connected to %s:%d", host, port);
  return CALIBRE_OK;
}

void calibre_disconnect(calibre_conn_t* conn) {
  if (!conn) return;

  if (conn->tcp_socket >= 0) {
    close(conn->tcp_socket);
    conn->tcp_socket = -1;
  }

  conn->connected = 0;
  conn->state = CALIBRE_STATE_IDLE;
  calibre_buf_reset(&conn->recv_buf);

  LOGI("Disconnected");
}

/* ============================================================================
 * Message Protocol
 * ============================================================================ */

/**
 * @brief Send raw bytes over TCP
 */
static calibre_err_t tcp_send_all(calibre_conn_t* conn, const void* data, size_t len) {
  const uint8_t* ptr = (const uint8_t*)data;
  size_t remaining = len;

  while (remaining > 0) {
    ssize_t sent = send(conn->tcp_socket, ptr, remaining, 0);
    if (sent < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      calibre_set_error(conn, CALIBRE_ERR_SOCKET, strerror(errno));
      return CALIBRE_ERR_SOCKET;
    }
    if (sent == 0) {
      calibre_set_error(conn, CALIBRE_ERR_DISCONNECTED, "Connection closed");
      return CALIBRE_ERR_DISCONNECTED;
    }
    ptr += sent;
    remaining -= sent;
  }

  return CALIBRE_OK;
}

/**
 * @brief Receive exact number of bytes
 */
static calibre_err_t tcp_recv_exact(calibre_conn_t* conn, void* data, size_t len, uint32_t timeout_ms) {
  uint8_t* ptr = (uint8_t*)data;
  size_t remaining = len;

  struct timeval start, now;
  gettimeofday(&start, NULL);

  while (remaining > 0) {
    if (conn->cancelled) {
      return CALIBRE_ERR_CANCELLED;
    }

    /* Check timeout */
    gettimeofday(&now, NULL);
    uint32_t elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
    if (elapsed > timeout_ms) {
      return CALIBRE_ERR_TIMEOUT;
    }

    /* Use select for timeout */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(conn->tcp_socket, &rfds);

    struct timeval tv;
    uint32_t remaining_ms = timeout_ms - elapsed;
    tv.tv_sec = remaining_ms / 1000;
    tv.tv_usec = (remaining_ms % 1000) * 1000;

    int ret = select(conn->tcp_socket + 1, &rfds, NULL, NULL, &tv);
    if (ret < 0) {
      if (errno == EINTR) continue;
      calibre_set_error(conn, CALIBRE_ERR_SOCKET, strerror(errno));
      return CALIBRE_ERR_SOCKET;
    }
    if (ret == 0) {
      return CALIBRE_ERR_TIMEOUT;
    }

    ssize_t received = recv(conn->tcp_socket, ptr, remaining, 0);
    if (received < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        continue;
      }
      calibre_set_error(conn, CALIBRE_ERR_SOCKET, strerror(errno));
      return CALIBRE_ERR_SOCKET;
    }
    if (received == 0) {
      calibre_set_error(conn, CALIBRE_ERR_DISCONNECTED, "Connection closed");
      return CALIBRE_ERR_DISCONNECTED;
    }

    ptr += received;
    remaining -= received;
  }

  return CALIBRE_OK;
}

calibre_err_t calibre_send_msg(calibre_conn_t* conn, const char* opcode, const char* json_payload) {
  if (!conn || conn->tcp_socket < 0) {
    return CALIBRE_ERR_INVALID_ARG;
  }

  /* Build message: [opcode, payload] as JSON array */
  char msg_buf[CALIBRE_JSON_BUF_SIZE];
  int msg_len;

  if (json_payload && json_payload[0]) {
    msg_len = snprintf(msg_buf, sizeof(msg_buf), "[\"%s\", %s]", opcode, json_payload);
  } else {
    msg_len = snprintf(msg_buf, sizeof(msg_buf), "[\"%s\", {}]", opcode);
  }

  if (msg_len < 0 || msg_len >= (int)sizeof(msg_buf)) {
    calibre_set_error(conn, CALIBRE_ERR_NOMEM, "Message too large");
    return CALIBRE_ERR_NOMEM;
  }

  /* Send length prefix as ASCII decimal */
  char len_prefix[16];
  int prefix_len = snprintf(len_prefix, sizeof(len_prefix), "%d", msg_len);

  LOGD("Sending: %s%s", len_prefix, msg_buf);

  calibre_err_t err = tcp_send_all(conn, len_prefix, prefix_len);
  if (err != CALIBRE_OK) return err;

  return tcp_send_all(conn, msg_buf, msg_len);
}

calibre_err_t calibre_recv_msg(calibre_conn_t* conn, char* opcode_buf, size_t opcode_size, char** json_out,
                               uint32_t timeout_ms) {
  if (!conn || conn->tcp_socket < 0) {
    return CALIBRE_ERR_INVALID_ARG;
  }

  /* Read length prefix (ASCII decimal terminated by non-digit) */
  char len_buf[16];
  size_t len_pos = 0;

  while (len_pos < sizeof(len_buf) - 1) {
    calibre_err_t err = tcp_recv_exact(conn, &len_buf[len_pos], 1, timeout_ms);
    if (err != CALIBRE_OK) return err;

    if (len_buf[len_pos] < '0' || len_buf[len_pos] > '9') {
      /* Non-digit means we have the full length, put back the char */
      break;
    }
    len_pos++;
  }

  len_buf[len_pos] = '\0';
  size_t msg_len = strtoul(len_buf, NULL, 10);

  if (msg_len == 0 || msg_len > CALIBRE_MAX_MSG_LEN) {
    calibre_set_error(conn, CALIBRE_ERR_PROTOCOL, "Invalid message length");
    return CALIBRE_ERR_PROTOCOL;
  }

  /* Ensure buffer is large enough */
  if (msg_len >= conn->recv_buf.capacity) {
    /* Reallocate buffer if needed */
    calibre_buf_free(&conn->recv_buf);
    if (calibre_buf_init(&conn->recv_buf, msg_len + 1) != CALIBRE_OK) {
      return CALIBRE_ERR_NOMEM;
    }
  }

  calibre_buf_reset(&conn->recv_buf);

  /* We already read the first byte of the message (the '[') */
  conn->recv_buf.data[0] = len_buf[len_pos]; /* This was the non-digit we stopped at */

  /* Read rest of message */
  calibre_err_t err = tcp_recv_exact(conn, conn->recv_buf.data + 1, msg_len - 1, timeout_ms);
  if (err != CALIBRE_OK) return err;

  conn->recv_buf.data[msg_len] = '\0';
  conn->recv_buf.len = msg_len;

  LOGD("Received: %s", (char*)conn->recv_buf.data);

  /* Parse JSON array: ["OPCODE", {...}] */
  char* json = (char*)conn->recv_buf.data;

  /* Find first string (opcode) */
  char* op_start = strchr(json, '"');
  if (!op_start) {
    calibre_set_error(conn, CALIBRE_ERR_JSON_PARSE, "Missing opcode");
    return CALIBRE_ERR_JSON_PARSE;
  }
  op_start++; /* Skip opening quote */

  char* op_end = strchr(op_start, '"');
  if (!op_end) {
    calibre_set_error(conn, CALIBRE_ERR_JSON_PARSE, "Malformed opcode");
    return CALIBRE_ERR_JSON_PARSE;
  }

  size_t op_len = op_end - op_start;
  if (op_len >= opcode_size) {
    op_len = opcode_size - 1;
  }
  memcpy(opcode_buf, op_start, op_len);
  opcode_buf[op_len] = '\0';

  /* Find payload (after comma and whitespace) */
  if (json_out) {
    char* payload = op_end + 1;
    while (*payload && (*payload == ',' || *payload == ' ' || *payload == '\t')) {
      payload++;
    }
    *json_out = payload;
  }

  return CALIBRE_OK;
}

/* ============================================================================
 * Main Processing Loop
 * ============================================================================ */

calibre_err_t calibre_process(calibre_conn_t* conn, uint32_t timeout_ms) {
  if (!conn) {
    return CALIBRE_ERR_INVALID_ARG;
  }

  calibre_err_t err = CALIBRE_OK;

  /* Process UDP discovery */
  if (conn->discovery_active) {
    calibre_process_discovery(conn);
  }

  /* Process TCP connection */
  if (conn->tcp_socket >= 0 && conn->state >= CALIBRE_STATE_HANDSHAKE) {
    /* Check for incoming data */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(conn->tcp_socket, &rfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(conn->tcp_socket + 1, &rfds, NULL, NULL, &tv);

    if (ret > 0) {
      /* Data available, receive message */
      char opcode[32];
      char* json = NULL;

      err = calibre_recv_msg(conn, opcode, sizeof(opcode), &json, timeout_ms ? timeout_ms : CALIBRE_RECV_TIMEOUT_MS);

      if (err != CALIBRE_OK) {
        if (err == CALIBRE_ERR_DISCONNECTED) {
          conn->connected = 0;
          conn->state = CALIBRE_STATE_IDLE;
        }
        return err;
      }

      /* Dispatch to appropriate handler */
      if (strcmp(opcode, "GET_INITIALIZATION_INFO") == 0) {
        err = calibre_handle_init_info(conn, json);
      } else if (strcmp(opcode, "SET_LIBRARY_INFO") == 0) {
        err = calibre_handle_library_info(conn, json);
      } else if (strcmp(opcode, "FREE_SPACE") == 0) {
        err = calibre_handle_free_space(conn, json);
      } else if (strcmp(opcode, "SEND_BOOK") == 0) {
        err = calibre_handle_send_book(conn, json);
      } else if (strcmp(opcode, "SEND_BOOKLISTS") == 0) {
        err = calibre_handle_booklists(conn, json);
      } else if (strcmp(opcode, "DISPLAY_MESSAGE") == 0) {
        err = calibre_handle_message(conn, json);
      } else if (strcmp(opcode, "NOOP") == 0) {
        err = calibre_handle_noop(conn);
      } else if (strcmp(opcode, "OK") == 0) {
        /* Server acknowledged our message */
        LOGD("Server acknowledged");
      } else {
        LOGW("Unknown opcode: %s", opcode);
      }
    } else if (ret < 0 && errno != EINTR) {
      calibre_set_error(conn, CALIBRE_ERR_SOCKET, strerror(errno));
      return CALIBRE_ERR_SOCKET;
    }
  }

  return err;
}
