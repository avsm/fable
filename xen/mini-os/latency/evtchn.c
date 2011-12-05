#include <mini-os/os.h>
#include <mini-os/events.h>
#include <mini-os/sched.h>

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
	if (*cntr < NR_ITERATIONS || 1)
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

	while (cntr < NR_ITERATIONS || 1)
		;

	end = NOW();
	printk("All done; took %ld nanoseconds for %d(%d) iterations\n", end - start, NR_ITERATIONS, cntr);

	return 0;
}

static evtchn_port_t cpu0_port, cpu1_port;
static long cpu1_cntr;

static void
cpu1_handler(evtchn_port_t port, struct pt_regs *regs, void *ignore)
{
	if (smp_processor_id() != 1)
		printk("cpu1_handler running on vcpu %d\n", smp_processor_id());
	unmask_evtchn(port);
	notify_remote_via_evtchn(cpu0_port);
	cpu1_cntr++;
}

struct cpu0_state {
	long cntr;
	long last_report_cntr;
	unsigned long last_report_time;
	unsigned long start;
};

static void
cpu0_handler(evtchn_port_t port, struct pt_regs *regs, void *_state)
{
	struct cpu0_state *state = _state;
	unmask_evtchn(port);
	if (state->cntr == -1)
		return;
	if (smp_processor_id() != 0)
		printk("cpu0_handler running on vcpu %d\n", smp_processor_id());
	notify_remote_via_evtchn(cpu1_port);
	state->cntr++;
	if (state->cntr == state->last_report_cntr + NR_ITERATIONS) {
		unsigned long now_time = NOW();
		printk("Took %ld nanoseconds for %d iterations (%ld each); total %ld for %d (%ld) (cpu 1 did %ld)\n",
		       now_time - state->last_report_time,
		       state->cntr - state->last_report_cntr,
		       (now_time - state->last_report_time) / (state->cntr - state->last_report_cntr),
		       now_time - state->start,
		       state->cntr,
		       (now_time - state->start) / state->cntr,
			cpu1_cntr);
		state->last_report_cntr = state->cntr;
		state->last_report_time = now_time;
	}
}

void boot_vcpu(int id, void (*callback)(void), start_info_t *si);
void bind_evtchn_to_vcpu0(int idx);
void bind_evtchn_to_vcpu1(int idx);

static void
start_vcpu1(void)
{
	printk("Running in vcpu %d\n", smp_processor_id());
	cpu1_port = bind_ipi(1, cpu1_handler, NULL);
	unmask_evtchn(cpu1_port);
	printk("CPU %d(1) bound to port %d\n", smp_processor_id(), cpu1_port);
	bind_evtchn_to_vcpu1(cpu1_port);
	for (;;)
		;
}

static void
run_intradomain(start_info_t *si)
{
	struct cpu0_state state = {};
	int r;

	cpu1_cntr = -1;
	state.cntr = -1;
	cpu1_port = -1;

	cpu0_port = bind_ipi(0, cpu0_handler, &state);
	if (cpu0_port == -1) {
		printk("failed to set up IPI ports\n");
		return;
	}
	bind_evtchn_to_vcpu0(cpu0_port);

	unmask_evtchn(cpu0_port);

	boot_vcpu(1, start_vcpu1, si);

	printk("vcpu 1 launched\n");
	msleep(900);

	state.last_report_time = state.start = NOW();
	state.cntr = 0;
	r = notify_remote_via_evtchn(cpu1_port);
	if (r != 0)
		printk("failed to wake cpu1 (%d)!\n", r);

	while (1)
		;

/*
	end = NOW();
	printk("All done; took %ld nanoseconds for %d(%d) iterations\n", end - start, NR_ITERATIONS, cntr);
	xenbus_printf(XBT_NIL, "results", "res", "%ld", end - start);
*/
}

static void
real_app_main(void *_si)
{
	start_info_t *si = _si;
	char *cmd_line = (char *)si->cmd_line;
	int i;

	printk("Real app main started\n");
	while (cmd_line[0] == ' ')
		cmd_line++;
	for (i = 0; cmd_line[i]; i++)
		;
	while (i >= 0 && (cmd_line[i] == 0 || cmd_line[i] == ' ')) {
		cmd_line[i] = 0;
		i--;
	}
	if (!strcmp(cmd_line, "server")) {
		run_server();
	} else if (!strcmp(cmd_line, "client")) {
		run_client();
	} else if (!strcmp(cmd_line, "intradomain")) {
		run_intradomain(si);
	} else {
		printk("Command line should say either server, client, or intradomain; actually says ``%s''\n",
		       si->cmd_line);
	}
}

int
app_main(start_info_t *si)
{
	printk("app_main started %s\n", si->cmd_line);

	create_thread("worker", real_app_main, si);

	return 0;
}
