#ifndef _LISTSORT_H
#define _LISTSORT_H

struct list_head;

struct list_head*
listsort(struct list_head *list,
	int(*cmp)(const struct list_head*, const struct list_head*));

#endif
