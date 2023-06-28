/*
 *  gcc -Os -static -o iova_stress iova_stress.c
 *
 * DMA_MAP / DMA_UNMAP 4K page every 2M in IOVA SPACE for a given number of IOVA
 * space terabytes.
 *
 * Example usage:
 * Finad a suitable device:
 * # lspci -v
 * 02:00.1 Ethernet controller: Intel Corporation Ethernet Adaptive Virtual Function (rev 11)
 *       Subsystem: Google, Inc. Device 00c3
 *       Flags: fast devsel, NUMA node 0, IOMMU group 83
 *       Memory at 3224d000000 (64-bit, prefetchable) [virtual] [size=4M]
 *       Memory at 32311d20000 (64-bit, prefetchable) [virtual] [size=64K]
 *       Capabilities: [b0] MSI-X: Enable- Count=2048 Masked-
 *       Capabilities: [70] Express Endpoint, MSI 00
 *       Capabilities: [100] Advanced Error Reporting
 *       Capabilities: [148] Alternative Routing-ID Interpretation (ARI)
 *       Capabilities: [158] Transaction Processing Hints
 *       Capabilities: [2ec] Access Control Services
 *       Capabilities: [1e4] Address Translation Service (ATS)
 *
 * Get vendor id for this device:
 * # lspci -n -s 0000:02:00.1
 *  02:00.1 0200: 8086:1889 (rev 11)
 *
 * Add it to a vfio-pci:
 *  echo 8086 1889 > /sys/bus/pci/drivers/vfio-pci/new_id
 *
 * Run this test with IOMMU group 83, and max iova space = 16T
 * # time  iova_stress  -g 83 -s 16
 * iova space:     0T      free memory:  1504G
 * iova space:     1T      free memory:  1503G
 * iova space:     2T      free memory:  1500G
 * iova space:     3T      free memory:  1498G
 * iova space:     4T      free memory:  1496G
 * iova space:     5T      free memory:  1494G
 * iova space:     6T      free memory:  1492G
 * iova space:     7T      free memory:  1490G
 * iova space:     8T      free memory:  1488G
 * iova space:     9T      free memory:  1486G
 * iova space:    10T      free memory:  1484G
 * iova space:    11T      free memory:  1482G
 * iova space:    12T      free memory:  1480G
 * iova space:    13T      free memory:  1479G
 * iova space:    14T      free memory:  1476G
 * iova space:    15T      free memory:  1474G
 *
 * real    0m24.699s
 * user    0m0.360s
 * sys     0m24.267s
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/vfio.h>
#include <sys/sysinfo.h>

#define IOMMU_GROUP_DEFAULT		83
#define IOVA_SPACE_SIZE_DEFAULT		45
#define DMA_SIZE			sysconf(_SC_PAGE_SIZE)

int
main (int argc, char *argv[]) {
	int container, group, opt;
	int iommu_group = IOMMU_GROUP_DEFAULT;
	unsigned long iova_space = IOVA_SPACE_SIZE_DEFAULT;
	struct vfio_group_status group_status = { .argsz = sizeof(group_status) };
	struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
	struct vfio_iommu_type1_dma_unmap dma_unmap = { .argsz = sizeof(dma_unmap) };
	struct sysinfo info;
	char group_path[256];

	while ((opt = getopt(argc, argv, "g:s:h")) != -1) {
		switch (opt) {
		case 'g':
			iommu_group = atoi(optarg);
			break;
		case 's':
			iova_space = atoi(optarg);
			break;
		case 'h':
		default: /* '?' */
			fprintf(stderr, "Usage: %s [-g iommu_group] [-m iova_max (in Terabytes)]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	/* Create a new container */
	container = open("/dev/vfio/vfio", O_RDWR);
	if (container < 0) {
		perror("open(\"/dev/vfio/vfio\"");
		exit(1);
	}

	/* Open the group */
	sprintf(group_path, "/dev/vfio/%d", iommu_group);
	group = open(group_path, O_RDWR);
	if (container < 0) {
		perror("open(group_path)");
		exit(1);
	}

	/* Add the group to the container */
	if (ioctl(group, VFIO_GROUP_SET_CONTAINER, &container)) {
		perror("ioctl(group, VFIO_GROUP_SET_CONTAINER, &container)");
		exit(1);
	}

	/* Enable the IOMMU model we want */
	if (ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)) {
		perror("ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)");
		exit(1);
	}

	/* Allocate some space and setup a DMA mapping */
	dma_map.vaddr = (__u64)mmap(0, DMA_SIZE, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	if ((void *)dma_map.vaddr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	dma_unmap.size = dma_map.size = DMA_SIZE;
	dma_unmap.iova = dma_map.iova = 0;
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	/* Do MAP_DMA / UNMAP_DMA */
	while (dma_map.iova < iova_space << 40) {
		/* Update status every 1T of VA space */
		if ((dma_map.iova % (1ul << 40)) == 0) {
			sysinfo(&info);
			printf("iova space: %5ldT\tfree memory: %5ldG\n", dma_map.iova >> 40, info.freeram * info.mem_unit >> 30);
		}
		dma_map.iova += 2 << 20;
		dma_unmap.iova += 2 << 20;
		if (ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map)) {
			/* might be pre-existing entries, ignore */
			continue;
		}
		if (ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap)) {
			fprintf(stderr, "MAP_UNDMA failed at 0x%lx\n", dma_unmap.iova);
			exit(1);
		}
	}
}
