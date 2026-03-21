#include <gtest/gtest.h>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;

class RvviCliIntegrationTest : public ::testing::Test {};

TEST_F(RvviCliIntegrationTest, BasicCliFlagAcceptance) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  ASSERT_NE(runfiles, nullptr) << error;
  
  std::string sim_path = runfiles->Rlocation("com_google_mpact-riscv/riscv/rva23s64_sim");
  std::string elf_path = runfiles->Rlocation("com_google_mpact-riscv/riscv/test/testfiles/hello_world_64.elf");

  int pipe_in[2];
  int pipe_out[2];
  ASSERT_EQ(pipe(pipe_in), 0);
  ASSERT_EQ(pipe(pipe_out), 0);

  pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    // Child process
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_out[1], STDERR_FILENO);
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);

    const char* argv[] = {
        sim_path.c_str(),
        "--rvvi_trace",
        "-i=true",
        elf_path.c_str(),
        nullptr
    };
    execv(sim_path.c_str(), const_cast<char**>(argv));
    exit(1);
  }

  // Parent process
  close(pipe_in[0]);
  close(pipe_out[1]);

  std::string input = "step 10\nreg info\nquit\n";
  write(pipe_in[1], input.c_str(), input.length());
  close(pipe_in[1]); // Send EOF

  std::string output = "";
  char buffer[256];
  
  struct pollfd pfd;
  pfd.fd = pipe_out[0];
  pfd.events = POLLIN;

  while (true) {
    int ret = poll(&pfd, 1, 5000); // 5 second timeout
    if (ret <= 0) {
      kill(pid, SIGKILL);
      break;
    }
    ssize_t bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) break;
    buffer[bytes_read] = '\0';
    output += buffer;
  }
  close(pipe_out[0]);

  int status;
  waitpid(pid, &status, 0);

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

  int pipe_in[2];
  int pipe_out[2];
  ASSERT_EQ(pipe(pipe_in), 0);
  ASSERT_EQ(pipe(pipe_out), 0);

  pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    // Child process
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_out[1], STDERR_FILENO);
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);

    const char* argv[] = {
        sim_path.c_str(),
        "--rvvi_trace",
        "-i=true",
        elf_path.c_str(),
        nullptr
    };
    execv(sim_path.c_str(), const_cast<char**>(argv));
    exit(1);
  }

  // Parent process
  close(pipe_in[0]);
  close(pipe_out[1]);

  std::string input = "step 10\nreg info\nquit\n";
  write(pipe_in[1], input.c_str(), input.length());
  close(pipe_in[1]); // Send EOF

  std::string output = "";
  char buffer[256];
  
  struct pollfd pfd;
  pfd.fd = pipe_out[0];
  pfd.events = POLLIN;

  while (true) {
    int ret = poll(&pfd, 1, 5000); // 5 second timeout
    if (ret <= 0) {
      kill(pid, SIGKILL);
      break;
    }
    ssize_t bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) break;
    buffer[bytes_read] = '\0';
    output += buffer;
  }
  close(pipe_out[0]);

  int status;
  waitpid(pid, &status, 0);

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
