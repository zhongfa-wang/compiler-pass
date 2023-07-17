add_llvm_library( InstCount MODULE
        InstCount.cpp

        PLUGIN_TOOL
        opt
        )