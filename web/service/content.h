#ifndef __CONTENT_H__
#define __CONTENT_H__

#include <cppcms/view.h>
#include <string>
#include <utility>
#include <vector>

#include <iostream>  // for debug


namespace content {

// html frontentd/backend
// scripts: names of scripts to be loaded
// static_prefix: path prefix for serving static contents (defined in
// credentials)
struct master : public cppcms::base_content {
  master() {}
  master(const std::vector<std::string> &_scripts,
         const std::vector<std::string> &_styles)
      : scripts(_scripts),styles(_styles) {}
  std::vector<std::string> scripts,styles;
  std::string static_prefix = STATIC_PREFIX;
};

struct home : public master {
  home() : master({"home"},{"home"}) {}
};

struct page : public master {
  page() : master({"home"},{"home"}) {}
};

#define SIMPLE_CLASS(x) \
  struct x : public page { \
    x() : page() {} \
  };

SIMPLE_CLASS(usage)
SIMPLE_CLASS(language)

struct samples : public page {
  samples(std::string _active) : page() {
    active=_active;
  }
  std::string active;
};

#define SAMPLES(x) \
    struct samples_##x : public samples { \
      samples_##x() : samples(#x) {} \
    };

SAMPLES(sum)
SAMPLES(psum)
SAMPLES(first1)
SAMPLES(lrank)
SAMPLES(psrch)
SAMPLES(hull)
SAMPLES(max)

struct ide : public master {
  ide(std::string _snippet) : master({"mode-wtstar","ide"},{"ide"}){
    snippet=_snippet;
  }
  std::string snippet;
};

}  // namespace content

#endif
