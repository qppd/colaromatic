#include "esp_now_link.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdio.h>

#include "hmi_data.h"

namespace {

const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t ZERO_MAC[6] = {0, 0, 0, 0, 0, 0};

/* Preferences namespace/keys: the controller uses "colaromatic"/"hmi_mac", the
   panel keeps its own namespace so the two boards can share one NVS layout
   convention without colliding. */
const char *NVS_NAMESPACE = "colahmi";
const char *NVS_KEY_PEER = "ctl_mac";

uint8_t s_mac[6] = {0};
uint8_t s_peer[6] = {0};
bool s_paired = false;
uint32_t s_seq = 1;
volatile uint32_t s_lastRxMs = 0;
uint32_t s_lastBeaconMs = 0;
uint32_t s_clearPeerAtMs = 0;
/* Set by linkUpdate when the paired controller has been silent long enough to
   have been replaced or reflashed. Read by the radio task. */
volatile bool s_repairing = false;

/* s_peer/s_paired are written by the WiFi task (pairing) and read by the loop
   task (beacons, commands), so every access to the pair goes through this. */
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

bool macIsZero(const uint8_t *m) { return memcmp(m, ZERO_MAC, 6) == 0; }

bool peerSnapshot(uint8_t out[6]) {
  portENTER_CRITICAL(&s_mux);
  const bool paired = s_paired;
  memcpy(out, s_peer, 6);
  portEXIT_CRITICAL(&s_mux);
  return paired;
}

bool peerRegistered(const uint8_t *mac) { return esp_now_is_peer_exist(mac); }

void ensurePeer(const uint8_t *mac) {
  if (peerRegistered(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = LINK_WIFI_CHANNEL;  // pinned: the controller does not hop
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  esp_now_add_peer(&peer);
}

void loadPeer() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return;
  if (prefs.getBytesLength(NVS_KEY_PEER) == 6) {
    uint8_t stored[6] = {0};
    prefs.getBytes(NVS_KEY_PEER, stored, 6);
    if (!macIsZero(stored)) {
      memcpy(s_peer, stored, 6);
      s_paired = true;
    }
  }
  prefs.end();
}

void savePeer() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putBytes(NVS_KEY_PEER, s_peer, 6);
  prefs.end();
}

void clearPeer() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.remove(NVS_KEY_PEER);
    prefs.end();
  }
  uint8_t old[6];
  portENTER_CRITICAL(&s_mux);
  memcpy(old, s_peer, 6);
  memcpy(s_peer, BROADCAST_MAC, 6);
  s_paired = false;
  portEXIT_CRITICAL(&s_mux);
  if (peerRegistered(old)) esp_now_del_peer(old);
}

void adoptPeer(const uint8_t *mac) {
  if (macIsZero(mac) || memcmp(mac, BROADCAST_MAC, 6) == 0) return;
  portENTER_CRITICAL(&s_mux);
  memcpy(s_peer, mac, 6);
  s_paired = true;
  portEXIT_CRITICAL(&s_mux);
  ensurePeer(mac);
  savePeer();
  char text[24];
  linkMacText((const uint8_t *)mac, text, sizeof(text));
  Serial.printf("[ESPNOW] paired with controller %s\n", text);
}

/* ---- receive ------------------------------------------------------------ */

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (info == nullptr || data == nullptr) return;
  if (len < (int)sizeof(LinkHeader)) return;

  LinkHeader hdr;
  memcpy(&hdr, data, sizeof(hdr));
  if (hdr.magic != LINK_MAGIC || hdr.version != LINK_VERSION) return;
  if (hdr.payloadLen != (uint16_t)(len - (int)sizeof(LinkHeader))) return;
  if (linkFrameCrc(&hdr, data + sizeof(LinkHeader), hdr.payloadLen) != hdr.crc) {
    return;  // corrupt frame: dropped before it reaches the screens
  }

  const uint8_t *src = info->src_addr;
  uint8_t peer[6];
  const bool paired = peerSnapshot(peer);
  const bool fromPeer = paired && memcmp(src, peer, 6) == 0;

  if (!fromPeer) {
    /* Only a STATE frame may pair us, and only when we have no peer or our
       peer has been silent long enough to have been replaced (controller
       reflashed or swapped, so its MAC no longer exists). An ACK or EVENT from
       a stranger is never enough: it would mean accepting a session this panel
       did not start. */
    if (hdr.kind != LINK_STATE) return;
    if (hdr.payloadLen < sizeof(LinkState)) return;
    if (paired && !s_repairing) return;
    adoptPeer(src);
  }

  const uint32_t now = millis();
  s_lastRxMs = now;

  switch (hdr.kind) {
    case LINK_STATE: {
      if (hdr.payloadLen < sizeof(LinkState)) return;
      LinkState st;
      memcpy(&st, data + sizeof(LinkHeader), sizeof(st));
      hmiOnState(st, now);
      return;
    }
    case LINK_ACK: {
      if (hdr.payloadLen < sizeof(LinkAck)) return;
      LinkAck ack;
      memcpy(&ack, data + sizeof(LinkHeader), sizeof(ack));
      hmiOnAck(ack, now);
      return;
    }
    case LINK_EVENT: {
      if (hdr.payloadLen < sizeof(LinkEvent)) return;
      LinkEvent ev;
      memcpy(&ev, data + sizeof(LinkHeader), sizeof(ev));
      /* The frame carries a NUL-padded char array; force termination in case a
         sender filled it completely. */
      ev.text[sizeof(ev.text) - 1] = '\0';
      hmiOnEvent(ev, now);
      return;
    }
    default:
      return;  // beacons/commands are controller-bound
  }
}

/* ---- send --------------------------------------------------------------- */

bool sendFrameTo(const uint8_t *target, uint8_t kind, uint8_t flags,
                 uint32_t seq, const void *payload, uint16_t len,
                 uint8_t repeat) {
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
  if (len > 0 && payload != nullptr) memcpy(buf + sizeof(hdr), payload, len);

  const uint16_t total = (uint16_t)(sizeof(hdr) + len);
  ensurePeer(target);

  bool ok = true;
  for (uint8_t i = 0; i < repeat; i++) {
    if (esp_now_send(target, buf, total) != ESP_OK) ok = false;
  }
  return ok;
}

bool sendFrame(uint8_t kind, uint8_t flags, uint32_t seq, const void *payload,
               uint16_t len, uint8_t repeat) {
  uint8_t peer[6];
  const bool paired = peerSnapshot(peer);
  return sendFrameTo(paired ? peer : BROADCAST_MAC, kind, flags, seq, payload,
                     len, repeat);
}

void sendBeacon(uint32_t now) {
  LinkBeacon beacon;
  beacon.uptimeMs = now;
  beacon.hmiVersion = HMI_LINK_VERSION;
  beacon.reserved = 0;
  /* Unpaired, or paired to a controller that no longer answers: advertise on
     the broadcast address with the PAIR flag so either the same board coming
     back or a replacement can adopt us. A healthy pair beacons unicast with no
     flag, so this cannot steal a working link. */
  uint8_t peer[6];
  const bool paired = peerSnapshot(peer);
  const bool advertise = !paired || s_repairing;
  const uint8_t flags = advertise ? LINK_FLAG_PAIR : 0;
  const uint8_t *target = advertise ? BROADCAST_MAC : peer;
  sendFrameTo(target, LINK_BEACON, flags, s_seq++, &beacon, sizeof(beacon), 1);
}

}  // namespace

/* ---- public API --------------------------------------------------------- */

void linkInit() {
  /* Offline by design: station mode, no AP, no internet, one fixed channel
     (LINK_WIFI_CHANNEL must match the controller's). */
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_read_mac(s_mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    char text[24];
    linkMacText(s_mac, text, sizeof(text));
    Serial.printf("[HMI] MAC (station): %s\n", text);
  }

  if (esp_wifi_set_channel(LINK_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("[ESPNOW] set_channel failed");
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] init failed");
    return;
  }

  /* Peer priority: NVS, then the constant burned into this file, then
     broadcast until a controller answers. The receive callback is registered
     afterwards so an early frame cannot adopt a peer that this setup then
     overwrites. */
  loadPeer();
  if (!s_paired) {
    const uint8_t configured[6] = CONTROLLER_PEER_MAC;
    if (!macIsZero(configured)) {
      memcpy(s_peer, configured, 6);
      s_paired = true;
    } else {
      memcpy(s_peer, BROADCAST_MAC, 6);
      s_paired = false;
    }
  }
  ensurePeer(s_paired ? s_peer : BROADCAST_MAC);
  esp_now_register_recv_cb(onReceive);

  /* 0 = nothing heard yet, so the UI says offline until a real frame arrives.
     (The controller gives itself a boot grace on its watchdog; the panel has
     nothing to be lenient about — showing zeros as telemetry would be a lie.) */
  s_lastRxMs = 0;
  s_lastBeaconMs = 0;  // beacon on the first update pass
  hmiSetConnected(false);
}

void linkUpdate(uint32_t nowMs) {
  if (s_clearPeerAtMs != 0 && (int32_t)(nowMs - s_clearPeerAtMs) >= 0) {
    s_clearPeerAtMs = 0;
    clearPeer();
    Serial.println("[ESPNOW] pairing mode: waiting for a controller");
  }

  /* A paired controller that has gone silent for HMI_REPAIR_SILENCE_MS is
     treated as replaced: start advertising on the broadcast address and accept
     a STATE frame from a new MAC (see onReceive). */
  const uint32_t lastRx = s_lastRxMs;
  s_repairing = linkPaired() && lastRx != 0 &&
                (nowMs - lastRx) > HMI_REPAIR_SILENCE_MS;

  if (s_lastBeaconMs == 0 || (nowMs - s_lastBeaconMs) >= LINK_BEACON_PERIOD_MS) {
    s_lastBeaconMs = nowMs;
    sendBeacon(nowMs);
  }

  /* The controller drops heater authority on its own after LINK_TIMEOUT_MS of
     silence; the UI must stop pretending the numbers are live at the same
     moment. */
  hmiSetConnected(lastRx != 0 && (nowMs - lastRx) < LINK_TIMEOUT_MS);
}

bool linkSendCmd(uint8_t opcode, uint8_t a, int16_t b) {
  /* Unpaired: a broadcast command is not from a peer, so the controller would
     drop it. Report the failure instead of letting a screen claim it sent. */
  uint8_t peer[6];
  if (!peerSnapshot(peer)) return false;

  LinkCmd cmd;
  cmd.opcode = opcode;
  cmd.a = a;
  cmd.b = b;
  return sendFrame(LINK_CMD, 0, s_seq++, &cmd, sizeof(cmd), LINK_CMD_REPEATS);
}

void linkOpenPairing(uint32_t nowMs) {
  /* Order matters: the OP_PAIR command has to reach the controller (which
     still has this panel as its peer) BEFORE the panel forgets it. The ack is
     optional — the controller acks and opens its own pairing on receipt. */
  uint8_t peer[6];
  if (peerSnapshot(peer)) linkSendCmd(OP_PAIR, HMI_PAIR_MAGIC, 0);
  s_clearPeerAtMs = nowMs + 500;
}

bool linkPaired() {
  uint8_t peer[6];
  return peerSnapshot(peer);
}
bool linkConfigured() { return linkPaired(); }
uint32_t linkLastRxMs() { return s_lastRxMs; }

/* Display only: the returned buffer is the live pair, which the radio task may
   update. Callers copy it immediately into a string. */
const uint8_t *linkPeerMac() {
  uint8_t peer[6];
  return peerSnapshot(peer) ? s_peer : BROADCAST_MAC;
}

void linkSelfMac(uint8_t out[6]) { memcpy(out, s_mac, 6); }

void linkMacText(const uint8_t mac[6], char *out, size_t n) {
  if (out == nullptr || n == 0) return;
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

/* ---- command wrappers --------------------------------------------------- */

bool hmiCmdStart() {
  /* b = setpoint degC*10, or LINK_SETPOINT_UNCHANGED to keep whatever the
     controller already has. */
  uint8_t profile = LINK_PROFILE_NONE;
  uint8_t mode = MODE_MANUAL;
  int16_t setpointC10 = 0;
  uint8_t duty = 0;
  hmiOperatorSnapshot(profile, mode, setpointC10, duty);
  const int16_t sp = (mode == MODE_AUTO) ? setpointC10 : LINK_SETPOINT_UNCHANGED;
  return linkSendCmd(OP_START, profile, sp);
}

bool hmiCmdHold() { return linkSendCmd(OP_HOLD, 0, 0); }
bool hmiCmdResume() { return linkSendCmd(OP_RESUME, 0, 0); }
bool hmiCmdCool() { return linkSendCmd(OP_COOL, 0, 0); }
bool hmiCmdEstop() { return linkSendCmd(OP_ESTOP, 0, 0); }
bool hmiCmdReset() { return linkSendCmd(OP_RESET, 0, 0); }
bool hmiCmdSetDuty(uint8_t pct) { return linkSendCmd(OP_SET_DUTY, pct, 0); }
bool hmiCmdSetMode(uint8_t mode) { return linkSendCmd(OP_SET_MODE, mode, 0); }
bool hmiCmdSelectProfile(uint8_t idx) {
  return linkSendCmd(OP_SELECT_PROFILE, idx, 0);
}
bool hmiCmdSetSetpointC10(int16_t c10) {
  return linkSendCmd(OP_SET_SETPOINT, 0, c10);
}
bool hmiCmdConfirmStage(uint8_t stage) {
  return linkSendCmd(OP_CONFIRM_STAGE, stage, 0);
}
bool hmiCmdConfirmEndpoint() { return linkSendCmd(OP_CONFIRM_ENDPOINT, 0, 0); }
