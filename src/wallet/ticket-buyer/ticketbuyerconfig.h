// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_TICKETBUYER_TICKETBUYERCFG_H
#define PAICOIN_WALLET_TICKETBUYER_TICKETBUYERCFG_H

#include "amount.h"
#include "support/allocators/secure.h"

// The ticket buyer (TB) configuration
class CTicketBuyerConfig {

public:
    // Enables the automatic ticket purchasing
    bool BuyTickets;

    // Account to buy tickets from
    std::string Account;

    // Account to derive voting addresses from; overridden by VotingAddr
    std::string VotingAccount;

    // Minimum amount to maintain in purchasing account
    CAmount Maintain;

    // Address to assign voting rights; overrides VotingAccount
    std::string VotingAddress;

    // Commitment address for stakepool fees
    std::string PoolFeeAddress;

    // Stakepool fee percentage (between 0-100)
    double PoolFees;

    // Limit maximum number of purchased tickets per block
    int Limit;

    // Minimum number of block confirmations required
    const int MinConf = 1;

    // Wallet passphrase
    SecureString Passphrase;

    CTicketBuyerConfig();
    CTicketBuyerConfig(CTicketBuyerConfig&& o);

    void ParseCommandline();
};

#endif // PAICOIN_WALLET_TICKETBUYER_TICKETBUYERCFG_H
