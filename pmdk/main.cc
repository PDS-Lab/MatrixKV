#include <cstring>
#include <string>
#include <cstdio>
#include <iostream>
using namespace std;

void fun1()
{
  string s1 = "hello, world!";
  string s2 = s1; 

  cout << "before: " << s2 << endl;
  char* ptr = const_cast<char*>(s1.c_str());
  *ptr = 'f';
  cout << "after: " << s2 << endl;
  cout << "s1:after: " << s1 << endl;
}

void fun2()
{
  string s1 = "hello, world!";
  string s2 = s1; 

  cout << "before: " << s2 << endl;
  s1[0] = 'f';
  cout << "after: " << s2 << endl;
  cout << "s1:after: " << s1 << endl;
}

int main()
{
  cout << "fun1: " << endl;
  fun1();

  cout << "fun2: " << endl;
  fun2();

  return 0;
}