# Pci Status and Shortcomings 

This document will try to summarize the current status of PCIe in Gatery. 

## Features 

### Tlp's and headers

Gatery now has `RequestHeader` and `CompletionHeader`, both containing `CommonHeader`, the information common to any TLP. There are functions to build these from raw TLP's (BVec) and to convert back to raw TLP's (the convertion back to a BVec -raw- tlp is, for the moment, the typecast `(BVec)` operator ).

Attention! Request headers are always 4 dw for now. (normally they are only 3 dw when using 32bit addresses)

### Completer Port

The completer port chain looks something like this:

| IP completer request vendor unlocked port | -> request TLP  -> | TLP to TileLink | -> TileLink A -> | Completing Circuit |

 
| IP completer completion vendor unlocked port |  <- completion TLP <- | TileLink to Tlp | <- TileLink D <- | Completing Circuit | 

it supports only single-beat, 32-bit completions.

### Requester Port

The requester port chain looks something like this:

| IP requester request vendor unlocked port | <- request TLP <- | TileLink A to TLP | <- TileLink A <- | Requesting Circuit |

| IP requester completion vendor unlocked port | -> completion TLP -> | TLP to TileLink D | -> TileLink D -> | Requesting Circuit |

it supports only single-beat , 32-bit or 256-bit completions.

There is an extendible Pcie Host model which, for the moment, behaves like a host with respect to the requester port and completes requests made by the circuit. It also features many answering styles (normal, chopped into 64 byte chunks, unsupported request)

## Blackboxes

### Intel Agilex P-Tile

Intel Agilex P-Tile blackbox has been written and link-up has been tested. No vendor unlocking functions yet, so no software or hardware implementation of completer or requester yet.

### AMD (Xilinx) UltraScale+ Pcie4C

Ultrascale+ blackbox has been written and link-up tested. Vendor unlocking functions have been written and tested for the functionalities in Completer Port and Requester Port sections.

## Shortcomings

- the requester port cannot write data to the host memory yet
- a 512-bit bus (Tlp'side) can only accomodate a 256-bit Tilelink bus. It should be 512 -> 512 for full data throughput capabilities
- no straddling allowed (both xilinx and intel), which limits the throughput a lot, since the host will inevitably answer with 512-bit chunks, which, counting the header, will take 2 full 512-bit beats, wasting nearly 75% of a complete beat (or about ~40% of total throughput).
- no support for busses smaller than header size (3 or 4 dw depending on direction)
- no PCIe Validator model yet
- no bursting at all. Only single beat operation allowed
- no checking of extended tag functionality. This functionality is assumed and the assumption is not guaranteed.
- making a simple test is way too verbose. Normally people using pcie, do not care about pcie. It should be seamless

