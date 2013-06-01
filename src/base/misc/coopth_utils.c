#include <assert.h>
#include "emu.h"
#include "utilities.h"
#include "coopth.h"
#include "coopth_utils.h"

#define MAX_TAGS 10
#define MAX_TAGGED_THREADS 5

struct coopth_tag_t {
    int cookie;
    int tid;
};

static struct coopth_tag_t tags[MAX_TAGS][MAX_TAGGED_THREADS] =
	{ [0 ... MAX_TAGS-1][0 ... MAX_TAGGED_THREADS-1] =
	{ .tid = COOPTH_TID_INVALID } };
static int tag_cnt;

int coopth_tag_alloc(void)
{
    if (tag_cnt >= MAX_TAGS) {
	error("Too many tags\n");
	leavedos(2);
    }
    return tag_cnt++;
}

static void coopth_tag_set(int tag, int cookie, int tid)
{
    int j, empty = -1;
    struct coopth_tag_t *tagp, *tagp2;
    assert(tag >= 0 && tag < tag_cnt);
    tagp = tags[tag];
    for (j = 0; j < MAX_TAGGED_THREADS; j++) {
	if (empty == -1 && tagp[j].tid == COOPTH_TID_INVALID)
	    empty = j;
	if (tagp[j].cookie == cookie && tagp[j].tid != COOPTH_TID_INVALID) {
	    dosemu_error("Coopth: tag %i(%i) already set\n", tag, cookie);
	    leavedos(2);
	}
    }
    if (empty == -1) {
	dosemu_error("Coopth: too many tags for %i\n", tag);
	leavedos(2);
    }

    tagp2 = &tagp[empty];
    tagp2->tid = tid;
    tagp2->cookie = cookie;
}

static void coopth_tag_clear(int tag, int cookie)
{
    int j;
    struct coopth_tag_t *tagp;
    assert(tag >= 0 && tag < tag_cnt);
    tagp = tags[tag];
    for (j = 0; j < MAX_TAGGED_THREADS; j++) {
	if (tagp[j].cookie == cookie) {
	    if (tagp[j].tid == COOPTH_TID_INVALID) {
		dosemu_error("Coopth: tag %i(%i) already cleared\n", tag, cookie);
		leavedos(2);
	    }
	    break;
	}
    }
    if (j >= MAX_TAGGED_THREADS) {
	dosemu_error("Coopth: tag %i(%i) not set\n", tag, cookie);
	leavedos(2);
    }
    tagp[j].tid = COOPTH_TID_INVALID;
}

int coopth_get_tid_by_tag(int tag, int cookie)
{
    int j, tid = COOPTH_TID_INVALID;
    struct coopth_tag_t *tagp;
    assert(tag >= 0 && tag < tag_cnt);
    tagp = tags[tag];
    for (j = 0; j < MAX_TAGGED_THREADS; j++) {
	if (tagp[j].cookie == cookie) {
	    if (tagp[j].tid == COOPTH_TID_INVALID) {
		dosemu_error("Coopth: tag %i(%i) cleared\n", tag, cookie);
		leavedos(2);
	    }
	    tid = tagp[j].tid;
	    break;
	}
    }
    if (tid == COOPTH_TID_INVALID) {
	dosemu_error("Coopth: tag %i(%i) not found\n", tag, cookie);
	leavedos(2);
    }
    return tid;
}

void coopth_sleep_tagged(int tag, int cookie)
{
    coopth_tag_set(tag, cookie, coopth_get_tid());
    coopth_sleep();
    coopth_tag_clear(tag, cookie);
}

void coopth_yield_tagged(int tag, int cookie)
{
    int tid = coopth_get_tid();
    /* first, make sure our thread is descheduled.
     * Otherwise this call makes no sense and sleep should be use instead. */
    assert(coopth_get_scheduled() != tid);
    coopth_tag_set(tag, cookie, tid);
    coopth_yield();
    coopth_tag_clear(tag, cookie);
}
