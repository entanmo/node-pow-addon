#include "Miner.h"

namespace entanmo {
	///////////////////////////////////////////////////////////////////////////
	WorkPackage::WorkPackage()
		: startNonce(0) {}

	WorkPackage::operator bool() const {
		return !source.empty() && !difficulty.empty();
	}

	///////////////////////////////////////////////////////////////////////////
	void MiningPause::setMiningPaused(MiningPauseReason pauseReason) {
		mMingPausedFlag.fetch_or((uint64_t)pauseReason, std::memory_order_seq_cst);
	}

	void MiningPause::clearMiningPaused(MiningPauseReason pauseReason) {
		mMingPausedFlag.fetch_and(!(uint64_t)pauseReason, std::memory_order_seq_cst);
	}

	MiningPauseReason MiningPause::getMiningPaused() {
		return (MiningPauseReason)mMingPausedFlag.load(std::memory_order_relaxed);
	}

	bool MiningPause::isMiningPaused() {
		return mMingPausedFlag.load(std::memory_order_relaxed) != (uint64_t)MiningPauseReason::kMingNotPaused;
	}

	///////////////////////////////////////////////////////////////////////////
	bool Miner::sExit = false;

	Miner::Miner(FarmFace &farm, const std::string &name, size_t index)
		: Worker(name+std::to_string(index)),
		mFarm(farm),
		mIndex(index) {}

	void Miner::setMiningPaused(MiningPauseReason pauseReason) {
		mMingPaused.setMiningPaused(pauseReason);
	}

	void Miner::clearMiningPaused(MiningPauseReason pauseReason) {
		mMingPaused.clearMiningPaused(pauseReason);
	}

	bool Miner::isMiningPaused() {
		return mMingPaused.isMiningPaused();
	}

	void Miner::setWork(const WorkPackage &work) {
		{
			std::lock_guard<std::mutex> l(mMutex);
			mWork = work;
		}

		kickMiner();
	}

	size_t Miner::index() const {
		return mIndex;
	}

	WorkPackage Miner::work() {
		std::lock_guard<std::mutex> l(mMutex);
		return mWork;
	}

	uint64_t Miner::getNonceStarter() const {
		return mIndex;
	}

	uint64_t Miner::getNonceIterator() const {
		return mFarm.minerCount();
	}
}