#include <cppcms/application.h>
#include <cppcms/applications_pool.h>
#include <cppcms/http_cookie.h>
#include <cppcms/http_file.h>
#include <cppcms/http_request.h>
#include <cppcms/http_response.h>
#include <cppcms/mount_point.h>
#include <cppcms/rpc_json.h>
#include <cppcms/service.h>
#include <cppcms/url_dispatcher.h>
#include <cppcms/url_mapper.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>

// for debug only
#include <iostream>
#include <unistd.h>
// end for debug only

#include "content.h"

#define SLOW_DEBUG 0

std::unordered_map<std::string,std::string> snippets;


/* *******************************************************
 * HTML SRVER
 * *******************************************************
 */

#define SIMPLE_METHOD(x) void x() { content::x data; render(#x, data);}

class html_server : public cppcms::application {
 public:
  html_server(cppcms::service &srv);
  SIMPLE_METHOD(home)
  SIMPLE_METHOD(page)
  SIMPLE_METHOD(usage)
  SIMPLE_METHOD(language)
  SIMPLE_METHOD(samples_sum)
  SIMPLE_METHOD(samples_psum)
  SIMPLE_METHOD(samples_first1)
  SIMPLE_METHOD(samples_lrank)
  SIMPLE_METHOD(samples_psrch)
  SIMPLE_METHOD(samples_hull)
  SIMPLE_METHOD(samples_max)

  void ide(std::string snippet_id) {
    content::ide data(snippets[snippet_id]);
    render("ide",data);
  }

  void main(std::string);
};

//   ---------------------------------------------------
//   CONSTRUCTOR

#define SIMPLE_DISPATCH(x) \
  dispatcher().assign("/"#x, &html_server::x, this); \
  mapper().assign(#x, "/"#x);

html_server ::html_server(cppcms::service &srv) : cppcms::application(srv) {
  // setup URL dispatching
  SIMPLE_DISPATCH(home)
  SIMPLE_DISPATCH(usage)
  SIMPLE_DISPATCH(language)
  SIMPLE_DISPATCH(samples_sum)
  SIMPLE_DISPATCH(samples_psum)
  SIMPLE_DISPATCH(samples_first1)
  SIMPLE_DISPATCH(samples_lrank)
  SIMPLE_DISPATCH(samples_psrch)
  SIMPLE_DISPATCH(samples_hull)
  SIMPLE_DISPATCH(samples_max)

  dispatcher().assign("/ide/(.*)",&html_server::ide,this,1);

  dispatcher().assign("",&html_server::home, this);
  mapper().root("/service/wtstar");
}

//   ---------------------------------------------------
//   PROCESS REQUEST
void html_server::main(std::string url) {
  if (SLOW_DEBUG) sleep(2);
  response().set_header("Content-Type","text/html; charset=utf-8");
  cppcms::application::main(url);
}

/* *******************************************************
 * RPC SRVER
 * *******************************************************
 */

class rpc_server : public cppcms::rpc::json_rpc_server {
 public:
  rpc_server(cppcms::service &srv);
  virtual void main(std::string url);

  // RPC methods
  void ping();
};

#define METHOD(m) \
  bind(#m, cppcms::rpc::json_method(&rpc_server::m, this), method_role);

//   ---------------------------------------------------
//   CONSTRUCTOR
rpc_server::rpc_server(cppcms::service &srv)
    : cppcms::rpc::json_rpc_server(srv) {
  METHOD(ping)
}

//   ---------------------------------------------------
//   PROCESS REQUEST
void rpc_server::main(std::string url) {
  if (SLOW_DEBUG) sleep(2);
  // std::cout << "rpc server: " << url << std::endl;
  // Handle CORS
  response().set_header("Access-Control-Allow-Origin", "*");
  response().set_header("Access-Control-Allow-Headers", "Content-Type");

  if (request().request_method() == "OPTIONS") {
    return;
  }
  cppcms::rpc::json_rpc_server::main(url);
}

void rpc_server::ping() {
  cppcms::json::object res;

  res["reply"] = "pong";
  return_result(res);
}

int main(int argc, char **argv) {
  std::cout<<"starting\n";

  #include "snippets.cc"


  if (SLOW_DEBUG) std::cout << "DEBUG VERSION WITH SLOWDOWN !! " << std::endl;
  try {
    cppcms::service srv(argc, argv);

    // run servers
    srv.applications_pool().mount(cppcms::applications_factory<rpc_server>(),
                                  cppcms::mount_point("/rpc", 0));

    srv.applications_pool().mount(cppcms::applications_factory<html_server>(),
                                  cppcms::mount_point());

    srv.run();
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
