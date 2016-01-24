/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
 */

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
#include "emu.h"
#include "init.h"
#include "sound/midi.h"
#include <alsa/asoundlib.h>

static snd_rawmidi_t *handle, *handle_v;
#define midoalsa_name "alsa"
#define midoalsa_longname "MIDI Output: ALSA device"
#define midoalsav_name "alsa_virtual"
#define midoalsav_longname "MIDI Output: ALSA virtual device (for MT32)"
static const char *client_name_param = "dev_name";
static const char *rawmidi_virt_name = "Virtual RawMIDI";
static const char *munt_client_name = "Munt MT-32";
static const char *device = "default";
static const char *device_v = "virtual";

static int midoalsa_open(snd_rawmidi_t **handle_p, const char *dev_name)
{
    int err;
    char *plu_name = (strcmp(dev_name, device_v) == 0) ? midoalsav_name : midoalsa_name;

    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
	S_printf("%s: can't open sequencer\n", plu_name);
	return 0;
    }
    snd_seq_set_client_name(seq, "DOSEMU");

    /* Find all pre-existing rawmidi_virt ports. */
    snd_seq_addr_t *rawmidi_virts = NULL;
    int rawmidi_virt_count = 0;
    snd_seq_client_info_t *cinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    snd_seq_port_info_t *pinfo;
    snd_seq_port_info_alloca(&pinfo);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
	snd_seq_port_info_set_port(pinfo, -1);
	while (snd_seq_query_next_port(seq, pinfo) >= 0) {
	    S_printf("%s: before, found rawmidi_virt port %d:%d\n",
		    plu_name, snd_seq_client_info_get_client(cinfo),
		    snd_seq_port_info_get_port(pinfo));

	    if (strcmp(snd_seq_port_info_get_name(pinfo), rawmidi_virt_name) == 0) {
		rawmidi_virts = realloc(rawmidi_virts, sizeof(snd_seq_addr_t) * ++rawmidi_virt_count);
		rawmidi_virts[rawmidi_virt_count-1].client = snd_seq_client_info_get_client(cinfo);
		rawmidi_virts[rawmidi_virt_count-1].port = snd_seq_port_info_get_port(pinfo);
	    }
	}
    }

    err = snd_rawmidi_open(NULL, handle_p, dev_name,
			   SND_RAWMIDI_NONBLOCK | SND_RAWMIDI_SYNC);
    if (err) {
	S_printf("%s: unable to open %s for writing: %s\n",
		 plu_name, dev_name, snd_strerror(err));
    } else {
	/* ALSA rawmidi gives us no way to obtain the sequencer client and port
	   without this racy approach? */
	char *dname = pcm_parse_params(config.snd_plugin_params,
		(strcmp(dev_name, device_v) == 0) ? midoalsav_name : midoalsa_name,
		client_name_param);
	if (dname) {
	    int client = -1, port = -1, dest_client = -1, dest_port = -1;
	    snd_seq_client_info_set_client(cinfo, -1);
	    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		/* Check for new rawmidi_virt ports that could belong to us. */
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
		    S_printf("%s: after, found rawmidi_virt port %d:%d\n",
			    plu_name, snd_seq_client_info_get_client(cinfo),
			    snd_seq_port_info_get_port(pinfo));

		    if (client < 0 && strcmp(snd_seq_port_info_get_name(pinfo), rawmidi_virt_name) == 0) {
			/* It's a rawmidi_virt client port, but is it a new one? */
			int found = 0;
			for (int i = 0; i < rawmidi_virt_count; i++) {
			    if (rawmidi_virts[i].client == snd_seq_client_info_get_client(cinfo) &&
				    rawmidi_virts[i].port == snd_seq_port_info_get_port(pinfo)) {
				found = 1;
				break;
			    }
			}
			if (!found) {
			    /* This is (hopefully) dosemu's rawmidi_virt port. */
			    client = snd_seq_client_info_get_client(cinfo);
			    port = snd_seq_port_info_get_port(pinfo);
			    S_printf("%s: using rawmidi_virt port %d:%d\n",
				    plu_name, client, port);
			    break;
			}
		    }
		}
		/* While we're here, look for the client the user asked for. */
		if (dest_client < 0 && strcmp(dname, snd_seq_client_info_get_name(cinfo)) == 0) {
		    dest_client = snd_seq_client_info_get_client(cinfo);
		    dest_port = snd_seq_port_info_get_port(pinfo);
		    S_printf("%s: found dev_name=%s port %d:%d\n",
			    plu_name, dname, dest_client, dest_port);

		}
	    }

	    if (client < 0 || dest_client < 0) {
		S_printf("%s: could not make connection for %s (got %d:%d to %d:%d)\n",
			plu_name, dname, client, port, dest_client, dest_port);

		if (strcmp(dname, munt_client_name) == 0) {
		    S_printf("%s: TODO: spawn private munt and connect\n", plu_name);
		}
	    } else {
		snd_seq_addr_t sender, dest;
		sender.client = client;
		sender.port = port;
		dest.client = dest_client;
		dest.port = dest_port;

		snd_seq_port_subscribe_t *subs;
		snd_seq_port_subscribe_alloca(&subs);
		snd_seq_port_subscribe_set_sender(subs, &sender);
		snd_seq_port_subscribe_set_dest(subs, &dest);

		int err;
		if ((err = snd_seq_subscribe_port(seq, subs) < 0)) {
		    S_printf("%s: connect %d:%d to %d:%d failed: %s\n",
			    plu_name, sender.client, sender.port, dest.client, dest.port, snd_strerror(err));
		}
	    }
	}
    }
    snd_seq_close(seq);
    free(rawmidi_virts);

    /* NONBLOCK flag is needed only so that open() not to block forever */
    snd_rawmidi_nonblock(*handle_p, 0);
    return 1;
}

static int midoalsa_init(void *arg)
{
    return midoalsa_open(&handle, device);
}

static void midoalsa_done(void *arg)
{
    if (!handle)
	return;
    snd_rawmidi_close(handle);
    handle = NULL;
}

static void midoalsa_write(unsigned char val)
{
    if (!handle)
	return;
    snd_rawmidi_write(handle, &val, 1);
}

static int midoalsa_cfg(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midoalsa_name);
}

static const struct midi_out_plugin midoalsa = {
    .name = midoalsa_name,
    .longname = midoalsa_longname,
    .open = midoalsa_init,
    .close = midoalsa_done,
    .write = midoalsa_write,
    .get_cfg = midoalsa_cfg,
    .stype = ST_GM,
    .weight = MIDI_W_PREFERRED,
};

static int midoalsav_init(void *arg)
{
    return midoalsa_open(&handle_v, device_v);
}

static void midoalsav_done(void *arg)
{
    if (!handle_v)
	return;
    snd_rawmidi_close(handle_v);
    handle_v = NULL;
}

static void midoalsav_write(unsigned char val)
{
    if (!handle_v)
	return;
    snd_rawmidi_write(handle_v, &val, 1);
}

static const struct midi_out_plugin midoalsa_v = {
    .name = midoalsav_name,
    .longname = midoalsav_longname,
    .open = midoalsav_init,
    .close = midoalsav_done,
    .write = midoalsav_write,
    .get_cfg = midoalsa_cfg,
    .stype = ST_MT32,
};

CONSTRUCTOR(static int midoalsa_register(void))
{
    midi_register_output_plugin(&midoalsa);
    midi_register_output_plugin(&midoalsa_v);
    return 0;
}
