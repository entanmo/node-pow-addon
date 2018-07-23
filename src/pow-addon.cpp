
#include "Farm.h"
#include "Miner-CL.h"
#include <node.h>
#include <uv.h>

#include <iostream>
#include <map>

using namespace std;
using namespace entanmo;

using v8::FunctionCallbackInfo;
using v8::Function;

using v8::Local;
using v8::Isolate;
using v8::Persistent;
using v8::HandleScope;

using v8::Value;
using v8::Number;
using v8::String;
using v8::Object;
using v8::Boolean;
using v8::Undefined;

class RWLockGuard {
public:
    RWLockGuard() = default;

    ~RWLockGuard() {
        uv_rwlock_destroy(&mRWLock);
    }

    void init() {
        uv_rwlock_init(&mRWLock);
    }

    void rdLock() {
        uv_rwlock_rdlock(&mRWLock);
    }

    void rdUnlock() {
        uv_rwlock_rdunlock(&mRWLock);
    }

    void wrLock() {
        uv_rwlock_wrlock(&mRWLock);
    }

    void wrUnlock() {
        uv_rwlock_wrunlock(&mRWLock);
    }

private:
    uv_rwlock_t mRWLock;
};

const char *kSealer = "opencl";

// Module global variables
uv_async_t async;
RWLockGuard gRWLock;

Farm gFarm;
Persistent<Function> uvCallback;
Solution gSolution;

// for libuv event-loop thread callback
void _uvThreadCallback(uv_async_t *handle) {
    // std::cout << "_uvThreadCallback " << s.nonce << std::endl;
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope handleScope(isolate);

    if (uvCallback.IsEmpty()) {
        // no callback
        return ;
    }
    Solution s;
    gRWLock.rdLock();
    s = *((Solution *)handle->data);
    gRWLock.rdUnlock();

    Local<Object> ret = Object::New(isolate);
    ret->Set(String::NewFromUtf8(isolate, "done"),
        Boolean::New(isolate, !s.stale));
    if (!s.stale) {
        Local<Object> value = Object::New(isolate);
        value->Set(String::NewFromUtf8(isolate, "src"),
            String::NewFromUtf8(isolate, s.work.source.c_str()));
        value->Set(String::NewFromUtf8(isolate, "nonce"),
            Number::New(isolate, (double)s.nonce));
        value->Set(String::NewFromUtf8(isolate, "difficulty"),
            String::NewFromUtf8(isolate, s.work.difficulty.c_str()));
        value->Set(String::NewFromUtf8(isolate, "target"),
            String::NewFromUtf8(isolate, s.target.c_str()));
        
        ret->Set(String::NewFromUtf8(isolate, "value"),
            value);
    }

    Local<Value> argv[] = { ret };
    Local<Function>::New(isolate, uvCallback)->Call(
        isolate->GetCurrentContext()->Global(),
        1, 
        argv
    );
    uvCallback.Reset();
}

// for cpp farm onSolutionFound callback
void _cppThreadCallback(const Solution &s) {
    // std::cout << "_cppThreadCallback " << s.nonce << std::endl;
    gRWLock.wrLock();
    gSolution = s;
    gRWLock.wrUnlock();
    async.data = (void *)&gSolution;
    uv_async_send(&async);
}

void _Init(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate();

    if (args.Length() <= 0) {
        std::cerr << "_Init arguments is empty." << std::endl;
        args.GetReturnValue().Set(Boolean::New(isolate, false));
        return ;
    }
    if (!args[0]->IsObject()) {
        std::cerr << "_Init arguments first arg must be object" << std::endl;
        args.GetReturnValue().Set(Boolean::New(isolate, false));
        return ;
    }

    unsigned localWorkSize = CLMiner::scDefaultLocalWorkSize;
    unsigned globalWorkSizeMultiplier = CLMiner::scDefaultGlobalWorkSizeMultiplier;
    unsigned numOfInstances = UINT_MAX;
    int platformId = 0;
    std::string kernelPath;
    Local<Object> cfg = Local<Object>::Cast(args[0]);
    const auto keyKernelPath = String::NewFromUtf8(isolate, "kernelPath");
    const auto keyLocalWorkSize = String::NewFromUtf8(isolate, "localWorkSize");
    const auto keyGlobalWorkSizeMultiplier = String::NewFromUtf8(isolate, "globalWorkSizeMultiplier");
    const auto keyNumOfInstances = String::NewFromUtf8(isolate, "numOfInstances");
    const auto keyPlatformId = String::NewFromUtf8(isolate, "platformId");

    // kernelPath 
    if (!cfg->Has(keyKernelPath) || !cfg->Get(keyKernelPath)->IsString()) {
        std::cerr << "_Init invalid kernelPath type" << std::endl;
        args.GetReturnValue().Set(Boolean::New(isolate, false));
        return ;
    }
    kernelPath = std::string(*String::Utf8Value(cfg->Get(keyKernelPath)));

    // localWorkSize
    if (cfg->Has(keyLocalWorkSize) && cfg->Get(keyLocalWorkSize)->IsNumber()) {
        localWorkSize = (unsigned)cfg->Get(keyLocalWorkSize)->NumberValue();
        if (localWorkSize <= 0) {
            localWorkSize = CLMiner::scDefaultLocalWorkSize;
        }
    }

    // globalWorkSizeMultiplier
    if (cfg->Has(keyGlobalWorkSizeMultiplier) && cfg->Get(keyGlobalWorkSizeMultiplier)->IsNumber()) {
        globalWorkSizeMultiplier = (unsigned)cfg->Get(keyGlobalWorkSizeMultiplier)->NumberValue();
        if (globalWorkSizeMultiplier <= 0) {
            globalWorkSizeMultiplier = CLMiner::scDefaultGlobalWorkSizeMultiplier;
        }
    }

    // numOfInstances
    if (cfg->Has(keyNumOfInstances) && cfg->Get(keyNumOfInstances)->IsNumber()) {
        numOfInstances = (unsigned)cfg->Get(keyNumOfInstances)->NumberValue();
        if (numOfInstances <= 0) {
            numOfInstances = UINT_MAX;
        }
    }

    // platformId
    if (cfg->Has(keyPlatformId) && cfg->Get(keyPlatformId)->IsNumber()) {
        platformId = (int)cfg->Get(keyPlatformId)->NumberValue();
        if (platformId < 0) {
            platformId = 0;
        }
    }

    // configureGPU
    if (!CLMiner::configureGPU(localWorkSize, 
        globalWorkSizeMultiplier, platformId)) {
        args.GetReturnValue().Set(Boolean::New(isolate, false));
        return ;
    }

    // configureKernel
    CLMiner::configureKernelPath(kernelPath);
    CLMiner::setNumInstances(numOfInstances);

    if (!gFarm.start(kSealer, false)) {
        args.GetReturnValue().Set(Boolean::New(isolate, false));
        return;
    }

    args.GetReturnValue().Set(Boolean::New(isolate, true));
}

void _Mint(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate();

    Local<Value> src = Local<Value>::Cast(args[0]);
    Local<Value> difficulty = Local<Value>::Cast(args[1]);
    Local<Function> callback = Local<Function>::Cast(args[2]);

    uvCallback.Reset(isolate, callback);

    WorkPackage w;
    w.source = std::string(*String::Utf8Value(src));
    w.difficulty = std::string(*String::Utf8Value(difficulty));
    gFarm.setWork(w);

    args.GetReturnValue().Set(Undefined(isolate));
}

void _Pause(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate();

    gFarm.pause();

    args.GetReturnValue().Set(Undefined(isolate));
}

///////////////////////////////////////////////////////////////////////////////
void AddonInit(Local<Object> exports, Local<Object> module) {
    // init Farm
    std::map<string, Farm::SealerDescriptor> sealers;
    Farm::SealerDescriptor clSealer;
    clSealer.instances = &CLMiner::instances;
    clSealer.create = [](FarmFace &farm, unsigned index) -> Miner * {
        return new CLMiner(farm, index);
    };
    sealers[kSealer] = clSealer;
    gFarm.setSealers(sealers);
    gFarm.setOnSolutionFound(_cppThreadCallback);
    gRWLock.init();
    uv_async_init(uv_default_loop(), &async, _uvThreadCallback);

    NODE_SET_METHOD(exports, "init", _Init);
    NODE_SET_METHOD(exports, "mint", _Mint);
    NODE_SET_METHOD(exports, "pause", _Pause);
}

NODE_MODULE(pow, AddonInit);