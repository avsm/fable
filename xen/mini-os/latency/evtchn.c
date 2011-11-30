#include <mini-os/os.h>
#include <mini-os/events.h>
#include <mini-os/sched.h>
#include <xen/vcpu.h>

#define NR_ITERATIONS 1000000

static void
server_handler(evtchn_port_t port, struct pt_regs *regs, void *ignore)
{
	notify_remote_via_evtchn(port);
}

static void
client_handler(evtchn_port_t port, struct pt_regs *regs, void *_cntr)
{
	int *cntr = _cntr;
	if (*cntr == -1)
		return;
	if (*cntr < NR_ITERATIONS)
		notify_remote_via_evtchn(port);
	(*cntr)++;
}

static int
run_server(void)
{
	int client_dom;
	int r;
	static evtchn_port_t port;

	printk("I am the server\n");
	do {
		client_dom = xenbus_read_integer("client_dom");
	} while (client_dom == -1);

	printk("Client domain is %d\n", client_dom);
	r = evtchn_alloc_unbound(client_dom, server_handler,
				 &port, &port);
	if (r) {
		printk("Failed to allocate unbound port; %d\n", r);
		return 1;
	}
	printk("Allocated port %d\n", port);

	unmask_evtchn(port);

	return 0;
}

static int
run_client(void)
{
	int server_dom;
	int server_port;
	int cntr;
	evtchn_port_t local_port;
	int r;
	int64_t start;
	int64_t end;

	do {
		server_dom = xenbus_read_integer("server_dom");
		server_port = xenbus_read_integer("server_port");
	} while (server_dom == -1 || server_port == -1);

	printk("Server domain:port is %d:%d\n", server_dom, server_port);

	cntr = -1;
	r = evtchn_bind_interdomain(server_dom, server_port,
				    client_handler, &cntr,
				    &local_port);
	if (r != 0) {
		printk("Error %d binding event channel\n", r);
		return 1;
	}
	printk("Bound to local port %d\n", local_port);

	unmask_evtchn(local_port);

	/* Give it a second to make sure everything is nice and
	 * stable. */
	/* For some reason msleep(1000) waits forever, but msleep(900)
	   does the right thing.  Meh. */
	msleep(900);

	if (cntr != -1)
		printk("Huh? cntr %d before we even started\n", cntr);
	cntr = 0;

	start = NOW();
	r = notify_remote_via_evtchn(local_port);
	if (r != 0)
		printk("Failed to notify remote on port %d!\n", local_port);

	while (cntr < NR_ITERATIONS)
		;

	end = NOW();
	printk("All done; took %ld nanoseconds for %d(%d) iterations\n", end - start, NR_ITERATIONS, cntr);

	return 0;
}

static evtchn_port_t cpu0_port, cpu1_port;

static unsigned char new_vcpu_stack[8192];
static void
__start_new_vcpu(unsigned long arg)
{
	printk("I am a new vcpu %ld\n", arg);
	for (;;)
		;
}

extern struct trap_info trap_table[];
void hypervisor_callback(void);
void failsafe_callback(void);

static void
cpu1_handler(evtchn_port_t port, struct pt_regs *regs, void *ignore)
{
	notify_remote_via_evtchn(cpu0_port);
}

static void
cpu0_handler(evtchn_port_t port, struct pt_regs *regs, void *_cntr)
{
	int *cntr = _cntr;
	if (*cntr == -1)
		return;
	if (*cntr < NR_ITERATIONS)
		notify_remote_via_evtchn(cpu1_port);
	(*cntr)++;
}

static void
run_intradomain(start_info_t *si)
{
	/* Bring up an additional vcpu */
	struct vcpu_guest_context initial_state = {};
	struct cpu_user_regs *regs = &initial_state.user_regs;
	struct trap_info *traps = &initial_state.trap_ctxt[0];
	int r;
	int cntr;
	unsigned long start, end;

	cntr = -1;
	cpu0_port = bind_ipi(0, cpu0_handler, &cntr);
	cpu1_port = bind_ipi(1, cpu1_handler, NULL);
	if (cpu0_port == -1 || cpu1_port == -1) {
		printk("failed to set up IPI ports\n");
		return;
	}

	unmask_evtchn(cpu0_port);
	unmask_evtchn(cpu1_port);

	initial_state.flags = VGCF_in_kernel;

	regs->rip = (unsigned long)__start_new_vcpu;
	regs->rsp = (unsigned long)new_vcpu_stack + sizeof(new_vcpu_stack);
	regs->rdi = 0xf001;
	regs->cs = FLAT_KERNEL_CS;
	regs->ss = FLAT_KERNEL_SS;
	regs->es = FLAT_KERNEL_DS;
	regs->ds = FLAT_KERNEL_DS;
	regs->fs = FLAT_KERNEL_DS;
	regs->gs = FLAT_KERNEL_DS;

	initial_state.kernel_ss = FLAT_KERNEL_DS;
	initial_state.ctrlreg[3] = virt_to_mach(si->pt_base);
	memcpy(traps, trap_table, sizeof(trap_table[0]) * 17);
	initial_state.event_callback_eip = (unsigned long)hypervisor_callback;
	initial_state.failsafe_callback_eip = (unsigned long)failsafe_callback;

	r = HYPERVISOR_vcpu_op(VCPUOP_initialise, 1, &initial_state);
	if (r != 0) {
		printk("cannot initialise vcpu 1\n");
		return;
	}
	r = HYPERVISOR_vcpu_op(VCPUOP_up, 1, NULL);
	if (r != 0) {
		printk("cannot up vcpu 1\n");
		return;
	}

	printk("vcpu 1 launched\n");
	msleep(900);

	cntr = 0;
	start = NOW();
	r = notify_remote_via_evtchn(cpu1_port);
	if (r != 0)
		printk("failed to wake cpu1 (%d)!\n", r);

	while (cntr < NR_ITERATIONS)
		;
	end = NOW();

	printk("All done; took %ld nanoseconds for %d(%d) iterations\n", end - start, NR_ITERATIONS, cntr);
}

static void
real_app_main(void *_si)
{
	start_info_t *si = _si;
	printk("Real app main started\n");
	if (!strcmp((char *)si->cmd_line, "server")) {
		run_server();
	} else if (!strcmp((char *)si->cmd_line, "client")) {
		run_client();
	} else if (!strcmp((char *)si->cmd_line, "intradomain")) {
		run_intradomain(si);
	} else {
		printk("Command line should say either server or client\n");
	}
}

int
app_main(start_info_t *si)
{
	printk("app_main started %s\n", si->cmd_line);

	create_thread("worker", real_app_main, si);

	return 0;
}
