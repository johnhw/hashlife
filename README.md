<img src="imgs/header.png" width="50%">

Implementation of Gosper's hashlife algorithm. See [johnhw.github.io/hashlife](https://johnhw.github.io/hashlife/index.md.html) for a full explanation.

Usage:

```python
from hashlife import construct, advance, expand
from lifeparsers import autoguess_life_file
from render import render_img

pat, _ = autoguess_life_file("lifep/gun30.lif")
node = construct(pat) # create quadtree
node_30 = advance(node, 30)
pts = expand(node_30)
render_img(pts)
```

<img src="imgs/gun30_30.png">

## Credits

Life patterns in `lifep/` collected by Alan Hensel.
