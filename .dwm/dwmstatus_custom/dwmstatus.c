/* dwmstatus */
/* icons status wi-fi, BAT, date, time */
#define _DEFAULT_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>

#define STATUSSZ 256
#define STRSZ 32

typedef enum {
  DOWN,
  UP
} status_t;

typedef enum {
  NONE,
  UNKNOWN,
  DISCHARGING,
  CHARGING
} battery_status_t;

typedef struct {
  int energy_now;
  int energy_full;
  battery_status_t status;
} battery_info_t;

static int freadint(const char *filename){
  int ret;
  FILE *file = fopen(filename, "r");
  if(!file) {
    fprintf(stderr, "Error opening %s.\n", filename);
    return 0;
  }
  fscanf(file, "%d", &ret);
  fclose(file);
  return ret;
}

static battery_status_t fread_battery_status(const char *filename){
  char buf[16];
  FILE *file = fopen(filename, "r");
  if(!file) {
    fprintf(stderr, "Error opening %s.\n", filename);
    return NONE;
  }
  fgets(buf, 16, file);
  fclose(file);
  if(!strcmp(buf, "Charging\n")) {
    return CHARGING;
  } else if(!strcmp(buf, "Discharging\n")) {
    return DISCHARGING;
  } else {
    return UNKNOWN;
  }
}

static status_t fread_wifi_status(const char *filename){
  char buf[16];
  FILE *file = fopen(filename, "r");
  if(!file) {
    fprintf(stderr, "Error opening %s.\n", filename);
    return NONE;
  }
  fgets(buf, 16, file);
  fclose(file);
  if(!strcmp(buf, "up\n")) {
    return UP;
  } else {
    return DOWN;
  }
}

static battery_info_t battery_get_info() {
  battery_info_t info;

  info.energy_now = freadint("/sys/class/power_supply/BAT0/energy_now");
  info.energy_full = freadint("/sys/class/power_supply/BAT0/energy_full");
  info.status = fread_battery_status("/sys/class/power_supply/BAT0/status");

  return info;
}

static int battery_get_percent(battery_info_t bat) {
  if(bat.energy_full) {
    return lrint(bat.energy_now * 100.0 / bat.energy_full);
  } else {
    return 0;
  }
}

static int fetch_wifi_info(char *wifi_str) {
  int len = 0;
  status_t wifi = fread_wifi_status("/sys/class/net/wlp4s0/operstate");

  if(wifi == UP) {
    len += snprintf(wifi_str, STRSZ, "\ue220");
  } else {
    len += snprintf(wifi_str, STRSZ, "\ue222");
  }

  return len;
}

static int fetch_battery_info(char *bat_str) {
  int len = 0;
  battery_info_t bat = battery_get_info();

  int bat_percent = battery_get_percent(bat);

  switch(bat.status) {
    case UNKNOWN :
    case CHARGING :
      len += snprintf(bat_str, STRSZ, "\ue23a");
      break;
    case DISCHARGING :
      	if(bat_percent > 56) {
        len += snprintf(bat_str, STRSZ, "\ue238");
      } else if(bat_percent > 24) {
        len += snprintf(bat_str, STRSZ, "\ue237");
      } else if(bat_percent > 10) {
        len += snprintf(bat_str, STRSZ, "\ue239");
      } else if(bat_percent > 2) {
	len += snprintf(bat_str, STRSZ, "\ue0a7");
      }
      break;
    default:
      len += snprintf(bat_str, STRSZ, "\ue238");
  }

  /* len += snprintf(bat_str + len, STRSZ - len, "", bat_percent); */

  return len;
}

static int fetch_current_time(char *time_str) {
  time_t current_time = time(NULL);
  struct tm *current_tm = localtime(&current_time);

  return strftime(time_str, STRSZ, "%a %d, %b %H:%M %p \ue0a5 ", current_tm);
}


// VPN Status
#include <net/if.h>
static status_t fetch_vpn_status (void) {
    const char * vpn_ifaces[] = { "tap", "tun", NULL };
    int nifaces;
    status_t status = DOWN;
    for (nifaces = 0; vpn_ifaces[nifaces]; nifaces++);

    struct if_nameindex * iface_indexes,
                        * iface_auxiliar;

    iface_indexes = if_nameindex();
    if ( iface_indexes) {
        for (iface_auxiliar = iface_indexes;
                iface_auxiliar->if_index || iface_auxiliar->if_name;
                iface_auxiliar++) {
            for (int i = 0; i < nifaces; i++) {
                if (strstr (iface_auxiliar->if_name, vpn_ifaces[i])) { status = UP; break; }
            }
        }
    }
    if_freenameindex(iface_indexes);
    return status;
}

static void fetch_vpn_info (char * vpnstatus) {
    memset(vpnstatus, 0x00, STRSZ);

    snprintf (vpnstatus, STRSZ, (fetch_vpn_status() == UP ? "\ue1c2" : "\ue1bc"));
}

#include <stdbool.h>

#ifndef OUTPUT_SIZE
#define OUTPUT_SIZE     1024
#endif /* end of include guard: OUTPUT_SIZE */

#ifndef PRINT_VOLUME
#define PRINT_VOLUME(var,code)    snprintf((var), 32, "%s", (code));
#endif /* end of include guard: PRINT_VOLUME */

static void display_volume(char * volumestr, const int volume) {
    if (volume == 0) {
        PRINT_VOLUME(volumestr, "\ue04f");
        return;
    }
    if (volume <= 25) {
        PRINT_VOLUME(volumestr, "\ue0e3");
        return;
    }
    if (volume <= 50) {
        PRINT_VOLUME(volumestr, "\ue0e2");
        return;
    }
    if (volume <= 75) {
        PRINT_VOLUME(volumestr, "\ue0e1");
        return;
    }
    if (volume <= 100) {
        PRINT_VOLUME(volumestr, "\ue0e0");
        return;
    }
    if (volume > 100) {
        PRINT_VOLUME(volumestr, "\ue05d");
        return;
    }

}

int crop_volume_level(const char * output_line) {
    char * end = NULL;

    end = strstr(output_line, "%");
    if (!end)
        return -1;
    end -= 1;
    *end = 0x00;

    while (*end != 'm') {
        end--;
    }
    end++;
    return atoi(end);
}

void fetch_volume_level(char * volume_status) {
    bool pulseaudio_command;
    const char * mixer_command = "/usr/bin/pulseaudio-ctl";
    char command_output[OUTPUT_SIZE] = { 0 };
    memset (command_output, 0x00, 1024);
    FILE * fp;

command_test:
    if (pulseaudio_command != 0)
        return;
    fp = popen(mixer_command, "r");
    if (!fp) {
        fprintf (stderr, "Failed to run command [!!]");
        pulseaudio_command = -1;
        goto command_test;
    }
    char * output_line = NULL;
    while(fgets (command_output, OUTPUT_SIZE, fp)) {
        if ((output_line = strstr(command_output, "Volume level")) != NULL) {
            output_line[strlen(output_line)-1] = 0x00;
            /* printf ("%s\n", output_line); */
            display_volume(volume_status, crop_volume_level(output_line));
            return;
        }
    }

    /* return crop_volume_level(command_output); */
}


int main(void) {
  static Display *dpy;
  char status[STATUSSZ];
  char battery[STRSZ];
  char wifi[STRSZ];
  char time[STRSZ];
  char vpnstatus[STRSZ];
  char volume_status[STRSZ];

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "dwmstatus: cannot open display.\n");
    return 1;
  }

  for (;;sleep(5)) {
    fetch_wifi_info(wifi);
    fetch_battery_info(battery);
    fetch_current_time(time);
    fetch_vpn_info (vpnstatus);
    fetch_volume_level(volume_status);

    snprintf(status, STATUSSZ, "%s %s %s %s %s", vpnstatus, wifi, battery, volume_status, time);

    XStoreName(dpy, DefaultRootWindow(dpy), status);
    XFlush(dpy);
  }

  XCloseDisplay(dpy);

  return 0;
}
/* the end */
