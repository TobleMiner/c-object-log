Objectlog
=========

Objectlog is an advanced, overwriting log structure for storage and retrieval
of binary data implemented in pure C.  
It supports storage of discontinuous data in a discontinuous memory space and
is thus ideally suited for platform with fragmented RAM address space.  
Since this is a log structure old entries will be overwritten automatically
making it a perfect match for all types of time series data.

# Usage example

See [example.c](/example.c) for a basic usage example.
