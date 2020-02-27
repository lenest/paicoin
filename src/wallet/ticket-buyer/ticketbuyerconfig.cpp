// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticketbuyerconfig.h"
#include "util.h"

CTicketBuyerConfig::CTicketBuyerConfig() :
    BuyTickets(false),
    Maintain(0),
    PoolFees(0.0),
    Limit(1)
{
    ParseCommandline();
}

CTicketBuyerConfig::CTicketBuyerConfig(CTicketBuyerConfig&& o)
{
    BuyTickets = o.BuyTickets;
    Account = o.Account;
    VotingAccount = o.VotingAccount;
    Maintain = o.Maintain;
    VotingAddress = o.VotingAddress;
    PoolFeeAddress = o.PoolFeeAddress;
    PoolFees = o.PoolFees;
    Limit = o.Limit;
}

void CTicketBuyerConfig::ParseCommandline()
{
    if (gArgs.IsArgSet("-tbbalancetomaintainabsolute"))
        Maintain = gArgs.GetArg("-tbbalancetomaintainabsolute", 0);

    if (gArgs.IsArgSet("-tbvotingaddress"))
        VotingAddress = gArgs.GetArg("-tbvotingaddress", "");

    if (gArgs.IsArgSet("-tblimit"))
        Limit = static_cast<int>(gArgs.GetArg("-tblimit", 0));

    if (gArgs.IsArgSet("-tbvotingaccount"))
        VotingAccount = gArgs.GetArg("-tbvotingaccount", "");
}
