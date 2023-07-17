# compiler-pass
This repository contains all the exercises I've had during learning compiler pass.

# Constantly used commands
clang -emit-llvm -S main.c -o main.bc

clang -emit-llvm -S main.c -o main.ll

opt -load ./build/InstCount/InstCount.so -InstCount main.bc -o /dev/null -enable-new-pm=0