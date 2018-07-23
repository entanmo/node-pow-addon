{
    "targets": [{
        "target_name": "pow",
        "sources": [
            "src/Farm.cpp",
            "src/Miner-CL.cpp",
            "src/Miner.cpp",
            "src/Worker.cpp",
            "src/pow-addon.cpp"
        ],
        "conditions": [
            ['OS=="win"', {
                "cflags": [
                    "-fPIC"
                ],
                "cflags_cc": [
                    "-fPIC"
                ],
                "include_dirs": [
                    "deps/opencl/win/include"
                ],
                "libraries": [
                    "../deps/opencl/win/lib/OpenCL.lib"
                ],
                "configurations": {
                    "Release": {
                        "msvs_settings": {
                            "VCCLCompilerTool": {
                                "AdditionalOptions": [
                                    "/MP /EHsc"
                                ]
                            }
                        }
                    }
                }
            }],
            ['OS=="linux"', {
                "cflags": [
                    "-fexceptions",
                    "--std=c++11",
                    "-pthread"
                ],
                "cflags_cc": [
                    "-fexceptions",
                    "--std=c++11",
                    "-pthread"
                ],
                "include_dirs": [
                    "deps/opencl/linux/include"
                ],
                "libraries": [
                    "../deps/opencl/linux/lib/libOpenCL.so"
                ]
            }]
        ]
    }]
}