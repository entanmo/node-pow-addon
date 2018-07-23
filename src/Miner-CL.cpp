#include "Miner-CL.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>

namespace entanmo {

	typedef struct {
		cl_int count;
		cl_ulong gid;
		cl_uint hash[SHA256_TARGET_SIZE];
	} SearchResult;

	std::vector<cl::Platform> getPlatforms() {
		std::vector<cl::Platform> platforms;
		try {
			cl::Platform::get(&platforms);
		} catch (const cl::Error &err) {
#if defined(CL_PLATFORM_NOT_FOUND_KHR)
			if (err.err() == CL_PLATFORM_NOT_FOUND_KHR) {
				std::cout << "No OpenCL platforms found" << std::endl;
			} else {
#endif
				throw err;
#if defined(CL_PLATFORM_NOT_FOUND_KHR)
			}
#endif
		}
		return platforms;
	}

	std::vector<cl::Device> getDevices(const std::vector<cl::Platform> &platforms, unsigned platformId) {
		std::vector<cl::Device> devices;
		size_t platform_num = std::min<size_t>(platformId, platforms.size() - 1);
		try {
			platforms[platform_num].getDevices(CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR, &devices);
		} catch (const cl::Error &err) {
			if (err.err() != CL_DEVICE_NOT_FOUND) {
				throw err;
			}
		}
		return devices;
	}

	///////////////////////////////////////////////////////////////////////////
	unsigned CLMiner::sWorkgrounpSize = CLMiner::scDefaultLocalWorkSize;
	unsigned CLMiner::sInitialGlobalWorkSize = CLMiner::scDefaultGlobalWorkSizeMultiplier * CLMiner::scDefaultLocalWorkSize;
	unsigned CLMiner::sPlatformId = 0;
	unsigned CLMiner::sNumInstances = 0;
	std::vector<int> CLMiner::sDevices(MAX_MINERS, -1);
	std::string CLMiner::sKernelPath;

	unsigned CLMiner::instances() {
		return CLMiner::sNumInstances > 0 ? CLMiner::sNumInstances : 1;
	}

	unsigned CLMiner::getNumDevices() {
		std::vector<cl::Platform> platforms = getPlatforms();
		if (platforms.empty()) {
			return 0;
		}

		std::vector<cl::Device> devices = getDevices(platforms, sPlatformId);
		if (devices.empty()) {
			std::cout << "No OpenCL devices found." << std::endl;
			return 0;
		}

		return devices.size();
	}

	void CLMiner::setNumInstances(unsigned instances) {
		CLMiner::sNumInstances = std::min<unsigned>(instances, CLMiner::getNumDevices());
	}

	void CLMiner::setDevices(const std::vector<unsigned> &devices, unsigned selectedSize) {
		for (unsigned i = 0; i < selectedSize; i++) {
			CLMiner::sDevices[i] = devices[i];
		}
	}

	bool CLMiner::configureGPU(unsigned localWorkSize, int globalWorkSizeMultiplier, unsigned platformId, bool exit) {
		CLMiner::sPlatformId = platformId;
		localWorkSize = ((localWorkSize + 7) / 8) * 8;
		CLMiner::sWorkgrounpSize = localWorkSize;
		CLMiner::sInitialGlobalWorkSize = globalWorkSizeMultiplier * localWorkSize;
		CLMiner::sExit = exit;

		std::vector<cl::Platform> platforms = getPlatforms();
		if (platforms.empty()) {
			return false;
		}
		if (platformId >= platforms.size()) {
			return false;
		}

		std::vector<cl::Device> devices = getDevices(platforms, platformId);
		if (devices.empty()) {
			std::cout << "No GPU device found." << std::endl;
			return false;
		}
		return true;
	}

	void CLMiner::configureKernelPath(const std::string &kernelpath) {
		CLMiner::sKernelPath = kernelpath;
	}

	CLMiner::CLMiner(FarmFace &farm, unsigned index)
		: Miner(farm, "cl-", index) {
	}

	CLMiner::~CLMiner() {
		stopWorking();
		kickMiner();
	}

	bool CLMiner::init() {
		try {
			if (CLMiner::sKernelPath.empty()) {
				return false;
			}

			std::string kernelCode;
			if (!this->_loadKernelCode(CLMiner::sKernelPath, kernelCode)) {
				return false;
			}

			std::vector<cl::Platform> platforms = getPlatforms();
			if (platforms.empty()) {
				return false;
			}

			unsigned platformIdx = std::min<unsigned>(CLMiner::sPlatformId, platforms.size() - 1);
			std::string platformName = platforms[platformIdx].getInfo<CL_PLATFORM_NAME>();
			
			std::vector<cl::Device> devices = getDevices(platforms, platformIdx);
			if (devices.empty()) {
				std::cout << "No OpenCL devices found." << std::endl;
				return false;
			}
			int idx = mIndex % devices.size();
			unsigned deviceId = CLMiner::sDevices[idx] > -1 ? CLMiner::sDevices[idx] : mIndex;
			cl::Device &device = devices[deviceId % devices.size()];

			char options[256] = { 0 };
			int computeCapability = 0;
			if (platformName == "NVIDIA CUDA") {
				cl_uint computeCapabilityMajor;
				cl_uint computeCapabilityMinor;
				clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV,
					sizeof(cl_uint), &computeCapabilityMajor, nullptr);
				clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV,
					sizeof(cl_uint), &computeCapabilityMinor, nullptr);
				computeCapability = computeCapabilityMajor * 10 + computeCapabilityMinor;
				int maxregs = computeCapability >= 35 ? 72 : 63;
				sprintf(options, "-cl-nv-maxrregcount=%d", maxregs);
			} else {
				sprintf(options, "%s", "");
			}

			mContext = cl::Context(std::vector<cl::Device>(&device, &device + 1));
			mQueue = cl::CommandQueue(mContext, device);

			mWorkgrounpSize = CLMiner::sWorkgrounpSize;
			mGlobalWorkSize = CLMiner::sInitialGlobalWorkSize;

			cl::Program::Sources sources{ { kernelCode.data(), kernelCode.size() } };
			cl::Program program(mContext, sources);
			try {
				program.build({ device }, options);
			} catch (const cl::BuildError &builderr) {
				std::cout << "OpenCL kernel build log:\n"
					<< program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
				std::cout << "OpenCL kernel build error (" << builderr.err() << "):\n"
					<< builderr.what() << std::endl;
				return false;
			}
			mKernalCode = kernelCode;
			mSearchKernel = cl::Kernel(program, "search_nonce");
			mSourceBuff = cl::Buffer(mContext, CL_MEM_READ_ONLY,
				SHA256_SOURCE_BLOCK_SIZE * SHA256_SOURCE_BLOCK_SIZE * sizeof(cl_char));
			mDifficultyBuff = cl::Buffer(mContext, CL_MEM_READ_ONLY,
				SHA256_SOURCE_BLOCK_SIZE * sizeof(cl_char));
			mTargetBuff = cl::Buffer(mContext, CL_MEM_WRITE_ONLY, sizeof(SearchResult));

			std::cout << "Platform: " << platformName
				<< ", Device: " << device.getInfo<CL_DEVICE_NAME>()
				<< ", Version: " << device.getInfo<CL_DEVICE_VERSION>() << std::endl;
		} catch (const cl::Error &err) {
			std::cerr << "[" << mIndex << "]OpenCL Error:" << err.err() << std::endl;
			if (CLMiner::sExit) {
				exit(1);
			}
			return false;
		}

		return true;
	}

	void CLMiner::kickMiner() {
		// TODO
	}

	void CLMiner::workLoop() {
		// std::cout << mIndex << "-workLoop" << std::endl;
		uint32_t const cZero = 0;
		uint64_t startNonce = 0;
		WorkPackage current;

		try {
			while (!shouldStop()) {
				if (isMiningPaused()) {
					std::this_thread::sleep_for(std::chrono::microseconds(100));
					continue;
				}

				const WorkPackage w = work();
				if (current.source != w.source) {
					if (!w) {
						std::this_thread::sleep_for(std::chrono::microseconds(100));
						continue;
					}

					// std::cout << mIndex << "New Work Initialize." << std::endl;
					mQueue.enqueueWriteBuffer(mSourceBuff, CL_FALSE, 0,
						w.source.size(), w.source.data());
					mQueue.enqueueWriteBuffer(mDifficultyBuff, CL_FALSE, 0,
						w.difficulty.size(), w.difficulty.data());
					mQueue.enqueueWriteBuffer(mTargetBuff, CL_FALSE, 0,
						sizeof(cZero), &cZero);

					mSearchKernel.setArg(0, mSourceBuff);
					mSearchKernel.setArg(1, (cl_uint)w.source.size());
					mSearchKernel.setArg(2, mDifficultyBuff);
					mSearchKernel.setArg(3, (cl_uint)w.difficulty.size());
					mSearchKernel.setArg(5, mTargetBuff);

					startNonce = getNonceStarter() * CLMiner::sInitialGlobalWorkSize;
				}
				if (!w) {
					std::this_thread::sleep_for(std::chrono::microseconds(100));
					continue;
				}

				SearchResult results;
				mQueue.enqueueReadBuffer(mTargetBuff, CL_TRUE, 0, sizeof(results), &results);
				if (results.count) {
					mQueue.enqueueWriteBuffer(mTargetBuff, CL_FALSE, 0, sizeof(cZero), &cZero);
				}
				mSearchKernel.setArg(4, (cl_ulong)startNonce);
				mQueue.enqueueNDRangeKernel(mSearchKernel, cl::NullRange, mGlobalWorkSize); //, mWorkgrounpSize);

				if (results.count) {
					if (!isMiningPaused()) {
						Solution solution;
						solution.nonce = current.startNonce + results.gid;
						memcpy(solution.hash, results.hash, sizeof(results.hash));
						char target[65] = { 0 };
						for (size_t i = 0; i < SHA256_TARGET_SIZE; i++) {
							sprintf(target + i * 8, "%08x", solution.hash[i]);
						}
						solution.target = std::string(target);
						solution.work = current;
						solution.stale = current.source != w.source;
						mFarm.submitProof(solution);
					}
				}

				current = w;
				current.startNonce = startNonce;
				startNonce += getNonceIterator() * CLMiner::sInitialGlobalWorkSize;
				mQueue.finish();
			}
			mQueue.finish();
		} catch (const cl::Error &err) {
			std::cerr << "[" << mIndex << "]OpenCL Error:" << err.err() << std::endl;
			if (CLMiner::sExit) {
				exit(1);
			}
		}
	} 

	bool CLMiner::_loadKernelCode(const std::string &kernelpath, std::string &kernelCode) {
		std::fstream f(kernelpath, std::fstream::in | std::fstream::binary);
		if (!f.is_open()) {
			return false;
		}

		size_t filesize;
		f.seekg(0, std::fstream::end);
		filesize = (size_t)f.tellg();
		f.seekg(0, std::fstream::beg);
		char *data = new char[filesize + 1];
		if (!data) {
			f.close();
			return false;
		}
		memset(data, 0, sizeof(char)* (filesize + 1));
		f.read(data, filesize);
		f.close();
		data[filesize] = '\0';
		kernelCode = data;
		delete[] data;
		return true;
	}
}