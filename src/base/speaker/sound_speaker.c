/*
 * speaker Emulation via sound card
 * inspired by wave generation function from QEMU: hw/audio/pcspk.c
 * =============================================================================
 */

#include "emu.h"
#include "speaker.h"
#include "timers.h"
#include "utilities.h"
#include "sound/sound.h"

#include <pthread.h>
#include <semaphore.h>

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define PCSPK_BUF_LEN 2470
#define PCSPK_SAMPLE_RATE 44100
#define PCSPK_MAX_FREQ (PCSPK_SAMPLE_RATE >> 1)
#define PCSPK_MIN_COUNT DIV_ROUND_UP(SPEAKER_PERIOD_BASE, PCSPK_MAX_FREQ) // 55

typedef struct {
	sndbuf_t sample_buf[PCSPK_BUF_LEN][SNDBUF_CHANS];
	unsigned int pit_count;
	unsigned int samples;
	unsigned int play_pos;
	int pcm_stream;
	sem_t sem;
	pthread_t thr;
	int running, stopping;
	unsigned short period;
	pthread_mutex_t run_mtx;
} PCSpkState;

static PCSpkState beep = {
	.pit_count = (unsigned int)-1,
	.run_mtx = PTHREAD_MUTEX_INITIALIZER
};

/*
 * wave generation function from QEMU: hw/audio/pcspk.c
 */
static inline void generate_samples(PCSpkState *s)
{
	unsigned int i;

	if (s->pit_count == 65535) {
		s->samples = PCSPK_BUF_LEN;
		for (i = 0; i < PCSPK_BUF_LEN; ++i)
			s->sample_buf[i][0] = -32;
	}
	else if (s->pit_count) {
		const uint32_t m = PCSPK_SAMPLE_RATE * s->pit_count;
		const uint32_t n = ((uint64_t)SPEAKER_PERIOD_BASE << 32) / m;

		/* multiple of wavelength for gapless looping */
		s->samples = ((PCSPK_BUF_LEN * SPEAKER_PERIOD_BASE / m * m) / (SPEAKER_PERIOD_BASE >> 1) + 1) >> 1;
		for (i = 0; i < s->samples; ++i)
			s->sample_buf[i][0] = (64 & (n * i >> 25)) - 32;
	} else {
		s->samples = PCSPK_BUF_LEN;
		for (i = 0; i < PCSPK_BUF_LEN; ++i)
			s->sample_buf[i][0] = 128; /* silence */
	}
}

static void speaker_process_samples(PCSpkState *s, unsigned int nsamples)
{
	unsigned int n;

	n = s->period;
	/* avoid frequencies that are not reproducible with sample rate */
	if (n < PCSPK_MIN_COUNT)
		n = 0;

	if (s->pit_count != n) {
		s->pit_count = n;
		s->play_pos = 0;
		generate_samples(s);
	}

	if (s->pit_count && s->pit_count != 65535) {
		const uint32_t m = PCSPK_SAMPLE_RATE * s->pit_count;
		/* round up to multiple of wavelength */
		nsamples = (((nsamples * SPEAKER_PERIOD_BASE + m - 1) / m
			     * m) / (SPEAKER_PERIOD_BASE >> 1) + 1) >> 1;
	}

	while (nsamples > 0) {
		n = _min(s->samples - s->play_pos, nsamples);
		pcm_write_interleaved(&s->sample_buf[s->play_pos], n, PCSPK_SAMPLE_RATE,
				      PCM_FORMAT_U8, 1, s->pcm_stream);
		s->play_pos = (s->play_pos + n) % s->samples;
		nsamples -= n;
	}
}

static void speaker_run(PCSpkState *s)
{
	unsigned int nsamples;
	double period, speaker_time_cur;
	hitimer_t curtime;

	curtime = GETusTIME(0);
	speaker_time_cur = pcm_get_stream_time(s->pcm_stream);
	if (curtime < speaker_time_cur)
		return;
	if (s->stopping) {
		pcm_flush(s->pcm_stream);
		s->running = 0;
		return;
	}
	period = pcm_frame_period_us(PCSPK_SAMPLE_RATE);
	nsamples = (curtime - speaker_time_cur) / period;
	speaker_process_samples(s, nsamples);
	if (debug_level('S') >= 7)
		S_printf("speaker: processed %i samples\n", nsamples);
}

static void *speaker_thread(void *arg)
{
	PCSpkState *s = arg;

	while (1) {
		sem_wait(&s->sem);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		pthread_mutex_lock(&s->run_mtx);
		if (s->running)
			speaker_run(s);
		pthread_mutex_unlock(&s->run_mtx);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}
	return NULL;
}

static void speaker_timer(void)
{
	sem_post(&beep.sem);
}

static void sound_speaker_on(void *gp, unsigned short period)
{
	PCSpkState *s = gp;

	pthread_mutex_lock(&s->run_mtx);
	s->period = period;
	if (!s->running)
		pcm_prepare_stream(s->pcm_stream);
	s->running = 1;
	s->stopping = 0;
	pthread_mutex_unlock(&s->run_mtx);
	sem_post(&s->sem);
}

static void sound_speaker_off(void *gp)
{
	PCSpkState *s = gp;

	pthread_mutex_lock(&s->run_mtx);
	s->stopping = 1;
	s->period = 0;
	pthread_mutex_unlock(&s->run_mtx);
}

void sound_speaker_init(void)
{
	if (config.speaker != SPKR_SOUND) return;
	register_speaker(&beep, sound_speaker_on, sound_speaker_off);
	sigalrm_register_handler(speaker_timer);
	beep.pcm_stream = pcm_allocate_stream(1, "PC-SPEAKER", (void *)MC_PCSP);
	sem_init(&beep.sem, 0, 0);
	pthread_create(&beep.thr, NULL, speaker_thread, &beep);
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__GLIBC__)
	pthread_setname_np(beep.thr, "dosemu: speaker");
#endif
}

void sound_speaker_done(void)
{
	if (config.speaker != SPKR_SOUND) return;
	pthread_cancel(beep.thr);
	pthread_join(beep.thr, NULL);
	sem_destroy(&beep.sem);
}
