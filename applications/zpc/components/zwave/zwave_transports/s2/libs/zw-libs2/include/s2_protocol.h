/* © 2017 Silicon Laboratories Inc.
 */
/*
 * s2_protocol.h
 *
 *  Created on: Jun 30, 2015
 *      Author: aes
 */

#ifndef PROTOCOL_S2_PROTOCOL_H_
#define PROTOCOL_S2_PROTOCOL_H_
#include <stdbool.h>
#include "S2.h"
#include "ctr_drbg.h"
#include "s2_classcmd.h"
#include "ZW_typedefs.h"
#include "ZW_classcmd.h"

/**
 * How many S2 sequence numbers are considered to be duplicates.
 * Specification requires at least one.  A value of zero would mean no
 * duplicate detection.
 */
#define S2_SEQ_DUPL_WINDOW_SIZE ((uint8_t)2)

#define UNENCRYPTED_CLASS 0xFF
#ifdef __C51__
#define SPAN_TABLE_SIZE 5
#define MPAN_TABLE_SIZE 5
#elif defined(ZW_CONTROLLER) && !defined(HOST_SECURITY_INCLUDED)
/* Largest size that fits in uint8_t (minus 1). */
#define SPAN_TABLE_SIZE 254
#define MPAN_TABLE_SIZE 254
#else
#define SPAN_TABLE_SIZE 10
#define MPAN_TABLE_SIZE 10
#endif
#define MOS_LIST_LENGTH 3
#if defined(EFR32ZG) || defined(ZW050x)
#define WORKBUF_SIZE 200
#else
#define WORKBUF_SIZE 1280
#endif
#define S2_MULTICAST

typedef uint8_t nonce_t[16];

#ifdef ZW_CONTROLLER
#define N_SEC_CLASS 7
#else
#define N_SEC_CLASS 3
#endif
#define DSK_SSA_CHALLENGE_LENGTH 2
#define DSK_CSA_CHALLENGE_LENGTH 4

typedef uint8_t public_key_t[32];

typedef enum
{
  SPAN_NOT_USED,
  SPAN_NO_SEQ,
  SPAN_SOS,
  SPAN_SOS_LOCAL_NONCE,
  SPAN_SOS_REMOTE_NONCE,
  SPAN_INSTANTIATE,
  SPAN_NEGOTIATED
} span_state_t;


typedef enum{
  S2_INC_IDLE,
  S2_AWAITING_KEX_GET,
  S2_AWAITING_KEX_REPORT,
  S2_AWAITING_KEY_USER_ACCEPT,
  S2_AWAITING_KEX_SET,
  S2_AWAITING_PUB_KEY_A,
  S2_AWAITING_PUB_KEY_B,
  S2_AWAITING_USER_ACCEPT,
  S2_AWAITING_USER_A_ACCEPT,
  S2_PENDING_ECHO_KEX_REPORT,
  S2_ECHO_KEX_SET_SENDING,
  S2_AWAITING_ECHO_KEX_SET,
  S2_AWAITING_ECHO_KEX_REPORT,
  S2_AWAITING_NET_KEY_GET,
  S2_AWAITING_NET_KEY_REPORT,
  S2_AWAITING_NET_KEY_VERIFY,
  S2_AWAITING_TRANSFER_END,
  S2_KEY_EXCHANGED,
  S2_SENDING_FINAL_TRANSFER_END,
  S2_ERROR_SENT,
  S2_INC_STATE_ANY,
}s2_inclusion_state_t;


struct SPAN
{
  union
  {
    CTR_DRBG_CTX rng;
    uint8_t r_nonce[16];
  } d;
  node_t lnode;
  node_t rnode;

  uint8_t rx_seq; // sequence number of last received message
  uint8_t tx_seq; // sequence number of last sent message

  security_class_t class_id; //The id of the security group in which this span is negotiated.
  span_state_t state;
};

struct MPAN
{
  node_t owner_id; //this is the node id of the node maintaining mcast group members
  uint8_t group_id; //a unique id generated by the group maintainer(sender)
  uint8_t inner_state[16]; //The Multicast  pre-agreed nonce inner state
  security_class_t class_id;

  enum
  {
    MPAN_NOT_USED, MPAN_SET, MPAN_MOS
  } state; //State of this entry
};

struct MOS_LIST {
  node_t node_id; //node_id if reserved to "not used"
  uint8_t group_id;
};

typedef enum {
  IDLE,
  WAIT_NONCE_RAPORT,
  SENDING_MSG,
  SENDING_MULTICAST,
  VERIFYING_DELIVERY,
  IS_MOS_WAIT_REPLY, //Wait for reply from application layer...
} states_t;

typedef struct {
  const s2_connection_t *con;
  union {
    struct {
      const uint8_t* buffer;
      uint16_t len;
    } buf;
    struct {
      uint8_t status;
      uint16_t time;
    } tx;
  } d;
} event_data_t;

typedef enum {
  SEND_MSG,
  SEND_MULTICAST,
  SEND_DONE,
  GOT_NONCE_GET,
  GOT_NONCE_RAPORT,
  GOT_ENC_MSG,
  GOT_BAD_ENC_MSG,
  GOT_ENC_MSG_MOS,
  TIMEOUT,
  ABORT,
  SEND_FOLLOW_UP,
} event_t;

typedef enum {
   AUTH_OK,
   PARSE_FAIL,
   AUTH_FAIL,
   SEQUENCE_FAIL,
} decrypt_return_code_t;

//#define S2_MULTICAST
struct S2
{
  struct sec_group {
    network_key_t enc_key; //Ke 16 bytes
    network_key_t mpan_key; //Ke 16 bytes
    uint8_t nonce_key[32]; //Knonce 32 bytes
  } sg[N_SEC_CLASS];

  uint8_t csa_support;
  uint8_t kex_set_byte2;
  uint8_t scheme_support;
  uint8_t curve_support;
  uint8_t key_granted;       // The granted keys bitmask. Including the LR bits if bootstrapping a controller only.
  /* The key_exchange field works a little differently on joining and inclusind side.
   * In joining side it has a single bit being left-shifted through all positions. Each
   * bit corresponds to an S2 key. If that key has been granted and is
   * supported by the grantee, it will be exchanged.
   * In including side, it is a bitmask of all keys exchanged so far.
   * This field includes the LR key bits. */
  /* TODO: Seperate key_exchange into separate variables (or union) for joining and including side*/
  uint8_t key_exchange;      // Single bit rolling across bitmask of all keys to keep track of which keys have been exchanged so far.
  uint8_t key_requested;     // A single bit identifying the key that has been requested most recently in a Network Key Get frame.
  uint8_t kex_report_keys;   // The unmodified keys from the incoming Kex Report. Used for echoing back. No filtering for LR etc.

  uint8_t loaded_keys; //Bit mask of S2 keys in use
  uint32_t my_home_id;

  const uint8_t* sec_commands_supported_frame;
  uint8_t sec_commands_supported_frame_size;

  s2_connection_t peer;
  s2_connection_t inclusion_peer;

  const uint8_t * buf;
  uint16_t length;

  struct SPAN  *span; //The current span
  struct MPAN  *mpan; //The current mpan


  struct SPAN span_table[SPAN_TABLE_SIZE];
#ifdef S2_MULTICAST
  struct MPAN mpan_table[MPAN_TABLE_SIZE];
  struct MOS_LIST mos_list[MOS_LIST_LENGTH];
#endif
  states_t fsm;
  uint8_t retry;
  s2_inclusion_state_t inclusion_state;
  enum {INCLUSION_MODE_CSA, INCLUSION_MODE_SSA} inclusion_mode;
  uint8_t kex_fail_code;
  uint8_t workbuf[WORKBUF_SIZE + 10 + 8 + 4];
  union {
    uint8_t inclusion_buf[40];
    uint8_t commands_sup_report_buf[40];
    uint8_t capabilities_report_buf[40];
  } u;
  uint8_t inclusion_buf_length;

  public_key_t public_key;
  bool is_keys_restored;
  //network_key_t temp_network_key;
};

#ifdef SINGLE_CONTEXT
#define s2_inclusion_post_event_peer(a, b)                  s2_inclusion_post_event(b)
#endif

/**
 * Must be called from s2_inclusion_send_done_callback implementation in glue layer
 * when a transmission success occured.
 *
 * @param[in] p_context Structure identifying the context for current inclusion.
 * @param[in] status    true on sucsess
 */
void s2_inclusion_send_done(struct S2 *p_context, uint8_t status);

/** @brief This function notifies the S2 inclusion state machine that a frame could not be decrypted.
 *
 *  @details During inclusion and exchange of public keys there is a risk that an invalid public
 *           key is used for calculating a common shared secret. This may happen in case an
 *           end-user provides wrond digits for the mising part of the DSK. If a frame cannot be
 *           decrypted the protocol can notify the inclusion state machine through this function.
 *
 * @param[in] p_context  Structure identifying the context for current inclusion.
 * @param[in] src        Defining the source and destination pair, where the failed decryption occured.
 */
void s2_inclusion_decryption_failure(struct S2 *p_context,s2_connection_t* src);

/**
 * @breif This function handles events received during secure inclusion of a node, but also check if \ref src
 * matches the current inclusion peer.
 */
void s2_inclusion_post_event(struct S2 *p_context,s2_connection_t* src);


/**
 * Load all keys from keystore
 */
void s2_restore_keys(struct S2 *p_context, bool make_keys_persist_se);


/** Update the network key of a context
 * A context is unique to a node id, ie a bridge module will have multiple contexts.
 *
 * \param ctx Pointer to the context to update.
 * \param key_id Id of key stored in secure vault, accessed via PSA APIs. 
 *
 * \param class_id security class to update
 *
 * \param net_key Network key to use for this context. Only the derived keys are stored in SRAM. Remember for clear the
 * net_key from SRAM after calling this function.
 *
 * \param temp_key_expand Set to true if temp key is being expanded.
 *
 * \param make_keys_persist_se Set to true if the key should be saved in as a persistent key.
 *
 */
uint8_t
S2_network_key_update(struct S2 *ctxt, uint32_t key_id, security_class_t class_id, const network_key_t net_key, uint8_t temp_key_expand, bool make_keys_persist_se);

#endif /* PROTOCOL_S2_PROTOCOL_H_ */