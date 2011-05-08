#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
	uint32_t curenv_index, i;
	if (!curenv)
		curenv_index = 0;
	else
		curenv_index = ENVX(curenv->env_id);
	i = (curenv_index + 1) % NENV;
	
	do {
		if (i != 0 && envs[i].env_status == ENV_RUNNABLE) {
			cprintf("(sched) next envid = %d\n", i);
			env_run(&envs[i]);
			return;
		}
		
		i = (i + 1) % NENV;
	} while(i != ((curenv_index + 1) % NENV));
	// reason behind do-while: want to allow the same proc to be scheduled

	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE) {
		cprintf("(sched) next envid = 0\n");
		env_run(&envs[0]);
	}
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}
