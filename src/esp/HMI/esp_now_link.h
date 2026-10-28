/*
 * esp_now_link.h — ESP-NOW transport to the ColAromatic machine controller.
 *
 * Replaces the UART link the reference HMI sketch used. The wire format is the
 * shared LinkFrames.h contract (same frame the controller's HmiLink speaks):
 * 12-byte header with magic/version/kind/flags/seq/length/CRC16, then payload.
 *
 * Addressing is by MAC, not broadcast:
 *   - CONTROLLER_PEER_MAC below is the controller's station MAC. Burn it in and
 *     the panel talks to that board only. Both boards print their own MAC at
 *     boot so the two constants can be filled in from the serial monitor.
 *   - The HMI also persists whatever MAC it paired with in NVS, so a unit that
 *     paired on the bench keeps working after a power cycle.
 *   - With no MAC known (fresh panel, blank constant) the HMI beacons to the
 *     broadcast address carrying LINK_FLAG_PAIR and adopts the first controller
 *     that answers with a valid STATE frame. The diagnostics screen's PAIR
 *     button wipes the stored MAC and re-runs that handshake.
 *
 * The beacon is also the operator's proof of life: the controller removes
 * heater authority if it hears nothing from the HMI for LINK_TIMEOUT_MS.
 */

#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

#include <Arduino.h>

#include "LinkFrames.h"

/* ---- paste the controller's MAC here ---------------------------------- *
 * Printed by the controller at boot as "controller MAC ...". All zero means
 * "pair with whatever answers" (adopts the peer over the radio and saves it).
 * On the controller side the matching constant is HMI_PEER_MAC in CONF.h. */
#define CONTROLLER_PEER_MAC {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

/* OP_PAIR argument the controller requires (CONF.h HMI_PAIR_MAGIC). */
#define HMI_PAIR_MAGIC 0x5A

/* Reported in the beacon; the controller does not act on it. */
#define HMI_LINK_VERSION 1

/* How long the HMI waits before it starts advertising for re-pairing when its
   paired controller has gone quiet (controller reflashed / NVS wiped). */
#define HMI_REPAIR_SILENCE_MS 10000

void linkInit();

/* Call from loop() on every pass: sends the beacon at LINK_BEACON_PERIOD_MS,
   times the link out at LINK_TIMEOUT_MS and applies a pending unpair. */
void linkUpdate(uint32_t nowMs);

/* Send one operator command. Repeated on the wire (LINK_CMD_REPEATS) with one
   sequence number, so the controller applies it exactly once. Returns false
   when the frame could not be queued. */
bool linkSendCmd(uint8_t opcode, uint8_t a, int16_t b);

/* Convenience wrappers used by the screens. */
bool hmiCmdStart();
bool hmiCmdHold();
bool hmiCmdResume();
bool hmiCmdCool();
bool hmiCmdEstop();
bool hmiCmdReset();
bool hmiCmdSetDuty(uint8_t pct);
bool hmiCmdSetMode(uint8_t mode);
bool hmiCmdSelectProfile(uint8_t idx);
bool hmiCmdSetSetpointC10(int16_t c10);
bool hmiCmdConfirmStage(uint8_t stage);
bool hmiCmdConfirmEndpoint();

/* OP_PAIR: tell the controller to drop this panel, then forget it locally and
   go back to the pairing beacon. */
void linkOpenPairing(uint32_t nowMs);

bool linkPaired();
bool linkConfigured();          // a unicast peer MAC is known
uint32_t linkLastRxMs();
const uint8_t *linkPeerMac();   // 6 bytes, broadcast while unpaired
void linkSelfMac(uint8_t out[6]);
void linkMacText(const uint8_t mac[6], char *out, size_t n);

#endif  // ESPNOW_LINK_H
