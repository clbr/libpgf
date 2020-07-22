#include <lrtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void huffman(const u32 totalprob[256], const u32 len, u8 canonical[256]);
void canoncode(const u8 n, const u8 canon[256], u32 *val, u32 *bits);

struct node_t {
	struct node_t *left, *right;
	u32 count;
};

static int nodecmp(const void *ap, const void *bp) {
	const struct node_t * const *a = ap;
	const struct node_t * const *b = bp;

	if ((*a)->count < (*b)->count)
		return -1;
	if ((*a)->count > (*b)->count)
		return 1;
	return 0;
}

#if 0
static void printtree(const struct node_t *base, const struct node_t *n, const u32 num,
			const u32 bits, const u32 rights) {


	if (!n) return;

	const u32 name = n - base;

	if (!n->left && !n->right) {
		printf("n%u [label=\"%u 0x%x (%u bits, %u rights)\"];\n",
			name, n->count,	num, bits, rights);
	} else {
		/*u32 i;
		for (i = 0; i < rights; i++) printf(" ");
		printf("^\n");*/
		printf("n%u -> n%lu;\n", name, n->left - base);
		printf("n%u -> n%lu;\n", name, n->right - base);
	}
	printtree(base, n->left, num << 1, bits + 1, rights);
	printtree(base, n->right, (num << 1) | 1, bits + 1, rights + 1);
}
#endif

static void canonicalize(u8 lens[256], const struct node_t *n, const u32 bits) {
	if (!n->left && !n->right) {
		lens[bits]++;
	} else {
		canonicalize(lens, n->left, bits + 1);
		canonicalize(lens, n->right, bits + 1);
	}
}

void huffman(const u32 totalprob[256], const u32 len, u8 canonical[256]) {
	u32 i, qlen = 0, freenode = len;
	struct node_t *queue[len];

	/*
	1. Create a leaf node for each symbol and add it to the priority queue.
	2. While there is more than one node in the queue:
		1. Remove the two nodes of highest priority (lowest probability) from the queue
		2. Create a new internal node with these two nodes as children and with
		   probability equal to the sum of the two nodes' probabilities.
		3. Add the new node to the queue.
	3. The remaining node is the root node and the tree is complete.
	*/

	struct node_t nodes[len * 2];
	memset(nodes, 0, sizeof(struct node_t) * len * 2);

	for (i = 0; i < len; i++) {
		nodes[i].count = totalprob[i];

		queue[i] = &nodes[i];
	}

	qlen = len;
	qsort(queue, qlen, sizeof(void *), nodecmp);

	while (qlen > 1) {
		struct node_t *one, *two;
		one = queue[0];
		two = queue[1];

		qlen -= 2;
		memmove(queue, &queue[2], qlen * sizeof(void *));

		struct node_t *new = &nodes[freenode++];
		new->left = one;
		new->right = two;
		new->count = one->count + two->count;

		queue[qlen++] = new;
		qsort(queue, qlen, sizeof(void *), nodecmp);
	}

//	printtree(nodes, queue[0], 0, 0, 0);

	// Convert it to canonical format
	canonicalize(canonical, queue[0], 0);
}

void canoncode(const u8 n, const u8 canon[256], u32 *val, u32 *bits) {
	/*
	code = 0
	while more symbols:
	    print symbol, code
	    code = (code + 1) << ((bit length of the next symbol) - (current bit length))
	*/
	u32 v = 0, i, curlen, mod = 1;
	for (i = 0; i < 256; i++)
		if (canon[i])
			break;
	curlen = i;

	for (i = 0; i < n; i++, mod++) {
		u32 shift = 0;
		if (mod >= canon[curlen]) {
			mod = 0;
			curlen++;
			shift++;
			while (!canon[curlen]) {
				curlen++;
				shift++;
			}
		}
		v = (v + 1) << shift;
	}

	*val = v;
	*bits = curlen;
}
