#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

struct scsi_device_info {
  int sgminor;
  int hostId;
  int channel;
  int target;
  int lun;
  int devtype;
  int ansirev;
  char *vendor;
  char *model;
  char *modelrev;
};

static char *scsi_device_types[] = {
  "Direct-Access",	/* 0 */
  "Sequential-Access",	/* 1 */
  "Printer",		/* 2 */
  "Processor",		/* 3 */
  "WORM",		/* 4 */
  "CD-ROM",		/* 5 */
  "Scanner",		/* 6 */
  "Optical Device",	/* 7 */
  "Medium Changer",	/* 8 */
  "Communications",	/* 9 */
  ""
};


static char *scsiprocfile = "/proc/scsi/scsi";

static char *strbetween(char *s, char **next, char *pre, char *post)
{
   char *p, *p2 = 0;
   p = strstr(s, pre);
   if (!p) return 0;
   p += strlen(pre);
   while (p[0] && isspace(p[0])) p++;
   if (post[0]) p2 = strstr(p, post);
   if (!p2) p2 = p + strlen(p);
   *next = p2;
   while ((p2 != p) && isspace(p2[-1])) *(--p2) = 0;
   return p;
}

static struct scsi_device_info *scan_scsi_proc_info(void) {
  struct scsi_device_info *devs;
  int maxdevs = 32;
  int dev = 0;
  int i;
  FILE *f;
  char buf[1024];
  char *p, *s;
  static char* attached = "Attached devices:";

  f = fopen(scsiprocfile, "r");
  if (!f) return 0;

  devs = malloc(sizeof(struct scsi_device_info) * (maxdevs+1));
  if (!devs) {
    fclose(f);
    return 0;
  }
  while (!feof(f)) {
    if (!fgets(buf, sizeof(buf), f)) break;
    if (!strncmp(buf, attached, sizeof(attached))) {
      if (strstr(buf, "none")) break;
      fgets(buf, sizeof(buf), f);
    }
    devs[dev].sgminor = dev;
    p = buf;
    s = strbetween(p,&p, "Host:", "Channel:");
    devs[dev].hostId = s[strlen(s)-1] -'0';
    s = strbetween(p,&p, "Channel:", "Id:");
    devs[dev].channel = strtoul(s,0,10);
    s = strbetween(p,&p, "Id:", "Lun:");
    devs[dev].target = strtoul(s,0,10);
    s = strbetween(p,&p, "Lun:", "");
    devs[dev].lun = strtoul(s,0,10);
    fgets(buf, sizeof(buf), f);
    p = buf;
    s = strbetween(p,&p, "Vendor:", "Model:");
    devs[dev].vendor = strdup(s);
    s = strbetween(p,&p, "Model:", "Rev:");
    devs[dev].model = strdup(s);
    s = strbetween(p,&p, "Rev:", "");
    devs[dev].modelrev = strdup(s);
    fgets(buf, sizeof(buf), f);
    p = buf;
    s = strbetween(p,&p, "Type:", "ANSI SCSI revision:");
    for (i=0; scsi_device_types[i]; i++) {
      if (!strcmp(s, scsi_device_types[i])) break;
    }
    if (scsi_device_types[i]) devs[dev].devtype = i;
    else devs[dev].devtype = -1;
    s = strbetween(p,&p, "ANSI SCSI revision:", "");
    if (s[2]) devs[dev].ansirev = 1;
    else devs[dev].ansirev = strtoul(s,0,16);
    dev++;
  }
  fclose(f);
  if (dev) {
    devs = realloc(devs, sizeof(struct scsi_device_info) * (dev+1));
    memset(&devs[dev], 0, sizeof(struct scsi_device_info));
    return devs;
  }
  free(devs);
  return 0;
}

int idcount[16];

static void check_ids(struct scsi_device_info *devs)
{
  int i;
  memset(idcount, 0, sizeof(idcount));
  if (!devs) return;
  for (i=0; devs[i].vendor; i++) {
    idcount[devs[i].target &15]++;
  }
}

static void print_scsi_devices(struct scsi_device_info *devs)
{
  int i;
  char *s;
  if (!devs) return;
  for (i=0; devs[i].vendor; i++) {
    if (devs[i].devtype == -1) s = "Unknown";
    else s = scsi_device_types[devs[i].devtype];
    printf("sg%d scsi%d ch%d ID%d Lun%d ansi%d %s(%d) %s %s %s\n", devs[i].sgminor,
      devs[i].hostId, devs[i].channel, devs[i].target, devs[i].lun,
      devs[i].ansirev, s, devs[i].devtype,
      devs[i].vendor, devs[i].model, devs[i].modelrev
    );
    printf("    $_aspi = \"sg%d:%s:%d\" (or \"%d/%d/%d/%d:%s:%d\")%s\n",
      devs[i].sgminor, s, devs[i].target,
      devs[i].hostId, devs[i].channel, devs[i].target, devs[i].lun, s, devs[i].target,
      idcount[devs[i].target &15] > 1 ? " <== multiple IDs" : ""
   );
  }
}  

int main(int argc, char **argv)
{
  struct scsi_device_info *devs;
  if (argv[1]) scsiprocfile = argv[1];

  devs = scan_scsi_proc_info();
  check_ids(devs);
  print_scsi_devices(devs);
  return 0;
}

