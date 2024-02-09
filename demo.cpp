#include <wsl/winadapter.h>
#include <d3dkmthk.h>

#include <unistd.h>
#include <linux/unistd.h>
#include <sys/mman.h>

#include <cstdio>

D3DKMT_HANDLE                   gAdapter;
D3DKMT_HANDLE			gDevice;
D3DKMT_HANDLE                   gContext;
D3DKMT_HANDLE                   gQueue;
D3DKMT_HANDLE                   gPagingQueue;
D3DKMT_HANDLE               	gPagingFence;
D3DKMT_HANDLE 			gFence;
uint64_t			gFenceAddr;

static void wait_syncobj(D3DKMT_HANDLE syncobj, UINT64 value)
{
	D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU wait_args = {
		.hDevice = gDevice,
		.ObjectCount = 1,
		.ObjectHandleArray = &syncobj,
		.FenceValueArray = &value,
	};

	NTSTATUS ret = D3DKMTWaitForSynchronizationObjectFromCpu(&wait_args);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTWaitForSynchronizationObjectFromCpu failed, ret = %08x\n", ret);
		exit(-1);
	}
}

int main (int argc, char *argv[])
{
	D3DKMT_ADAPTERINFO adapter = { };

	D3DKMT_ENUMADAPTERS2 enum_adapter = {
		.NumAdapters = 1,
		.pAdapters = &adapter,
	};

	NTSTATUS ret = D3DKMTEnumAdapters2(&enum_adapter);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTEnumAdapters2 failed, ret = %08x\n", ret);
		return -1;
	}

	D3DKMT_OPENADAPTERFROMLUID open_adapter = {
		.AdapterLuid = adapter.AdapterLuid,
	};

	ret = D3DKMTOpenAdapterFromLuid(&open_adapter);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTOpenAdapterFromLuid failed, ret = %08x\n", ret);
		return -1;
	}
	gAdapter = open_adapter.hAdapter;

	D3DKMT_CREATEDEVICE create_device = {
		.hAdapter = gAdapter,
	};

	ret = D3DKMTCreateDevice(&create_device);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTCreateDevice failed, ret = %08x\n", ret);
		return -1;
	}
	gDevice = create_device.hDevice;

	D3DKMT_CREATECONTEXTVIRTUAL create_context = {
		.hDevice = gDevice,
		.NodeOrdinal = 1,
		.EngineAffinity = 1 << 0,
		.ClientHint = D3DKMT_CLIENTHINT_OPENCL,
	};

	ret = D3DKMTCreateContextVirtual(&create_context);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTCreateContextVirtual failed, ret = %08x\n", ret);
		return -1;
	}
	gContext = create_context.hContext;

	uint32_t alloc_size = 4096 * 4;
	void* host_ptr = mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (host_ptr == MAP_FAILED) {
		printf ("mmap failed\n");
		return -1;
	}
	memset(host_ptr, 0, alloc_size);

	uint32_t alloc_priv[134] = {
		0x000001d8, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000001,
		0x000001d8, 0x00000001, 0x00000080, 0x00000000, 0x04000008, 0x00004000, 0x00001000, 0x00000001,
		0x00000004, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00004000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000000c4, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000114, 0x00000000, 0x00000000, 0x000000c4, 0x00000000, 0x00000000, 0x00000000,
	};

	alloc_priv[21] = alloc_size;
	alloc_priv[42] = alloc_size;

	D3DDDI_ALLOCATIONINFO2 alloc_info = {};
	alloc_info.pSystemMem = host_ptr;
	alloc_info.pPrivateDriverData = alloc_priv;
	alloc_info.PrivateDriverDataSize = sizeof(alloc_priv);

	D3DKMT_CREATEALLOCATION crate_alloc = {};
	crate_alloc.hDevice = gDevice;
	crate_alloc.pAllocationInfo2 = &alloc_info;
	crate_alloc.NumAllocations = 1;
	// crate_alloc.Flags.ExistingSysMem = 1;

	ret = D3DKMTCreateAllocation2(&crate_alloc);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTCreateAllocation2 failed, ret = %08x\n", ret);
		return -1;
	}

	D3DKMT_CREATEPAGINGQUEUE create_page_queue = { };
	create_page_queue.hDevice = gDevice;
	create_page_queue.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;

	ret = D3DKMTCreatePagingQueue(&create_page_queue);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTCreatePagingQueue failed, ret = %08x\n", ret);
		return -1;
	}

	gPagingQueue = create_page_queue.hPagingQueue;
	gPagingFence = create_page_queue.hSyncObject;

	D3DDDI_MAPGPUVIRTUALADDRESS map_va = { };
	map_va.hPagingQueue = gPagingQueue;
	map_va.hAllocation = alloc_info.hAllocation;
	map_va.SizeInPages = alloc_size/4096;
	map_va.Protection.Write = 1;

	ret = D3DKMTMapGpuVirtualAddress(&map_va);
	if (ret != STATUS_SUCCESS && ret != STATUS_PENDING) {
		printf("D3DKMTMapGpuVirtualAddress failed, ret = %08x\n", ret);
		return -1;
	}

	wait_syncobj(gPagingFence, map_va.PagingFenceValue);

	D3DDDI_MAKERESIDENT make_resident = {
		.hPagingQueue = gPagingQueue,
		.NumAllocations = 1,
		.AllocationList = &alloc_info.hAllocation,
	};

	ret = D3DKMTMakeResident(&make_resident);
	if (ret != STATUS_SUCCESS && ret != STATUS_PENDING) {
		printf("D3DKMTMakeResident failed, ret = %08x\n", ret);
		return -1;
	}

	wait_syncobj(gPagingFence, make_resident.PagingFenceValue);

	D3DKMT_CREATESYNCHRONIZATIONOBJECT2 create_fence = {};
	create_fence.hDevice = gDevice;
	create_fence.Info.Type = D3DDDI_MONITORED_FENCE;
	create_fence.Info.MonitoredFence.InitialFenceValue = 0,
	create_fence.Info.MonitoredFence.EngineAffinity = 1 << 0;

	ret = D3DKMTCreateSynchronizationObject2(&create_fence);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTCreateSynchronizationObject2 failed, ret = %08x\n", ret);
		return -1;
	}

	gFence = create_fence.hSyncObject;
	gFenceAddr = (uint64_t)create_fence.Info.MonitoredFence.FenceValueCPUVirtualAddress;

	uint64_t command_size = 4096;
	uint64_t command_gpu_va = map_va.VirtualAddress;
	uint32_t *command_cpu_va = (uint32_t *)host_ptr;

	for (int i = 0; i < 4096 / 4; i++)
		command_cpu_va[i] = 0xffff1000; // nop

	uint64_t src_gpu_va = map_va.VirtualAddress + 4096;
	uint64_t dst_gpu_va = map_va.VirtualAddress + 4096 + 4096;

	uint64_t *src_cpu = (uint64_t *)((char *)host_ptr + 4096);
	uint64_t *dst_cpu = (uint64_t *)((char *)host_ptr + 4096 + 4096);

	*src_cpu = 0xdeadbeefdeadbeef;

	// copy 8 bytes from src_gpu_va to dst_gpu_va
	command_cpu_va[0] = 0xc0055000;
	command_cpu_va[1] = 0x60300000;
	command_cpu_va[2] = src_gpu_va;
	command_cpu_va[3] = (src_gpu_va >> 32);
	command_cpu_va[4] = dst_gpu_va;
	command_cpu_va[5] = (dst_gpu_va >> 32);
	command_cpu_va[6] = 8; // byte count

	uint32_t submission_priv[18] = {
		0x00000005, 0x00000001, 0x00000000, 0x00000040,
		0x00001000, 0x00000000, 0x00000000, 0x00000000,
		0x30000000, 0x00000000, 0x00000000, 0x00000000,
	};

	*(uint64_t *)(submission_priv + 8) = command_gpu_va;

	D3DKMT_SUBMITCOMMAND submit_command = {};
	submit_command.Commands = command_gpu_va;
	submit_command.CommandLength = command_size;
	submit_command.BroadcastContextCount = 1;
	submit_command.BroadcastContext[0] = gContext;
	submit_command.pPrivateDriverData = submission_priv,
	submit_command.PrivateDriverDataSize = sizeof(submission_priv),

	ret = D3DKMTSubmitCommand(&submit_command);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTSubmitCommand failed, ret = %08x\n", ret);
		return -1;
	}

	uint64_t signal_value = 0x1234;

	D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU signal_fence = { };
	signal_fence.hContext = gContext;
	signal_fence.ObjectCount = 1;
	signal_fence.ObjectHandleArray = &gFence;
	signal_fence.MonitoredFenceValueArray = &signal_value;

	ret = D3DKMTSignalSynchronizationObjectFromGpu(&signal_fence);
	if (ret != STATUS_SUCCESS) {
		printf("D3DKMTSignalSynchronizationObjectFromGpu failed, ret = %08x\n", ret);
		return -1;
	}

	int pid = fork();
	if (!pid) {
		while ( *(uint64_t *)gFenceAddr != signal_value) {}

		printf("child: tgid = %ld, pid = %d, dst_cpu = %016lx\n",
		       syscall(__NR_gettid), getpid(), *((uint64_t *)dst_cpu));
		exit(0);
	}

	// trigger copy-on-write from parent process
	*(uint32_t *)((char *)host_ptr + 4096 + 4096 + 1024) = 0x0;

	wait_syncobj(gFence, signal_value);

	printf("parent: tgid = %ld, pid = %d, dst_cpu = %016lx\n",
	       syscall(__NR_gettid), getpid(), *((uint64_t *)dst_cpu));

	return 0;
}
