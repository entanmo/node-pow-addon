'use strict';

const pow = require('bindings')('pow');
const path = require('path');
const fs = require('fs');

const KERNEL_PATH = path.resolve(__dirname, "kernels/mint-kernel.cl");

module.exports = {
    setup: function (cfgPath) {
        let powConfig = {
            kernelPath: KERNEL_PATH
        };

        try {
           if (fs.existsSync(cfgPath)) {
               const jsonData = require(cfgPath);
               // loclWorkSize
               if (jsonData.localWorkSize && typeof jsonData.localWorkSize === "number") {
                   powConfig.localWorkSize = jsonData.localWorkSize
               }
               // globalWorkSizeMultiplier
               if (jsonData.globalWorkSizeMultiplier && typeof jsonData.globalWorkSizeMultiplier === "number") {
                   powConfig.globalWorkSizeMultiplier = jsonData.globalWorkSizeMultiplier;
               }
               // platformId
               if (jsonData.platformId && typeof jsonData.platformId === "number") {
                   powConfig.platformId = jsonData.platformId;
               }
               // numOfInstances
               if (jsonData.numOfInstances && typeof jsonData.numOfInstances === "number") {
                   powConfig.numOfInstances = jsonData.numOfInstances;
               }
           }
        } catch (e) {
            console.log("get config_miner.json failure.");
        }
        return pow.init(powConfig);
    },

    mint: function (src, difficulty, cb) {
        pow.mint(src, difficulty, cb);
    },

    pending: function (cb) {
        pow.pause();

        setImmediate(cb);
    }
}