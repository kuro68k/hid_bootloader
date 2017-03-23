// opt_output.h

extern bool opt_quiet;
extern bool opt_silent;

#define quiet_printf(...)	do { if (!opt_quiet) printf(__VA_ARGS__); } while(0)
#define silent_printf(...)	do { if (!opt_silent) printf(__VA_ARGS__); } while(0)
