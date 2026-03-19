#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

class RvviCliIntegrationTest : public ::testing::Test {
};

TEST_F(RvviCliIntegrationTest, BasicCliFlagAcceptance) {
  // Feed "quit\n" to interactive mode to terminate gracefully.
  std::string cmd = "echo 'quit' | ./riscv/rva23s64_sim --rvvi_trace -i=true riscv/test/testfiles/hello_world_64.elf 2>&1";
  FILE* pipe = popen(cmd.c_str(), "r");
  ASSERT_NE(pipe, nullptr);
  
  char buffer[128];
  std::string output = "";
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }
  int status = pclose(pipe);
  
  EXPECT_TRUE(WIFEXITED(status)) << "Daemon pipeline did not exit cleanly. Output: " << output;
  EXPECT_EQ(WEXITSTATUS(status), 0) << "Daemon pipeline failed with non-zero exit code. Output: " << output;

  // It shouldn't complain about an unknown flag.
  EXPECT_EQ(output.find("Unknown command line flag 'rvvi_trace'"), std::string::npos) 
      << "CLI must accept --rvvi_trace\n" << output;
}
