# Markov Chain Text Generator

This is small program which reads a corpus file of examples. Then a [Markov
Chain](https://en.wikipedia.org/wiki/Markov_chain) is built using that corpus to
produce text that is structured similarly.  If the corpus file is large enough,
the text that is produced might even be comphrensible a lot of the time.

## How do I use it?

Simply clone this repo and compile it by running `make` which will produce a
`markov` executable. Then run the program by giving it a corpus file.

One thing that needs to be known about the corpus file structure is that the
program doesn't have any idea about sentence structure, e.g. it doesn't treat
the period as a stopping point for a sentence. This program doesn't understand
the structure of any language, it simply reads white-space delimited chunks of
characters until it reaches a newline.  This isn't a limitation because it means
the program can be used to produce text for any purpose other than generating
English sentences.

I have added the first chapter of Pride and Prejudice (from the incredible
[Project Gutenberg](https://www.gutenberg.org/)) to the repo under the name
`sample.txt`. Here's some example output using that sample corpus:
    $ ./markov sample.txt
    “Do you will be sure! A single man of them, Mr. Bennet made no compassion for my share of them, Mr. Bennet was less difficult to hearing it.” 
