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
#include <math.h>
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
    int in;
    int out;
    int ctrl;
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
    lad->descriptor->connect_port(handle, lad->ctrl, &lah->control);

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
	    lad->in = i;
	else if (LADSPA_IS_PORT_OUTPUT(psDescriptor->PortDescriptors[i]) &&
		LADSPA_IS_PORT_AUDIO(psDescriptor->PortDescriptors[i]))
	    lad->out = i;
	else if (LADSPA_IS_PORT_CONTROL(psDescriptor->PortDescriptors[i]))
	    lad->ctrl = i;
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

static void ladspa_start(int h)
{
    struct la_h *lah = &handles[h];
    struct lads *lad = lah->lad;
    LADSPA_Handle handle = lah->handle;

    if (lad->descriptor->activate)
	lad->descriptor->activate(handle);
}

static void ladspa_stop(int h)
{
    struct la_h *lah = &handles[h];
    struct lads *lad = lah->lad;
    LADSPA_Handle handle = lah->handle;

    if (lad->descriptor->deactivate)
	lad->descriptor->deactivate(handle);
}

static int ladspa_cfg(void *arg)
{
    return PCM_CF_ENABLED;
}

#define UC2F(v) ((*(unsigned char *)(v) - 128) / 128.0)
#define SC2F(v) ((*(signed char *)(v)) / 128.0)
#define US2F(v) ((*(unsigned short *)(v) - 32768) / 32768.0)
#define SS2F(v) ((*(unsigned short *)(v)) / 32768.0)
#define F2UC(v) ((unsigned char)(lrintf((v) * 127) + 128))
#define F2SC(v) ((signed char)(lrintf((v) * 127)))
#define F2US(v) ((unsigned short)(lrintf((v) * 32767) + 32768))
#define F2SS(v) ((unsigned short)(lrintf((v) * 32767)))

static float sample_to_float(void *data, int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
	return UC2F(data);
    case PCM_FORMAT_S8:
	return SC2F(data);
    case PCM_FORMAT_U16_LE:
	return US2F(data);
    case PCM_FORMAT_S16_LE:
	return SS2F(data);
    default:
	error("PCM: format %i is not supported\n", format);
	return 0;
    }
}

static void float_to_sample(float sample, sndbuf_t *buf, int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
	*buf = F2UC(sample);
	break;
    case PCM_FORMAT_S8:
	*buf = F2SC(sample);
	break;
    case PCM_FORMAT_U16_LE:
	*buf = F2US(sample);
	break;
    case PCM_FORMAT_S16_LE:
	*buf = F2SS(sample);
	break;
    default:
	error("PCM: format1 %i is not supported\n", format);
    }
}

static int ladspa_process(int h, sndbuf_t buf[][SNDBUF_CHANS],
	int nframes, int channels, int format, int srate)
{
    struct la_h *lah = &handles[h];
    struct lads *lad = lah->lad;
    LADSPA_Handle handle = lah->handle;
    LADSPA_Data tmp_in[nframes], tmp_out[nframes];
    int i, j;
    if (srate != lah->srate) {
	error("ladspa: wrong sampling rate\n");
	return 0;
    }
    lad->descriptor->connect_port(handle, lad->in, tmp_in);
    lad->descriptor->connect_port(handle, lad->out, tmp_out);
    for (i = 0; i < channels; i++) {
	for (j = 0; j < nframes; j++)
	    tmp_in[j] = sample_to_float(&buf[j][i], format);
	lad->descriptor->run(handle, nframes);
	for (j = 0; j < nframes; j++)
	    float_to_sample(tmp_out[j], &buf[j][i], format);
    }
    return nframes;
}

static struct pcm_efp ladspa = {
    .name = ladspa_name,
    .longname = ladspa_longname,
    .open = ladspa_open,
    .close = ladspa_close,
    .start = ladspa_start,
    .stop = ladspa_stop,
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
