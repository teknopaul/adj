#ifndef _ADJ_MOD_INCLUDED_
#define _ADJ_MOD_INCLUDED_

void* adj_mod_autoconfigure(adj_seq_info_t* adj, uint32_t flags);

void* adj_mod_manual_configure(adj_seq_info_t* adj, char *module, uint32_t flags);

#endif // _ADJ_MOD_INCLUDED_