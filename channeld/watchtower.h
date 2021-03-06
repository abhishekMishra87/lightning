#ifndef LIGHTNING_CHANNELD_WATCHTOWER_H
#define LIGHTNING_CHANNELD_WATCHTOWER_H
#include "config.h"
#include <common/initial_channel.h>

struct ext_key;

const struct brocoin_tx *
penalty_tx_create(const tal_t *ctx,
		  const struct channel *channel,
		  u32 penalty_feerate,
		  u32 *final_index,
		  struct ext_key *final_ext_key,
		  u8 *final_scriptpubkey,
		  const struct secret *revocation_preimage,
		  const struct brocoin_txid *commitment_txid,
		  s16 to_them_outnum, struct amount_bro to_them_bros,
		  int hsm_fd);

#endif /* LIGHTNING_CHANNELD_WATCHTOWER_H */
