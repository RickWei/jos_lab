// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.
	thisenv = 0;
    thisenv=envs+ENVX(sys_getenvid());
    
    //challenge for sfork
    /*
    int esp;
    asm volatile("mov %%esp,%0\n"
                 "subl $0x80,%%esp\n"
                 "mov %%esp,%%eax\n"
                 "mov %1,%%ebx\n"
                 "mov $0,%%edx\n"
                 "copy:\n"
                 "mov (%%ebx),%%ecx\n"
                 "mov %%ecx,(%%eax)\n"
                 "add $1,%%eax\n"
                 "add $1,%%ebx\n"
                 "add $1,%%edx\n"
                 "cmpl $0x80,%%edx\n"
                 "jne copy\n"
                 :"=m"(esp)
                 :"m"(thisenv));
    thisenv=(void*)(esp-0x80);
     */
    
	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

