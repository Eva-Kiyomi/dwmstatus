/* Nyam Nyam :) */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>

#include <X11/Xlib.h>

char *tzutc = "UTC";

static Display *dpy;

char *smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void settz(const char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *mktimes(const char *fmt, const char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *readfile(const char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	return smprintf("%s", line);
}

char *getbattery(const char *base)
{
	char *co;

	co = readfile(base, "capacity");
	
	if(co == NULL)
	{
		free(co);
		return smprintf("");	
	}
	
	int i = 0;
    while (i < 4)
    {	
        if (!isdigit(co[i]))
        {
            co[i] = '\0';
            break;
        }
        i++;
    }

    char *result = strdup(co);
    free(co);
    return result;

}

char *gettemperature(const char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *execscript(const char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;

	memset(retval, 0, sizeof(retval));

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");
    
	size_t len = strlen(retval);
	if (len > 0 && retval[len - 1] == '\n') {
    		retval[len - 1] = '\0';
	}

	return smprintf("%s", retval);
}

char* get_battery_dishcharge_time()
{
    char* result = NULL;
    if(strcmp(execscript("cat /sys/class/power_supply/BAT1/status"),"Discharging") == 0)                                                        
    {
        result = (char*)malloc(30 * sizeof(char));
        sprintf(result, "(%s)", execscript("upower -i $(upower -e | grep 'battery') | grep 'time to empty' | awk '{print $4, $5}'"));
    }
    
    return result;
}

/********** CONFIGURATION **********/
const char *BATTERY_PATH        = "/sys/class/power_supply/BAT1";
const char *THERMAL_ZONE_PATH   = "/sys/devices/virtual/thermal/thermal_zone6";
const char *VOLUME_CMD          = "pactl get-sink-volume @DEFAULT_SINK@ | grep -Po '\\d+(?=%)' | head -n 1";
const char *KB_LAYOUT_CMD       = "xkblayout-state print '%n'";
const char *TIME_FORMAT         = "%A %d.%m.%Y %T";
const char *TIME_ZONE           = "Europe/Kyiv";
/**********************************/

/********** Functions block for getting all statuses **********/
char *get_battery_status() {
    char *bat = getbattery(BATTERY_PATH);
    char *remain = get_battery_dishcharge_time();
    char *status = smprintf("󱊣%s%%%s", bat, remain ? remain : "");
    free(bat);
    free(remain);
    return status;
}

char *get_volume_status() {
    char *vol = execscript(VOLUME_CMD);
    char *status = smprintf(" %s%%", vol);
    free(vol);
    return status;
}

char *get_temp_status() {
    char *t0 = gettemperature(THERMAL_ZONE_PATH, "temp");
    char *status = smprintf(" %s", t0);
    free(t0);
    return status;
}

char *get_kb_status() {
    char *kb = execscript(KB_LAYOUT_CMD);
    char *status = smprintf("󰌌 %s", kb);
    free(kb);
    return status;
}

char *get_time_status() {
    return mktimes(TIME_FORMAT, TIME_ZONE);
}


int main(void)
{
	if (!(dpy = XOpenDisplay(NULL))) 
    {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) 
    {
	    char *batStatus = get_battery_status();
        char *volStatus = get_volume_status();
        char *tempStatus = get_temp_status();
        char *kbStatus = get_kb_status();
        char *timeStatus = get_time_status();

        char *myStatus = smprintf("%s | %s | %s | %s | %s", tempStatus, batStatus, volStatus, kbStatus, timeStatus);
        setstatus(myStatus);	

	    free(batStatus);
        free(volStatus);
        free(tempStatus);
        free(kbStatus);
        free(timeStatus);
        free(myStatus);
    }

	XCloseDisplay(dpy);

	return 0;
}
