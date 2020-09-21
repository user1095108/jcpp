#include <fstream>

#include <iostream>

#include <vector>

#include "js0ncpp.hpp"

int main()
{
  jcpp::js0n const j("{\"foo\":\"bar\",\"barbar\":[1,2,3],\"obj\":[{\"a\":\"b\"}],\"a\":true}");

  //
  jcpp::dec::map("foo", std::cout, "barbar", std::cout, "obj", std::cout)(j);
  std::cout << std::endl;

  //
  jcpp::dec::map("barbar",
    jcpp::dec::array(
      [](jcpp::js0n const& j){jcpp::dec::decode(j, std::cout); std::cout << std::endl;}
    )
  )(j);

  //
  struct S
  {
    std::string_view a;

    auto from_js0n() noexcept
    {
      return jcpp::dec::map("a", a);
    }
  } s;

  //
  jcpp::dec::map("obj", jcpp::dec::array(s))(j);
  std::cout << s.a << std::endl;

  //
  std::vector<S> vs;
  jcpp::dec::map("obj", vs)(j);
  std::cout << vs.size() << " " << vs.front().a << std::endl;

  return 0;
}
