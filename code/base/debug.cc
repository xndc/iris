#include "base/debug.hh"

void InitDebugSystem(int argc, char* argv[]) {
	loguru::g_preamble_header = false;
	loguru::g_preamble_date = false;
	loguru::g_preamble_time = false;
	loguru::g_preamble_uptime = true;
	loguru::g_preamble_verbose = false;
	loguru::g_preamble_pipe = false;
	loguru::init(argc, argv);
}
