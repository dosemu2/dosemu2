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
 * Purpose: uncycled (or linear) linked list with the ability to
 * quickly find head. This is mainly a singly-linked list API-wise,
 * but all operations are done in constant time: no traversal required
 * to look up prev element on deletion, and no traversal required to
 * find head (we keep separate pointers to head).
 *
 * Author: @stsp
 * With some inspiration from linux kernel's list.h.
 *
 */

#ifndef ULIST_H
#define ULIST_H

#ifndef container_of
#define container_of(ptr, type, member) ( \
		    (type *)((char *)(ptr) - offsetof(type,member)))
#endif

struct ulist_head {
	struct ulist_head *next;
	struct ulist_head *prev;
};

struct ulist_ent {
	struct ulist_head list;
	struct ulist_head *head;
};

static inline void ulist_add(struct ulist_ent *entry, struct ulist_head *head)
{
	struct ulist_head *next = head->next;

	if (next)
		next->prev = &entry->list;
	entry->list.next = next;
	entry->list.prev = head;
	entry->head = head;
	head->next = &entry->list;
}

static inline void ulist_del(struct ulist_ent *entry)
{
	struct ulist_head *next = entry->list.next;
	struct ulist_head *prev = entry->list.prev;

	if (next)
		next->prev = prev;
	/* prev must not be null as deleting head is forbidden */
	prev->next = next;
}

static inline int ulist_empty(const struct ulist_head *head)
{
	return !head->next;
}

static inline struct ulist_head *ulist_get_head(struct ulist_ent *entry)
{
	return entry->head;
}

#define ulist_entry(ptr, type, member) container_of(ptr, type, member)

#define ulist_first(head) \
	ulist_entry((head)->next, struct ulist_ent, list)

#define ulist_first_entry(ptr, type, member) \
	ulist_entry((ptr)->next, type, member.list)

#define __UL(p) ((p) ? container_of(p, struct ulist_ent, list) : NULL)

#define ulist_for_each(pos, head) \
	for (pos = __UL((head)->next); pos; pos = __UL(pos->list.next))

#endif
