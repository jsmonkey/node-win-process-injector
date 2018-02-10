{
    "targets": [
        {
            "target_name": "win_process_injector",
            "sources": [
                "src/index.cc",
                "src/win_process.cc"
            ],
            "include_dirs": [
                "<(module_root_dir)/lib/win-process-injector/include"
            ],
            "libraries": [
                "-l<(module_root_dir)/lib/win-process-injector/bin/win_process_injector_lib.lib"
            ]
        }
    ]
}