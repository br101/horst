
#include "list.h"

/*
 * sorting a linked list.
 *
 * The algorithm used is Mergesort, because that works really well
 * on linked lists, without requiring the O(N) extra space it needs
 * when you do it on arrays.
 *
 */

/*
 * This file is copyright 2001 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


/*
 * This is the actual sort function. Notice that it returns the new
 * head of the list. (It has to, because the head will not
 * generally be the same element after the sort.) So unlike sorting
 * an array, where you can do
 *
 *     sort(myarray);
 *
 * you now have to do
 *
 *     list = listsort(mylist);
 */
struct list_head*
listsort(struct list_head *list,
	int(*cmp)(const struct list_head*, const struct list_head*))
{
	struct list_head *p, *q, *e, *tail, *oldhead;
	int insize, nmerges, psize, qsize, i;

	if (!list)
		return NULL;

	insize = 1;

	while (1)
	{
		p = list;
		oldhead = list;	/* used for circular linkage */
		list = NULL;
		tail = NULL;
		nmerges = 0;	/* count number of merges we do in this pass */

		while (p)
		{
			nmerges++;  /* there exists a merge to be done */

			/* step `insize' places along from p */
			q = p;
			psize = 0;
			for (i = 0; i < insize; i++) {
				psize++;
				q = (q->next == oldhead ? NULL : q->next);
				if (!q)
					break;
			}

			/* if q hasn't fallen off end, we have two lists to merge */
			qsize = insize;

			/* now we have two lists; merge them */
			while (psize > 0 || (qsize > 0 && q))
			{
				/* decide whether next element of merge comes from p or q */
				if (psize == 0) {
					/* p is empty; e must come from q. */
					e = q; q = q->next; qsize--;
					if (q == oldhead) q = NULL;
				} else if (qsize == 0 || !q) {
					/* q is empty; e must come from p. */
					e = p; p = p->next; psize--;
					if (p == oldhead) p = NULL;
				} else if (cmp(p,q) <= 0) {
					/* First element of p is lower (or same);
					 * e must come from p. */
					e = p; p = p->next; psize--;
					if (p == oldhead) p = NULL;
				} else {
					/* First element of q is lower; e must come from q. */
					e = q; q = q->next; qsize--;
					if (q == oldhead) q = NULL;
				}
				/* add the next element to the merged list */
				if (tail)
					tail->next = e;
				else
					list = e;
				/* Maintain reverse pointers */
				e->prev = tail;
				tail = e;
			}

			/* now p has stepped `insize' places along, and q has too */
			p = q;
		}
		tail->next = list;
		list->prev = tail;

		/* If we have done only one merge, we're finished. */
		if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
			return list;
		/* Otherwise repeat, merging lists twice the size */
		insize *= 2;
	}
}

#if 0
/*
 * Small test rig with three test orders. The list length 13 is
 * chosen because that means some passes will have an extra list at
 * the end and some will not.
 */

#include <stdio.h>

struct element {
	struct list_head list;
	int i;
};

int
elem_cmp(const struct list_head *a, const struct list_head *b)
{
	struct element *ea = list_entry(a, struct element, list);
	struct element *eb = list_entry(b, struct element, list);
	return ea->i - eb->i;
}

int main(void) {
	#define n 13
	struct element k[n], *head, *p;
	struct list_head* lh;

	int order[][n] = {
		{ 0,1,2,3,4,5,6,7,8,9,10,11,12 },
		{ 6,2,8,4,11,1,12,7,3,9,5,0,10 },
		{ 12,11,10,9,8,7,6,5,4,3,2,1,0 },
	};
	int i, j;

	for (j = 0; j < n; j++)
		k[j].i = j;

	listsort(NULL, &elem_cmp);

	for (i = 0; i < sizeof(order)/sizeof(*order); i++)
	{
		int *ord = order[i];
		head = &k[ord[0]];
		for (j = 0; j < n; j++) {
			if (j == n-1)
				k[ord[j]].list.next = &k[ord[0]].list;
			else
				k[ord[j]].list.next = &k[ord[j+1]].list;
			if (j == 0)
				k[ord[j]].list.prev = &k[ord[n-1]].list;
			else
			    k[ord[j]].list.prev = &k[ord[j-1]].list;
		}

		printf("before:");
		p = head;
		do {
			printf(" %d", p->i);
			//if (p->list.next && p->list.next->prev != p->list)
			//	printf(" [REVERSE LINK ERROR!]");
			p = list_entry(p->list.next, struct element, list);
		} while (p != head);
		printf("\n");

		lh = listsort(&head->list, &elem_cmp);
		head = list_entry(lh, struct element, list);

		printf(" after:");
		p = head;
		do {
			printf(" %d", p->i);
			//if (p->next && p->next->prev != p)
			//	printf(" [REVERSE LINK ERROR!]");
			p = list_entry(p->list.next, struct element, list);
		} while (p != head);
		printf("\n");

	}
	return 0;
}
#endif
