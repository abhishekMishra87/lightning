#include "config.h"
#include <brocoin/script.h>
#include <ccan/cast/cast.h>
#include <ccan/fdpass/fdpass.h>
#include <ccan/tal/str/str.h>
#include <closingd/closingd_wiregen.h>
#include <common/close_tx.h>
#include <common/closing_fee.h>
#include <common/derive_basepoints.h>
#include <common/htlc.h>
#include <common/memleak.h>
#include <common/peer_billboard.h>
#include <common/peer_failed.h>
#include <common/peer_io.h>
#include <common/per_peer_state.h>
#include <common/read_peer_msg.h>
#include <common/status.h>
#include <common/subdaemon.h>
#include <common/type_to_string.h>
#include <common/utils.h>
#include <common/version.h>
#include <common/wire_error.h>
#include <errno.h>
#include <gossipd/gossipd_peerd_wiregen.h>
#include <hsmd/hsmd_wiregen.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <wally_bip32.h>
#include <wire/peer_wire.h>
#include <wire/wire_sync.h>

/* stdin == requests, 3 == peer, 4 = hsmd */
#define REQ_FD STDIN_FILENO
#define HSM_FD 4

static void notify(enum log_level level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	wire_sync_write(REQ_FD,
			take(towire_closingd_notification(NULL,
							  level,
							  tal_vfmt(tmpctx, fmt,
								   ap))));

	va_end(ap);
}

static struct brocoin_tx *close_tx(const tal_t *ctx,
				   const struct chainparams *chainparams,
				   struct per_peer_state *pps,
				   const struct channel_id *channel_id,
				   u32 *local_wallet_index,
				   const struct ext_key *local_wallet_ext_key,
				   u8 *scriptpubkey[NUM_SIDES],
				   const struct brocoin_outpoint *funding,
				   struct amount_bro funding_bros,
				   const u8 *funding_wscript,
				   const struct amount_bro out[NUM_SIDES],
				   enum side opener,
				   struct amount_bro fee,
				   struct amount_bro dust_limit,
				   const struct brocoin_outpoint *wrong_funding)
{
	struct brocoin_tx *tx;
	struct amount_bro out_minus_fee[NUM_SIDES];

	out_minus_fee[LOCAL] = out[LOCAL];
	out_minus_fee[REMOTE] = out[REMOTE];
	if (!amount_bro_sub(&out_minus_fee[opener], out[opener], fee))
		peer_failed_warn(pps, channel_id,
				 "Funder cannot afford fee %s (%s and %s)",
				 type_to_string(tmpctx, struct amount_bro, &fee),
				 type_to_string(tmpctx, struct amount_bro,
						&out[LOCAL]),
				 type_to_string(tmpctx, struct amount_bro,
						&out[REMOTE]));

	status_debug("Making close tx at = %s/%s fee %s",
		     type_to_string(tmpctx, struct amount_bro, &out[LOCAL]),
		     type_to_string(tmpctx, struct amount_bro, &out[REMOTE]),
		     type_to_string(tmpctx, struct amount_bro, &fee));

	/* FIXME: We need to allow this! */
	tx = create_close_tx(ctx,
			     chainparams,
			     local_wallet_index, local_wallet_ext_key,
			     scriptpubkey[LOCAL], scriptpubkey[REMOTE],
			     funding_wscript,
			     funding,
			     funding_bros,
			     out_minus_fee[LOCAL],
			     out_minus_fee[REMOTE],
			     dust_limit);
	if (!tx)
		peer_failed_err(pps, channel_id,
				"Both outputs below dust limit:"
				" funding = %s"
				" fee = %s"
				" dust_limit = %s"
				" LOCAL = %s"
				" REMOTE = %s",
				type_to_string(tmpctx, struct amount_bro, &funding_bros),
				type_to_string(tmpctx, struct amount_bro, &fee),
				type_to_string(tmpctx, struct amount_bro, &dust_limit),
				type_to_string(tmpctx, struct amount_bro, &out[LOCAL]),
				type_to_string(tmpctx, struct amount_bro, &out[REMOTE]));

	if (wrong_funding)
		brocoin_tx_input_set_outpoint(tx, 0, wrong_funding);

	return tx;
}

/* Handle random messages we might get, returning the first non-handled one. */
static u8 *closing_read_peer_msg(const tal_t *ctx,
				 struct per_peer_state *pps,
				 const struct channel_id *channel_id)
{
	for (;;) {
		u8 *msg;

		clean_tmpctx();
		msg = peer_read(ctx, pps);
		if (!handle_peer_error(pps, channel_id, msg))
			return msg;
	}
}

static void send_offer(struct per_peer_state *pps,
		       const struct chainparams *chainparams,
		       const struct channel_id *channel_id,
		       const struct pubkey funding_pubkey[NUM_SIDES],
		       const u8 *funding_wscript,
		       u32 *local_wallet_index,
		       const struct ext_key *local_wallet_ext_key,
		       u8 *scriptpubkey[NUM_SIDES],
		       const struct brocoin_outpoint *funding,
		       struct amount_bro funding_bros,
		       const struct amount_bro out[NUM_SIDES],
		       enum side opener,
		       struct amount_bro our_dust_limit,
		       struct amount_bro fee_to_offer,
		       const struct brocoin_outpoint *wrong_funding,
		       const struct tlv_closing_signed_tlvs_fee_range *tlv_fees)
{
	struct brocoin_tx *tx;
	struct brocoin_signature our_sig;
	struct tlv_closing_signed_tlvs *close_tlvs;
	u8 *msg;

	/* BOLT #2:
	 *
	 *   - MUST set `signature` to the Brocoin signature of the close
	 *     transaction, as specified in [BOLT
	 *     #3](03-transactions.md#closing-transaction).
	 */
	tx = close_tx(tmpctx, chainparams, pps, channel_id,
		      local_wallet_index,
		      local_wallet_ext_key,
		      scriptpubkey,
		      funding,
		      funding_bros,
		      funding_wscript,
		      out,
		      opener, fee_to_offer, our_dust_limit,
		      wrong_funding);

	/* BOLT #3:
	 *
	 * ## Closing Transaction
	 *...
	 * Each node offering a signature... MAY eliminate its
	 * own output.
	 */
	/* (We don't do this). */
	wire_sync_write(HSM_FD,
			take(towire_hsmd_sign_mutual_close_tx(NULL,
							     tx,
							     &funding_pubkey[REMOTE])));
	msg = wire_sync_read(tmpctx, HSM_FD);
	if (!fromwire_hsmd_sign_tx_reply(msg, &our_sig))
		status_failed(STATUS_FAIL_HSM_IO,
			      "Bad hsm_sign_mutual_close_tx reply %s",
			      tal_hex(tmpctx, msg));

	status_debug("sending fee offer %s",
		     type_to_string(tmpctx, struct amount_bro, &fee_to_offer));

	/* Add the new close_tlvs with our fee range */
	if (tlv_fees) {
		close_tlvs = tlv_closing_signed_tlvs_new(msg);
		close_tlvs->fee_range
			= cast_const(struct tlv_closing_signed_tlvs_fee_range *,
				     tlv_fees);
		notify(LOG_INFORM, "Sending closing fee offer %s, with range %s-%s",
		       type_to_string(tmpctx, struct amount_bro, &fee_to_offer),
		       type_to_string(tmpctx, struct amount_bro, &tlv_fees->min_fee_bronees),
		       type_to_string(tmpctx, struct amount_bro, &tlv_fees->max_fee_bronees));
	} else
		close_tlvs = NULL;

	assert(our_sig.sighash_type == SIGHASH_ALL);
	msg = towire_closing_signed(NULL, channel_id, fee_to_offer, &our_sig.s,
				    close_tlvs);

	peer_write(pps, take(msg));
}

static void tell_master_their_offer(const struct brocoin_signature *their_sig,
				    const struct brocoin_tx *tx,
				    struct brocoin_txid *tx_id)
{
	u8 *msg = towire_closingd_received_signature(NULL, their_sig, tx);
	if (!wire_sync_write(REQ_FD, take(msg)))
		status_failed(STATUS_FAIL_MASTER_IO,
			      "Writing received to master: %s",
			      strerror(errno));

	/* Wait for master to ack, to make sure it's in db. */
	msg = wire_sync_read(NULL, REQ_FD);
	if (!fromwire_closingd_received_signature_reply(msg, tx_id))
		master_badmsg(WIRE_CLOSINGD_RECEIVED_SIGNATURE_REPLY, msg);
	tal_free(msg);
}

/* Returns fee they offered. */
static struct amount_bro
receive_offer(struct per_peer_state *pps,
	      const struct chainparams *chainparams,
	      const struct channel_id *channel_id,
	      const struct pubkey funding_pubkey[NUM_SIDES],
	      const u8 *funding_wscript,
	      u32 *local_wallet_index,
	      const struct ext_key *local_wallet_ext_key,
	      u8 *scriptpubkey[NUM_SIDES],
	      const struct brocoin_outpoint *funding,
	      struct amount_bro funding_bros,
	      const struct amount_bro out[NUM_SIDES],
	      enum side opener,
	      struct amount_bro our_dust_limit,
	      struct amount_bro min_fee_to_accept,
	      const struct brocoin_outpoint *wrong_funding,
	      struct brocoin_txid *closing_txid,
	      struct tlv_closing_signed_tlvs_fee_range **tlv_fees)
{
	u8 *msg;
	struct channel_id their_channel_id;
	struct amount_bro received_fee;
	struct brocoin_signature their_sig;
	struct brocoin_tx *tx;
	struct tlv_closing_signed_tlvs *close_tlvs;

	/* Wait for them to say something interesting */
	do {
		msg = closing_read_peer_msg(tmpctx, pps, channel_id);

		/* BOLT #2:
		 *
		 *  - upon reconnection:
		 *     - MUST ignore any redundant `funding_locked` it receives.
		 */
		/* This should only happen if we've made no commitments, but
		 * we don't have to check that: it's their problem. */
		if (fromwire_peektype(msg) == WIRE_FUNDING_LOCKED)
			msg = tal_free(msg);
		/* BOLT #2:
		 *     - if it has sent a previous `shutdown`:
		 *       - MUST retransmit `shutdown`.
		 */
		else if (fromwire_peektype(msg) == WIRE_SHUTDOWN)
			msg = tal_free(msg);
		/* channeld may have sent ping: ignore pong! */
		else if (fromwire_peektype(msg) == WIRE_PONG)
			msg = tal_free(msg);
	} while (!msg);

	their_sig.sighash_type = SIGHASH_ALL;
	if (!fromwire_closing_signed(msg, msg, &their_channel_id,
				     &received_fee, &their_sig.s,
				     &close_tlvs))
		peer_failed_warn(pps, channel_id,
				 "Expected closing_signed: %s",
				 tal_hex(tmpctx, msg));

	/* BOLT #2:
	 *
	 * The receiving node:
	 *   - if the `signature` is not valid for either variant of closing transaction
	 *   specified in [BOLT #3](03-transactions.md#closing-transaction)
	 *   OR non-compliant with LOW-S-standard rule...:
	 *     - MUST send a `warning` and close the connection, or send an
	 *	 `error` and fail the channel.
	 */
	tx = close_tx(tmpctx, chainparams, pps, channel_id,
		      local_wallet_index,
		      local_wallet_ext_key,
		      scriptpubkey,
		      funding,
		      funding_bros,
		      funding_wscript,
		      out, opener, received_fee, our_dust_limit,
		      wrong_funding);

	if (!check_tx_sig(tx, 0, NULL, funding_wscript,
			  &funding_pubkey[REMOTE], &their_sig)) {
		/* Trim it by reducing their output to minimum */
		struct brocoin_tx *trimmed;
		struct amount_bro trimming_out[NUM_SIDES];

		if (opener == REMOTE)
			trimming_out[REMOTE] = received_fee;
		else
			trimming_out[REMOTE] = AMOUNT_BRO(0);
		trimming_out[LOCAL] = out[LOCAL];

		/* BOLT #3:
		 *
		 * Each node offering a signature:
		 *   - MUST round each output down to whole bronees.
		 *   - MUST subtract the fee given by `fee_bronees` from the
		 *     output to the funder.
		 *   - MUST remove any output below its own
		 *    `dust_limit_bronees`.
		 *   - MAY eliminate its own output.
		 */
		trimmed = close_tx(tmpctx, chainparams, pps, channel_id,
				   local_wallet_index,
				   local_wallet_ext_key,
				   scriptpubkey,
				   funding,
				   funding_bros,
				   funding_wscript,
				   trimming_out,
				   opener, received_fee, our_dust_limit,
				   wrong_funding);
		if (!trimmed
		    || !check_tx_sig(trimmed, 0, NULL, funding_wscript,
				     &funding_pubkey[REMOTE], &their_sig)) {
			peer_failed_warn(pps, channel_id,
					 "Bad closing_signed signature for"
					 " %s (and trimmed version %s)",
					 type_to_string(tmpctx,
							struct brocoin_tx,
							tx),
					 trimmed ?
					 type_to_string(tmpctx,
							struct brocoin_tx,
							trimmed)
					 : "NONE");
		}
		tx = trimmed;
	}

	status_debug("Received fee offer %s",
		     type_to_string(tmpctx, struct amount_bro, &received_fee));

	if (tlv_fees) {
		if (close_tlvs) {
			*tlv_fees = tal_steal(tlv_fees, close_tlvs->fee_range);
		} else {
			*tlv_fees = NULL;
		}
	}

	if (close_tlvs && close_tlvs->fee_range) {
		notify(LOG_INFORM, "Received closing fee offer %s, with range %s-%s",
		       type_to_string(tmpctx, struct amount_bro, &received_fee),
		       type_to_string(tmpctx, struct amount_bro,
				      &close_tlvs->fee_range->min_fee_bronees),
		       type_to_string(tmpctx, struct amount_bro,
				      &close_tlvs->fee_range->max_fee_bronees));
	} else {
		notify(LOG_INFORM, "Received closing fee offer %s, without range",
		       type_to_string(tmpctx, struct amount_bro, &received_fee));
	}

	/* Master sorts out what is best offer, we just tell it any above min */
	if (amount_bro_greater_eq(received_fee, min_fee_to_accept)) {
		status_debug("...offer is reasonable");
		tell_master_their_offer(&their_sig, tx, closing_txid);
	}

	return received_fee;
}

struct feerange {
	enum side higher_side;
	struct amount_bro min, max;
};

static void init_feerange(struct feerange *feerange,
			  struct amount_bro commitment_fee,
			  const struct amount_bro offer[NUM_SIDES])
{
	feerange->min = AMOUNT_BRO(0);

	/* FIXME: BOLT 2 previously said that we have to set it to less than
	 * the final commit fee: we do this for now, still:
	 *
	 *  - MUST set `fee_bronees` less than or equal to the base
         *    fee of the final commitment transaction, as calculated
         *    in [BOLT #3](03-transactions.md#fee-calculation).
	 */
	feerange->max = commitment_fee;

	if (amount_bro_greater(offer[LOCAL], offer[REMOTE]))
		feerange->higher_side = LOCAL;
	else
		feerange->higher_side = REMOTE;

	status_debug("Feerange init %s-%s, %s higher",
		     type_to_string(tmpctx, struct amount_bro, &feerange->min),
		     type_to_string(tmpctx, struct amount_bro, &feerange->max),
		     feerange->higher_side == LOCAL ? "local" : "remote");
}

static void adjust_feerange(struct feerange *feerange,
			    struct amount_bro offer, enum side side)
{
	bool ok;

	/* FIXME: BOLT 2 previously said that we have to set it to less than
	 * the final commit fee: we do this for now, still:
	 *
	 *     - MUST propose a value "strictly between" the received
	 *      `fee_bronees` and its previously-sent `fee_bronees`.
	 */
	if (side == feerange->higher_side)
		ok = amount_bro_sub(&feerange->max, offer, AMOUNT_BRO(1));
	else
		ok = amount_bro_add(&feerange->min, offer, AMOUNT_BRO(1));

	status_debug("Feerange %s update %s: now %s-%s",
		     side == LOCAL ? "local" : "remote",
		     type_to_string(tmpctx, struct amount_bro, &offer),
		     type_to_string(tmpctx, struct amount_bro, &feerange->min),
		     type_to_string(tmpctx, struct amount_bro, &feerange->max));

	if (!ok)
		status_failed(STATUS_FAIL_INTERNAL_ERROR,
			      "Overflow in updating fee range");
}

/* Do these two ranges overlap?  If so, return that range. */
static bool get_overlap(const struct tlv_closing_signed_tlvs_fee_range *r1,
			const struct tlv_closing_signed_tlvs_fee_range *r2,
			struct tlv_closing_signed_tlvs_fee_range *overlap)
{
 	if (amount_bro_greater(r1->min_fee_bronees, r2->min_fee_bronees))
		overlap->min_fee_bronees = r1->min_fee_bronees;
	else
		overlap->min_fee_bronees = r2->min_fee_bronees;
 	if (amount_bro_less(r1->max_fee_bronees, r2->max_fee_bronees))
		overlap->max_fee_bronees = r1->max_fee_bronees;
	else
		overlap->max_fee_bronees = r2->max_fee_bronees;

	return amount_bro_less_eq(overlap->min_fee_bronees,
				  overlap->max_fee_bronees);
}

/* Is this amount in this range? */
static bool amount_in_range(struct amount_bro amount,
			    const struct tlv_closing_signed_tlvs_fee_range *r)
{
	return amount_bro_greater_eq(amount, r->min_fee_bronees)
		&& amount_bro_less_eq(amount, r->max_fee_bronees);
}

/* Figure out what we should offer now. */
static struct amount_bro
adjust_offer(struct per_peer_state *pps, const struct channel_id *channel_id,
	     const struct feerange *feerange, struct amount_bro remote_offer,
	     struct amount_bro min_fee_to_accept, u64 fee_negotiation_step,
	     u8 fee_negotiation_step_unit)
{
	struct amount_bro min_plus_one, range_len, step_bro, result;
	struct amount_mbro step_mbro;

	/* Within 1 bronee?  Agree. */
	if (!amount_bro_add(&min_plus_one, feerange->min, AMOUNT_BRO(1)))
		peer_failed_warn(pps, channel_id,
				 "Fee offer %s min too large",
				 type_to_string(tmpctx, struct amount_bro,
						&feerange->min));

	if (amount_bro_greater_eq(min_plus_one, feerange->max))
		return remote_offer;

	/* feerange has already been adjusted so that our new offer is ok to be
	 * any number in [feerange->min, feerange->max] and after the following
	 * min_fee_to_accept is in that range. Thus, pick a fee in
	 * [min_fee_to_accept, feerange->max]. */
	if (amount_bro_greater(feerange->min, min_fee_to_accept))
		min_fee_to_accept = feerange->min;

	/* Max is below our minimum acceptable? */
	if (!amount_bro_sub(&range_len, feerange->max, min_fee_to_accept))
		peer_failed_warn(pps, channel_id,
				 "Feerange %s-%s"
				 " below minimum acceptable %s",
				 type_to_string(tmpctx, struct amount_bro,
						&feerange->min),
				 type_to_string(tmpctx, struct amount_bro,
						&feerange->max),
				 type_to_string(tmpctx, struct amount_bro,
						&min_fee_to_accept));

	if (fee_negotiation_step_unit ==
	    CLOSING_FEE_NEGOTIATION_STEP_UNIT_BRONEE) {
		/* -1 because the range boundary has already been adjusted with
		 * one from our previous proposal. So, if the user requested a
		 * step of 1 bronee at a time we should just return our end of
		 * the range from this function. */
		step_mbro = amount_mbro((fee_negotiation_step - 1)
					* MBRO_PER_BRO);
	} else {
		/* fee_negotiation_step is e.g. 20 to designate 20% from
		 * range_len (which is in bronee), so:
		 * range_len * fee_negotiation_step / 100 [bro]
		 * is equivalent to:
		 * range_len * fee_negotiation_step * 10 [mbro] */
		step_mbro = amount_mbro(range_len.bronees /* Raw: % calc */ *
					fee_negotiation_step * 10);
	}

	step_bro = amount_mbro_to_bro_round_down(step_mbro);

	if (feerange->higher_side == LOCAL) {
		if (!amount_bro_sub(&result, feerange->max, step_bro))
			/* step_bro > feerange->max, unlikely */
			return min_fee_to_accept;

		if (amount_bro_less_eq(result, min_fee_to_accept))
			return min_fee_to_accept;
	} else {
		if (!amount_bro_add(&result, min_fee_to_accept, step_bro))
			/* overflow, unlikely */
			return feerange->max;

		if (amount_bro_greater_eq(result, feerange->max))
			return feerange->max;
	}

	return result;
}

#if DEVELOPER
/* FIXME: We should talk to lightningd anyway, rather than doing this */
static void closing_dev_memleak(const tal_t *ctx,
				u8 *scriptpubkey[NUM_SIDES],
				const u8 *funding_wscript)
{
	struct htable *memtable;

	memtable = memleak_find_allocations(tmpctx, NULL, NULL);

	memleak_remove_pointer(memtable, ctx);
	memleak_remove_pointer(memtable, scriptpubkey[LOCAL]);
	memleak_remove_pointer(memtable, scriptpubkey[REMOTE]);
	memleak_remove_pointer(memtable, funding_wscript);

	dump_memleak(memtable, memleak_status_broken);
}
#endif /* DEVELOPER */

/* Figure out what weight we actually expect for this closing tx (using zero fees
 * gives the largest possible tx: larger values might omit outputs). */
static size_t closing_tx_weight_estimate(u8 *scriptpubkey[NUM_SIDES],
					 const u8 *funding_wscript,
					 const struct amount_bro *out,
					 struct amount_bro funding_bros,
					 struct amount_bro dust_limit,
					 u32 *local_wallet_index,
					 const struct ext_key *local_wallet_ext_key)
{
	/* We create a dummy close */
	struct brocoin_tx *tx;
	struct brocoin_outpoint dummy_funding;

	memset(&dummy_funding, 0, sizeof(dummy_funding));
	tx = create_close_tx(tmpctx, chainparams,
			     local_wallet_index, local_wallet_ext_key,
			     scriptpubkey[LOCAL], scriptpubkey[REMOTE],
			     funding_wscript,
			     &dummy_funding,
			     funding_bros,
			     out[LOCAL],
			     out[REMOTE],
			     dust_limit);

	/* We will have to append the witness */
	return brocoin_tx_weight(tx) + brocoin_tx_2of2_input_witness_weight();
}

/* Get the minimum and desired fees */
static void calc_fee_bounds(size_t expected_weight,
			    u32 min_feerate,
			    u32 desired_feerate,
			    u32 *max_feerate,
			    struct amount_bro commitment_fee,
			    struct amount_bro funding,
			    enum side opener,
			    struct amount_bro *minfee,
			    struct amount_bro *desiredfee,
			    struct amount_bro *maxfee)
{
	*minfee = amount_tx_fee(min_feerate, expected_weight);
	*desiredfee = amount_tx_fee(desired_feerate, expected_weight);

	/* BOLT #2:
	 * - if it is not the funder:
	 *  - SHOULD set `max_fee_bronees` to at least the `max_fee_bronees`
	 *   received
	 *...
	 * Note that the non-funder is not paying the fee, so there is
	 * no reason for it to have a maximum feerate.
	 */
	if (opener == REMOTE) {
		*maxfee = funding;

	/* This used to appear in BOLT #2: we still set it for non-anchor
	 * peers who may still enforce it:
	 * - If the channel does not use `option_anchor_outputs`:
	 *   - MUST set `fee_bronees` less than or equal to the base fee of
	 *     the final commitment transaction, as calculated in
	 *     [BOLT #3](03-transactions.md#fee-calculation).
	 */
	} else if (max_feerate) {
		*maxfee = amount_tx_fee(*max_feerate, expected_weight);

		status_debug("deriving max fee from rate %u -> %s (not %s)",
			     *max_feerate,
			     type_to_string(tmpctx, struct amount_bro, maxfee),
			     type_to_string(tmpctx, struct amount_bro, &commitment_fee));

		/* option_anchor_outputs sets commitment_fee to max, so this
		 * doesn't do anything */
		if (amount_bro_greater(*maxfee, commitment_fee)) {
			/* FIXME: would be nice to notify close cmd here! */
			status_unusual("Maximum feerate %u would give fee %s:"
				       " we must limit it to the final commitment fee %s",
				       *max_feerate,
				       type_to_string(tmpctx, struct amount_bro,
						      maxfee),
				       type_to_string(tmpctx, struct amount_bro,
						      &commitment_fee));
			*maxfee = commitment_fee;
		}
	} else
		*maxfee = commitment_fee;

	/* Can't exceed maxfee. */
	if (amount_bro_greater(*minfee, *maxfee))
		*minfee = *maxfee;

	if (amount_bro_less(*desiredfee, *minfee)) {
		status_unusual("Our ideal fee is %s (%u bros/perkw),"
			       " but our minimum is %s: using that",
			       type_to_string(tmpctx, struct amount_bro, desiredfee),
			       desired_feerate,
			       type_to_string(tmpctx, struct amount_bro, minfee));
		*desiredfee = *minfee;
	}
	if (amount_bro_greater(*desiredfee, *maxfee)) {
		status_unusual("Our ideal fee is %s (%u bros/perkw),"
			       " but our maximum is %s: using that",
			       type_to_string(tmpctx, struct amount_bro, desiredfee),
			       desired_feerate,
			       type_to_string(tmpctx, struct amount_bro, maxfee));
		*desiredfee = *maxfee;
	}

	status_debug("Expected closing weight = %zu, fee %s (min %s, max %s)",
		     expected_weight,
		     type_to_string(tmpctx, struct amount_bro, desiredfee),
		     type_to_string(tmpctx, struct amount_bro, minfee),
		     type_to_string(tmpctx, struct amount_bro, maxfee));
}

/* We've received one offer; if we're opener, that means we've already sent one
 * too. */
static void do_quickclose(struct amount_bro offer[NUM_SIDES],
			  struct per_peer_state *pps,
			  const struct channel_id *channel_id,
			  const struct pubkey funding_pubkey[NUM_SIDES],
			  const u8 *funding_wscript,
			  u32 *local_wallet_index,
			  const struct ext_key *local_wallet_ext_key,
			  u8 *scriptpubkey[NUM_SIDES],
			  const struct brocoin_outpoint *funding,
			  struct amount_bro funding_bros,
			  const struct amount_bro out[NUM_SIDES],
			  enum side opener,
			  struct amount_bro our_dust_limit,
			  const struct brocoin_outpoint *wrong_funding,
			  struct brocoin_txid *closing_txid,
			  const struct tlv_closing_signed_tlvs_fee_range *our_feerange,
			  const struct tlv_closing_signed_tlvs_fee_range *their_feerange)
{
	struct tlv_closing_signed_tlvs_fee_range overlap;


	/* BOLT #2:
	 *   - if the message contains a `fee_range`:
	 *     - if there is no overlap between that and its own `fee_range`:
	 *       - SHOULD send a warning
	 *       - MUST fail the channel if it doesn't receive a broisfying `fee_range` after a reasonable amount of time
	 */
	/* (Note we broisfy the "MUST fail" by our close command unilteraltimeout) */
	if (!get_overlap(our_feerange, their_feerange, &overlap)) {
		peer_failed_warn(pps, channel_id,
			       "Unable to agree on a feerate."
			       " Our range %s-%s, other range %s-%s",
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &our_feerange->min_fee_bronees),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &our_feerange->max_fee_bronees),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &their_feerange->min_fee_bronees),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &their_feerange->max_fee_bronees));
		return;
	}

	status_info("performing quickclose in range %s-%s",
		    type_to_string(tmpctx, struct amount_bro,
				   &overlap.min_fee_bronees),
		    type_to_string(tmpctx, struct amount_bro,
				   &overlap.max_fee_bronees));

	/* BOLT #2:
	 * - otherwise:
	 *   - if it is the funder:
	 *     - if `fee_bronees` is not in the overlap between the sent
	 *       and received `fee_range`:
	 *       - MUST fail the channel
	 *     - otherwise:
	 *       - MUST reply with the same `fee_bronees`.
	 */
	if (opener == LOCAL) {
		if (!amount_in_range(offer[REMOTE], &overlap)) {
			peer_failed_warn(pps, channel_id,
			       "Your fee %s was not in range:"
			       " Our range %s-%s, other range %s-%s",
			       type_to_string(tmpctx,
					      struct amount_bro, &offer[REMOTE]),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &our_feerange->min_fee_bronees),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &our_feerange->max_fee_bronees),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &their_feerange->min_fee_bronees),
			       type_to_string(tmpctx,
					      struct amount_bro,
					      &their_feerange->max_fee_bronees));
			return;
		}
		/* Only reply if we didn't already completely agree. */
		if (!amount_bro_eq(offer[LOCAL], offer[REMOTE])) {
			offer[LOCAL] = offer[REMOTE];
			send_offer(pps, chainparams,
				   channel_id, funding_pubkey, funding_wscript,
				   local_wallet_index, local_wallet_ext_key,
				   scriptpubkey, funding,
				   funding_bros, out, opener,
				   our_dust_limit,
				   offer[LOCAL],
				   wrong_funding,
				   our_feerange);
		}
	} else {
		/* BOLT #2:
		 * - otherwise (it is not the funder):
		 *   - if it has already sent a `closing_signed`:
		 *     - if `fee_bronees` is not the same as the value it sent:
		 *       - MUST fail the channel
		 *   - otherwise:
		 *     - MUST propose a `fee_bronees` in the overlap between
		 *       received and (about-to-be) sent `fee_range`.
		 */
		if (!amount_in_range(offer[LOCAL], &overlap)) {
			/* Hmm, go to edges. */
			if (amount_bro_greater(offer[LOCAL],
					       overlap.max_fee_bronees)) {
				offer[LOCAL] = overlap.max_fee_bronees;
				status_unusual("Lowered offer to max allowable"
					       " %s",
					       type_to_string(tmpctx,
							      struct amount_bro,
							      &offer[LOCAL]));
			} else if (amount_bro_less(offer[LOCAL],
						   overlap.min_fee_bronees)) {
				offer[LOCAL] = overlap.min_fee_bronees;
				status_unusual("Increased offer to min allowable"
					       " %s",
					       type_to_string(tmpctx,
							      struct amount_bro,
							      &offer[LOCAL]));
			}
		}
		send_offer(pps, chainparams,
			   channel_id, funding_pubkey, funding_wscript,
			   local_wallet_index, local_wallet_ext_key,
			   scriptpubkey, funding,
			   funding_bros, out, opener,
			   our_dust_limit,
			   offer[LOCAL],
			   wrong_funding,
			   our_feerange);

		/* They will reply unless we completely agreed. */
		if (!amount_bro_eq(offer[LOCAL], offer[REMOTE])) {
			offer[REMOTE]
				= receive_offer(pps, chainparams,
						channel_id, funding_pubkey,
						funding_wscript,
						local_wallet_index, local_wallet_ext_key,
						scriptpubkey, funding,
						funding_bros,
						out, opener,
						our_dust_limit,
						our_feerange->min_fee_bronees,
						wrong_funding,
						closing_txid,
						NULL);
			/* BOLT #2:
			 * - otherwise (it is not the funder):
			 *   - if it has already sent a `closing_signed`:
			 *     - if `fee_bronees` is not the same as the value
			 *       it sent:
			 *       - MUST fail the channel
			 */
			if (!amount_bro_eq(offer[LOCAL], offer[REMOTE])) {
				peer_failed_warn(pps, channel_id,
						 "Your fee %s was not equal to %s",
			       type_to_string(tmpctx,
					      struct amount_bro, &offer[REMOTE]),
			       type_to_string(tmpctx,
					      struct amount_bro, &offer[LOCAL]));
				return;
			}
		}
	}

	peer_billboard(true, "We agreed on a closing fee of %"PRIu64" bronee for tx:%s",
		       offer[LOCAL],
		       type_to_string(tmpctx, struct brocoin_txid, closing_txid));
}

int main(int argc, char *argv[])
{
	setup_locale();

	const tal_t *ctx = tal(NULL, char);
	struct per_peer_state *pps;
	u8 *msg;
	struct pubkey funding_pubkey[NUM_SIDES];
	struct brocoin_txid closing_txid;
	struct brocoin_outpoint funding;
	struct amount_bro funding_bros, out[NUM_SIDES];
	struct amount_bro our_dust_limit;
	struct amount_bro min_fee_to_accept, commitment_fee, offer[NUM_SIDES],
		max_fee_to_accept;
	u32 min_feerate, initial_feerate, *max_feerate;
	struct feerange feerange;
	enum side opener;
	u32 *local_wallet_index;
	struct ext_key *local_wallet_ext_key;
	u8 *scriptpubkey[NUM_SIDES], *funding_wscript;
	u64 fee_negotiation_step;
	u8 fee_negotiation_step_unit;
	char fee_negotiation_step_str[32]; /* fee_negotiation_step + "bro" */
	struct channel_id channel_id;
	enum side whose_turn;
	bool use_quickclose;
	struct tlv_closing_signed_tlvs_fee_range *our_feerange, **their_feerange;
	struct brocoin_outpoint *wrong_funding;

	subdaemon_setup(argc, argv);

	status_setup_sync(REQ_FD);

	msg = wire_sync_read(tmpctx, REQ_FD);
	if (!fromwire_closingd_init(ctx, msg,
				    &chainparams,
				    &channel_id,
				    &funding,
				    &funding_bros,
				    &funding_pubkey[LOCAL],
				    &funding_pubkey[REMOTE],
				    &opener,
				    &out[LOCAL],
				    &out[REMOTE],
				    &our_dust_limit,
				    &min_feerate, &initial_feerate, &max_feerate,
				    &commitment_fee,
				    &local_wallet_index,
				    &local_wallet_ext_key,
				    &scriptpubkey[LOCAL],
				    &scriptpubkey[REMOTE],
				    &fee_negotiation_step,
				    &fee_negotiation_step_unit,
				    &use_quickclose,
				    &wrong_funding))
		master_badmsg(WIRE_CLOSINGD_INIT, msg);

	/* stdin == requests, 3 == peer, 4 = hsmd */
	pps = notleak(new_per_peer_state(ctx));
	per_peer_state_set_fd(pps, 3);

	funding_wscript = brocoin_redeem_2of2(ctx,
					      &funding_pubkey[LOCAL],
					      &funding_pubkey[REMOTE]);

	/* Start at what we consider a reasonable feerate for this tx. */
	calc_fee_bounds(closing_tx_weight_estimate(scriptpubkey,
						   funding_wscript,
						   out, funding_bros,
						   our_dust_limit,
						   local_wallet_index,
						   local_wallet_ext_key),
			min_feerate, initial_feerate, max_feerate,
			commitment_fee, funding_bros, opener,
			&min_fee_to_accept, &offer[LOCAL], &max_fee_to_accept);

	/* Write values into tlv for updated closing fee neg */
	their_feerange = tal(ctx, struct tlv_closing_signed_tlvs_fee_range *);
	*their_feerange = NULL;

	if (use_quickclose) {
		our_feerange = tal(ctx, struct tlv_closing_signed_tlvs_fee_range);
		our_feerange->min_fee_bronees = min_fee_to_accept;
		our_feerange->max_fee_bronees = max_fee_to_accept;
	} else
		our_feerange = NULL;

	snprintf(fee_negotiation_step_str, sizeof(fee_negotiation_step_str),
		 "%" PRIu64 "%s", fee_negotiation_step,
		 fee_negotiation_step_unit ==
			 CLOSING_FEE_NEGOTIATION_STEP_UNIT_PERCENTAGE
		     ? "%"
		     : "bro");

	status_debug("out = %s/%s",
		     type_to_string(tmpctx, struct amount_bro, &out[LOCAL]),
		     type_to_string(tmpctx, struct amount_bro, &out[REMOTE]));
	status_debug("dustlimit = %s",
		     type_to_string(tmpctx, struct amount_bro, &our_dust_limit));
	status_debug("fee = %s",
		     type_to_string(tmpctx, struct amount_bro, &offer[LOCAL]));
	status_debug("fee negotiation step = %s", fee_negotiation_step_str);
	if (wrong_funding)
		status_unusual("Setting wrong_funding_txid to %s:%u",
			       type_to_string(tmpctx, struct brocoin_txid,
					      &wrong_funding->txid),
			       wrong_funding->n);

	peer_billboard(
	    true,
	    "Negotiating closing fee between %s and %s bronee (ideal %s) "
	    "using step %s",
	    type_to_string(tmpctx, struct amount_bro, &min_fee_to_accept),
	    type_to_string(tmpctx, struct amount_bro, &max_fee_to_accept),
	    type_to_string(tmpctx, struct amount_bro, &offer[LOCAL]),
	    fee_negotiation_step_str);

	/* BOLT #2:
	 *
	 * The funding node:
	 *  - after `shutdown` has been received, AND no HTLCs remain in either
	 *    commitment transaction:
	 *    - SHOULD send a `closing_signed` message.
	 */
	whose_turn = opener;
	for (size_t i = 0; i < 2; i++, whose_turn = !whose_turn) {
		if (whose_turn == LOCAL) {
			send_offer(pps, chainparams,
				   &channel_id, funding_pubkey, funding_wscript,
				   local_wallet_index, local_wallet_ext_key,
				   scriptpubkey, &funding,
				   funding_bros, out, opener,
				   our_dust_limit,
				   offer[LOCAL],
				   wrong_funding,
				   our_feerange);
		} else {
			if (i == 0)
				peer_billboard(false, "Waiting for their initial"
					       " closing fee offer");
			else
				peer_billboard(false, "Waiting for their initial"
					       " closing fee offer:"
					       " ours was %s",
					       type_to_string(tmpctx,
							      struct amount_bro,
							      &offer[LOCAL]));
			offer[REMOTE]
				= receive_offer(pps, chainparams,
						&channel_id, funding_pubkey,
						funding_wscript,
						local_wallet_index,
						local_wallet_ext_key,
						scriptpubkey, &funding,
						funding_bros,
						out, opener,
						our_dust_limit,
						min_fee_to_accept,
						wrong_funding,
						&closing_txid,
						their_feerange);

			if (our_feerange && *their_feerange) {
				do_quickclose(offer,
					      pps, &channel_id, funding_pubkey,
					      funding_wscript,
					      local_wallet_index, local_wallet_ext_key,
					      scriptpubkey,
					      &funding,
					      funding_bros, out, opener,
					      our_dust_limit,
					      wrong_funding,
					      &closing_txid,
					      our_feerange, *their_feerange);
				goto exit_thru_the_giftshop;
			}
		}
	}

	/* Now we have first two points, we can init fee range. */
	init_feerange(&feerange, max_fee_to_accept, offer);

	/* Apply (and check) opener offer now. */
	adjust_feerange(&feerange, offer[opener], opener);

	/* Now any extra rounds required. */
	while (!amount_bro_eq(offer[LOCAL], offer[REMOTE])) {
		/* Still don't agree: adjust feerange based on previous offer */
		adjust_feerange(&feerange,
				offer[!whose_turn], !whose_turn);

		if (whose_turn == LOCAL) {
			offer[LOCAL] = adjust_offer(pps,
						    &channel_id,
						    &feerange, offer[REMOTE],
						    min_fee_to_accept,
						    fee_negotiation_step,
						    fee_negotiation_step_unit);
			send_offer(pps, chainparams, &channel_id,
				   funding_pubkey, funding_wscript,
				   local_wallet_index,
				   local_wallet_ext_key,
				   scriptpubkey, &funding,
				   funding_bros, out, opener,
				   our_dust_limit,
				   offer[LOCAL],
				   wrong_funding,
				   our_feerange);
		} else {
			peer_billboard(false, "Waiting for another"
				       " closing fee offer:"
				       " ours was %"PRIu64" bronee,"
				       " theirs was %"PRIu64" bronee,",
				       offer[LOCAL], offer[REMOTE]);
			offer[REMOTE]
				= receive_offer(pps, chainparams, &channel_id,
						funding_pubkey,
						funding_wscript,
						local_wallet_index,
						local_wallet_ext_key,
						scriptpubkey, &funding,
						funding_bros,
						out, opener,
						our_dust_limit,
						min_fee_to_accept,
						wrong_funding,
						&closing_txid,
						their_feerange);
		}

		whose_turn = !whose_turn;
	}

	peer_billboard(true, "We agreed on a closing fee of %"PRIu64" bronee for tx:%s",
		       offer[LOCAL],
		       type_to_string(tmpctx, struct brocoin_txid, &closing_txid));

exit_thru_the_giftshop:
#if DEVELOPER
	/* We don't listen for master commands, so always check memleak here */
	tal_free(wrong_funding);
	tal_free(our_feerange);
	tal_free(their_feerange);
	tal_free(max_feerate);
	tal_free(local_wallet_index);
	tal_free(local_wallet_ext_key);
	closing_dev_memleak(ctx, scriptpubkey, funding_wscript);
#endif

	/* We're done! */

	/* Sending the below will kill us! */
	wire_sync_write(REQ_FD, take(towire_closingd_complete(NULL)));
	tal_free(ctx);
	daemon_shutdown();

	return 0;

}
