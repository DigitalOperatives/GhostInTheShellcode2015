# Solution to the 300 Programmign Challenge: Edgy

The server that was running during the CTF is available [here](https://github.com/LightningTH/GiTS/tree/master/2015/Edgy).

This challenge asked the participant to solve a series of increasingly
challenging [Blek](http://blekgame.com/)-like ASCII puzzles.
Straightforward application of conventional search algorithms proved
fruitless, since the puzzles got exponentially harder and harder.  A
neat trick using shifts in the obstacle field proved to be the winning
solution.  There are a couple other solutions out there that took a
similar approach, but this one is unique if only because it was
written in C++ as opposed to the ubiquitous Python.  (I later
regretted that choice after learning that the last challenge required
`zlib` decompression.)

A complete write-up is forthcoming on the [Digital Operatives
blog](http://digitaloperatives.com/blog/).