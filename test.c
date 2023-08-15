#include "stdio.h"

int main()
{
  // int a = 11;
  // int b = 1919;
  // int c = 810;
  // int d = 45;
  //   c = d*2;
  // if (a > 4)
  // {
  //   b = a;
  // }

  // b = c;
  // if (d > 14)
  // {
  //   c = a;
  //   d = a;
  //   // b = a * c;
  //   // c = b * a;
  // }

  // int arr;
  // for (int i = 0; i < 20; i++)
  // {
  //   arr = i * i;
  // }

  // int arr1[10] = {1, 1};
  // for (int i = 0; i < 10; i++)
  // {
  //   arr1[i] = arr1[i - 1] + arr1[i - 2];
  // }

  int idx[10] = {0,1,2,3,4,5,6,7,8,9};
  int addr = 5;
  int addri = 6;
  int value;
  for (int j=0; j<10; j++){
    if (addr == j) {
    value = idx[addr];
    }
  }
}