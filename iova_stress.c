/*
 * Author: pasha.tatashin@soleen.com
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/vfio.h>
#include <sys/sysinfo.h>
#include <sys/param.h>
#include <time.h>

#define IOMMU_GROUP_DEFAULT		83
#define IOVA_SPACE_SIZE_DEFAULT		45
#define DMA_SIZE			sysconf(_SC_PAGE_SIZE)

static unsigned long gethrtime(void)
{
	struct timespec ts;
	unsigned long result;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

	result = 1000000000ull * (unsigned long)ts.tv_sec;
	result += ts.tv_nsec;

	return result;
}

static void dma_map_unmap(int container, unsigned long iova_space,
			  unsigned long dma_size, int verbose) {

	struct vfio_iommu_type1_dma_map dma_map = { 0 };
	struct vfio_iommu_type1_dma_unmap dma_unmap = { 0 };

	dma_map.argsz = sizeof(dma_map);
	dma_unmap.argsz = sizeof(dma_unmap);

	/* Allocate some space and setup a DMA mapping */
	dma_map.vaddr = (__u64)mmap(0, dma_size, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	if ((void *)dma_map.vaddr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	dma_unmap.size = dma_map.size = dma_size;
	dma_unmap.iova = dma_map.iova = 0;
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	while (dma_map.iova < iova_space << 40) {
		/* Update status every 1T of VA space */
		if (verbose && (dma_map.iova % (1ul << 40)) == 0) {
			struct sysinfo info;

			sysinfo(&info);
			printf("iova space: %5ldT\tfree memory: %5ldG\n", dma_map.iova >> 40, info.freeram * info.mem_unit >> 30);
		}
		dma_map.iova += MAX(dma_size, 2 << 20);
		dma_unmap.iova += MAX(dma_size, 2 << 20);
		if (ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map)) {
			/* might be pre-existing entries, ignore */
			continue;
		}
		if (ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap)) {
			fprintf(stderr, "VFIO_IOMMU_UNMAP_DMA failed at 0x%lx\n", dma_unmap.iova);
			exit(1);
		}
	}

	munmap((void *)dma_map.vaddr, dma_size);
}

int
main (int argc, char *argv[]) {
	int container, group, opt, verbose = 0;
	int iommu_group = IOMMU_GROUP_DEFAULT;
	unsigned long iova_space = IOVA_SPACE_SIZE_DEFAULT;
	struct vfio_group_status group_status = { .argsz = sizeof(group_status) };
	long freeram_before, freeram_after, iommu;
	unsigned long s1, s2, dma_size;
	unsigned long first_dma_size = DMA_SIZE;
	unsigned long second_dma_size = 0;
	struct sysinfo info;
	char group_path[256];
	int rv = EXIT_FAILURE;

	while ((opt = getopt(argc, argv, "g:s:hS:v")) != -1) {
		switch (opt) {
		case 'g':
			iommu_group = atoi(optarg);
			break;
		case 's':
			iova_space = atoi(optarg);
			break;
		case 'S':
			/* argument is in MB */
			second_dma_size = atoi(optarg) << 20;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			rv = EXIT_SUCCESS;
		default: /* '?' */
			fprintf(stderr, "Usage: %s [-g iommu_group] [-s iova_max (in Terabytes)] [-S second_run_dma_size (in MB)] [-v]\n",
				argv[0]);
			exit(rv);
		}
	}

	/* Create a new container */
	container = open("/dev/vfio/vfio", O_RDWR);
	if (container < 0) {
		perror("open(\"/dev/vfio/vfio\"");
		exit(rv);
	}

	/* Open the group */
	sprintf(group_path, "/dev/vfio/%d", iommu_group);
	group = open(group_path, O_RDWR);
	if (group < 0) {
		perror("open(group_path)");
		exit(rv);
	}

	/* Add the group to the container */
	if (ioctl(group, VFIO_GROUP_SET_CONTAINER, &container)) {
		perror("ioctl(group, VFIO_GROUP_SET_CONTAINER, &container)");
		exit(rv);
	}

	/* Enable the IOMMU model we want */
	if (ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)) {
		perror("ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)");
		exit(rv);
	}

	dma_size = first_dma_size;;
	sysinfo(&info);
	freeram_before = info.freeram * info.mem_unit;
	do {
		s1 = gethrtime();
		dma_map_unmap(container, iova_space, dma_size, verbose);
		s2 = gethrtime();
		sysinfo(&info);
		freeram_after = info.freeram * info.mem_unit;

		iommu = freeram_before - MIN(freeram_before, freeram_after);
		printf("dma_size: %7ldK iova space: %ldT iommu: ~%7ldM time: %4ld.%03lds\n",
			dma_size / 1024,
			iova_space,
			iommu >> 20,
			(s2 - s1) / 1000000000, ((s2 - s1) % 1000000000) / 1000000);
		dma_size = second_dma_size;
		second_dma_size = 0;
	} while (dma_size);

	return EXIT_SUCCESS;
}
