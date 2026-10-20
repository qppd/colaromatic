#pragma once

#include <Arduino.h>
#include <esp_now.h>

#include "LinkFrames.h"

/*
 * HmiLink — ESP-NOW radio link between the machine controller and the HMI
 * touch panel. No access point, no internet: both boards talk directly
 * (offline operation, docs/05_serial_protocol.md §7).
 *
 * Pairing
 *   - Each board stores its peer MAC in NVS (Preferences).
 *   - Unpaired, the controller adopts the first HMI that beacons and answers
 *     with a STATE frame; the unpaired HMI adopts the first frame it sees.
 *   - A paired controller re-adopts only when its own HMI has been silent for
 *     30 s AND the beacon carries the PAIR flag (the operator asked for it on
 *     a freshly wiped panel). Frames from any other MAC are ignored, so a
 *     neighbouring panel cannot inject commands.
 *   - OP_PAIR (with HMI_PAIR_MAGIC) makes the HMI clear its stored peer and
 *     re-run the handshake.
 *
 * Delivery
 *   - Commands are repeated by the sender (LINK_CMD_REPEATS) and applied
 *     exactly once here; the last ACK is kept and re-sent for duplicates, so
 *     a lost ACK does not make the operator press twice.
 *   - State frames go out every LINK_STATE_PERIOD_MS; events and ACKs are
 *     repeated on the wire. CRC16 rejects corrupt frames before they can
 *     change machine state.
 */
class HmiLink {
 public:
  struct Command {
    LinkCmd cmd;
    uint32_t seq = 0;
  };

  bool begin();
  void service(uint32_t nowMs, const LinkState &state);

  /* Pop one new command (each sequence number is returned exactly once). */
  bool takeCommand(Command &out);
  /* Re-send the last ACK when a duplicate of an already-applied command
     arrives (the original ACK may have been lost on the radio). */
  void resendAckIfNeeded();

  void sendAck(uint32_t cmdSeq, uint8_t result, uint8_t opcode,
               uint16_t faults, const char *text);
  void sendEvent(uint8_t type, uint8_t a, uint8_t b, uint16_t value,
                 uint32_t sessionMs, const char *text);

  bool linkOk(uint32_t nowMs) const {
    return _paired && (nowMs - _lastRxMs) < LINK_TIMEOUT_MS;
  }
  bool paired() const { return _paired; }
  void openPairing(uint32_t nowMs);  // OP_PAIR: forget the peer

 private:
  /* ESP-NOW receive callback (WiFi task context): validates the frame,
     handles pairing and stores one pending command. */
  static void recvCallback(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len);
  void handleFrame(const uint8_t *mac, const uint8_t *data, int len);
  bool adoptPeer(const uint8_t *mac, uint32_t nowMs);
  bool sendFrame(uint8_t kind, uint8_t flags, uint32_t seq,
                 const void *payload, uint16_t len, uint8_t repeat);
  void loadPeer();
  void savePeer();

  uint8_t _peerMac[6] = {0};
  bool _paired = false;
  volatile uint32_t _lastRxMs = 0;
  uint32_t _lastStateMs = 0;
  uint32_t _seq = 1;
  uint32_t _repairSilenceMs = 0;   // when our own peer went silent

  /* pending incoming command (single slot: commands are rare and repeated) */
  volatile bool _cmdPending = false;
  LinkCmd _cmd = {};
  volatile uint32_t _cmdSeq = 0;

  /* last applied command, for duplicate re-ACK */
  uint32_t _lastCmdSeq = 0;
  bool _lastCmdValid = false;
  volatile bool _reAck = false;
  bool _justPaired = false;
  portMUX_TYPE _cmdMux = portMUX_INITIALIZER_UNLOCKED;

  LinkAck _lastAck = {};
};
