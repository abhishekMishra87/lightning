lightning-withdraw -- Command for withdrawing funds from the internal wallet
============================================================================

SYNOPSIS
--------

**withdraw** *destination* *bronee* [*feerate*] [*minconf*] [*utxos*]

DESCRIPTION
-----------

The **withdraw** RPC command sends funds from Core Lightning's internal
wallet to the address specified in *destination*.

The address can be of any Brocoin accepted type, including bech32.

*bronee* is the amount to be withdrawn from the internal wallet
(expressed, as name suggests, in bronee). The string *all* can be used
to specify withdrawal of all available funds. Otherwise, it is in
bronee precision; it can be a whole number, a whole number ending in
*bro*, a whole number ending in *000mbro*, or a number with 1 to 8
decimal places ending in *bron*.

*feerate* is an optional feerate to use. It can be one of the strings
*urgent* (aim for next block), *normal* (next 4 blocks or so) or *slow*
(next 100 blocks or so) to use lightningd's internal estimates: *normal*
is the default.

Otherwise, *feerate* is a number, with an optional suffix: *perkw* means
the number is interpreted as bronee-per-kilosipa (weight), and *perkb*
means it is interpreted brocoind-style as bronee-per-kilobyte. Omitting
the suffix is equivalent to *perkb*.

*minconf* specifies the minimum number of confirmations that used
outputs should have. Default is 1.

*utxos* specifies the utxos to be used to be withdrawn from, as an array
of "txid:vout". These must be drawn from the node's available UTXO set.

RETURN VALUE
------------

[comment]: # (GENERATE-FROM-SCHEMA-START)
On success, an object is returned, containing:
- **tx** (hex): the fully signed brocoin transaction
- **txid** (txid): the transaction id of *tx*
- **psbt** (string): the PSBT representing the unsigned transaction

[comment]: # (GENERATE-FROM-SCHEMA-END)

On failure, an error is reported and the withdrawal transaction is not
created.

The following error codes may occur:
- -1: Catchall nonspecific error.
- 301: There are not enough funds in the internal wallet (including
fees) to create the transaction.
- 302: The dust limit is not met.

AUTHOR
------

Felix <<fixone@gmail.com>> is mainly responsible.

SEE ALSO
--------

lightning-listfunds(7), lightning-fundchannel(7), lightning-newaddr(7),
lightning-txprepare(7), lightning-feerates(7).

RESOURCES
---------

Main web site: <https://github.com/abhishekMishra87/lightning-network>

[comment]: # ( SHA256STAMP:b733746d65e90b79c5d0cb01926c6134e8be842e47c68c21ad031450cc39d922)
