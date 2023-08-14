# compiler-pass
This repository contains all the exercises I've had during learning compiler pass.

# MemorySSA
clang -Xclang -disable-O0-optnone -c -emit-llvm -fno-discard-value-names -S -o test.bc test.c

opt -passes='print<memoryssa>' test.ll

# Commands for Branch counter - New Pass Manager
## main.c
clang -emit-llvm -S -g -c main.c -o main.bc

opt -load-pass-plugin=/home/zhongfa/passtest/build/InstCount/InstCount.so -passes="branch-count-pass" main.bc -o main.ll
## test.c

clang -emit-llvm -S -g -c test.c -o test.bc

opt -load-pass-plugin=/home/zhongfa/passtest/build/InstCount/InstCount.so -passes="branch-count-pass" test.bc -o test.ll

clang -g test.ll

# Commands for Target Counter - New Pass Manager
clang -emit-llvm -S -O0 -g -c test.c -o test.bc

opt -load-pass-plugin=/home/zhongfa/passtest/build/TargetCount/TargetCount.so -passes="target-count-pass" test.bc -o test.ll
//local
opt -load-pass-plugin=/home/rigel/projects/compiler-pass/build/TargetCount/TargetCount.so -passes="target-count-pass" test.bc -o test.ll

clang -g -O0 test.ll

# two passes
opt -load-pass-plugin=/home/zhongfa/passtest/build/InstCount/InstCount.so -load-pass-plugin=/home/zhongfa/passtest/build/BranchCount/BranchCount.so -passes="branch-count-pass,target-count-pass" test.bc -o test.ll



# Legacy: Constantly used commands
clang -emit-llvm -S main.c -o main.bc

clang -emit-llvm -S main.c -o main.ll

opt -load ./build/InstCount/InstCount.so -InstCount main.bc -o /dev/null -enable-new-pm=0

clang -flegacy-pass-manager -Xclang -load -Xclang /home/zhongfa/passtest/build/InstCount/InstCount.so -c main.c

clang -S -emit-llvm -O1  -o main.bc -g main.c   

opt -load /home/zhongfa/passtest/build/InstCount/InstCount.so -InstCount main.bc -o main_out.bc -enable-new-pm=0 -S

clang main_out.bc -g  