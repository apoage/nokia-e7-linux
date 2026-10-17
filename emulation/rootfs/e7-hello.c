/*
 * e7-hello.c — ARMv6 toolchain smoke test (task 103)
 *
 * Built by build-buildroot-root.sh with the musl.cc
 * armv6-linux-musleabi cross, in two flavors shipped in the image:
 *   /usr/bin/e7-hello      (static  — the rescue/deploy unit)
 *   /usr/bin/e7-hello-dyn  (dynamic — exercises /lib/ld-musl-arm.so.1)
 * Exercises uname, /proc reads, stdio and soft-float libm.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/utsname.h>

int main(void)
{
	struct utsname u;
	char line[256], cpu[128] = "?";
	FILE *f = fopen("/proc/cpuinfo", "r");

	if (f) {
		while (fgets(line, sizeof line, f))
			if (!strncmp(line, "Processor", 9) ||
			    !strncmp(line, "model name", 10)) {
				sscanf(strchr(line, ':') + 1, " %127[^\n]", cpu);
				break;
			}
		fclose(f);
	}

	if (uname(&u) == 0)
		printf("uname: %s %s %s %s\n",
		       u.sysname, u.nodename, u.release, u.machine);
	printf("cpu:   %s\n", cpu);
	printf("math:  sqrt(2)=%.6f pow(2,10)=%.1f (musl soft-float)\n",
	       sqrt(2.0), pow(2.0, 10.0));
	puts("e7-hello: ARMv6 toolchain OK");
	return 0;
}
