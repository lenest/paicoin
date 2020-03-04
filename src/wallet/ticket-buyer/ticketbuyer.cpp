// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticketbuyer.h"
#include "wallet/wallet.h"
#include "validation.h"
#include <numeric>

CTicketBuyer::CTicketBuyer(CWallet* wallet) :
    shouldRun(true),
    thread(&CTicketBuyer::mainLoop, this)
{
    pwallet = wallet;
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

void CTicketBuyer::start()
{
    config.buyTickets = true;
}

void CTicketBuyer::stop()
{
    config.buyTickets = false;
}

void CTicketBuyer::mainLoop()
{
    int64_t height{0};
    int64_t nextIntervalStart{0}, currentInterval{0}, intervalSize{Params().GetConsensus().nStakeDiffWindowSize};
    int64_t expiry{0};
    CAmount spendable{0};
    CAmount sdiff{0};
    int buy{0};
    int max{0};
    bool shouldRelock{false};

    while (shouldRun.load()) {
        // wait until activated
        if (! config.buyTickets) {
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
        shouldRelock = pwallet->IsLocked();
        if (shouldRelock && ! pwallet->Unlock(config.passphrase)) {
            LogPrintf("CTicketBuyer: Purchased tickets: Wrong passphrase");
            break;
        }

        if (chainActive.Tip() == nullptr) {
            if (shouldRelock) pwallet->Lock();
            continue;
        }

        // calculate the height of the first block in the
        // next stake difficulty interval:
        height = chainActive.Height();
        if (height + 2 >= nextIntervalStart) {
            currentInterval = height / intervalSize + 1;
            nextIntervalStart = currentInterval * intervalSize;

            // Skip this purchase when no more tickets may be purchased in the interval and
            // the next sdiff is unknown. The earliest any ticket may be mined is two
            // blocks from now, with the next block containing the split transaction
            // that the ticket purchase spends.
            if (height + 2 == nextIntervalStart) {
                LogPrintf("CTicketBuyer: Skipping purchase: next sdiff interval starts soon");
                if (shouldRelock) pwallet->Lock();
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
        if (! pwallet->GetBroadcastTransactions()) {
            if (shouldRelock) pwallet->Lock();
            continue;
        }

        // Determine how many tickets to buy
        spendable = pwallet->GetAvailableBalance();
        if (spendable < config.maintain) {
            LogPrintf("CTicketBuyer: Skipping purchase: low available balance");
            if (shouldRelock) pwallet->Lock();
            continue;
        }
        spendable -= config.maintain;

        sdiff = calcNextRequiredStakeDifficulty(chainActive.Tip()->GetBlockHeader(), chainActive.Tip(), Params());

        buy = static_cast<int>(spendable / sdiff);
        if (buy == 0) {
            LogPrintf("CTicketBuyer: Skipping purchase: low available balance");
            if (shouldRelock) pwallet->Lock();
            continue;
        }

        max = Params().GetConsensus().nMaxFreshStakePerBlock;
        if (buy > max)
            buy = max;

        if (config.limit > 0 && buy > config.limit)
            buy = config.limit;

        const auto&& r = pwallet->PurchaseTicket(config.account, spendable, config.minConf, config.votingAddress, buy, config.poolFeeAddress, config.poolFees, expiry, 0 /*TODO Make sure this is handled correctly*/);
        if (r.first.size() > 0 && r.second.code == CWalletError::SUCCESSFUL) {
            LogPrintf("CTicketBuyer: Purchased tickets: %s", std::accumulate(r.first.begin(), r.first.end(), std::string()));
        } else {
            LogPrintf("CTicketBuyer: Failed to purchase tickets: (%d) %s", r.second.code, r.second.message.c_str());
        }

        if (shouldRelock) pwallet->Lock();
    }
}
