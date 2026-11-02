#include "ssh.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <libssh2.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>

#include <cstring>
#include <string>

namespace mt {
namespace ssh {
namespace {

volatile State g_state = State::Idle;
volatile uint32_t g_generation = 0;
char g_message[160] = "";

// The request, copied in by connect().
std::string g_user, g_host, g_known, g_pass;
uint16_t g_port = 22;
volatile bool g_wantConnect = false, g_wantClose = false;
volatile int g_trust = -1;  // -1 waiting, 0 no, 1 yes
volatile bool g_havePass = false;

std::string g_keyPem, g_pub;
char g_hostKey[64] = "", g_hostKeyType[32] = "";
volatile int g_cols = 80, g_rows = 24;
volatile bool g_resized = false;

StreamBufferHandle_t g_rx = nullptr, g_tx = nullptr;
TaskHandle_t g_task = nullptr;

void setState(State s, const char *fmt = nullptr, ...) {
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_message, sizeof g_message, fmt, ap);
    va_end(ap);
  }
  g_state = s;
  g_generation++;
}

// ---------------------------------------------------------------- keys --

struct Rng {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  Rng() {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, (const uint8_t *)"t5m-ssh", 7);
  }
  ~Rng() {
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
  }
};

void putString(std::string &out, const uint8_t *data, size_t n) {
  const uint8_t len[4] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n};
  out.append((const char *)len, 4);
  out.append((const char *)data, n);
}

// "ecdsa-sha2-nistp256 AAAA... tab5-multitool" from the PEM private key.
std::string publicLine(const std::string &pem) {
  if (pem.empty()) return "";
  Rng rng;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  std::string line;
  if (mbedtls_pk_parse_key(&pk, (const uint8_t *)pem.c_str(), pem.size() + 1, nullptr, 0, mbedtls_ctr_drbg_random,
                           &rng.drbg) == 0 &&
      mbedtls_pk_get_type(&pk) == MBEDTLS_PK_ECKEY) {
    mbedtls_ecp_keypair *kp = mbedtls_pk_ec(pk);
    uint8_t q[65];
    size_t qlen = 0;
    if (mbedtls_ecp_point_write_binary(&kp->MBEDTLS_PRIVATE(grp), &kp->MBEDTLS_PRIVATE(Q),
                                       MBEDTLS_ECP_PF_UNCOMPRESSED, &qlen, q, sizeof q) == 0) {
      std::string blob;
      putString(blob, (const uint8_t *)"ecdsa-sha2-nistp256", 19);
      putString(blob, (const uint8_t *)"nistp256", 8);
      putString(blob, q, qlen);
      uint8_t b64[256];
      size_t n = 0;
      mbedtls_base64_encode(b64, sizeof b64, &n, (const uint8_t *)blob.data(), blob.size());
      line = "ecdsa-sha2-nistp256 " + std::string((const char *)b64, n) + " tab5-multitool";
    }
  }
  mbedtls_pk_free(&pk);
  return line;
}

// --------------------------------------------------------------- session --

int g_sock = -1;
LIBSSH2_SESSION *g_session = nullptr;
LIBSSH2_CHANNEL *g_channel = nullptr;

void teardown() {
  if (g_channel) {
    libssh2_channel_free(g_channel);
    g_channel = nullptr;
  }
  if (g_session) {
    libssh2_session_disconnect(g_session, "bye");
    libssh2_session_free(g_session);
    g_session = nullptr;
  }
  if (g_sock >= 0) {
    close(g_sock);
    g_sock = -1;
  }
  g_pass.assign(g_pass.size(), '\0');
  g_pass.clear();
}

bool resolve(const std::string &host, uint16_t port, sockaddr_in *out) {
  memset(out, 0, sizeof *out);
  out->sin_family = AF_INET;
  out->sin_port = htons(port);
  if (inet_aton(host.c_str(), &out->sin_addr)) return true;
  addrinfo hints = {}, *res = nullptr;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
    out->sin_addr = ((sockaddr_in *)res->ai_addr)->sin_addr;
    freeaddrinfo(res);
    return true;
  }
  // name.local: ask mDNS.
  const size_t dot = host.rfind(".local");
  if (dot != std::string::npos && dot + 6 == host.size()) {
    MDNS.begin("tab5-multitool");
    IPAddress ip = MDNS.queryHost(host.substr(0, dot).c_str(), 3000);
    if (ip != IPAddress()) {
      out->sin_addr.s_addr = (uint32_t)ip;
      return true;
    }
  }
  return false;
}

bool tcpConnect(const sockaddr_in &addr) {
  g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (g_sock < 0) return false;
  fcntl(g_sock, F_SETFL, fcntl(g_sock, F_GETFL) | O_NONBLOCK);
  int r = ::connect(g_sock, (const sockaddr *)&addr, sizeof addr);
  if (r < 0 && errno != EINPROGRESS) return false;
  fd_set w;
  FD_ZERO(&w);
  FD_SET(g_sock, &w);
  timeval tv = {10, 0};
  if (select(g_sock + 1, nullptr, &w, nullptr, &tv) <= 0) return false;
  int err = 0;
  socklen_t len = sizeof err;
  getsockopt(g_sock, SOL_SOCKET, SO_ERROR, &err, &len);
  if (err) return false;
  fcntl(g_sock, F_SETFL, fcntl(g_sock, F_GETFL) & ~O_NONBLOCK);
  int one = 1;
  setsockopt(g_sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);  // keystrokes go out at once
  return true;
}

// libssh2 1.11.1's mbedTLS backend copies an EC key into a buffer one byte
// longer and leaves that byte unset, but mbedTLS only reads PEM that ends in a
// zero. Zeroed allocations make that byte zero.
void *zalloc(size_t n, void **) { return calloc(1, n); }
void zfree(void *p, void **) { free(p); }
void *zrealloc(void *p, size_t n, void **) { return realloc(p, n); }

const char *sessionError() {
  char *msg = nullptr;
  libssh2_session_last_error(g_session, &msg, nullptr, 0);
  return msg ? msg : "unknown error";
}

// Keyboard-interactive (what macOS asks for passwords by default): answer
// every prompt with the password.
void kbdCallback(const char *, int, const char *, int, int num_prompts, const LIBSSH2_USERAUTH_KBDINT_PROMPT *,
                 LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses, void **) {
  for (int i = 0; i < num_prompts; i++) {
    responses[i].text = strdup(g_pass.c_str());
    responses[i].length = g_pass.size();
  }
}

bool waitFor(volatile bool &flag) {
  while (!flag && !g_wantClose) vTaskDelay(pdMS_TO_TICKS(20));
  return flag && !g_wantClose;
}

bool authenticate() {
  const char *methods = libssh2_userauth_list(g_session, g_user.c_str(), g_user.size());
  if (!methods) return libssh2_userauth_authenticated(g_session);  // "none" worked
  if (strstr(methods, "publickey") && !g_keyPem.empty()) {
    setState(State::Connecting, "Trying this Tab5's key...");
    // Pass the public key too: libssh2's mbedTLS backend can only work it out
    // from an RSA private key.
    if (libssh2_userauth_publickey_frommemory(g_session, g_user.c_str(), g_user.size(), g_pub.c_str(), g_pub.size(),
                                              g_keyPem.c_str(), g_keyPem.size(), nullptr) == 0)
      return true;
    Serial.printf("ssh: key not accepted: %s\n", sessionError());
  }
  const bool pw = strstr(methods, "password"), kbd = strstr(methods, "keyboard-interactive");
  if (!pw && !kbd) {
    setState(State::Closed, "The server only accepts: %s", methods);
    return false;
  }
  for (int tries = 0; tries < 3; tries++) {
    g_havePass = false;
    setState(State::Password, tries ? "Permission denied, please try again." : "");
    if (!waitFor((volatile bool &)g_havePass)) return false;
    setState(State::Connecting, "Authenticating...");
    int r = -1;
    if (pw) r = libssh2_userauth_password(g_session, g_user.c_str(), g_pass.c_str());
    if (r != 0 && kbd) r = libssh2_userauth_keyboard_interactive(g_session, g_user.c_str(), kbdCallback);
    g_pass.assign(g_pass.size(), '\0');
    g_pass.clear();
    if (r == 0) return true;
  }
  setState(State::Closed, "Permission denied.");
  return false;
}

void session() {
  setState(State::Connecting, "Connecting to %s...", g_host.c_str());
  // Just after boot Wi-Fi may still be joining.
  for (int i = 0; i < 150 && WiFi.status() != WL_CONNECTED && !g_wantClose; i++) {
    if (i == 5) setState(State::Connecting, "Waiting for Wi-Fi...");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (WiFi.status() != WL_CONNECTED) return setState(State::Closed, "Wi-Fi is not connected.");
  sockaddr_in addr;
  if (!resolve(g_host, g_port, &addr)) return setState(State::Closed, "Could not find %s.", g_host.c_str());
  if (!tcpConnect(addr)) return setState(State::Closed, "Could not connect to %s port %u.", g_host.c_str(), g_port);

  g_session = libssh2_session_init_ex(zalloc, zfree, zrealloc, nullptr);
  libssh2_session_set_blocking(g_session, 1);
  libssh2_session_set_timeout(g_session, 20000);
  if (libssh2_session_handshake(g_session, g_sock) != 0)
    return setState(State::Closed, "Key exchange failed: %s", sessionError());

  // Host key: SHA-256 fingerprint, as OpenSSH shows it.
  const uint8_t *hash = (const uint8_t *)libssh2_hostkey_hash(g_session, LIBSSH2_HOSTKEY_HASH_SHA256);
  size_t keyLen = 0;
  int keyType = 0;
  libssh2_session_hostkey(g_session, &keyLen, &keyType);
  const char *types[] = {"unknown", "RSA", "DSA", "ECDSA", "ECDSA", "ECDSA", "ED25519"};
  snprintf(g_hostKeyType, sizeof g_hostKeyType, "%s", keyType >= 0 && keyType <= 6 ? types[keyType] : "unknown");
  uint8_t b64[48];
  size_t n = 0;
  mbedtls_base64_encode(b64, sizeof b64, &n, hash, 32);
  while (n && b64[n - 1] == '=') n--;  // OpenSSH drops the padding
  snprintf(g_hostKey, sizeof g_hostKey, "SHA256:%.*s", (int)n, b64);
  if (!g_known.empty() && g_known != g_hostKey)
    return setState(State::Closed, "THE HOST KEY HAS CHANGED (now %s). Not connecting.", g_hostKey);
  if (g_known.empty()) {
    g_trust = -1;
    setState(State::HostKey, "");
    while (g_trust < 0 && !g_wantClose) vTaskDelay(pdMS_TO_TICKS(20));
    if (g_trust != 1) return setState(State::Closed, "Host key not trusted.");
  }

  if (!authenticate()) {
    if (g_state != State::Closed) setState(State::Closed, "Disconnected.");
    return;
  }

  setState(State::Connecting, "Starting a shell...");
  g_channel = libssh2_channel_open_session(g_session);
  if (!g_channel) return setState(State::Closed, "Could not open a channel: %s", sessionError());
  libssh2_channel_request_pty_ex(g_channel, "xterm-256color", 14, nullptr, 0, g_cols, g_rows, 0, 0);
  if (libssh2_channel_shell(g_channel) != 0) return setState(State::Closed, "No shell: %s", sessionError());
  g_resized = false;
  setState(State::Open, "Connected to %s.", g_host.c_str());

  libssh2_session_set_blocking(g_session, 0);
  uint8_t buf[1024];
  uint8_t out[512];
  size_t outLen = 0, outOff = 0;
  while (!g_wantClose) {
    bool idle = true;
    ssize_t r = libssh2_channel_read(g_channel, (char *)buf, sizeof buf);
    if (r > 0) {
      // Don't drop output: wait for the screen to take it.
      size_t done = 0;
      while (done < (size_t)r && !g_wantClose) done += xStreamBufferSend(g_rx, buf + done, r - done, pdMS_TO_TICKS(50));
      idle = false;
    } else if (r < 0 && r != LIBSSH2_ERROR_EAGAIN) {
      return setState(State::Closed, "Connection lost: %s", sessionError());
    }
    r = libssh2_channel_read_stderr(g_channel, (char *)buf, sizeof buf);
    if (r > 0) {
      xStreamBufferSend(g_rx, buf, r, pdMS_TO_TICKS(50));
      idle = false;
    }
    if (outOff == outLen) {
      outLen = xStreamBufferReceive(g_tx, out, sizeof out, 0);
      outOff = 0;
    }
    if (outOff < outLen) {
      r = libssh2_channel_write(g_channel, (const char *)out + outOff, outLen - outOff);
      if (r > 0) outOff += r, idle = false;
      else if (r < 0 && r != LIBSSH2_ERROR_EAGAIN) return setState(State::Closed, "Connection lost: %s", sessionError());
    }
    if (g_resized) {
      g_resized = false;
      libssh2_channel_request_pty_size(g_channel, g_cols, g_rows);
    }
    if (libssh2_channel_eof(g_channel)) return setState(State::Closed, "Connection to %s closed.", g_host.c_str());
    if (idle) {
      // Sleep until the server sends something, or 20 ms for keys.
      fd_set rd;
      FD_ZERO(&rd);
      FD_SET(g_sock, &rd);
      timeval tv = {0, 20000};
      select(g_sock + 1, &rd, nullptr, nullptr, &tv);
    }
  }
  setState(State::Closed, "Disconnected.");
}

void task(void *) {
  libssh2_init(0);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!g_wantConnect) continue;
    g_wantConnect = false;
    g_wantClose = false;
    xStreamBufferReset(g_rx);
    xStreamBufferReset(g_tx);
    session();
    teardown();
    g_wantClose = false;
  }
}

}  // namespace

State state() { return g_state; }
uint32_t generation() { return g_generation; }
const char *message() { return g_message; }

void setDeviceKey(const char *pem) {
  g_keyPem = pem ? pem : "";
  g_pub = publicLine(g_keyPem);
}
const char *publicKey() { return g_pub.c_str(); }

bool generateDeviceKey(char *buf, size_t cap) {
  Rng rng;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) == 0 &&
            mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk), mbedtls_ctr_drbg_random, &rng.drbg) == 0 &&
            mbedtls_pk_write_key_pem(&pk, (uint8_t *)buf, cap) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}

void connect(const char *user, const char *host, uint16_t port, const char *knownKey) {
  if (!g_task) {
    g_rx = xStreamBufferCreate(32768, 1);
    g_tx = xStreamBufferCreate(4096, 1);
    // Internal RAM stack: mbedTLS and libssh2 are deep.
    xTaskCreatePinnedToCore(task, "ssh", 24576, nullptr, 4, &g_task, 1);
  }
  if (g_state == State::Connecting || g_state == State::HostKey || g_state == State::Password ||
      g_state == State::Open)
    return;
  g_user = user;
  g_host = host;
  g_port = port;
  g_known = knownKey ? knownKey : "";
  g_hostKey[0] = g_hostKeyType[0] = 0;
  g_wantConnect = true;
  setState(State::Connecting, "Connecting to %s...", host);
  xTaskNotifyGive(g_task);
}

const char *hostKey() { return g_hostKey; }
const char *hostKeyType() { return g_hostKeyType; }
void trustHostKey(bool yes) { g_trust = yes ? 1 : 0; }

void password(const char *pw) {
  g_pass = pw;
  g_havePass = true;
}

void disconnect() {
  if (g_state != State::Idle && g_state != State::Closed) g_wantClose = true;
}

void resize(int cols, int rows) {
  g_cols = cols;
  g_rows = rows;
  g_resized = true;
}

size_t read(uint8_t *buf, size_t cap) { return g_rx ? xStreamBufferReceive(g_rx, buf, cap, 0) : 0; }
size_t write(const uint8_t *data, size_t n) {
  return g_rx && g_state == State::Open ? xStreamBufferSend(g_tx, data, n, 0) : 0;
}

}  // namespace ssh
}  // namespace mt
