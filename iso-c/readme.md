# C Implementation of Gosper's Hashlife Algorithm

This is written to be equivalent to the Python version. It is designed for clarity and is not particularly optimised. 

## Building

```
make
```

will build `hashlife`. To build the tests, `make test` then `./test_hashlife`.

## Usage

```
./hashlife pat/breeder.rle 1024
```

will run `breeder.rle` forward by 1024 generations and output the resulting pattern in RLE format.

## Implementation

This implementation exposes roughly the same API as the Python implementation. It uses a very simple linear probing hash table, which is resized to keep a max 25% load factor. This isn't memory efficient but it is simple and keeps things fast enough for real use. 

Nodes in the quadtree are interned and given unique stable integer IDs. These are stored in the hash table for fast `join` operations. 

Successive generations are also cached, in a second "parallel" hash table, keyed by node ID and generation. The generation cache is dead-simple: no probing and a new successor kicks out any previous cached values. As the load is low and the successor cache is never required (it can always be recomputed) this gives a quick, simple implementation without managing a second hash table.

See [hashlife.h](hashlife.h) for details.