/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.
    
    // LAB 3: Your code here.
    user_mem_assert(curenv,s,len,PTE_U);
    
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

    // LAB 4: Your code here.
    struct Env *e;
    int ret;
    extern struct Env* envs;
    if(ret=env_alloc(&e, curenv->env_id),ret)
        return ret;
    e->env_status=ENV_NOT_RUNNABLE;
    e->env_tf=curenv->env_tf;
    e->env_tf.tf_regs.reg_eax=0;
    return e->env_id;
    
	panic("sys_exofork not implemented");
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
    if (status!=ENV_NOT_RUNNABLE&&status!=ENV_RUNNABLE) return -E_INVAL;
    struct Env *e;
    int ret;
    if (ret=envid2env(envid,&e,1),ret)  return ret;
    e->env_status=status;
    return 0;
    
	panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
    struct Env* e;
    if (envid2env(envid,&e,1)<0) {
        return -E_BAD_ENV;
    }
    user_mem_assert(e,tf,sizeof(struct Trapframe),PTE_U);
    e->env_tf=*tf;
    e->env_tf.tf_cs=GD_UT|3;
    e->env_tf.tf_eflags|=FL_IF;
    
    return 0;
    
	panic("sys_env_set_trapframe not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
    struct Env *e;
    int ret;
    if (ret=envid2env(envid,&e,1),ret) return ret;
    e->env_pgfault_upcall=func;
    return 0;
    
	panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
    struct Env *e;
    int ret;
    if (ret=envid2env(envid,&e,1),ret) return ret;
    
    if (va>=(void*)UTOP||(int)va%PGSIZE!=0) return -E_INVAL;
    int flag=PTE_U|PTE_P;
    if ((perm&flag)!=flag) return -E_INVAL;
    flag=PTE_U|PTE_P|PTE_AVAIL|PTE_W;
    if (perm&~flag) return -E_INVAL;
    
    struct PageInfo *pg = page_alloc(1);
    if (!pg) return -E_NO_MEM;
    ret=page_insert(e->env_pgdir,pg,va,perm);
    if (ret) {
        page_free(pg);
        return ret;
    }
    
    return 0;
    
	panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
    struct Env *src,*des;
    int ret;
    if(ret=envid2env(srcenvid,&src,1),ret) return ret;
    if(ret=envid2env(dstenvid,&des,1),ret) return ret;
    if (srcva>=(void*)UTOP||dstva>=(void*)UTOP||(int)srcva%PGSIZE!=0||(int)dstva%PGSIZE!=0) {
        return -E_INVAL;
    }
    pte_t *pte;
    struct PageInfo *pg=page_lookup(src->env_pgdir,srcva,&pte);
    if (!pg) return -E_INVAL;
    int flag=PTE_U|PTE_P;
    if ((perm&flag)!=flag) return -E_INVAL;
    if (((*pte&PTE_W)==0)&&(perm&PTE_W)) return -E_INVAL;
    ret=page_insert(des->env_pgdir,pg,dstva,perm);
    return ret;
    
    
	panic("sys_page_map not implemented");
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().
    struct Env *e;
    int ret;
    if (ret=envid2env(envid,&e,1),ret)  return ret;
    if (va>=(void*)UTOP||ROUNDDOWN(va,PGSIZE)!=va)
        return -E_INVAL;
    pte_t *pte;
    struct PageInfo *pg=page_lookup(e->env_pgdir,va,&pte);
    int flag=PTE_U|PTE_P;
    if ((*pte&flag)!=flag) return -E_BAD_ENV;
    page_remove(e->env_pgdir, va);
    return 0;
    
    
	// LAB 4: Your code here.
	panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.

static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
    struct Env *e;
    int ret;
    if (ret=envid2env(envid, &e, 0),ret) return ret;
    if (!e->env_ipc_recving) return -E_IPC_NOT_RECV;
    e->env_ipc_perm=0;
    e->env_ipc_from=curenv->env_id;
    if (srcva<(void*)UTOP) {
        pte_t *pte;
        struct PageInfo *pg=page_lookup(curenv->env_pgdir,srcva,&pte);
        if (!pg) return -E_INVAL;
        int flag=PTE_U|PTE_P;
        if ((perm&flag)!=flag) return -E_INVAL;
        
        //if (((*pte)&perm)!= perm) return -E_INVAL;
        if ((perm&PTE_W)&&!(*pte&PTE_W)) return -E_INVAL;
        if (srcva!=ROUNDDOWN(srcva, PGSIZE)) return -E_INVAL;
        if (e->env_ipc_dstva<(void*)UTOP) {
            ret=page_insert(e->env_pgdir,pg,e->env_ipc_dstva,perm);
            if (ret) return ret;
            e->env_ipc_perm=perm;
        }
    }
    e->env_ipc_recving=0;
    e->env_ipc_value=value;
    e->env_status=ENV_RUNNABLE;
    e->env_tf.tf_regs.reg_eax=0;
    return 0;
    
	panic("sys_ipc_try_send not implemented");
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
    // LAB 4: Your code here.
    if (dstva<(void*)UTOP){
        if (dstva!=ROUNDDOWN(dstva, PGSIZE))
            return -E_INVAL;
    }
    curenv->env_ipc_recving=1;
    curenv->env_status=ENV_NOT_RUNNABLE;
    curenv->env_ipc_dstva=dstva;
    sys_yield();
    return 0;
    
	panic("sys_ipc_recv not implemented");
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
    return time_msec();
	panic("sys_time_msec not implemented");
}

//challenge
static int
sys_change_pr(int pr) {
    curenv->pr=pr;
    return 0;
}

static int
sys_proc_save(envid_t envid, struct proc_status *ps)
{
    struct Env *olde;
    struct PageInfo *pg;
    int offset;
    if (envid2env(envid, &olde, 1) <0)
        return -E_BAD_ENV;
    if (user_mem_check(curenv, ps, sizeof(struct proc_status), PTE_U|PTE_W|PTE_P) <0)
        return -E_FAULT;
    ps->env = *olde;
    if ((pg=page_lookup(olde->env_pgdir, (void *)(USTACKTOP-PGSIZE), NULL))==NULL)
        return -E_FAULT;
    offset = olde->env_tf.tf_esp+PGSIZE-USTACKTOP;
    memmove(ps->stack, page2kva(pg), PGSIZE);
    cprintf("process %x has been saved\n", envid);
    return 0;
}
static int
sys_proc_restore(envid_t envid, const struct proc_status *ps)
{
    struct Env *olde;
    struct PageInfo *pg;
    int offset;
    if (envid2env(envid, &olde, 1) <0)
        return -E_BAD_ENV;
    if (user_mem_check(curenv, ps, sizeof(struct proc_status), PTE_U|PTE_P) <0)
        return -E_FAULT;
    *olde = ps->env;
    if ((pg=page_lookup(olde->env_pgdir, (void *)(USTACKTOP-PGSIZE), NULL))==NULL)
        return -E_FAULT;
    offset = olde->env_tf.tf_esp+PGSIZE-USTACKTOP;
    memmove(page2kva(pg), ps->stack, PGSIZE);
    cprintf("process %x has been restored\n",envid);
    return 0;
}
static int
sys_exec(uint32_t eip, uint32_t esp, void * v_ph, uint32_t phnum)
{
    
    curenv->env_tf.tf_eip = eip;
    curenv->env_tf.tf_esp = esp;
    
    int perm, i;
    uint32_t tmp = 0xe0000000;
    uint32_t va, end;
    struct PageInfo * pg;
    
    struct Proghdr * ph = (struct Proghdr *) v_ph;
    for (i = 0; i < phnum; i++, ph++) {
        if (ph->p_type != ELF_PROG_LOAD)
            continue;
        perm = PTE_P | PTE_U;
        if (ph->p_flags & ELF_PROG_FLAG_WRITE)
            perm |= PTE_W;
        
        end = ROUNDUP(ph->p_va + ph->p_memsz, PGSIZE);
        for (va = ROUNDDOWN(ph->p_va, PGSIZE); va != end; tmp += PGSIZE, va += PGSIZE) {
            if ((pg = page_lookup(curenv->env_pgdir, (void *)tmp, NULL)) == NULL)
                return -E_NO_MEM;
            if (page_insert(curenv->env_pgdir, pg, (void *)va, perm) < 0)
                return -E_NO_MEM;
            page_remove(curenv->env_pgdir, (void *)tmp);
        }
    }
    
    if ((pg = page_lookup(curenv->env_pgdir, (void *)tmp, NULL)) == NULL)
        return -E_NO_MEM;
    if (page_insert(curenv->env_pgdir, pg, (void *)(USTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W) < 0)
        return -E_NO_MEM;
    page_remove(curenv->env_pgdir, (void *)tmp);
    
    env_run(curenv);
    return 0;
}

//lab6 send packet
static int
sys_tx_pkt(struct tx_desc *td) {
    struct PageInfo *pp;
    pp=page_lookup(curenv->env_pgdir, (void *)(uint32_t)(td->addr),0);
    td->addr=page2pa(pp)|PGOFF(td->addr);
    while(1) {
        if (e1000_put_tx_desc(td) == 0) {
            break;
        }
    }
    return 0;
}
void
user_mem_page_replace(uintptr_t va, struct PageInfo *pt)
{
    user_mem_assert(curenv, (const void*)va, PGSIZE, PTE_U | PTE_P);
    pte_t *pte;
    struct PageInfo *page = page_lookup(curenv->env_pgdir, (void *)va, &pte);
    int ref = page->pp_ref;
    page->pp_ref = pt->pp_ref;
    pt->pp_ref = ref;
    *pte = page2pa(pt) | PGOFF(*pte);
    tlb_invalidate(curenv->env_pgdir, (void*)va);
    return;
}

static int
sys_rx_pkt(struct rx_desc *rd) {
    int r;
    struct rx_desc kr=*rd;
    struct PageInfo *pp;
    pp=page_lookup(curenv->env_pgdir, (void *)(uint32_t)(kr.addr),0);
    kr.addr=page2pa(pp)|PGOFF(kr.addr);
    r=e1000_get_rx_desc(&kr);
    if (r!=0) {
        return r;
    }
    user_mem_page_replace(rd->addr, pa2page(kr.addr));
    kr.addr=rd->addr;
    *rd=kr;
    return 0;
}


// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
    
    
    //panic("syscall not implemented");
    int ret = 0;
    switch (syscallno) {
        case SYS_cputs:
            sys_cputs((char*)a1, a2);
            ret = 0;
            break;
        case SYS_cgetc:
            ret = sys_cgetc();
            break;
        case SYS_getenvid:
            ret = sys_getenvid();
            break;
        case SYS_env_destroy:
            sys_env_destroy(a1);
            ret = 0;
            break;
        case SYS_yield:
            sys_yield();
            break;
        case SYS_exofork:
            ret=sys_exofork();
            break;
        case SYS_page_alloc:
            sys_page_alloc(a1,(void*)a2,a3);
            break;
        case SYS_page_map:
            sys_page_map(a1,(void*)a2,a3,(void*)a4,a5);
            break;
        case SYS_page_unmap:
            sys_page_unmap(a1,(void*)a2);
            break;
        case SYS_env_set_status:
            sys_env_set_status(a1,a2);
            break;
        
        //challenge
        case SYS_change_pr:
            ret=sys_change_pr(a1);
            break;
        case SYS_proc_save:
            ret=sys_proc_save(a1,(void*)a2);
            break;
        case SYS_proc_restore:
            ret=sys_proc_restore(a1,(void*)a2);
            break;
        case SYS_exec:
            ret=sys_exec((uint32_t)a1, (uint32_t)a2, (void *)a3, (uint32_t)a4);
            break;
        /////
        case SYS_env_set_pgfault_upcall:
            ret=sys_env_set_pgfault_upcall(a1,(void*)a2);
            break;
        case SYS_ipc_try_send:
            ret=sys_ipc_try_send(a1,a2,(void*)a3,a4);
            break;
        case SYS_ipc_recv:
            ret=sys_ipc_recv((void*)a1);
            break;
        case SYS_env_set_trapframe:
            sys_env_set_trapframe(a1,(void*)a2);
            break;
        case SYS_time_msec:
            ret=sys_time_msec();
            break;
        //lab6
        case SYS_tx_pkt:
            ret=sys_tx_pkt((void*)a1);
            break;
        case SYS_rx_pkt:
            ret=sys_rx_pkt((void*)a1);
            break;
            
            
        default:
            panic("here %e\n",syscallno);
            ret = -E_INVAL;
    }
    return ret;
}

void
syscall_fast()
{
    uint32_t syscallno,a1,a2,a3,a4;
    uint32_t ret_eip,ret_esp;
    asm volatile("mov %%eax,%0\n"
                 "mov %%edx,%1\n"
                 "mov %%ecx,%2\n"
                 "mov %%ebx,%3\n"
                 "mov %%edi,%4\n"
                 "mov %%esi,%5\n"
                 "mov 0(%%ebp),%%eax\n"
                 "mov %%eax,%6\n"
                 :"=m"(syscallno),"=m"(a1),"=m"(a2),"=m"(a3),"=m"(a4),"=m"(ret_eip),"=m"(ret_esp));
    int ret=0;
    switch (syscallno) {
        case SYS_cputs:
            sys_cputs((char*)a1, a2);
            ret = 0;
            break;
        case SYS_cgetc:
            ret = sys_cgetc();
            break;
        case SYS_getenvid:
            ret = sys_getenvid();
            break;
        case SYS_env_destroy:
            sys_env_destroy(a1);
            ret = 0;
            break;
        default:
            ret = -E_INVAL;
    }
    asm volatile("movl %0,%%eax\n"
                 "sysexit\n"
                 :
                 :"m"(ret), "d"(ret_eip),"c"(ret_esp));
}


