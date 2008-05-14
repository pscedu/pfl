/* $Id$ */

struct psc_streenode {
	struct psclist_head	 ptn_sibling;
	struct psclist_head	 ptn_children;	/* branch */
	void			*ptn_data;
};

#define psc_stree_foreach_child(child, ptn)		\
	psclist_for_each_entry((child), &(ptn)->ptn_children, ptn_sibling)

struct psc_streenode *
     psc_stree_addchild(struct psc_streenode *, void *);
void psc_stree_init(struct psc_streenode *);
