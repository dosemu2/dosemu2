#ifndef SEQUENCR_H
#define SEQUENCR_H

void *sequencer_init(void);
void sequencer_done(void *handle);
void sequencer_clear(void *handle);
struct seq_item_s;
struct seq_item_s *sequencer_add(void *handle, unsigned long long tstamp);
void *sequencer_get(void *handle);
unsigned long long sequencer_get_next(void *handle);

void sequencer_add_tag(struct seq_item_s *i, int tag, int data);
void sequencer_free(struct seq_item_s *item);
int sequencer_find(struct seq_item_s *item, int tag);

#endif
