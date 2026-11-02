// SSH client for the Terminal app: libssh2 on the Arduino core's mbedTLS,
// running in its own task. The terminal asks the questions (host, host key,
// password) and passes the answers in; the session's bytes move through
// read() and write() like the serial port's.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mt {
namespace ssh {

enum class State : uint8_t {
  Idle,        // never connected
  Connecting,  // resolving, connecting, key exchange, authenticating
  HostKey,     // the server's key is new: answer with trustHostKey()
  Password,    // waiting for password()
  Open,        // a shell is running
  Closed,      // ended; message() says why
};

State state();
// Changes on every state change.
uint32_t generation();
// What happened last, for the screen ("Connecting to ...", "Permission denied", ...).
const char *message();

// The device key (ECDSA P-256) as a PEM private key; empty to have one made.
// The OpenSSH public key line for authorized_keys comes from publicKey().
void setDeviceKey(const char *pem);
const char *publicKey();
// Makes a new device key (PEM) into buf; false if it fails.
bool generateDeviceKey(char *buf, size_t cap);

// Starts a session. knownKey is the host key fingerprint seen before
// ("SHA256:..."), or "" if this host is new.
void connect(const char *user, const char *host, uint16_t port, const char *knownKey);
// The server's key fingerprint and type, once known.
const char *hostKey();
const char *hostKeyType();
void trustHostKey(bool yes);
void password(const char *pw);
void disconnect();

void resize(int cols, int rows);
size_t read(uint8_t *buf, size_t cap);
size_t write(const uint8_t *data, size_t n);

}  // namespace ssh
}  // namespace mt
