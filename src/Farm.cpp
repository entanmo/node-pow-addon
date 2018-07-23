#include "Farm.h"
#include <iostream>

namespace entanmo {
    Farm::Farm()
        :mLastMixed(false) {}

    void Farm::setSealers(const std::map<std::string, SealerDescriptor> &sealers) {
        mSealers = sealers;
    }

    bool Farm::start(const std::string &sealer, bool mixed) {
        std::lock_guard<std::mutex> l(mMinerWork);
        if (!mMiners.empty() && mLastSealer == sealer) {
            return true;
        }

        if (!mSealers.count(sealer)) {
            return false;
        }
        if (!mixed) {
            mMiners.clear();
        }
        auto ins = mSealers[sealer].instances();
        unsigned start = 0;
        if (!mixed) {
            mMiners.reserve(ins);
        } else {
            start = mMiners.size();
            ins += start;
            mMiners.reserve(ins);
        }
        // Init Miner instances
        unsigned count = 0;
        for (unsigned i = start; i < ins; ++i) {
            mMiners.push_back(std::shared_ptr<Miner>(mSealers[sealer].create(*this, i)));
            if (mMiners.back()->init()) {
                mMiners.back()->startWorking();
                count++;
            } else {
                mMiners.pop_back();
            }
        }

        mIsMining.store(true, std::memory_order_relaxed);
        mLastSealer = sealer;
        mLastMixed = mixed;
        std::cout << "Farm::start " << count << " Miners Initialized" << std::endl;
        return true;
    }
    void Farm::stop()  {
        if (isMining()) {
            {
                std::lock_guard<std::mutex> l(mMinerWork);
                mMiners.clear();
                mIsMining.store(false, std::memory_order_relaxed);
            }
        }
    }

    void Farm::pause() {
        std::lock_guard<std::mutex> l(mMinerWork);
        for (auto const &m : mMiners) {
            m->setMiningPaused(MiningPauseReason::kMiningPausedWaitForTStart);
        }
    }

    bool Farm::isMining() const {
        return mIsMining.load(std::memory_order_relaxed);
    }

    void Farm::setWork(const WorkPackage &w) {
        std::lock_guard<std::mutex> l(mMinerWork);
        if (w.source == mWork.source && w.difficulty == mWork.difficulty) {
            return;
        }

        mWork = w;
        for (auto const &m : mMiners) {
            m->setWork(mWork);
            m->clearMiningPaused(MiningPauseReason::kMiningPausedWaitForTStart);
        }
    }

    void Farm::setOnSolutionFound(const SolutionFound &functor) {
        mOnSolutionFound = functor;
    }

    void Farm::submitProof(const Solution &s) {
        pause();
        if (mOnSolutionFound) {
            mOnSolutionFound(s);
        }
    }

    void Farm::failedSolution() {
        // TODO
    }

    unsigned Farm::minerCount() const {
		return mMiners.size();
    }
}
