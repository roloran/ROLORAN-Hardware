/* rdcp-modem-plugin.cpp */

#include <Arduino.h>
#include "rdcp-modem-plugin.h"
#include "nsa.h"
#include "rdcp-modem-constants.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-hal.h"
#include "rdcp-modem-hardware-settings.h"
#include "rdcp-modem-lora-settings.h"
#include "rdcp-modem-lora-radio.h"
#include "rdcp-modem-rdcp-v04.h"
#include "rdcp-modem-scheduler.h"
#include <Wire.h>
#include <cstdint>
#include "HT_SSD1306Wire.h"
#include "rdcp-modem-persistence.h"
#include "unishox2.h"
#include "rdcp-modem-crypto.h"

/*
  ================================== CIRE BUTTON DEMO / PROTOTYPE ==================================
*/

/*

Initscript for Heltec V3.2 device:

+SET RADIO NUM 1
+SET CHANNEL NUM 1
+SET CHANNEL LORA 00 869.525 125 07 5 12 00 15 1 2
+SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 0
+SET RADIO INTERFACE 0 0 0 1 0 -1 -1 -1 -1
+RADIOINIT

*/


// Find out the length of an array
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// serial input
extern nsa_global nsa;
static char line_uppercase[SERIALINPUTLEN];
static char melody_cmd[SERIALINPUTLEN];

// LED setup
const int ledPin = GPIO_NUM_6;     // change to your GPIO
const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;  // 8-bit = 0-255

bool button_press_by_wakeup = OPTION_DISABLED;

volatile uint8_t ledBrightness = 0;
volatile bool ledBlink = false;

void led_blink()
{
  ledBrightness = 255;
  ledBlink = true;
}

void led_on()
{
  ledBrightness = 255;
  ledBlink = false;
}

void led_off()
{
  ledBrightness = 0;
  ledBlink = false;
}

// Buzzer Setup
const int buzPin = GPIO_NUM_5;
const int buzChannel = 4;
const int buzResolution = 8;  // 8-bit = 0-255
uint32_t buzVolume = 128; // max seems to be 128, higher values can be set but have no or detremental effect

static QueueHandle_t buzTaskQueue;

// Up to 8 tones per melody
#define MELODY_LEN 8

struct buzz_tone {
  uint32_t freq;
  uint32_t duration;
  uint32_t gap;
};

const buzz_tone *melody;

buzz_tone success_melody[MELODY_LEN] = {
  { 800, 120, 0},
  {1000, 120, 0},
  {1300, 120, 0}
};
buzz_tone failure_melody[MELODY_LEN] = {
  {1200, 150, 0},
  { 900, 150, 0},
  { 600, 250, 0}
};
buzz_tone transmission_melody[MELODY_LEN] = {
  {659, 120, 0},
  {784, 120, 0},
  {880, 120, 0},
  {784, 120, 0},
  {659, 120, 0}
};
buzz_tone boot_melody[MELODY_LEN] = {
  {220, 300, 50},
  {294, 300, 50},
  {330, 300, 50},
  {440, 600, 50}
};
buzz_tone wakeup_melody[MELODY_LEN] = {
  {220, 300, 50},
  {294, 600, 50},
  {330, 300, 50},
  {440, 600, 50}
};

enum Buz_Cmd {
  CMD_BUZ_SUCCESS,      // played when all expected ACKs are received
  CMD_BUZ_FAILURE,      // played when a transmission failure is encountered
  CMD_BUZ_TRANSMISSION, // played during RDCP transmission
  CMD_BUZ_BOOT,         // played on intial boot
  CMD_BUZ_WAKEUP,       // played when waking up from sleep
  CMD_BUZ_TEST,         // test tones, go through increasing frequencies
};

struct melody {
  buzz_tone* tones;
  Buz_Cmd cmd;
  uint8_t repeats;
  uint32_t delay;
};

// To be indexable through Buz_Cmd *must* keep same order
struct melody melodies[5] = {
  {success_melody, CMD_BUZ_SUCCESS, 1, 0},
  {failure_melody, CMD_BUZ_FAILURE, 1, 0},
  {transmission_melody, CMD_BUZ_TRANSMISSION, 3, 400},
  {boot_melody, CMD_BUZ_BOOT, 1, 0},
  {wakeup_melody, CMD_BUZ_WAKEUP, 1, 0},
};

/**
 * Instruct the buzzer task to buz at next free moment
 */
void cmd_buzz(Buz_Cmd buz_cmd){
  xQueueSend(buzTaskQueue, &buz_cmd, portMAX_DELAY);
}

void buzz(uint32_t freq, uint32_t buz_volume, uint32_t time_ms)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(buzPin, freq, buzResolution);
#else
  ledcSetup(buzChannel, freq, buzResolution);
  ledcAttachPin(buzPin, buzChannel);
#endif

  // Buzz for given time and then turn buzzing off
  ledcWrite(buzChannel, buz_volume);
  vTaskDelay(pdMS_TO_TICKS(time_ms));
  ledcWrite(buzChannel, 0);
}

// on the first prototype device, the button connects GPIO Pin 7 to GND on press
#define CIRE_BUTTON_PIN GPIO_NUM_7

// holding or repeatedly pressing the button within this number of milliseconds is considered a single press
#define CIRE_BUTTON_REPEAT_INTERVAL 10000

// GPIO7 must remain low for this interval before it is accepted as a button press, in milliseconds.
uint16_t cire_button_depressed_timeout = 0;

// Idle time before turning off the display and entering deep sleep, in seconds.
uint16_t cire_button_idle_timeout = 120;

// Time to wait for the corresponding ACK, in seconds.
uint16_t cire_timeout_ack_da = 60;
uint16_t cire_timeout_ack_hq = 900;

enum WakeupBehavior {
  WAKEUPBEHAVIOR_IDLE,
  WAKEUPBEHAVIOR_CIRE,
};

WakeupBehavior cire_button_wakeup_behavior = WAKEUPBEHAVIOR_CIRE;

bool cire_button_initialized             = OPTION_DISABLED;
int64_t cire_button_last_press_timestamp = TIMESTAMP_ZERO;
int64_t cire_button_low_since             = TIMESTAMP_ZERO;
int64_t cire_button_display_on_since     = TIMESTAMP_ZERO;
bool cire_button_cire_ongoing            = OPTION_DISABLED;
bool cire_button_cire_on_wakeup_pending  = OPTION_DISABLED;

// Imported libraries and code snippets for OLED display based on
// https://github.com/HelTecAutomation/Heltec_ESP32/blob/master/examples/OLED/SimpleDemo/SimpleDemo.ino
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// CIRE-related variables taken from ROLODECK implementation
uint8_t cire_retry = ZERO_LENGTH;        // How often did we already try to send a CIRE?
int64_t cire_starttime = TIMESTAMP_ZERO; // When was the CIRE sent?
#define CIRE_STATE_NONE    0
#define CIRE_STATE_WAIT_DA 1
#define CIRE_STATE_WAIT_HQ 2
uint8_t  cire_state = CIRE_STATE_NONE;
uint16_t cire_seqnrs[]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint16_t cire_current_seqnr = RDCPv04_SEQUENCENR_SPECIAL_ZERO;
uint8_t  cire_current_subtype = 0x00;
uint16_t cire_current_refnr = RDCPv04_OA_REFNR_SPECIAL_ZERO;
char     cire_current_text[INFOLEN];

uint16_t cire_button_default_entry_point = 0x0200; // default: Use MERLIN Basis Neuhaus as EP
const uint8_t CIRE_BUTTON_DEFAULT_HQ_SHARED_SECRET[KEYLENAES] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
  0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
  0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
};

extern device_config cfg;

// Function prototypes
void rdcp_cire_ack(uint16_t origin, uint16_t confirmedseqnr, uint8_t acktype);
void cire_button_send_cire(void);
bool cire_button_hq_shared_secret_is_unset(void);
void get_inline_hash(struct rdcpv04_message *m, uint8_t payloadprefixlength, uint8_t *hashtarget);
void cire_button_sleep_if_idle(void);

/*
  ==================================================================================================
*/

bool cire_button_hq_shared_secret_is_unset(void)
{
  for (int i = COUNT_ZERO; i < KEYLENAES; i++)
  {
    if (cfg.rdcp_hqaeskey[i] != ZEROBYTE) return false;
  }
  return true;
}

/**
 * Display three short lines of text on the Heltec LoRa32 V3 device's OLED display.
 */
void cire_button_display_threeliner(const char* line1, const char* line2, const char* line3)
{
  display.displayOn();
  display.clear();
  display.drawString(0, 00, line1);
  display.drawString(0, 20, line2);
  display.drawString(0, 40, line3);
  display.display();
  cire_button_display_on_since = my_millis();
  return;
}

bool plugin_serial(const char* line)
{
  /* Handle any additional Serial commands here. Return true if handled, false if not handled. */

  // Most commands should operate on the upper case input
  strncpy(line_uppercase, line, SERIALINPUTLEN);
  for (int x=COUNT_ZERO; line_uppercase[x] != '\0'; x++) line_uppercase[x] = toUpperCase(line_uppercase[x]);

  if (nsa_startsWith(line_uppercase, "SET WAKEUPBEHAVIOR "))
  {
    nsa_strsplice(line_uppercase);

    if (strcmp(nsa.part[2], "CIRE") == 0)
    {
      cire_button_wakeup_behavior = WAKEUPBEHAVIOR_CIRE;
    }
    else if (strcmp(nsa.part[2], "IDLE") == 0)
    {
      cire_button_wakeup_behavior = WAKEUPBEHAVIOR_IDLE;
    }
    else
    {
      serial_writeln("ERROR: Unknown wake-up behavior; use CIRE or IDLE");
      return true;
    }

    char wakeup_info[INFOLEN];
    snprintf(wakeup_info, INFOLEN, "INFO: Wake-up behavior set to %s", nsa.part[2]);
    serial_writeln(wakeup_info);
    return true;
  }

  if (nsa_startsWith(line_uppercase, "SET TIMEOUT "))
  {
    nsa_strsplice(line_uppercase);
    const char* timeout_name = nsa.part[2];
    const char* timeout_value = nsa.part[3];
    long timeout_amount = strtol(timeout_value, NULL, BASE10);
    bool is_depressed_timeout = strcmp(timeout_name, "DEPRESSED") == 0;

    if ((timeout_amount < (is_depressed_timeout ? 0 : 1)) || (timeout_amount > UINT16_MAX))
    {
      serial_writeln(is_depressed_timeout
        ? "ERROR: DEPRESSED timeout must be an integer between 0 and 65535 milliseconds"
        : "ERROR: Timeout must be an integer between 1 and 65535 seconds");
      return true;
    }

    if (strcmp(timeout_name, "DA") == 0)
    {
      cire_timeout_ack_da = (uint16_t) timeout_amount;
    }
    else if (strcmp(timeout_name, "HQ") == 0)
    {
      cire_timeout_ack_hq = (uint16_t) timeout_amount;
    }
    else if (strcmp(timeout_name, "SLEEP") == 0)
    {
      cire_button_idle_timeout = (uint16_t) timeout_amount;
    }
    else if (strcmp(timeout_name, "DEPRESSED") == 0)
    {
      cire_button_depressed_timeout = (uint16_t) timeout_amount;
    }
    else
    {
      serial_writeln("ERROR: Unknown timeout; use DA, HQ, SLEEP, or DEPRESSED");
      return true;
    }

    char timeout_info[INFOLEN];
    const char* timeout_unit = strcmp(timeout_name, "DEPRESSED") == 0 ? "milliseconds" : "seconds";
    snprintf(timeout_info, INFOLEN, "INFO: %s timeout set to %ld %s", timeout_name, timeout_amount, timeout_unit);
    serial_writeln(timeout_info);
    return true;
  }

  if (nsa_startsWith(line_uppercase, "MELODY ")){
    nsa_substring(line_uppercase, 7);
    strncpy(melody_cmd, nsa.result, SERIALINPUTLEN);

    if (nsa_startsWith(melody_cmd, "SETTONE ")){
      // MELODY TONE 3 4 1000 120 50
      // -> Melody 3
      // -> Tone 4
      // -> 1000 Hz
      // -> 120 msec
      // -> Pause 50 msec
      nsa_strsplice(melody_cmd);

      long melody_id = strtol(nsa.part[1], NULL, BASE10);
      long tone_id = strtol(nsa.part[2], NULL, BASE10);
      long freq = strtol(nsa.part[3], NULL, BASE10);
      long duration = strtol(nsa.part[4], NULL, BASE10);
      long gap = strtol(nsa.part[5], NULL, BASE10);

      // basic sanity check
      if (melody_id >= ARRAY_LEN(melodies) || tone_id >= MELODY_LEN) {
        serial_writeln("ERROR: Invalid Melody or Tone ID");
        return true;
      }

      buzz_tone *tone = &melodies[melody_id].tones[tone_id];
      tone->freq = freq;
      tone->duration = duration;
      tone->gap = gap;

      return true;
    }

    if (nsa_startsWith(melody_cmd, "MODIFY ")){
      // MELODY MODIFY 3 5 300
      // -> Melody 3
      // -> Repeats 5 times
      // -> 300 msec between repititions
      nsa_strsplice(melody_cmd);

      long melody_id = strtol(nsa.part[1], NULL, BASE10);
      long repeats = strtol(nsa.part[2], NULL, BASE10);
      long delay = strtol(nsa.part[3], NULL, BASE10);

      // basic sanity check
      if (melody_id >= ARRAY_LEN(melodies)) {
        serial_writeln("ERROR: Invalid Melody ID");
        return true;
      }

      melodies[melody_id].repeats = repeats;
      melodies[melody_id].delay = delay;

      return true;
    }

    if (nsa_startsWith(melody_cmd, "PLAY ")){
      // MELODY PLAY 3
      // -> plays melody 3
      nsa_strsplice(melody_cmd);

      long melody_id = strtol(nsa.part[1], NULL, BASE10);

      // basic sanity check
      if (melody_id >= ARRAY_LEN(melodies)) {
        serial_writeln("ERROR: Invalid Melody ID");
        return true;
      }

      cmd_buzz(melodies[melody_id].cmd);

      return true;
    }
    if (nsa_startsWith(melody_cmd, "TEST")){
      // MELODY TEST
      // -> TEST tones of the buzzer

      cmd_buzz(CMD_BUZ_TEST);

      return true;
    }
    if (nsa_startsWith(melody_cmd, "VOLUME ")){
      // MELODY VOLUME 128
      // -> changes the volume to 128
      nsa_strsplice(melody_cmd);

      long volume = strtol(nsa.part[1], NULL, BASE10);

      // basic sanity check
      if (volume > 128 || volume < 0) {
        serial_writeln("ERROR: Volume exceeds limits [0; 128]");
        return true;
      }

      buzVolume = volume;

      return true;
    }
  }
  return false;
}

extern lora_message    current_lora_message;
extern rdcpv04_message current_rdcpv04_message;
extern bool            current_rdcpv04_message_is_duplicate;

void plugin_incoming(uint8_t lora_payload_type)
{
  /* Handle incoming LoRa packets based on their payload type here. */
  /*
    The received LoRa packet is available as `current_lora_message`.
    If it is an RDCP v0.4 Message with a valid CRC, it is also available as `current_rdcpv04_message`.
    Amend further external variables if required.
  */
  if ((lora_payload_type == PAYLOAD_TYPE_RDCP_V04) && !current_rdcpv04_message_is_duplicate)
  {
    if (current_rdcpv04_message.header.message_type == RDCPv04_MSGTYPE_ACK)
    {
      bool has_signature = true;
      bool valid_signature = false;

      if (current_rdcpv04_message.header.rdcp_payload_length < RDCPv04_PAYLOAD_SIZE_ACK_UNSIGNED)
      {
        serial_writeln("WARNING: RDCP Payload too short for RDCP MT ACK");
        return;
      }

      if ((current_rdcpv04_message.header.origin <= RDCPv04_ADDRESS_HQ_UPPERBOUND) &&
          (current_rdcpv04_message.header.rdcp_payload_length < RDCPv04_PAYLOAD_SIZE_ACK_UNSIGNED + RDCPv04_SIGNATURE_LENGTH))
      {
        serial_writeln("WARNING: RDCP ACK from HQ device is not signed, ignoring");
        return;
      }

      if (current_rdcpv04_message.header.rdcp_payload_length ==
          RDCPv04_PAYLOAD_SIZE_ACK_UNSIGNED + RDCPv04_SIGNATURE_LENGTH)
      {
        has_signature = true;
      }
      else
      {
        has_signature = false;
      }

      uint16_t confirmed_seqnr = current_rdcpv04_message.payload.data[0] + current_rdcpv04_message.payload.data[1] * 256;
      uint8_t acktype = current_rdcpv04_message.payload.data[2];

      if (has_signature)
      {
        uint8_t sha[SHABUFSIZE];
        get_inline_hash(&current_rdcpv04_message, RDCPv04_PAYLOAD_SIZE_ACK_UNSIGNED, sha);
        uint8_t sig[SIGBUFSIZE];
        for (int i=COUNT_ZERO; i<RDCPv04_SIGNATURE_LENGTH; i++)
        {
          sig[i] = current_rdcpv04_message.payload.data[RDCPv04_PAYLOAD_SIZE_ACK_UNSIGNED + i];
        }
        valid_signature = schnorr_verify_signature(sha, SHABUFSIZE, sig);

        if (valid_signature)
        {
          serial_writeln("INFO: RDCP ACK message has valid Schnorr signature from HQ device");
        }
        else
        {
          serial_writeln("WARNING: Invalid HQ Schnorr signature for RDCP ACK - ignoring");
          return;
        }
      }

      // Handle ACKs for CIREs
      rdcp_cire_ack(current_rdcpv04_message.header.origin, confirmed_seqnr, acktype);
    } // message type was ACK
  } // non-duplicate RDCP v0.4 message was processed

  return;
}

void rdcp_cire_callback(void)
{
  cire_starttime = my_millis();
  cire_state = CIRE_STATE_WAIT_DA;
  for (int i=0; i<16; i++)
  {
    if (cire_seqnrs[i] == 0x0000)
    {
      cire_seqnrs[i] = cire_current_seqnr;
      break;
    }
  }
  serial_writeln("INFO: Internal CIRE Callback executed, ACK timeout timer started");
  return;
}

/**
 * Re-send the previous CIRE again after a timeout. Triggered by rdcp_cire_check().
 */
void rdcp_cire_resend(void)
{
  int64_t now = my_millis();
  cire_state = CIRE_STATE_NONE;
  cire_retry++;

  if (cire_retry > 5)
  {
    cire_retry = 0;
    for (int i=0; i<16; i++) cire_seqnrs[i] = RDCPv04_SEQUENCENR_SPECIAL_ZERO;
    serial_writeln("WARNING: CIRE could not be sent (ACK missing), giving up");
    cire_button_display_threeliner("Meldung NICHT", "zugestellt, keine", "Verbindung");
    cire_button_cire_ongoing = false;
    led_off();
    noTone(buzPin);
    return;
  }

  char buf[INFOLEN];
  snprintf(buf, INFOLEN, "INFO: Re-sending CIRE %d, re-try #%d", cire_current_refnr, cire_retry);
  serial_writeln(buf);

  cire_button_send_cire();
  return;
}

void set_ciretime_da(uint16_t timeout)
{
  cire_timeout_ack_da = timeout;
  return;
}

void set_ciretime_hq(uint16_t timeout)
{
  cire_timeout_ack_hq = timeout;
  return;
}

/**
 * Check whether we have an ongoing CIRE for which the required ACKs have not yet
 * been received. Must be called periodically.
 */
void rdcp_cire_check(void)
{
  if (cire_state == CIRE_STATE_NONE) return;

  int64_t now = my_millis();
  if (cire_state == CIRE_STATE_WAIT_DA)
  {
    if (now - cire_starttime > cire_timeout_ack_da * SECONDS_TO_MILLISECONDS)
    {
      serial_writeln("WARNING: CIRE Timeout waiting for ACK from DA");
      cire_button_display_threeliner("Wird neu gesendet,", "da keine Reaktion", "von MERLIN-Basis");
      rdcp_cire_resend();
    }
  }

  if (cire_state == CIRE_STATE_WAIT_HQ)
  {
    if (now - cire_starttime > cire_timeout_ack_hq * 1000)
    {
      serial_writeln("WARNING: CIRE Timeout waiting for ACK from HQ");
      cire_button_display_threeliner("Wird neu gesendet,", "da keine Reaktion", "von Krisenstab");
      rdcp_cire_resend();
    }
  }
  return;
}

/**
 * Check whether an incoming ACK matches our ongoing CIRE.
 * If it does, finish the CIRE processing.
 */
void rdcp_cire_ack(uint16_t origin, uint16_t confirmedseqnr, uint8_t acktype)
{
  // Check whether a received ACK matches one of our CIRE SeqNrs.
  // When sent by DA, continue the process. When sent by HQ, finish it.
  if (cire_state == CIRE_STATE_NONE) return; // Nothing to do.

  int64_t now = my_millis();

  bool matches_cire = false;
  for (int i=0; i<16; i++) if ((cire_seqnrs[i] != 0) && (cire_seqnrs[i] == confirmedseqnr)) matches_cire = true;

  if (!matches_cire)
  {
    serial_writeln("INFO: Received ACK does not match current CIRE - ignoring");
    return;
  }

  if ((origin >= RDCPv04_ADDRESS_DA_LOWERBOUND) && (origin < RDCPv04_ADDRESS_MG_LOWERBOUND))
  {
    // Received ACK was from a DA, not by HQ
    if (acktype == 0x00)
    {
      serial_writeln("INFO: Positive ACK from DA received, waiting for HQ ACK");
      cire_state = CIRE_STATE_WAIT_HQ;
      cire_button_display_threeliner("Notfallmeldung", "in MERLIN-Basis", "verarbeitet.");
    }
    else if (acktype == 0x01)
    {
      serial_writeln("INFO: Negative ACK from DA received, aborting CIRE procedure");
      cire_state = CIRE_STATE_NONE;
      for (int i=0; i<16; i++) cire_seqnrs[i] = RDCPv04_SEQUENCENR_SPECIAL_ZERO;
      cire_retry = 0;
      cire_button_cire_ongoing = false;
      cire_button_display_threeliner("Kein Krisenbetrieb.", "Notfall kann nicht", "gemeldet werden.");
      cmd_buzz(CMD_BUZ_FAILURE);
    }
    else if (acktype == 0x02)
    {
      serial_writeln("INFO: Positive-negative ACK from DA received, waiting for HQ ACK");
      cire_state = CIRE_STATE_WAIT_HQ;
      cire_button_display_threeliner("Notfallmeldung wird", "von MERLIN-Basis", "verarbeitet. (keiner da)");
      cmd_buzz(CMD_BUZ_FAILURE);
    }
    else
    {
      serial_writeln("WARNING: Unknown ACK type");
      return;
    }
  }

  if ((origin > 0x0000) && (origin < RDCPv04_ADDRESS_BBKDA_LOWERBOUND))
  {
    // Received ACK was from HQ; in any case, clear current CIRE state:
    for (int i=0; i<16; i++) cire_seqnrs[i] = RDCPv04_SEQUENCENR_SPECIAL_ZERO;
    cire_retry = 0;
    cire_state = CIRE_STATE_NONE;
    cire_button_cire_ongoing = false;

    if (acktype == 0x00)
    {
      serial_writeln("INFO: Positive ACK from HQ received");
      cire_button_display_threeliner("Notfallmeldung ist", "beim Krisenstab", "angekommen.");
      cmd_buzz(CMD_BUZ_SUCCESS);
    }
    else if (acktype == 0x01)
    {
      serial_writeln("INFO: Negative ACK from HQ received");
      cire_button_display_threeliner("Kein Krisenbetrieb.", "Notfall kann nicht", "gemeldet werden.");
    }
    else if (acktype == 0x02)
    {
      serial_writeln("INFO: Positive-negative ACK from HQ received");
      cire_button_display_threeliner("Notfallmeldung ist", "beim Krisenstab", "angekommen. (keiner da)");
    }
    else
    {
      serial_writeln("WARNING: Unknown ACK type");
      return;
    }
  }

  return;
}

void plugin_callback(uint8_t radio, uint8_t channel, int cb_type, bool cad_result)
{
  /* Handle callbacks here. */
  int64_t now = my_millis();

  if ((cb_type == CALLBACK_TYPE_TXFIN) && cire_button_cire_ongoing)
  {
    cire_button_display_threeliner("Notfall-Meldung", "wurde per Funk", "gesendet. Warten!");
    led_blink();
    rdcp_cire_callback();
  }
  else if (cb_type == CALLBACK_TYPE_TXSTART)
  {

  }
  else if (cb_type == CALLBACK_TYPE_CADRES)
  {
    /* cad_result will be CAD_CHANNEL_FREE or CAD_CHANNEL_BUSY */
  }

  /* Note that no RX callback is provided for plugins currently. plugin_incoming() should be used. */

  return;
}

void cire_button_send_cire(void)
{
  int64_t now = my_millis();
  led_on();
  cmd_buzz(CMD_BUZ_TRANSMISSION);
  cire_button_display_threeliner("Notfall wird", "jetzt gemeldet.", "Bitte warten.");

  struct rdcpv04_message rm;
  uint8_t data[DATALEN];
  uint16_t me = cfg.rdcp_address;

  /* The reference identifies the operation; the sequence identifies this CIRE burst. */
  cire_current_subtype = 0x00; // always "emergency" for red button devices
  uint16_t sequence_number = persistence_get_next_rdcp_sequence_number(me);
  snprintf(cire_current_text, DATALEN, "Der rote Alarmknopf wurde aktiviert! Dringender Notfall ohne weitere Angaben!");

  /* Fill the RDCP Header with already known information */
  rm.header.origin = me;
  rm.header.sender = me;
  rm.header.destination  = 0x00FF; // HQ multicast
  rm.header.message_type = RDCPv04_MSGTYPE_CITIZEN_REPORT;
  rm.header.counter = 4;
  rm.header.sequence_number = sequence_number;
  cire_current_seqnr = rm.header.sequence_number;

  uint16_t primary_ep = cfg.default_entry_point;
  rm.header.relay1 = (uint8_t) ((primary_ep & 0x000F) * 16) + (uint8_t) 0x0;
  rm.header.relay2 = 0xEE; // magic value "no relay"
  rm.header.relay3 = 0xEE;

  /* Prepare the MT-specific header at the beginning of the RDCP Payload */
  rm.payload.data[0] = 0x00; // subtype emergency
  rm.payload.data[1] = cire_current_refnr % 256;
  rm.payload.data[2] = cire_current_refnr / 256;

  /* Convert the text message to its Unishox2 representation */
  char buf[INFOLEN];
  unsigned int len = strlen(cire_current_text);
  memset(buf, 0, sizeof(buf));
  int c_total = unishox2_compress_simple(cire_current_text, len, buf);

  /* Fill the RDCP Payload with the Unishox2 content */
  for (int i=0; i < c_total; i++)
  {
    rm.payload.data[i+3] = buf[i];
  }

  /* RDCP Payload length is subheader length (3) + Unishox2 length + AES-GMC AuthTag size (16) */
  rm.header.rdcp_payload_length = 3 + c_total + 16;

  /* AES-GCM encrypt the RDCP Payload */
  uint8_t ciphertext[DATALEN];
  uint8_t iv[12];
  uint8_t gcmauthtag[16];
  uint8_t additional_data[8];
  uint8_t additional_data_size = 8;

  memset(iv, 0, sizeof(iv));
  iv[0] = rm.header.origin % 256;
  iv[1] = rm.header.origin / 256;
  iv[2] = rm.header.sequence_number % 256;
  iv[3] = rm.header.sequence_number / 256;
  iv[4] = rm.header.destination % 256;
  iv[5] = rm.header.destination / 256;
  iv[6] = rm.header.message_type;
  iv[7] = rm.header.rdcp_payload_length;
  for (int i=0; i<additional_data_size; i++) additional_data[i] = iv[i];

  encrypt_aes256gcm((uint8_t *) &rm.payload.data, rm.header.rdcp_payload_length - 16,
                    additional_data, additional_data_size,
                    cfg.rdcp_hqaeskey, KEYLENAES,
                    iv, 12,
                    ciphertext, gcmauthtag, 16);

  /* Copy ciphertext and GCM AuthTag into the RDCP Payload */
  for (int i=0; i<rm.header.rdcp_payload_length-16; i++) rm.payload.data[i] = ciphertext[i];
  for (int i=0; i<16; i++) rm.payload.data[rm.header.rdcp_payload_length-16+i] = gcmauthtag[i];

  /* Finalize the RDCP Header by calculating the checksum */
  uint8_t data_for_crc[DATALEN];
  memcpy(&data_for_crc, &rm.header, RDCPv04_HEADER_SIZE - 2);
  for (int i=0; i < rm.header.rdcp_payload_length; i++)
  {
    data_for_crc[i + RDCPv04_HEADER_SIZE - 2] = rm.payload.data[i];
  }
  uint16_t actual_crc = crc16_rdcpv04(data_for_crc, RDCPv04_HEADER_SIZE - 2 + rm.header.rdcp_payload_length);
  rm.header.checksum = actual_crc;

  /* Schedule the crafted message for sending */
  memcpy(&data, &rm.header, RDCPv04_HEADER_SIZE);
  for (int i=0; i<rm.header.rdcp_payload_length; i++) data[i + RDCPv04_HEADER_SIZE] = rm.payload.data[i];

  scheduler_enqueue(CHANNEL1, PAYLOAD_TYPE_RDCP_V04,
                    data, RDCPv04_HEADER_SIZE + rm.header.rdcp_payload_length,
                    SCHEDULING_MODE_CHANNEL_FREE, TX_CALLBACK_NONE, ZERO_TIMESTAMP);

  return;
}

void cire_button_check_timer(void)
{
  rdcp_cire_check(); // Use ROLODECK implementation for first prototype
  return;
}

void cire_button_display_screensaver(void)
{
  if (my_millis() - cire_button_display_on_since > cire_button_idle_timeout * SECONDS_TO_MILLISECONDS)
  {
    display.displayOff();
  }
  return;
}

extern SX1262* sx1262_radios[MAX_NUMBER_OF_RADIOS];
void cire_button_sleep_if_idle(void)
{
#if defined(ESP32)
  static bool sleep_unavailable_warning_printed = OPTION_DISABLED;

  if (cire_button_cire_ongoing) return;
  if (my_millis() - cire_button_display_on_since <= cire_button_idle_timeout * SECONDS_TO_MILLISECONDS) return;

  if (cfg.additional_wakeup_pin == PIN_NOT_USED)
  {
    if (!sleep_unavailable_warning_printed)
    {
      serial_writeln("WARNING: Not entering deep sleep, no wake-up pin configured");
      sleep_unavailable_warning_printed = OPTION_ENABLED;
    }
    return;
  }

  if (cfg.radios_initialized && sx1262_radios[0] != NULL)
  {
    sx1262_radios[0]->standby(); // do not wake up by LoRa packet reception
    sx1262_radios[0]->sleep(false);
  }

  sleep_deep();
#endif

  return;
}

void ledTask(void* param)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(ledPin, pwmFreq, pwmResolution);
#else
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(ledPin, pwmChannel);
#endif

  while (true)
  {
    ledcWrite(pwmChannel, ledBrightness);
    vTaskDelay(pdMS_TO_TICKS(1000));
    uint8_t brightness = ledBlink ? 0 : ledBrightness;
    ledcWrite(pwmChannel, brightness);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void buzTask(void* param)
{
  enum Buz_Cmd cmd;

  while (true)
  {
    if (xQueueReceive(buzTaskQueue, &cmd, portMAX_DELAY) == pdPASS) {
      if (cmd == CMD_BUZ_TEST){
        char buf[INFOLEN];
        for (int freq = 200; freq < 4000; freq+=100){
          snprintf(buf, INFOLEN, "INFO: Playing test tone at %d Hz", freq);
          serial_writeln(buf);
          buzz(freq, buzVolume, 1000);
        }
      } else {
        // get commanded melody
        struct melody* melody = &melodies[cmd];

        // play the melody
        for (int j = 0; j < melody->repeats; j++)
        {
          for (int i = 0; i < MELODY_LEN; i++){
            // at least the Heltec V4 crashes if the frequency is below 200
            // also 0 is used to indicate to skip that tone
            if (melody->tones[i].freq < 200){
              continue;
            }
            buzz(melody->tones[i].freq, buzVolume, melody->tones[i].duration);
            vTaskDelay(pdMS_TO_TICKS(melody->tones[i].gap));
          }
          vTaskDelay(pdMS_TO_TICKS(melody->delay));
        }
      }
    }
  }
}

void plugin_loop(void)
{
  /* Handle any periodic tasks here. Called in each main loop. */

  int64_t now = my_millis();
  bool cire_button_pressed = false;

  /* Initialize the hardware we need for the CIRE Button prototype */
  if (!cire_button_initialized)
  {
    xTaskCreatePinnedToCore(
        ledTask,
        "ledTask",
        2048,
        nullptr,
        1,        // priority
        nullptr,
        1         // core (0 or 1)
      );

    buzTaskQueue = xQueueCreate(8, sizeof(Buz_Cmd));

    xTaskCreatePinnedToCore(
        buzTask,
        "buzTask",
        2048,
        nullptr,
        1,        // priority
        nullptr,
        1         // core (0 or 1)
      );

    /* Configure the input before validating an EXT1 wake from the button. */
    pinMode(CIRE_BUTTON_PIN, INPUT_PULLUP);

#if defined(ESP32)
    if (cfg.woken_from_deep_sleep == OPTION_ENABLED)
    {
      if (cire_button_wakeup_behavior == WAKEUPBEHAVIOR_CIRE)
      {
        cire_button_cire_on_wakeup_pending = OPTION_ENABLED;
      }
      else if ((esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) &&
               (cfg.additional_wakeup_pin == CIRE_BUTTON_PIN) &&
               (esp_sleep_get_ext1_wakeup_status() & (1ULL << CIRE_BUTTON_PIN)))
      {
        button_press_by_wakeup = OPTION_ENABLED;
      }
    }
#endif

    if ((button_press_by_wakeup == OPTION_ENABLED) || cire_button_cire_on_wakeup_pending)
    {
      // wakup from sleep
      cmd_buzz(CMD_BUZ_WAKEUP);
    } else {
      // cold start
      cmd_buzz(CMD_BUZ_BOOT);
    }

    if (cfg.rdcp_address == 0x0000) cfg.rdcp_address = 0xA110; // demo RDCP address for this device
    if (cfg.default_entry_point == RDCP_ADDRESS_ZERO)
    {
      cfg.default_entry_point = cire_button_default_entry_point;
    }
    if (cire_button_hq_shared_secret_is_unset())
    {
      for (int i = COUNT_ZERO; i < KEYLENAES; i++) cfg.rdcp_hqaeskey[i] = CIRE_BUTTON_DEFAULT_HQ_SHARED_SECRET[i];
    }

    /* Prepare the used display for human-readable feedback */
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
    delay(100); // recommended by Heltec firmware examples
    display.init();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    cire_button_display_threeliner("MERLIN", "Alarmknopf", "ist betriebsbereit");

    // use 12 bit precision for LEDC timer
    #define LEDC_TIMER_12_BIT 12

    // use 5000 Hz as a LEDC base frequency
    #define LEDC_BASE_FREQ 5000

    // fade LED PIN (replace with LED_BUILTIN constant for built-in LED)
    #define LED_PIN 4

    if (!cfg.radios_initialized)
    {
      cire_button_display_threeliner("Bitte erst", "die Konfiguration", "einrichten. ;-)");
    }

    /* Make sure we initialize only once */
    cire_button_initialized = true;
  }

  if (button_press_by_wakeup)
  {
    serial_writeln("INFO: Wake-up reason was button press");
  }
  if (cire_button_cire_on_wakeup_pending)
  {
    serial_writeln("INFO: Wake-up behavior is CIRE; sending a CIRE");
    cire_button_cire_on_wakeup_pending = OPTION_DISABLED;
    cire_button_pressed = true;
    cire_button_last_press_timestamp = now;
  }

  /* Require a stable low level to reject wake-up glitches and button bounce. */
  if (digitalRead(CIRE_BUTTON_PIN) == LOW)
  {
    /* Turn on the display when the button is pressed in any case */
    display.displayOn();
    cire_button_display_on_since = my_millis();

    if (cire_button_low_since == TIMESTAMP_ZERO) cire_button_low_since = now;

    /* React only after debounce, unless the same press was handled recently. */
    if ((now - cire_button_low_since >= cire_button_depressed_timeout) &&
        (button_press_by_wakeup || (now - cire_button_last_press_timestamp > CIRE_BUTTON_REPEAT_INTERVAL)))
    {
      cire_button_pressed = true;
      cire_button_last_press_timestamp = now;
      button_press_by_wakeup = OPTION_DISABLED;
    }
  }
  else
  {
    cire_button_low_since = TIMESTAMP_ZERO;
    if (button_press_by_wakeup == OPTION_ENABLED)
    {
      serial_writeln("INFO: Ignoring EXT1 wake-up: button was no longer pressed");
      button_press_by_wakeup = OPTION_DISABLED;
    }
  }

  /* If the button has been pressed, we need to send a CIRE unless the most recent one is still ongoing */
  if (cire_button_pressed)
  {
    serial_writeln("INFO: CIRE Button has been pressed!");
    if (!cire_button_cire_ongoing)
    {
      cire_button_cire_ongoing = true;
      cire_retry = ZERO_LENGTH;
      cire_current_refnr = persistence_get_next_cire_reference_number(cfg.rdcp_address);
      cire_button_send_cire();
    }
  }

  /*
    If we have sent a CIRE, but did not receive the MERLIN Base and MERLIN HQ ACKs yet,
    check for timeouts and consider re-sending a fresh CIRE.
  */
  if (cire_button_cire_ongoing)
  {
    cire_button_check_timer();
  }

  /*
    To conserve energy, we turn off the display after a while
  */
  cire_button_display_screensaver();
  cire_button_sleep_if_idle();

  return;
}

/* EOF rdcp-modem-plugin.cpp */
