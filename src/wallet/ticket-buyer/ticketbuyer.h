// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_TICKETBUYER_TICKETBUYER_H
#define PAICOIN_WALLET_TICKETBUYER_TICKETBUYER_H

#include "ticketbuyerconfig.h"
#include "validationinterface.h"

#include <thread>

class CWallet;

class CTicketBuyer : public CValidationInterface {
public:
    CTicketBuyer() = delete;
    CTicketBuyer(CWallet* wallet);
    CTicketBuyer(CTicketBuyer&& o);
    virtual ~CTicketBuyer();

    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload);
    //virtual void BlockConnected(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex, const std::vector<CTransactionRef> &txnConflicted);

private:
    void mainLoop();

    CTicketBuyerConfig config;

    CWallet* pwallet;

    std::atomic<bool> shouldRun;

    std::thread thread;

    std::condition_variable cv;
    std::mutex mtx;
};

#endif // PAICOIN_WALLET_TICKETBUYER_TICKETBUYER_H
