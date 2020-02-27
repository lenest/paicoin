// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticketbuyer.h"
#include "wallet/wallet.h"
#include "validation.h"
#include <numeric>

CTicketBuyer::CTicketBuyer(CWallet* wallet)
{
    pwallet = wallet;
}

CTicketBuyer::CTicketBuyer(CTicketBuyer&& o) :
    config(std::move(o.config)),
    pwallet(o.pwallet),
    thread(&CTicketBuyer::mainLoop, this)
{
    shouldRun.store(true);
    ::RegisterValidationInterface(this);
}

CTicketBuyer::~CTicketBuyer()
{
    //::UnregisterValidationInterface(this);

    shouldRun.store(false);

    // TODO verify if waiting for the thread is a possible cause for a deadlock
    //thread.join();
}

void CTicketBuyer::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    // we have to wait until the entire blockchain is downloaded
    if (fInitialDownload)
        return;

    std::unique_lock<std::mutex> lck{mtx};
    cv.notify_one();
}

//void CTicketBuyer::BlockConnected(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex, const std::vector<CTransactionRef> &txnConflicted)
//{
//}

void CTicketBuyer::mainLoop()
{
    CBlockIndex* chainTip = nullptr;
    int64_t height{0};
    int64_t nextIntervalStart{0}, currentInterval{0}, intervalSize{Params().GetConsensus().nStakeDiffWindowSize};
    int64_t expiry{0};
    CAmount spendable{0};
    CAmount sdiff{0};
    int buy{0};
    int max{0};

    while (shouldRun.load()) {
        // wait until activated
        if (! config.BuyTickets) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // wait until new block is received
        std::unique_lock<std::mutex> lck{mtx};
        cv.wait(lck);
        lck.unlock();

        if (pwallet == nullptr)
            continue;

        // unlock wallet
        // TODO add password to the config
        // TODO add proper error handling
        if (pwallet->IsLocked() && ! pwallet->Unlock(config.Passphrase)) {
            break;
        }

        chainTip = chainActive.Tip();
        if (chainTip == nullptr)
            continue;

        // calculate the height of the first block in the
        // next stake difficulty interval:
        height = chainTip->nHeight;
        if (height + 2 >= nextIntervalStart) {
            currentInterval = height / intervalSize + 1;
            nextIntervalStart = currentInterval * intervalSize;

            // Skip this purchase when no more tickets may be purchased in the interval and
            // the next sdiff is unknown. The earliest any ticket may be mined is two
            // blocks from now, with the next block containing the split transaction
            // that the ticket purchase spends.
            if (height + 2 == nextIntervalStart) {
                LogPrintf("CTicketBuyer: Skipping purchase: next sdiff interval starts soon");
                continue;
            }

            // Set expiry to prevent tickets from being mined in the next
            // sdiff interval.  When the next block begins the new interval,
            // the ticket is being purchased for the next interval; therefore
            // increment expiry by a full sdiff window size to prevent it
            // being mined in the interval after the next.
            expiry = nextIntervalStart;
            if (height + 1 == nextIntervalStart) {
                expiry += intervalSize;
            }
        }

        // TODO check rescan point
        // Don't buy tickets for this attached block when transactions are not
        // synced through the tip block.

        // Cannot purchase tickets if not broadcasting
        if (! pwallet->GetBroadcastTransactions())
            continue;

        // Determine how many tickets to buy
        spendable = pwallet->GetAvailableBalance();
        if (spendable < config.Maintain) {
            LogPrintf("CTicketBuyer: Skipping purchase: low available balance");
            continue;
        }
        spendable -= config.Maintain;

        sdiff = calcNextRequiredStakeDifficulty(chainTip->GetBlockHeader(), chainTip, Params());

        buy = static_cast<int>(spendable / sdiff);
        if (buy == 0) {
            LogPrintf("CTicketBuyer: Skipping purchase: low available balance");
            continue;
        }

        max = Params().GetConsensus().nMaxFreshStakePerBlock;
        if (buy > max)
            buy = max;

        if (config.Limit > 0 && buy > config.Limit)
            buy = config.Limit;

        CWalletError error;
        std::vector<std::string> tickets = pwallet->PurchaseTicket(config.Account, spendable, config.MinConf, config.VotingAddress, buy, config.PoolFeeAddress, config.PoolFees, expiry, 0 /*TODO Make sure this is handled correctly*/, error);
        if (tickets.size() > 0 && error.code == CWalletError::SUCCESSFUL) {
            LogPrintf("CTicketBuyer: Purchased tickets: %s", std::accumulate(tickets.begin(), tickets.end(), std::string()));
        } else {
            LogPrintf("CTicketBuyer: Failed to purchase tickets: (%d) %s", error.code, error.message.c_str());
        }
    }
}
