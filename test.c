#include "stdio.h"

int main() {
  int arr[10] = {1,1};
  int idx[10] = {0,1,2,3,4,5,6,7,8,9};
  for (int i=2; i<10 ; i++){
    arr[i] = arr[i-1] + arr[i-2];
  }
  int addr=5;
  int value;
  for (int j=0; j<10; j++){
    if (addr == j) {
    value = arr[idx[addr]];
    }
  }
}