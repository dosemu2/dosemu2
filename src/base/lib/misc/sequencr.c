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
 * simple sequencer library.
 *
 * Author: @stsp
 *
 */
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include "dosemu_debug.h"
#include "sequencr.h"

struct seq_tag_s {
  int tag;
  int data;
};

#define MAX_TAGS 5
struct seq_item_s {
  struct seq_item_s *next;
  unsigned long long tstamp;
  struct seq_tag_s tags[MAX_TAGS];
  int ntags;
};

struct seq_s {
  struct seq_item_s *head;
  struct seq_item_s *tail;
  pthread_mutex_t seq_mtx;
};

void *sequencer_init(void)
{
  struct seq_s *s = malloc(sizeof(*s));
  assert(s);
  s->head = s->tail = NULL;
  pthread_mutex_init(&s->seq_mtx, NULL);
  return s;
}

static struct seq_item_s *do_get(struct seq_s *s)
{
  struct seq_item_s *ret = s->head;
  if (!ret)
    return NULL;
  s->head = ret->next;
  if (!s->head)
    s->tail = NULL;
  return ret;
}

static void do_free(struct seq_item_s *i)
{
  free(i);
}

static void do_clear(struct seq_s *s)
{
  while (s->head) {
    struct seq_item_s *i = do_get(s);
    do_free(i);
  }
}

void sequencer_done(void *handle)
{
  struct seq_s *s = handle;
  do_clear(s);
  pthread_mutex_destroy(&s->seq_mtx);
  free(s);
}

void sequencer_clear(void *handle)
{
  struct seq_s *s = handle;
  pthread_mutex_lock(&s->seq_mtx);
  do_clear(s);
  pthread_mutex_unlock(&s->seq_mtx);
}

struct seq_item_s *sequencer_add(void *handle, unsigned long long tstamp)
{
  struct seq_s *s = handle;
  struct seq_item_s *i;

  i = malloc(sizeof(*i));
  assert(i);
  i->next = NULL;
  i->tstamp = tstamp;
  i->ntags = 0;

  pthread_mutex_lock(&s->seq_mtx);
  if (s->tail) {
    if (tstamp < s->tail->tstamp) {
      error("time goes backwards? %lli < %lli\n", tstamp, s->tail->tstamp);
      i->tstamp = s->tail->tstamp;
    }
    s->tail->next = i;
  } else {
    assert(!s->head);
    s->head = i;
  }
  s->tail = i;
  pthread_mutex_unlock(&s->seq_mtx);
  return i;
}

void sequencer_add_tag(struct seq_item_s *i, int tag, int data)
{
  assert(i->ntags < MAX_TAGS);
  i->tags[i->ntags].tag = tag;
  i->tags[i->ntags].data = data;
  i->ntags++;
}

void *sequencer_get(void *handle)
{
  struct seq_s *s = handle;
  struct seq_item_s *i;

  pthread_mutex_lock(&s->seq_mtx);
  i = do_get(s);
  pthread_mutex_unlock(&s->seq_mtx);
  return i;
}

unsigned long long sequencer_get_next(void *handle)
{
  struct seq_s *s = handle;
  unsigned long long ret = 0;

  pthread_mutex_lock(&s->seq_mtx);
  if (s->head)
    ret = s->head->tstamp;
  pthread_mutex_unlock(&s->seq_mtx);
  return ret;
}

void sequencer_free(struct seq_item_s *item)
{
  do_free(item);
}

int sequencer_find(struct seq_item_s *item, int tag)
{
  int i;
  for (i = 0; i < item->ntags; i++) {
    if (item->tags[i].tag == tag)
      return item->tags[i].data;
  }
  return -1;
}
