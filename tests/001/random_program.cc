#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <string>

using namespace std;

class type_members {
  map<string, string> members;
};

map<string, type_members> types;

unsigned long now() {
  return chrono::system_clock::now().time_since_epoch() /
         chrono::milliseconds(1);
}

string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
string digits = "0123456789";
string space = " \t";
string idsym = letters + digits + "_";
set<string> keywords = {"type",  "input", "output", "if",     "else", "for",
                        "while", "pardo", "do",     "return", "size", "dim"};

char random_letter(const string &s) { return s[rand() % s.size()]; }

string random_id() {
  int len=rand()%15+1;
  static string first_symbol = letters + "_";
  while (1) {
    string result(len, '?');
    if (len == 1)
      result[0] = random_letter(letters);
    else
      result[0] = random_letter(first_symbol);
    for (int i = 1; i < len; i++) result[i] = random_letter(idsym);
    if (keywords.count(result) == 0) return result;
  }
}

void random_space(int ilen) {
  if (rand() % 1000 > 800) return;
  int len = rand() % ilen + 1;
  for (int i = 0; i < len; i++) {
    cout << random_letter(space);
    if (rand() % 1000 > 780) cout << endl;
  }
}

string random_type() {
  int type = rand() % types.size();
  for (auto it = types.begin(); type >= 0; ++it)
    if (--type < 0) return it->first;
  return "huh";
}

void type_definition() {
  string name;

  while (1) {
    name = random_id();
    if (types.count(name) == 0) break;
  }

  random_space(2);
  cout << "type ";
  random_space(3);
  cout << name;
  random_space(2);
  cout << "{";
  random_space(4);

  set<string> members;

  int nmemb = rand() % 8 + 1;
  for (int i = 0; i < nmemb; i++) {
    random_space(3);
    cout << random_type() << " ";
    for (int memvars = rand() % 4 + 1; memvars;) {
      string memname;
      while (1) {
        memname = random_id();
        if (members.count(memname) == 0 && types.count(memname) == 0 && memname!=name) break;
      }
      members.insert(memname);
      random_space(3);
      cout << memname;
      random_space(3);
      cout << ((--memvars > 0) ? "," : ";");
      random_space(3);
    }
  }

  cout << "}";
  random_space(4);

  types[name] = {};
}

void variable_definition() {
  if (rand() % 1000 > 800) cout << "output ";
  random_space(3);
  cout<<random_type()<<" ";
  random_space(3);
  cout<<random_id()<<";";
}

int main() {
  srand(now());

  types["int"] = {};
  types["float"] = {};
  types["char"] = {};

  for (int i = 0; i < 1000; i++) {
    int what = rand() % 1;
    switch (what) {
      case 0:
        type_definition();
        break;
      case 1:
        variable_definition();
        break;
    }
  }
  cout << endl;
  return 0;
}
