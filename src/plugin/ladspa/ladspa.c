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
 *
 * Linux Audio Developers Simple Plugin API...
 * What did they mean? Simple API for plugins, or horrible API
 * for simple plugins? I guess the later.
 * Well, maybe for Linux Audio Developers this API is simple, but
 * for dosemu developer its just a royal pita.
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

struct lp {
    const char *plugin;
    const char *name;
    const enum EfpType type;
};
static struct lp plugins[] = {
    { "filter.so", "hpf", EFP_HPF },
    { NULL, NULL, EFP_NONE }
};

#define MAX_LADSPAS 5
struct lads {
    struct lp *link;
    void *dl_handle;
    const LADSPA_Descriptor *descriptor;
    const LADSPA_PortDescriptor *in;
    const LADSPA_PortDescriptor *out;
    const LADSPA_PortDescriptor *ctrl;
};
static struct lads ladspas[MAX_LADSPAS];
static int num_ladspas;

#define MAX_HANDLES 5
struct la_h {
    LADSPA_Handle handle;
    struct lads *lad;
    LADSPA_Data control;
    int srate;
};
static struct la_h handles[MAX_HANDLES];
static int num_handles;

static struct lads *find_lad(void *arg)
{
    struct lp *plu;
    int i;
    for (i = 0; i < MAX_LADSPAS && (plu = ladspas[i].link); i++) {
	if (plu == arg)
	    return &ladspas[i];
    }
    return NULL;
}

static int ladspa_setup(int srate, float control, void *arg)
{
    LADSPA_Handle handle;
    struct la_h *lah = &handles[num_handles];
    struct lads *lad = find_lad(arg);
    if (!lad) {
	error("ladspa: setup failed\n");
	return -1;
    }
    handle = lad->descriptor->instantiate(lad->descriptor, srate);
    if (!handle) {
	error("ladspa: failed to instantiate %s\n", lad->link->name);
	return -1;
    }
    assert(num_handles < MAX_HANDLES);
    lah->handle = handle;
    lah->lad = lad;
    lah->control = control;
    lah->srate = srate;
    return num_handles++;
}

static int ladspa_open(void *arg)
{
    struct lp *plu = arg;
    void *dl_handle;
    LADSPA_Descriptor_Function pfDescriptorFunction;
    const LADSPA_Descriptor *psDescriptor;
    int i;
    struct lads *lad = &ladspas[num_ladspas];
    assert(plu->plugin && plu->name);
    dl_handle = loadLADSPAPluginLibrary(plu->plugin);
    if (!dl_handle) {
	error("ladspa: failed to load %s: %s\n", plu->plugin, dlerror());
	return 0;
    }
    pfDescriptorFunction = (LADSPA_Descriptor_Function)
	dlsym(dl_handle, "ladspa_descriptor");
    if (!pfDescriptorFunction) {
	error("ladspa: %s: %s\n", plu->plugin, dlerror());
	goto out_err;
    }
    for (i = 0;; i++) {
	psDescriptor = pfDescriptorFunction(i);
	if (!psDescriptor)
	    break;
	if (strcmp(plu->name, psDescriptor->Label) == 0)
	    break;
    }
    if (!psDescriptor) {
	error("ladspa: failed to find %s\n", plu->name);
	goto out_err;
    }

    assert(num_ladspas < MAX_LADSPAS);
    for (i = 0; i < psDescriptor->PortCount; i++) {
	if (LADSPA_IS_PORT_INPUT(psDescriptor->PortDescriptors[i]) &&
		LADSPA_IS_PORT_AUDIO(psDescriptor->PortDescriptors[i]))
	    lad->in = &psDescriptor->PortDescriptors[i];
	else if (LADSPA_IS_PORT_OUTPUT(psDescriptor->PortDescriptors[i]) &&
		LADSPA_IS_PORT_AUDIO(psDescriptor->PortDescriptors[i]))
	    lad->out = &psDescriptor->PortDescriptors[i];
	else if (LADSPA_IS_PORT_CONTROL(psDescriptor->PortDescriptors[i]))
	    lad->ctrl = &psDescriptor->PortDescriptors[i];
    }
    lad->descriptor = psDescriptor;
    lad->dl_handle = dl_handle;
    lad->link = arg;
    num_ladspas++;
    return 1;

out_err:
    dlclose(dl_handle);
    return 0;
}

static void ladspa_close(void *arg)
{
    struct lads *lad = find_lad(arg);
    dlclose(lad->dl_handle);
}

static int ladspa_cfg(void *arg)
{
    return PCM_CF_ENABLED;
}

static int ladspa_process(int handle, sndbuf_t buf[][SNDBUF_CHANS],
	int nframes, int channels, int format, int srate)
{
    struct la_h *lah = &handles[handle];
    if (srate != lah->srate) {
	error("ladspa: wrong sampling rate\n");
	return 0;
    }
    return nframes;
}

static struct pcm_efp ladspa = {
    .name = ladspa_name,
    .longname = ladspa_longname,
    .open = ladspa_open,
    .close = ladspa_close,
    .setup = ladspa_setup,
    .get_cfg = ladspa_cfg,
    .process = ladspa_process,
    .flags = PCM_F_PASSTHRU,
};

CONSTRUCTOR(static void ladspa_init(void))
{
    struct lp *p;
    for (p = plugins; p->plugin; p++)
	pcm_register_efp(&ladspa, p->type, p);
}
