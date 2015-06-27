/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: ladspa support.
 *
 * Author: Stas Sergeev
 */
#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include <ladspa.h>
#include "dosemu_debug.h"
#include "utils.h"
#include "init.h"
#include "sound/sound.h"

#define ladspa_name "ladspa"
#define ladspa_longname "Sound Effect Processor: ladspa"

/* what to do with sample rate??? */
#define LAD_SRATE 44100

#define MAX_LADSPAS 5
struct lads {
    void *dl_handle;
    LADSPA_Handle handle;
};
static struct lads ladspas[MAX_LADSPAS];
static int num_ladspas;

static int ladspa_setup(char *lname, char *pname, int srate)
{
    void *dl_handle;
    LADSPA_Handle handle;
    LADSPA_Descriptor_Function pfDescriptorFunction;
    const LADSPA_Descriptor *psDescriptor;
    int i;
    struct lads *lad = &ladspas[num_ladspas];
    dl_handle = loadLADSPAPluginLibrary(lname);
    if (!dl_handle) {
	error("ladspa: failed to load %s: %s\n", lname, dlerror());
	return 0;
    }
    pfDescriptorFunction = (LADSPA_Descriptor_Function)
	dlsym(dl_handle, "ladspa_descriptor");
    if (!pfDescriptorFunction) {
	error("ladspa: %s: %s\n", lname, dlerror());
	goto out_err;
    }
    for (i = 0;; i++) {
	psDescriptor = pfDescriptorFunction(i);
	if (!psDescriptor)
	    break;
	if (strcmp(pname, psDescriptor->Label) == 0)
	    break;
    }
    if (!psDescriptor) {
	error("ladspa: failed to find %s\n", pname);
	goto out_err;
    }
    handle = psDescriptor->instantiate(psDescriptor, srate);
    if (!handle) {
	error("ladspa: failed to instantiate %s\n", pname);
	goto out_err;
    }
    for (i = 0; i < psDescriptor->PortCount; i++) {
	if (LADSPA_IS_PORT_INPUT(psDescriptor->PortDescriptors[i]) &&
		LADSPA_IS_PORT_AUDIO(psDescriptor->PortDescriptors[i])) {
	    /* TODO */
	} else if (LADSPA_IS_PORT_OUTPUT(psDescriptor->PortDescriptors[i]) &&
		LADSPA_IS_PORT_AUDIO(psDescriptor->PortDescriptors[i])) {
	    /* TODO */
	} else if (LADSPA_IS_PORT_CONTROL(psDescriptor->PortDescriptors[i])) {
	    /* TODO */
	}
    }
    assert(num_ladspas < MAX_LADSPAS);
    lad->dl_handle = dl_handle;
    lad->handle = handle;
    num_ladspas++;
    return 1;

out_err:
    dlclose(dl_handle);
    return 0;
}

static int ladspa_open(void *arg)
{
    return ladspa_setup("filter.so", "hpf", LAD_SRATE);
}

static int ladspa_cfg(void *arg)
{
    return PCM_CF_ENABLED;
}

static int ladspa_process(sndbuf_t buf[][SNDBUF_CHANS], int nframes,
	int channels, int format, int srate)
{
    if (srate != LAD_SRATE) {
	error("ladspa: wrong sampling rate\n");
	return 0;
    }
    return nframes;
}

static struct pcm_efp ladspa = {
    .name = ladspa_name,
    .longname = ladspa_longname,
    .open = ladspa_open,
    .get_cfg = ladspa_cfg,
    .process = ladspa_process,
    .flags = PCM_F_PASSTHRU,
};

CONSTRUCTOR(static void ladspa_init(void))
{
    pcm_register_efp(&ladspa, NULL);
}
