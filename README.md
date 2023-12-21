# iova_stress
DMA_MAP / DMA_UNMAP 4K page every 2M in IOVA SPACE for a given number of IOVA space terabytes.

## Example usage
### Find a suitable device:
```
# lspci -v
  02:00.1 Ethernet controller: Intel Corporation Ethernet Adaptive Virtual Function (rev 11)
        Subsystem: Google, Inc. Device 00c3
        Flags: fast devsel, NUMA node 0, IOMMU group 83
        Memory at 3224d000000 (64-bit, prefetchable) [virtual] [size=4M]
        Memory at 32311d20000 (64-bit, prefetchable) [virtual] [size=64K]
        Capabilities: [b0] MSI-X: Enable- Count=2048 Masked-
        Capabilities: [70] Express Endpoint, MSI 00
        Capabilities: [100] Advanced Error Reporting
        Capabilities: [148] Alternative Routing-ID Interpretation (ARI)
        Capabilities: [158] Transaction Processing Hints
        Capabilities: [2ec] Access Control Services
        Capabilities: [1e4] Address Translation Service (ATS)
```
 
### Get vendor id for this device:
```
# lspci -n -s 0000:02:00.1
   02:00.1 0200: 8086:1889 (rev 11)
```
 
### Add it to a vfio-pci:
```
echo 8086 1889 > /sys/bus/pci/drivers/vfio-pci/new_id
```

### Use vfio_new_id
Alternativly to the above, use vfio_new_id script to add vfio ids for
each device in the system.
```
# vfio_new_id
```

### Build iova_stress
```
$ make
cc -o iova_stress -static -Os iova_stress.c
```

 ### Run this test with IOMMU group 83, and max iova space = 16T

Kernel: 6.7, Driver: Intel IOMMU:
```
yqbtg12:/home# ./iova_stress -s 16 -g 83
dma_size:       4K iova space: 16T iommu: ~  32783M time:   22.297s
```
Above example show that iommu page table uses approxemetly 32G of system memoery
when 16T of IOVA space is touched.
