#include <types.h>
#include <lib.h>
#include <vm.h>
#include <vmstats.h>

void stats_init(void){
	stats.tlb_faults=0;
	stats.tlb_faults_w_free=0;
	stats.tlb_faults_w_replace=0;
	stats.tlb_invalidation=0;
	stats.tlb_reloads=0;
	stats.page_faults_zero=0;
	stats.page_faults_disk=0;
	stats.page_faults_elf=0;
	stats.page_faults_swapfile=0;
	stats.swapfile_writes=0;
	return;
}

void print_stats(void){
	
	if(stats.tlb_faults!=stats.tlb_faults_w_free+stats.tlb_faults_w_replace)
		kprintf("WARNING!\tTLB faults != TLB faults with free + TLB faults with replace\n");
	if(stats.tlb_faults!=stats.page_faults_zero+stats.page_faults_disk+stats.tlb_reloads)
		kprintf("WARNING!\tTLB faults != Page faults (zeroed) + Page faults (disk) + TLB faults reloads\n");
	if(stats.page_faults_disk!=stats.page_faults_elf+stats.page_faults_swapfile)
		kprintf("WARNING!\tPage faults (disk) != Page faults (elf) + Page faults (swapfile)\n");
	
	kprintf("TLB faults: %d\n", stats.tlb_faults);
	kprintf("TLB faults with Free: %d\n", stats.tlb_faults_w_free);
	kprintf("TLB faults with Replace: %d\n", stats.tlb_faults_w_replace);
	kprintf("TLB invalidations: %d\n", stats.tlb_invalidation);
	kprintf("TLB reloads: %d\n", stats.tlb_reloads);
	kprintf("Page faults (zeroed): %d\n", stats.page_faults_zero);
	kprintf("Page faults (disk): %d\n", stats.page_faults_disk);
	kprintf("Page faults elf: %d\n", stats.page_faults_elf);
	kprintf("Page faults swapfile: %d\n", stats.page_faults_swapfile);
	kprintf("Swapfile writes: %d\n", stats.swapfile_writes);
	return;
}
