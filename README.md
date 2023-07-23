# compiler-pass
This repository contains all the exercises I've had during learning compiler pass.

# Constantly used commands
clang -emit-llvm -S main.c -o main.bc

clang -emit-llvm -S main.c -o main.ll

opt -load ./build/InstCount/InstCount.so -InstCount main.bc -o /dev/null -enable-new-pm=0

clang -flegacy-pass-manager -Xclang -load -Xclang /home/zhongfa/passtest/build/InstCount/InstCount.so -c main.c

clang -S -emit-llvm -O1  -o main.bc -g main.c   

opt -load /home/zhongfa/passtest/build/InstCount/InstCount.so -InstCount main.bc -o main_out.bc -enable-new-pm=0 -S

clang main_out.bc -g  

# Commands for New Pass Manager

clang -emit-llvm -S -g -c main.c -o main.bc

opt -load-pass-plugin=/home/zhongfa/passtest/build/InstCount/InstCount.so -passes="branch-count-pass" main.bc -o main.ll
