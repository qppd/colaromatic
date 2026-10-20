#include "HmiLink.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

/* A paired controller will re-adopt a *pairing* beacon from a different MAC
   only after its own HMI has been silent this long (freshly wiped panel). */
static const uint32_t REPAIR_SILENCE_MS = 30000;

static HmiLink *s_instance = nullptr;

void HmiLink::recvCallback(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len) {
  if (s_instance != nullptr && info != nullptr) {
    s_instance->handleFrame(info->src_addr, data, len);
  }
}

bool HmiLink::begin() {
  s_instance = this;

  /* Offline by design: station mode, no AP, no connection, fixed channel. */
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_wifi_set_channel(LINK_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    return false;
  }
  if (esp_now_init() != ESP_OK) {
    return false;
  }
  if (esp_now_register_recv_cb(&HmiLink::recvCallback) != ESP_OK) {
    return false;
  }

  loadPeer();
  if (_paired) {
    if (!esp_now_is_peer_exist(_peerMac)) {
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, _peerMac, 6);
      peer.channel = 0;
      peer.encrypt = false;
      esp_now_add_peer(&peer);
    }
  }
  _lastRxMs = millis();  // same boot grace as the serial watchdog
  return true;
}

// ---- pairing -------------------------------------------------------------

void HmiLink::loadPeer() {
  Preferences prefs;
  if (prefs.begin("colaromatic", true)) {
    if (prefs.getBytesLength("hmi_mac") == 6) {
      prefs.getBytes("hmi_mac", _peerMac, 6);
      _paired = true;
    }
    prefs.end();
  }
}

void HmiLink::savePeer() {
  Preferences prefs;
  if (prefs.begin("colaromatic", false)) {
    prefs.putBytes("hmi_mac", _peerMac, 6);
    prefs.end();
  }
}

bool HmiLink::adoptPeer(const uint8_t *mac, uint32_t nowMs) {
  memcpy(_peerMac, mac, 6);
  _paired = true;
  _lastRxMs = nowMs;
  _justPaired = true;
  if (!esp_now_is_peer_exist(_peerMac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, _peerMac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
      return false;
    }
  }
  savePeer();
  return true;
}

void HmiLink::openPairing(uint32_t nowMs) {
  (void)nowMs;
  _paired = false;
  _justPaired = false;
  memset(_peerMac, 0, sizeof(_peerMac));
  if (esp_now_is_peer_exist(_peerMac)) {
    esp_now_del_peer(_peerMac);
  }
  _lastCmdValid = false;
  savePeer();
}

// ---- receive path --------------------------------------------------------

void HmiLink::handleFrame(const uint8_t *mac, const uint8_t *data, int len) {
  if (data == nullptr || len < (int)sizeof(LinkHeader)) return;
  LinkHeader hdr;
  memcpy(&hdr, data, sizeof(hdr));
  if (hdr.magic != LINK_MAGIC || hdr.version != LINK_VERSION) return;
  if (hdr.payloadLen != (uint16_t)(len - (int)sizeof(LinkHeader))) return;
  if (linkFrameCrc(&hdr, data + sizeof(LinkHeader), hdr.payloadLen) != hdr.crc) {
    return;  // corrupt radio frame: dropped before it can change state
  }

  const uint32_t now = millis();
  const bool fromPeer = _paired && memcmp(mac, _peerMac, 6) == 0;

  switch (hdr.kind) {
    case LINK_BEACON: {
      if (!_paired) {
        adoptPeer(mac, now);
        return;
      }
      if (fromPeer) {
        _lastRxMs = now;
        return;
      }
      /* Someone else's panel, explicitly asking to pair, while our own has
         been quiet: adopt it (the operator wiped the panel). */
      if ((hdr.flags & LINK_FLAG_PAIR) != 0 &&
          (now - _lastRxMs) > REPAIR_SILENCE_MS) {
        adoptPeer(mac, now);
      }
      return;
    }

    case LINK_CMD: {
      if (!_paired || !fromPeer) return;
      if (hdr.payloadLen < sizeof(LinkCmd)) return;
      LinkCmd cmd;
      memcpy(&cmd, data + sizeof(LinkHeader), sizeof(cmd));

      portENTER_CRITICAL(&_cmdMux);
      _lastRxMs = now;
      if (_lastCmdValid && hdr.seq == _lastCmdSeq) {
        /* Duplicate of a command already accepted: ask for a re-ACK only. */
        _reAck = true;
      } else {
        _cmd = cmd;
        _cmdSeq = hdr.seq;
        _cmdPending = true;
        _lastCmdSeq = hdr.seq;
        _lastCmdValid = true;
      }
      portEXIT_CRITICAL(&_cmdMux);
      return;
    }

    default:
      /* Any other valid frame from our peer (state is HMI-bound) still counts
         as proof of life. */
      if (fromPeer) _lastRxMs = now;
      return;
  }
}

bool HmiLink::takeCommand(Command &out) {
  bool got = false;
  portENTER_CRITICAL(&_cmdMux);
  if (_cmdPending) {
    out.cmd = _cmd;
    out.seq = _cmdSeq;
    _cmdPending = false;
    got = true;
  }
  portEXIT_CRITICAL(&_cmdMux);
  return got;
}

void HmiLink::resendAckIfNeeded() {
  bool needed = _reAck;
  _reAck = false;
  if (needed && _lastCmdValid) {
    sendFrame(LINK_ACK, 0, _seq++, &_lastAck, sizeof(_lastAck), LINK_CMD_REPEATS);
  }
}

// ---- send path -----------------------------------------------------------

bool HmiLink::sendFrame(uint8_t kind, uint8_t flags, uint32_t seq,
                        const void *payload, uint16_t len, uint8_t repeat) {
  if (!_paired) return false;
  uint8_t buf[sizeof(LinkHeader) + 128];
  if (len > sizeof(buf) - sizeof(LinkHeader)) return false;

  LinkHeader hdr;
  hdr.magic = LINK_MAGIC;
  hdr.version = LINK_VERSION;
  hdr.kind = kind;
  hdr.flags = flags;
  hdr.seq = seq;
  hdr.payloadLen = len;
  hdr.crc = 0;
  hdr.crc = linkFrameCrc(&hdr, (const uint8_t *)payload, len);

  memcpy(buf, &hdr, sizeof(hdr));
  if (len > 0) memcpy(buf + sizeof(hdr), payload, len);

  const uint16_t total = sizeof(hdr) + len;
  bool ok = true;
  for (uint8_t i = 0; i < repeat; i++) {
    /* esp_now_send copies the frame into the TX queue, so the buffer may be
       reused after it returns. MAC-level ACKs make each attempt reliable. */
    if (esp_now_send(_peerMac, buf, total) != ESP_OK) ok = false;
  }
  return ok;
}

void HmiLink::service(uint32_t nowMs, const LinkState &state) {
  if (!_paired) return;

  if (_justPaired || (nowMs - _lastStateMs) >= LINK_STATE_PERIOD_MS) {
    _justPaired = false;
    _lastStateMs = nowMs;
    sendFrame(LINK_STATE, 0, _seq++, &state, sizeof(state), 1);
  }

  resendAckIfNeeded();
}

void HmiLink::sendAck(uint32_t cmdSeq, uint8_t result, uint8_t opcode,
                      uint16_t faults, const char *text) {
  memset(&_lastAck, 0, sizeof(_lastAck));
  _lastAck.cmdSeq = cmdSeq;
  _lastAck.faults = faults;
  _lastAck.result = result;
  _lastAck.opcode = opcode;
  if (text != nullptr) {
    strncpy(_lastAck.text, text, sizeof(_lastAck.text) - 1);
  }
  sendFrame(LINK_ACK, 0, _seq++, &_lastAck, sizeof(_lastAck), LINK_CMD_REPEATS);
}

void HmiLink::sendEvent(uint8_t type, uint8_t a, uint8_t b, uint16_t value,
                        uint32_t sessionMs, const char *text) {
  LinkEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.sessionMs = sessionMs;
  ev.type = type;
  ev.a = a;
  ev.b = b;
  ev.value = value;
  if (text != nullptr) {
    strncpy(ev.text, text, sizeof(ev.text) - 1);
  }
  sendFrame(LINK_EVENT, 0, _seq++, &ev, sizeof(ev), LINK_CMD_REPEATS);
}
