// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/pmap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
    { "backtrace", "display a listing of function call frames", mon_backtrace },
    { "showmappings", "display the information of page tables in the certain range", mon_showmappings },
    { "set", "set or clear the permission of page", mon_set },
    { "showvm", "Dump the contents of a range of memory given virtual address range", mon_showvm },
    { "showpm", "Dump the contents of a range of memory given physical address range", mon_showpm }
    
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
    struct Eipdebuginfo info;
    cprintf("Stack backtrace:\n");
    uint32_t* ebp=(uint32_t*)read_ebp();
    while (ebp) {
        cprintf("  ebp %08x eip %08x args %08x %08x %08x %08x %08x\n",ebp,*(ebp+1),*(ebp+2),*(ebp+3),*(ebp+4),*(ebp+5),*(ebp+6));
        if(debuginfo_eip(*(ebp+1),&info)==-1){
            cprintf("wrong eip\n");
        }
        cprintf("         %s:%d: %.*s+%d\n",info.eip_file,info.eip_line,info.eip_fn_namelen,info.eip_fn_name,*(ebp+1)-info.eip_fn_addr);
        ebp=(uint32_t*)*ebp;
        
    }
	return 0;
}

int str2int(char *s,int base){
    int temp=0;
    if (base==16) {
        s+=2;
        while (*s!='\0') {
            if (*s<='9'&&*s>='0') {
                temp=temp*16+(*s-'0');
            }
            else if (*s<='f'&&*s>='a'){
                temp=temp*16+(*s-'a')+10;
            }
            else {
                temp=temp*16+(*s-'A')+10;
            }
            s++;
        }
    }
    else{
        while (*s!='\0') {
            temp=temp*10+(*s-'0');
            s++;
        }
    }
    
    return temp;
}

extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
extern pde_t *kern_pgdir;
int
mon_showmappings(int argc, char **argv, struct Trapframe *tf){
    if (argc!=3) {
        cprintf("FORMAT WRONG:showmappings begin_addr end_addr\n");
        return 0;
    }
    uintptr_t begin_addr=str2int(argv[1],16);
    uintptr_t end_addr=str2int(argv[2],16);
    for (int addr=begin_addr; addr<=end_addr; addr+=0x1000) {
        pte_t *pte=pgdir_walk(kern_pgdir,(void*)addr,0);
        if (!pte) {
            cprintf("addr: %x unmapped\n",addr);
            continue;
        }
        cprintf("vaddr[%x,%x) paddr:%p with ",addr,addr+0x1000,*pte&0xFFFFF000);
        cprintf("%c%c%c\n",(*pte&PTE_U)?'U':'-',(*pte&PTE_W)?'W':'-',(*pte&PTE_P)?'P':'-');
    }
    return 0;
}

int mon_set(int argc, char **argv, struct Trapframe *tf) {
    if (argc!=4) {
        cprintf("FORMAT WRONG:setper addr [0|1:clear or set] [P|W|U]\n");
        return 0;
    }
    uintptr_t addr=str2int(argv[1],16);
    pte_t *pte=pgdir_walk(kern_pgdir,(void *)addr,0);
    if (!pte) {
        cprintf("addr: %x unmapped\n",addr);
        return 0;
    }
    cprintf("%x before set: ",addr);
    cprintf("%c%c%c\n",(*pte&PTE_U)?'U':'-',(*pte&PTE_W)?'W':'-',(*pte&PTE_P)?'P':'-');
    uint32_t perm=0;
    if (argv[3][0]=='P') perm=PTE_P;
    if (argv[3][0]=='W') perm=PTE_W;
    if (argv[3][0]=='U') perm=PTE_U;
    if (argv[2][0]=='0')
        *pte=*pte&~perm;
    else
        *pte=*pte|perm;
    cprintf("%x after  set: ", addr);
    cprintf("%c%c%c\n",(*pte&PTE_U)?'U':'-',(*pte&PTE_W)?'W':'-',(*pte&PTE_P)?'P':'-');
    return 0;
}

int mon_showvm(int argc, char **argv, struct Trapframe *tf){
    if (argc!=3) {
        cprintf("FORMAT WRONG: showvm addr n\n");
        return 0;
    }
    int** addr=(int**)str2int(argv[1],16);
    int n=str2int(argv[2],10);
    for (int i=0;i<n;i+=4)
        cprintf("%x:0x%08x 0x%08x 0x%08x 0x%08x\n",addr+i,addr[i],addr[i+1],addr[i+2],addr[i+3]);
    return 0;
}

int mon_showpm(int argc, char **argv, struct Trapframe *tf){
    if (argc!=3) {
        cprintf("FORMAT WRONG: showpm addr n\n");
        return 0;
    }
    physaddr_t addr=str2int(argv[1],16);
    int n=str2int(argv[2],10);
    int base=ROUNDDOWN(addr,PGSIZE);
    int off=addr-base;
    while (n) {
        int count=(off+n)<PGSIZE?n:PGSIZE-off;
        for (int i=0; i<1024; i++) {
            pde_t pde=(pde_t)kern_pgdir[i];
            if (pde) {
                pte_t* pgtable=KADDR(PTE_ADDR(pde));
                for (int j=0; j<1024; j++) {
                    if ((pgtable[j]&(~0xFFF))==base) {
                        uint32_t* va=(uint32_t*)((i<<PDXSHIFT)|(j<<PTXSHIFT));
                        for (int i=off;i<count;i+=4)
                            cprintf("%x:0x%08x 0x%08x 0x%08x 0x%08x\n",va+i,va[i],va[i+1],va[i+2],va[i+3]);
                    }
                }
            }
        }
        n=n-count;
        off=0;
    }
    
    return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");
    
    //int x = 1, y = 3, z = 4;
    //cprintf("x %d, y %x, z %d\n", x, y, z);
    
    //unsigned int i = 0x00646c72;
    //cprintf("H%x Wo%s", 57616, &i);
    
    //cprintf("x=%d y=%d", 3);
    
	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
