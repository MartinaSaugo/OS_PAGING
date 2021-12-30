#ifndef _STATS_H_
#define _STATS_H_

typedef struct {
	int tlb_faults;
	int tlb_faults_w_free;
	int tlb_faults_w_replace;
	int tlb_invalidation;
	int tlb_reloads;
	int page_faults_zero;
	int page_faults_disk;
	int page_faults_elf;
	int page_faults_swapfile;
	int swapfile_writes;
} stats_t;

stats_t stats;

void stats_init(void);
void print_stats(void);

#endif
