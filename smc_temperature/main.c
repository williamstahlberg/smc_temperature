//
//  main.c
//  smc_temperature
//
//  Created by subli on 4/6/20.
//  Copyright © 2020 subli. All rights reserved.
//

#include "smc.h"
#include <stdio.h>

int main(int argc, const char * argv[]) {

	bool decimals = 0;
	if (argc > 1 && strstr(argv[1], "-f"))
		decimals = 1;

	/* TG PRO CROSS-REF NAMES */
	char *names_tg[] = {
		"CPU Core 1",
		"CPU Core 2",
		"CPU Core 3",
		"CPU Core 4",
		"CPU Core 5",
		"CPU Core 6",
		"CPU PECI",
		"CPU Proximity",
		"CPU System Agent Core",
		"",
		"AMD Radeon Pro 5300M",
		"AMD Radeon Pro 5300M Proximity",
		"AMD Radeon Pro 5300M VRAM Proximity",
		"Intel UHD Graphics 630 Core",
		"",
		"Battery",
		"Battery Management Unit",
		"Battery Proximity",
		"",
		"Bottom Case",
		"Case",
		"Top Case",
		"",
		"Airflow Left",
		"Airflow Right",
		"Ambient Virtual",
		"Fin Stack Proximity Left",
		"Fin Stack Proximity Right",
		"Platform Controller Hub",
		"",
		"Memory Proximity",
		"",
		"Trackpad",
		"Trackpad Actuator",
		"",
		"Thunderbolt Left Ports",
		"Thunderbolt Right Ports",
		"",
		"Wireless Proximity",
		"",
		"SSD Left Proximity",
		"SSD Right Proximity",
	};

	/* TG PRO CROSS-REF KEYS */
	char *keys_tg[] = {
		"TC1C",
		"TC2C",
		"TC3C",
		"TC4C",
		"TC5C",
		"TC6C",
		"TCXC",
		"TC0P",
		"TCSA",
		"",
		"TGDD",
		"TG0P",
		"TGVP",
		"TCGC",
		"",
		"TB0T",
		"TB1T",
		"TB2T",
		"",
		"Ts0S",
		"Ts2S",
		"Ts1S",
		"",
		"TaLC",
		"TaRC",
		"TA0V",
		"Th2H",
		"Th1H",
		"TPCD",
		"",
		"TM0P",
		"",
		"Ts0P",
		"Ts1P",
		"",
		"TTLD",
		"TTRD",
		"",
		"TW0P",
		"",
		"TH1a",
		"TH0a",
	};
	
	size_t n = sizeof(keys_tg)/sizeof(keys_tg[0]);
	float *vals = getSmcValues(keys_tg, n);
	
	for (int i = 0; i < n; i++) {
		if (strlen(keys_tg[i]) == 0) {
			printf("\n");
			continue;
		}
		
		if (strstr(names_tg[i], "Battery") && (int)vals[i] >= 40) { // BATTERY WARNING
			printf("\e[093;040m%s: %.*f°C \e[0m\n", names_tg[i], decimals, vals[i]);
		} else if ((int)vals[i] >= 100) { // CPU/GPU WARNING
			printf("\e[105;101m%s: %.*f°C \e[0m\n", names_tg[i], decimals, vals[i]);
		} else if ((int)vals[i] >= 90) { // CPU/GPU WARNING
			printf("\e[001;040m%s: %.*f°C \e[0m\n", names_tg[i], decimals, vals[i]);
		} else if ((int)vals[i] >= 80) { // CPU/GPU WARNING
			printf("\e[093;040m%s: %.*f°C \e[0m\n", names_tg[i], decimals, vals[i]);
		} else if ((int)vals[i] <= 15) { // LOW TEMP WARNING
			printf("\e[036;040m%s: %.*f°C \e[0m\n", names_tg[i], decimals, vals[i]);
		} else {
			printf("%s: %.*f°C\n", names_tg[i], decimals, vals[i]);
		}
	}
	
	return 0;
}
