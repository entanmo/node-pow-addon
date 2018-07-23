#ifndef __WORKER_H__
#define __WORKER_H__

#ifdef WIN32
#if defined(_HAS_EXCEPTIONS)
#undef _HAS_EXCEPTIONS
#endif
#endif

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <cassert>
#include <mutex>
#include <memory>
#include <climits>
#include <functional>

namespace entanmo {
	enum class WorkerState {
		kStarting,
		kStarted,
		kStopping,
		kStopped, 
		kKilling
	};

	class Worker {
	public:
		Worker(const std::string &_name)
			: mName(_name) {
		}

		Worker(const Worker &) = delete;

		Worker & operator=(const Worker &) = delete;

		virtual ~Worker();

		void startWorking();

		void stopWorking();

		bool shouldStop() const {
			return mState != WorkerState::kStarted;
		}

	private:

		virtual void workLoop() = 0;

		std::string mName;
		mutable std::mutex mMutex;
		std::unique_ptr<std::thread> mWork;
		std::atomic<WorkerState> mState = { WorkerState::kStarting };
	};
}
#endif