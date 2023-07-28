add_llvm_library( BranchCount MODULE
        BranchCount.cpp

        #PLUGIN_TOOL
        #opt
        )

# 添加调试符号
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")