/*  Author: pasha.tatashin@soleen.com
 *
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
	if (group < 0) {
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
			fprintf(stderr, "VFIO_IOMMU_UNMAP_DMA failed at 0x%lx\n", dma_unmap.iova);
			exit(1);
		}
	}
}
