/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#define DEBUG
#include "debug.h"

#include "exec_parser.h"

static so_exec_t *exec;
static int **mapped_pages;
static char *exec_path;

void handler(int sig, siginfo_t *info, void *ucontext)
{
	long page_sz = sysconf(_SC_PAGE_SIZE);
	uintptr_t fault_addr = (uintptr_t)info->si_addr;
	int segments_no = exec->segments_no;

	for (int i = 0; i < segments_no; ++i) {

		uintptr_t start_addr = exec->segments[i].vaddr;
		uintptr_t end_addr = exec->segments[i].vaddr + exec->segments[i].mem_size;

		if (fault_addr >= start_addr && fault_addr < end_addr) {

			unsigned int page_no = (fault_addr - start_addr) / page_sz;
			if (mapped_pages[i][page_no] == 0) {

				unsigned int perm = exec->segments[i].perm;

            	/* build prot mask for mmap*/
				int prot = 0;
				if (perm & PERM_R) prot |= PROT_READ;
				if (perm & PERM_W) prot |= PROT_WRITE;
				if (perm & PERM_X) prot |= PROT_EXEC;

				int open_flags = (perm & PERM_W) ? O_RDWR : O_RDONLY;
				int fd = open(exec_path, open_flags);
				uintptr_t page_addr = start_addr + page_no * page_sz;
				off_t offset = exec->segments[i].offset + page_no * page_sz;
				
				mmap((void*)page_addr, page_sz, prot, MAP_PRIVATE | MAP_FIXED, fd, offset);

				close(fd);

				mapped_pages[i][page_no] = 1;

				uintptr_t seg_file_end = exec->segments[i].vaddr + exec->segments[i].file_size;
                uintptr_t seg_mem_end  = exec->segments[i].vaddr + exec->segments[i].mem_size;

                /* 	2 cases:
					- Page's address is beyond segment's end in file
					- Segment's end in file is between page's address and end of the page	
				
					Therefore page is either entire .bss section page or
					partial .bss section page and the rest belongs to another section
				*/
                if (page_addr + page_sz > seg_file_end) {

                    uintptr_t zero_start = (page_addr < seg_file_end) ? seg_file_end : page_addr;
                    size_t zero_len = 0;
                    if (seg_mem_end >= page_addr) {
                        uintptr_t zero_end = (seg_mem_end < page_addr + page_sz) ? seg_mem_end : (page_addr + page_sz);
                        if (zero_end > zero_start)
                            zero_len = (size_t)(zero_end - zero_start);
                    }
                    if (zero_len > 0)
                        memset((void*)zero_start, 0, zero_len);
                }
				return;
			}
			else {
				break;
			}
		}
	}
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigaction(SIGSEGV, &sa, NULL);
	return;
}

int so_init_loader(void)
{
	/* TODO: initialize on-demand loader */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigaction(SIGSEGV, &sa, NULL);
	return -1;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	exec_path = path;

	int segments_no = exec->segments_no;
	int page_sz = getpagesize();
	mapped_pages = malloc(sizeof(*mapped_pages) * segments_no);
	for (int i = 0; i < segments_no; i++) {
		int seg_page_no = exec->segments[i].mem_size % page_sz == 0 ?
							exec->segments[i].mem_size / page_sz :
							exec->segments[i].mem_size / page_sz + 1;
		mapped_pages[i] = malloc(sizeof(*mapped_pages[i]) * seg_page_no);

		for (int j = 0; j < seg_page_no; j++) {
			mapped_pages[i][j] = 0;
		}
	}

	so_start_exec(exec, argv);

	return -1;
}
