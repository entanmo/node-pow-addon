#ifndef __CL_MINER_H__
#define __CL_MINER_H__

#include "Miner.h"

#ifndef WIN32
#pragma GCC diagnostic push
#if __GNUC__ >= 6
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#pragma GCC diagnostic ignored "-Wmissing-braces"
#endif

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS true
#define CL_HPP_ENABLE_EXCEPTIONS true
#define CL_HPP_CL_1_2_DEFAULT_BUILD true
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include "CL/cl2.hpp"
#ifndef WIN32
#pragma GCC diagnostic pop
#endif

// macOS OpenCL fix:
#ifndef CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV
#define CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV       0x4000
#endif

#ifndef CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV
#define CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV       0x4001
#endif

namespace entanmo {
	class CLMiner : public Miner {
	public:
		static const unsigned scDefaultLocalWorkSize = 128;
		static const unsigned scDefaultGlobalWorkSizeMultiplier = 8192;

	public:
		static unsigned instances();
		static unsigned getNumDevices();
		static void setNumInstances(unsigned int instances);
		static void setDevices(const std::vector<unsigned> &devices, unsigned selectedSize);

		static bool configureGPU(unsigned localWorkSize, int globalWorkSizeMultiplier, unsigned platformId, bool exit = false);

		static void configureKernelPath(const std::string &kernelpath);

	public:
		CLMiner(FarmFace &farm, unsigned index);

		~CLMiner() override;

	protected:
		void kickMiner() override;

	private:
		void workLoop() override;

		bool init();

		bool _loadKernelCode(const std::string &kernelpath, std::string &kernelCode);

	private:
		cl::Context mContext;
		cl::CommandQueue mQueue;
		cl::Kernel mSearchKernel;

		cl::Buffer mSourceBuff;
		cl::Buffer mDifficultyBuff;
		cl::Buffer mTargetBuff;

		unsigned mGlobalWorkSize = 0;
		unsigned mWorkgrounpSize = 0;
		std::string mKernalCode;

		static unsigned sPlatformId;
		static unsigned sNumInstances;
		static std::vector<int> sDevices;
		static std::string sKernelPath;

		static unsigned sWorkgrounpSize;
		static unsigned sInitialGlobalWorkSize;
	};
}

#endif
