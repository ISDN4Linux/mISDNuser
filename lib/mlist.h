/* mlist.h
 *
 * Simple doubly linked list implementation and helper
 * function reduce version of linux kernel list.h
 */
              
#ifndef _MLIST_H
#define _MLIST_H

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

/*
 * list_add_head - add a new entry on the top
 * @new: new entry to be added
 * @head: list head to add it after
 *
 */
static inline void list_add_head(struct list_head *new, struct list_head *head)
{
	head->next->prev = new;
	new->next = head->next;
	new->prev = head;
	head->next = new;
}

/*
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	head->prev->next = new;
	new->next = head;
	new->prev = head->prev;
	head->prev = new;
}

#define	LIST_POISON1	0xdeadbee1
#define LIST_POISON2	0xdeadbee2

/*
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 */
static inline void list_del(struct list_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = (void *)LIST_POISON1;
	entry->prev = (void *)LIST_POISON2;
}

/*
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/*
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/*
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
        	pos = pos->next)

/*
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif
