#ifndef __MINER_H__
#define __MINER_H__

#include "Worker.h"

#define LOG2_MAX_MINERS 5u
#define MAX_MINERS (1u << LOG2_MAX_MINERS)

#define SHA256_SOURCE_BLOCK_SIZE 64
#define SHA256_SOURCE_MAX_SIZE 2018
#define SHA256_TARGET_SIZE 8

namespace entanmo {
	enum class MiningPauseReason : uint64_t {
		kMingNotPaused					= 0x00000000,
		kMiningPausedWaitForTStart		= 0x00000001,
		kMingPausedAPI					= 0x00000002
	};

	struct WorkPackage {
		std::string source;
		std::string difficulty;
		uint64_t startNonce;

		WorkPackage();

		explicit operator bool() const;
	};

	struct Solution {
		uint64_t nonce;
		unsigned hash[SHA256_TARGET_SIZE];
		std::string target;
		WorkPackage work;
		bool stale;
	};

	struct MiningPause {
		std::atomic<uint64_t> mMingPausedFlag = { (uint64_t)MiningPauseReason::kMingNotPaused };

		void setMiningPaused(MiningPauseReason pauseReason);
		void clearMiningPaused(MiningPauseReason pauseReason);
		MiningPauseReason getMiningPaused();
		bool isMiningPaused();
	};

	class Miner;

	class FarmFace {
	public:
		virtual ~FarmFace() = default;

		virtual void submitProof(const Solution &s) = 0;
		virtual void failedSolution() = 0;

		virtual unsigned minerCount() const = 0;
	};

	class Miner : public Worker {
	public:
		Miner(FarmFace &farm, const std::string &name, size_t index);

		virtual ~Miner() = default;

		virtual bool init() = 0;

		void setMiningPaused(MiningPauseReason pauseReason);
		void clearMiningPaused(MiningPauseReason pauseReason);
		bool isMiningPaused();

		void setWork(const WorkPackage &work);

		size_t index() const;
		
	protected:
		virtual void kickMiner() = 0;

		WorkPackage work();
		uint64_t getNonceStarter() const;
		uint64_t getNonceIterator() const;

	protected:
		static bool sExit;
		FarmFace &mFarm;
		const size_t mIndex = 0;

	private:
		MiningPause mMingPaused;
		WorkPackage mWork;
		std::mutex mMutex;
	};

	
}
#endif