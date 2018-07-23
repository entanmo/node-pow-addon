#include "Worker.h"

#include <iostream>

using namespace std;

namespace entanmo {
	void Worker::startWorking() {
		lock_guard<mutex> l(mMutex);
		if (mWork) {
			WorkerState ex = WorkerState::kStopped;
			mState.compare_exchange_strong(ex, WorkerState::kStarting);
		} else {
			mState = WorkerState::kStarting;
			mWork.reset(new thread([&]() {
				while (mState != WorkerState::kKilling) {
					WorkerState ex = WorkerState::kStarting;
					bool ok = mState.compare_exchange_strong(ex, WorkerState::kStarted);
					(void)ok;

					try {
						workLoop();
					} catch (const std::exception &_e) {
						std::cerr << "Exception throw in Worker thread: " << _e.what() << std::endl;
					}

					ex = mState.exchange(WorkerState::kStopped);
					if (ex == WorkerState::kKilling || ex == WorkerState::kStarting) {
						mState.exchange(ex);
					}

					while (mState == WorkerState::kStopped) {
						this_thread::sleep_for(chrono::milliseconds(20));
					}
				}
			}));
		}

		while (mState == WorkerState::kStarting) {
			this_thread::sleep_for(chrono::microseconds(20));
		}
	}

	void Worker::stopWorking() {
		lock_guard<mutex> l(mMutex);
		if (mWork) {
			WorkerState ex = WorkerState::kStarted;
			mState.compare_exchange_strong(ex, WorkerState::kStopping);

			while (mState != WorkerState::kStopped) {
				this_thread::sleep_for(chrono::microseconds(20));
			}
		}
	}

	Worker::~Worker() {
		lock_guard<mutex> l(mMutex);
		if (mWork) {
			mState.exchange(WorkerState::kKilling);
			mWork->join();
			mWork.reset();
		}
	}
}