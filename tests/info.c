#include "minunit.h"
#include "info.h"


MU_TEST(test_asdf_info_returns) {
  mu_check(asdf_info() == 0);
}


MU_TEST_SUITE(test_asdf_info) {
  MU_RUN_TEST(test_asdf_info_returns);
}


int main(int argc, char *argv[]) {
  MU_RUN_SUITE(test_asdf_info);
  MU_REPORT();
  return MU_EXIT_CODE;
}
