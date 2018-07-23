#ifndef __FARM_H__
#define __FARM_H__

#include "Miner.h"

namespace entanmo {
	class Farm : public FarmFace {
	public:
		struct SealerDescriptor {
			std::function<unsigned()> instances;
			std::function<Miner *(FarmFace &, unsigned)> create;
		};

		using SolutionFound = std::function<void(const Solution &)>;

	public:
		Farm();

		bool start(const std::string &sealer, bool mixed);
		void stop();
		void pause();

		void setSealers(const std::map<std::string, SealerDescriptor> &sealers);
		bool isMining() const;
		void setWork(const WorkPackage &w);

	public:
		void setOnSolutionFound(const SolutionFound &functor) ;

		virtual void submitProof(const Solution &s) override;
		virtual void failedSolution() override;
		virtual unsigned minerCount() const override;

	private:
		SolutionFound mOnSolutionFound;

		mutable std::mutex mMinerWork;
		std::vector<std::shared_ptr<Miner>> mMiners;
		WorkPackage mWork;
		std::atomic<bool> mIsMining;
		std::map<std::string, SealerDescriptor> mSealers;
		std::string mLastSealer;
		bool mLastMixed;
	};
}
#endif