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

### Build iova_stress
```
# make
cc -o iova_stress -static -Os iova_stress.c
```

 ### Run this test with IOMMU group 83, and max iova space = 16T
```
 # iova_stress  -g 83 -s 16
  iova space:     0T      free memory:  1504G
  iova space:     1T      free memory:  1503G
  iova space:     2T      free memory:  1500G
  iova space:     3T      free memory:  1498G
  iova space:     4T      free memory:  1496G
  iova space:     5T      free memory:  1494G
  iova space:     6T      free memory:  1492G
  iova space:     7T      free memory:  1490G
  iova space:     8T      free memory:  1488G
  iova space:     9T      free memory:  1486G
  iova space:    10T      free memory:  1484G
  iova space:    11T      free memory:  1482G
  iova space:    12T      free memory:  1480G
  iova space:    13T      free memory:  1479G
  iova space:    14T      free memory:  1476G
  iova space:    15T      free memory:  1474G
```
