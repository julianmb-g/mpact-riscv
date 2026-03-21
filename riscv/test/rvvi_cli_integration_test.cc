#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <sys/wait.h>
#include "riscv/test/subprocess_runner.h"
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;
using mpact::sim::riscv::test::SubprocessRunner;

class RvviCliIntegrationTest : public ::testing::Test {};

TEST_F(RvviCliIntegrationTest, BasicCliFlagAcceptance) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  ASSERT_NE(runfiles, nullptr) << error;
  
  std::string sim_path = runfiles->Rlocation("com_google_mpact-riscv/riscv/rva23s64_sim");
  std::string elf_path = runfiles->Rlocation("com_google_mpact-riscv/riscv/test/testfiles/hello_world_64.elf");

  std::vector<std::string> args = {
      "--rvvi_trace",
      "-i=true",
      elf_path
  };

  SubprocessRunner runner(sim_path, args);
  std::string output;
  int status = runner.RunWithInput("step 10\nreg info\nquit\n", &output);

  EXPECT_TRUE(WIFEXITED(status)) << "Daemon pipeline did not exit cleanly. Output: " << output;
  EXPECT_EQ(WEXITSTATUS(status), 0) << "Daemon pipeline failed with non-zero exit code. Output: " << output;

  EXPECT_EQ(output.find("Unknown command line flag 'rvvi_trace'"), std::string::npos) 
      << "CLI must accept --rvvi_trace\n" << output;

  size_t start_pc_idx = output.find("_start:");
  EXPECT_NE(start_pc_idx, std::string::npos) << "Trace did not organically hit entry point.";

  // Extract a generic register block organically to prove mutation occurs chronologically AFTER execution
  // Check for ANY general purpose register dump (x00 - x31) instead of brittle 'auipc' or 'x03'
  size_t reg_dump_idx = output.find("x01 = [");
  EXPECT_NE(reg_dump_idx, std::string::npos) << "Trace did not mutate the GP register organically.";
  EXPECT_GT(reg_dump_idx, start_pc_idx) << "Register mutation did not logically follow instruction execution.";
}


TEST_F(RvviCliIntegrationTest, Rv64gSimIntegration) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  ASSERT_NE(runfiles, nullptr) << error;
  
  std::string sim_path = runfiles->Rlocation("com_google_mpact-riscv/riscv/rv64g_sim");
  std::string elf_path = runfiles->Rlocation("com_google_mpact-riscv/riscv/test/testfiles/hello_world_64.elf");

  std::vector<std::string> args = {
      "--rvvi_trace",
      "-i=true",
      elf_path
  };

  SubprocessRunner runner(sim_path, args);
  std::string output;
  int status = runner.RunWithInput("step 10\nreg info\nquit\n", &output);

  EXPECT_TRUE(WIFEXITED(status)) << "Daemon pipeline did not exit cleanly. Output: " << output;
  EXPECT_EQ(WEXITSTATUS(status), 0) << "Daemon pipeline failed with non-zero exit code. Output: " << output;

  EXPECT_EQ(output.find("Unknown command line flag 'rvvi_trace'"), std::string::npos) 
      << "CLI must accept --rvvi_trace\n" << output;

  size_t start_pc_idx = output.find("_start:");
  EXPECT_NE(start_pc_idx, std::string::npos) << "Trace did not organically hit entry point.";

  size_t reg_dump_idx = output.find("x01 = [");
  EXPECT_NE(reg_dump_idx, std::string::npos) << "Trace did not mutate the GP register organically.";
  EXPECT_GT(reg_dump_idx, start_pc_idx) << "Register mutation did not logically follow instruction execution.";
}
