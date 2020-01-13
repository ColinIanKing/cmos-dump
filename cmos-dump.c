/*
 * Copyright (C) 2009-2020 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/capability.h>

static uint64_t get_capabilities(void)
{
	FILE *fp;
	uint64_t flags = 0ULL;
	char buffer[1024];

	fp = fopen("/proc/self/status", "r");
	if (!fp)
		return flags;

	(void)memset(buffer, 0, sizeof(buffer));

	while (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
		if (strstr(buffer, "CapPrm:") != NULL) {
			if (sscanf(buffer, "%*s %" SCNx64, &flags) == 1)
				break;
			flags = 0ULL;
				break;
		}
	}
	(void)fclose(fp);
	return flags;
}

static int port_seek(const int fd, const uint8_t offset)
{
	if (lseek(fd, (off_t)offset, SEEK_SET) < 0) {
		(void)fprintf(stderr, "Cannot seek to port %x" PRIx8 ": errno=%d (%s)\n",
			offset, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static int port_write(const int fd, const uint8_t offset, const uint8_t value)
{
	if (port_seek(fd, offset) < 0)
		return -1;

	if (write(fd, &value, sizeof(value)) != sizeof(value)) {
		(void)fprintf(stderr, "Cannot write value 0x%" PRIx8
			" to /dev/port offset 0x%" PRIx8 ": errno=%d (%s)\n",
			value, offset, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static int port_read(const int fd, const uint8_t offset, uint8_t *const value)
{
	if (port_seek(fd, offset) < 0)
		return -1;
	if (read(fd, value, sizeof(*value)) != sizeof(*value)) {
		fprintf(stderr, "Cannot read value "
			" from /dev/port offset 0x%" PRIx8 ": errno=%d (%s)\n",
			offset, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static uint8_t cmos_read(const uint8_t offset)
{
	uint8_t value;
	int fd;

	fd = open("/dev/port", O_RDWR | O_NDELAY);
	if (fd < 0) {
		(void)fprintf(stderr, "Cannot open /dev/port: errno=%d (%s)\n",
			errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (port_write(fd, 0x70, offset) < 0) {
		(void)close(fd);
		exit(EXIT_FAILURE);
	}

	/* Port delay */
	if (port_write(fd, 0x80, 0x00) < 0) {
		(void)close(fd);
		exit(EXIT_FAILURE);
	}

	if (port_read(fd, 0x71, &value) < 0) {
		(void)close(fd);
		exit(EXIT_FAILURE);
	}

	(void)close(fd);
	return value;
}

int main(int argc, char **argv)
{
	uint8_t i;
	uint16_t tmp;

	static const char *cmos_shutdown_status[] = {
		"Power on or soft reset",
		"Memory size pass",
		"Memory test pass",
		"Memory test fail",

		"INT 19h reboot",		
		"Flush keyboard and jmp via 40h:67h",
		"Protected mode tests pass",
		"Protected mode tests fail",

		"Used by POST during protected-mode RAM test",
		"Int 15h (block move)",
		"Jmp via 40h:67h",
		"Used by 80386",
	};

	static const char *floppy_disk[] = {
		"None",
		"360KB 5.25\" Drive",
		"1.2MB 5.25\" Drive",
		"720KB 3.5\" Drive",
		"1.44MB 3.5\" Drive",
		"2.88MB 3.5\" Drive",
		"Unknown",
		"Unknown"
	};

	static const char * const hard_disk[] = {
		"None",
		"Type 1",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Type 14",
		"Type 16-47"
	};

	static const char * const primary_display[] = {
		"BIOS selected",
		"CGA 40 column",
		"CGA 80 column",
		"Monochrome"
	};


	static const char * const divider[8] = {
		"4.194 MHz",
		"1.049 MHz",
		"32.768 KHz (default)",
		"unknown",
		"test mode",
		"test mode",
		"reset / disable",
		"reset / disable",
	};

	static const char * const rate_selection[16] = {
		"none",
		"3.90625 millseconds",
		"7.8215 milliseconds",
		"122.070 microseconds",
		"244.141 microseconds",
		"488.281 microseconds",
		"976.562 microseconds (default)",
		"1.953125 milliseconds",
		"3.90625 milliseconds",
		"7.8215 milliseconds",
		"15.625 milliseconds",
		"31.25 milliseconds",
		"62.5 milliseconds",
		"125 milliseconds",
		"250 milliseconds",
		"500 milliseconds"
	};

	uint8_t data[0x80];

	(void)argc;

	if ((get_capabilities() & CAP_SYS_RAWIO) == 0) {
		(void)fprintf(stderr, "Must have CAP_SYS_RAWIO to run %s "
			"(hint: run as root)\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < sizeof(data); i++)
		data[i] = cmos_read(i);

	(void)printf("CMOS Memory Dump:\n");
	for (i = 0; i < sizeof(data); i+= 8) {
		(void)printf("  %2.2x: %2.2x %2.2x %2.2x %2.2x  %2.2x %2.2x %2.2x %2.2x\n",
			i,
			data[i], data[i+1], data[i+2], data[i+3],
			data[i+4], data[i+5], data[i+6], data[i+7]);
	}
	(void)printf("\n");

	(void)printf("RTC Current Time: (CMOS 0x00..0x09)\n");
	(void)printf("  RTC seconds:            %2.2x\n", data[0]);
	(void)printf("  RTC minutes:            %2.2x\n", data[2]);
	(void)printf("  RTC hours:              %2.2x\n", data[4]);
	(void)printf("  RTC day of week:        %2.2x\n", data[6]);
	(void)printf("  RTC date day:           %2.2x\n", data[7]);
	(void)printf("  RTC date month:         %2.2x\n", data[8]);
	(void)printf("  RTC date year:          %2.2x\n", data[9]);
	(void)printf("\n");

	(void)printf("RTC Alarm:\n");
	(void)printf("  RTC seconds:            %2.2x\n", data[1]);
	(void)printf("  RTC minutes:            %2.2x\n", data[3]);
	(void)printf("  RTC hours:              %2.2x\n", data[5]);
	(void)printf("\n");

	(void)printf("Status Register A: (CMOS 0x0a): 0x%2.2x\n", data[10]);
	(void)printf("  Rate freq:              %1.1x (%s)\n",
		data[10] & 0xf, rate_selection[data[10] & 0xf]);
	(void)printf("  Timer freq divider:     %1.1x (%s)\n",
		(data[10] >> 4) & 0x7, divider[(data[10] >> 4) & 0x7]);
	(void)printf("  Update in progress:     %1.1x\n", (data[10] >> 7) & 1);
	(void)printf("\n");

	(void)printf("Status Register B: (CMOS 0x0b): 0x%2.2x\n", data[11]);
	(void)printf("  Daylight savings:       %1.1x (%s)\n",
		(data[11] >> 1) & 1, (data[11] >> 1) & 1 ? "Enabled" : "Disabled");
	(void)printf("  24 Hour Clock:          %1.1x (%s)\n", (data[11] >> 1) & 1,
		(data[11] >> 1) & 1 ? "12 Hour" : "24 Hour");
	(void)printf("  Data Mode (DM):          %1.1x (%s)\n", (data[11] >> 2) & 1,
		(data[11] >> 2) & 1 ? "Binary" : "BCD");
	(void)printf("  Square Wave:            %1.1x (%s)\n", (data[11] >> 3) & 1,
		(data[11] >> 2) & 1 ? "Enabled" : "Disabled");
	(void)printf("  Update ended IRQ:       %1.1x (%s)\n",
		(data[11] >> 4) & 1, (data[11] >> 3) & 1 ? "Enabled" : "Disabled");
	(void)printf("  Alarm IRQ:              %1.1x (%s)\n",
		(data[11] >> 5) & 1, (data[11] >> 5) & 1 ? "Enabled" : "Disabled");
	(void)printf("  Periodic IRQ:           %1.1x (%s)\n",
		(data[11] >> 6) & 1, (data[11] >> 6) & 1 ? "Enabled" : "Disabled");
	(void)printf("  Clock update cycle:     %1.1x (%s)\n", (data[11] >> 7) & 1,
		(data[11] >> 7) & 1 ? "Abort update in progress" : "Update normally");
	(void)printf("\n");

	(void)printf("Status Register C: (CMOS 0x0c): 0x%2.2x\n", data[12]);
	(void)printf("  UF flag:                0x%1.1x\n", (data[12] >> 4) & 1);
	(void)printf("  AF flag:                0x%1.1x\n", (data[12] >> 5) & 1);
	(void)printf("  PF flag:                0x%1.1x\n", (data[12] >> 6) & 1);
	(void)printf("  IRQF flag:              0x%1.1x\n", (data[12] >> 7) & 1);
	(void)printf("\n");

	(void)printf("Status Register D: (CMOS 0x0d): 0x%2.2x\n", data[13]);
	(void)printf("  Valid CMOS RAM flag:    0x%1.1x (%s)\n",
		(data[13] >> 7) & 1, (data[13] >> 7) & 1 ? "Battery Good": "Battery Dead");
	(void)printf("\n");

	(void)printf("Diagnostic Status: (CMOS 0x0e): 0x%2.2x\n", data[14]);
	(void)printf("  CMOS time status:       0x%1.1x (%s)\n",
		(data[14] >> 2) & 1, (data[14] >> 2) & 1 ? "Invalid": "Valid");
	(void)printf("  Fixed disk init:        0x%1.1x (%s)\n",
		(data[14] >> 3) & 1, (data[14] >> 3) & 1 ? "Bad": "Good");
	(void)printf("  Memory size check:      0x%1.1x (%s)\n",
		(data[14] >> 4) & 1, (data[14] >> 4) & 1 ? "Bad": "Good");
	(void)printf("  Config info status:     0x%1.1x (%s)\n",
		(data[14] >> 5) & 1, (data[14] >> 5) & 1 ? "Invalid": "Valid");
	(void)printf("  CMOS checksum status:   0x%1.1x (%s)\n",
		(data[14] >> 6) & 1, (data[14] >> 6) & 1 ? "Bad": "Good");
	(void)printf("  CMOS power loss:        0x%1.1x (%s)\n",
		(data[14] >> 7) & 1, (data[14] >> 7) & 1 ? "Lost power": "Not lost power");
	(void)printf("\n");

	(void)printf("CMOS Shutdown Status: (CMOS 0x0f): 0x%2.2x (%s)\n\n", data[15],
		data[15] < 0xb ? cmos_shutdown_status[data[15]] : "Perform power-on reset");

	(void)printf("Floppy Disk Type: (CMOS 0x10): 0x%2.2x\n", data[16]);
	(void)printf("  Drive 0: %s\n", floppy_disk[((data[16] >> 4) & 0xf)]);
	(void)printf("  Drive 1: %s\n", floppy_disk[((data[16] >> 0) & 0xf)]);
	(void)printf("\n");

	(void)printf("Hard Disk Type: (CMOS 0x12, Obsolete): 0x%2.2x\n", data[18]);
	(void)printf("  Drive 0: %s\n", hard_disk[((data[18] >> 4) & 0xf)]);
	(void)printf("  Drive 1: %s\n", hard_disk[((data[18] >> 0) & 0xf)]);
	(void)printf("\n");

	(void)printf("Installed H/W: (CMOS 0x14): 0x%2.2x\n", data[20]);
	(void)printf("  Maths Coprocessor:      0x%1.1x (%s)\n",
		(data[20] >> 1) & 1, (data[20] >> 1) & 1 ? "Installed": "Not Installed");
	(void)printf("  Keyboard:               0x%1.1x (%s)\n",
		(data[20] >> 2) & 1, (data[20] >> 2) & 1 ? "Installed": "Not Installed");
	(void)printf("  Display Adaptor:        0x%1.1x (%s)\n",
		(data[20] >> 3) & 1, (data[20] >> 3) & 1 ? "Installed": "Not Installed");
	(void)printf("  Primary Display:        0x%1.1x (%s)\n",
		(data[20] >> 4) & 3, primary_display[(data[20] >> 4) & 3]);
	(void)printf("  Floppy Drives:          0x%2.2x (%d drives)\n",
		(data[20] >> 6) & 3, ((data[20] >> 6) & 3) + 1);
	(void)printf("\n");

	tmp = ((data[22] << 8) | (data[21]));
	(void)printf("Base Mem: (CMOS 0x16):\n");
	(void)printf("  0x%2.2x%2.2x (%" PRIu16 "K)\n", data[22], data[21], tmp);
	(void)printf("\n");

	tmp = ((data[24] << 8) | (data[25]));
	(void)printf("Extended Mem: (CMOS 0x18):\n");
	(void)printf("  0x%2.2x%2.2x (%" PRIu16 "K) %s\n",
		data[24], data[23], tmp, tmp > (16 * 1024) ? "[untrustworthy]" : "");
	(void)printf("\n");

	(void)printf("Hard Disk Extended Types (CMOS 0x19, 0x1a):\n");
	(void)printf("  Hard Disk 0:            0x%2.2x\n", data[25]);
	(void)printf("  Hard Disk 1:            0x%2.2x\n", data[26]);
	(void)printf("\n");
	
	(void)printf("CMOS Checksum:(CMOS 0x2e):0x%2.2x%2.2x\n", data[47], data[46]);
	(void)printf("\n");

	(void)printf("Extended Mem: (CMOS 0x30):0x%2.2x%2.2x\n", data[49], data[48]);
	(void)printf("\n");

	(void)printf("Century Date: (CMOS 0x32):%2.2x\n", data[50]);
	(void)printf("\n");
	(void)printf("POST Information Flag (CMOS 0x33):\n");
	(void)printf("  POST cache test:        0x%1.1x %s\n",
		(data[51] >> 0) & 1, (data[51] >> 0) & 1 ? "Failed" : "Passed");
	(void)printf("  BIOS size:              0x%1.1x %s\n",
		(data[51] >> 7) & 1, (data[51] >> 7) & 1 ? "128KB" : "64KB");
	(void)printf("\n");

	exit(EXIT_SUCCESS);
}
