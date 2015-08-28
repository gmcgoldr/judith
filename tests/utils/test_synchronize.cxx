#include <iostream>
#include <stdexcept>
#include <cmath>

#include "utils.h"

bool approxEqual(double v1, double v2, double tol=1E-10) {
  return std::fabs(v1-v2) < tol;
}

int test_good() {
  std::vector<ULong64_t> times1 = { 0, 1, 2, 3, 4,  5,  6};
  std::vector<ULong64_t> times2 = { 0, 2, 4, 6, 8, 10, 12};

  std::vector<bool> target1 = { 1, 1, 1, 1, 0, 0, 0};
  std::vector<bool> target2 = { 1, 1, 1, 1, 0, 0, 0};

  std::vector<bool> write1;
  std::vector<bool> write2;

  Utils::synchronize(times1, times2, write1, write2, 2., 1e-6, 3);

  for (size_t i = 0; i < write1.size(); i++) {
    if (target1[i] != write1[i] || target2[i] != write2[i]) {
      std::cerr << "Utils: synchronize: failed good case" << std::endl;
      return -1;
    }
  }

  return 0;
}

int test_basic() {
  std::vector<ULong64_t> times1 = { 0, 1, 2, 3, 4,  5,  6};
  std::vector<ULong64_t> times2 = { 0,    4, 6, 8, 10, 12, 14};

  // Can't write 1st since buffer not synchronized, skip second on time1,
  // then write next two which have synchronized buffers and drop last three
  // due to buffer size (note +1 event since buffer is of deltas)
  std::vector<bool> target1 = { 0, 0, 1, 1, 0, 0, 0};
  std::vector<bool> target2 = { 0, 1, 1, 0, 0, 0, 0};

  std::vector<bool> write1;
  std::vector<bool> write2;

  // Ratio of 2 means time2 is 2x faster than time 1
  // Buffer of 3 means 4 events at end are discarded because the last 3 deltas
  // (gaps between 4 events), must be synchronized
  Utils::synchronize(times1, times2, write1, write2, 2., 1e-6, 3);

  if (target1.size() != write1.size() || write1.size() != write2.size()) {
    std::cerr << "Utils: synchronize: bad write length" << std::endl;
    return -1;
  }

  for (size_t i = 0; i < write1.size(); i++) {
    if (target1[i] != write1[i] || target2[i] != write2[i]) {
      std::cerr << "Utils: synchronize: failed basic case" << std::endl;
      return -1;
    }
  }

  return 0;
}

int test_double() {
  std::vector<ULong64_t> times1 = { 0, 1, 2, 3, 4,  5,  6};
  std::vector<ULong64_t> times2 = { 0,       6, 8, 10, 12, 14, 16};

  std::vector<bool> target1 = { 0, 0, 0, 1, 0, 0, 0};
  std::vector<bool> target2 = { 0, 1, 0, 0, 0, 0, 0};

  std::vector<bool> write1;
  std::vector<bool> write2;

  Utils::synchronize(times1, times2, write1, write2, 2., 1e-6, 3);

  for (size_t i = 0; i < write1.size(); i++) {
    if (target1[i] != write1[i] || target2[i] != write2[i]) {
      std::cerr << "Utils: synchronize: failed double case" << std::endl;
      return -1;
    }
  }

  return 0;
}

int main() {
  int retval = 0;

  try {
    if ((retval = test_good()) != 0) return retval;
    if ((retval = test_basic()) != 0) return retval;
    if ((retval = test_double()) != 0) return retval;
  }
  
  catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}
