<img src="imgs/header.png" width="50%">

Python Implementation of Gosper's hashlife algorithm. See [johnhw.github.io/hashlife](https://johnhw.github.io/hashlife/index.md.html) for a full explanation.

*An equivalent plain ISO C implementation of the Python code is available in [iso-c/](iso-c/)*

Usage:

```python
from hashlife import construct, advance, expand
from lifeparsers import autoguess_life_file
from render import render_img

pat, _ = autoguess_life_file("lifep/gun30.lif") 
node = construct(pat) # create quadtree
node_30 = advance(node, 30) # forward 30 generations
pts = expand(node_30) # convert to point list
render_img(pts) # render as image
```

<img src="imgs/gun30_30.png">

## C implementation

See [iso-c/README.md](iso-c/README.md) for details.

## Credits

Life patterns in `lifep/` collected by Alan Hensel.
