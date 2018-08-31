#include <iostream>
#include <unordered_map>
#include <utility>
#include <string>
#include <random>
#include <algorithm>
#include <functional>
#include <fstream>
#include <sstream>
#include <map>

#define STOP_CHAR '\n'

/*
 * Several problems with the parser:
 *  - how to handle context specific 'stop_chars'? For example, in a quotation
 *  how to we handle a period showing up? How to handle 'Mrs. Long'?
 *
 * The easiest way to handle this is to set only the stop character for \n and
 * let the 'word' be anything including punction. E.g. a word can be 'and',
 * 'Mrs.', 'Long', 'here,' and 'immediately;'.
 *
 * This means the Markov Chain will take the form of the text it's given in a
 * literal sense (the actually look of the text) and in the sense of word
 * choice and placement inside a sentence.
 *
 *  Any more advanced and we must get into NLP/full-blown parser.
 */
class Parser {
public:
    Parser (std::stringstream &stream)
        : stream(stream)
    {
    }

    std::string
    next ()
    {
        std::string buff;
        char next;

        /* Skip any preceding whitespace */
        while (!done() && stream.peek() == ' ')
            stream.get();

        /* 
         * If we've ran into the stop character, skip all of them and return
         * the stop character as a string.
         */
        if (!done() && stream.peek() == STOP_CHAR) {
            while (!done() && stream.peek() == STOP_CHAR)
                stream.get();
            return std::string(1, STOP_CHAR);
        }

        while (!done()) {
            next = stream.peek();

            /* stop after hitting any whitespace */
            if (next == ' ')
                break;

            /* stop after hitting the stop character */
            if (next == STOP_CHAR)
                break;

            /* finally eat the character on the stream */
            buff += stream.get();
        }

        return buff;
    }

    bool
    done ()
    {
        return (stream.rdbuf()->in_avail() == 0);
    }

protected:
    std::stringstream &stream;
};

class Word {
public:
    Word ()
        : word_string(std::string())
        , num_transitions(0)
        , delta(std::discrete_distribution<int>())
    { }

    Word (std::string word)
        : word_string(word)
        , num_transitions(0)
        , delta(std::discrete_distribution<int>())
    { }

    /*
     * Update the list of transitions for this word.
     */
    void
    update_transition (std::string next)
    {
        transitions[next]++;
        num_transitions++;
    }

    /*
     * Regenerate the delta distribution and its lookup table.
     */
    void
    cache ()
    {
        std::vector <double> p; /* probabilties */

        delta_lookup.clear();

        /* The index of the `delta_lookup` corresponds to the index in `p'. */
        for (auto &pair : transitions) {
            delta_lookup.push_back(pair.first);
            p.push_back((double) pair.second / (double) num_transitions);
        }

        delta = std::discrete_distribution<int>(p.begin(), p.end());
    }

    void
    inspect ()
    {
        int transition_count = 0;
        double prob_count = 0.0;

        printf("\"%s\"\n", word_string.c_str());
        for (auto &pair : transitions) {
            printf("\t -> %s probability (%d / %d): %lg\n",
                    pair.first.c_str(),
                    pair.second,
                    num_transitions,
                    (double) pair.second / (double) num_transitions);
            transition_count += pair.second;
            prob_count += (double) pair.second / (double) num_transitions;
        }

        printf("\t total transitions: %d / %d => %lg\n",
                transition_count, num_transitions, prob_count);
    }

    std::string
    next (std::mt19937 &generator)
    {
        return delta_lookup[delta(generator)];
    }

    std::string
    string ()
    {
        if (word_string == std::string()) {
            std::cerr << "Word has no string value!" << std::endl;
            exit(1);
        }
        return word_string;
    }
    
protected:
    std::string word_string;

    /*
     * Every word that followed the current word and the amount of times it
     * appeared.
     */
	std::map<std::string, int> transitions;

    /*
     * The total number of transitions that occured in the corpus from this
     * word.
     */
    int num_transitions;

    /*
     * This will return some integer based on the distribution we give it. The
     * number it returns is the index into `transition`.
     */
    std::discrete_distribution<int> delta;

    /*
     * The lookup table for our delta transition. Initialized in the `cache` 
     * method.
     */
	std::vector<std::string> delta_lookup;
};

class MarkovChain {
public:
    MarkovChain ()
        : generator(NULL)
        , dictionary(NULL)
    { }

    MarkovChain (std::mt19937 *gen, std::unordered_map<std::string, Word> *dict)
        : generator(gen)
        , dictionary(dict)
    { }

    bool
    done ()
    {
        return (current_word == std::string(1, STOP_CHAR));
    }

    std::string
    current ()
    {
        return current_word;
    }

    /*
     * Return the next word from the current.
     */
    std::string
    next ()
    {
        std::string key = current_word;

        if (!(dictionary || generator)) {
            std::cerr << "Invalid Markov Chain invoked!" << std::endl;
            exit(1);
        }

        if (key.empty()) {
            /* "STOP_CHAR" is the key to the starting word (or empty string) */
            key = std::string(1, STOP_CHAR);
        }

        current_word = (*dictionary)[key].next(*generator);
        return current_word;
    }

protected:
    std::mt19937 *generator;
    std::string current_word; /* used as a key into dictionary */
    std::unordered_map<std::string, Word> *dictionary;
};

class Corpus {
public:
    Corpus ()
        : not_built(true)
    {
        /*
         * A ridiculous incantation which basically just gets 1024 random bytes
         * from the random_device and uses them as a seed for the random number
         * generator.
         */
        std::mt19937::result_type data[1024];
        std::random_device source;
        std::generate(std::begin(data), std::end(data), std::ref(source));
        std::seed_seq seeds(std::begin(data), std::end(data));
        generator = std::mt19937(seeds);
    }

    int
    num_words ()
    {
        return dictionary.size();
    }

    void
    build (std::string filename)
    {
        std::ifstream file(filename);
        std::stringstream buff;

        buff << file.rdbuf();
        file.close();
        build(buff);
    }

    /*
     * Build the corpus from the stream.
     */
    void
    build (std::stringstream &stream)
    {
        std::string curr, next;
        std::string stop_string;
        Parser p(stream);

        /*
         * `p.next` can return a string that contains STOP_CHAR which is
         * a delimiter we use to represent the `end' of a Markov chain.
         *
         * Since the string "STOP_CHAR" will not be read in under normal
         * conditions, it can be used as the key for the dictionary for the
         * start word. For the first run of the loop, we must manually set
         * it.
         */
        stop_string = std::string(1, STOP_CHAR);
        curr = stop_string;
        while (!p.done()) {
            /*
             * If p.next produces "STOP_CHAR" here, it is not an issue. Since
             * "STOP_CHAR" is the key for the starting transition, that means
             * as the Markov Chains inevitably end their transition, they will
             * produce the "STOP_CHAR" string. This means the chain can be
             * restarted because that key is for the starting transition table.
             */
            next = p.next();

            /*
             * Need to make sure parser didn't run out of characters for next
             * read. If it did, then there is nothing to transition to, but we
             * will give it the stop string to complete the cycle. The statement
             * will be test next iteration.
             */
            if (p.done())
                next = stop_string;

            add_pair(curr, next);
            
            /* 
             * Set the next transition as the current transition to keep the
             * chain.
             */
            curr = next;
        }

        /* 
         * Build cache of transition tables for each word. Note that the start
         * word is transparently handled here because its key is returned from
         * the Parser as "STOP_CHAR".
         */
        for (auto &pair : dictionary)
            pair.second.cache();

        not_built = false;
    }

    MarkovChain
    chain ()
    {
        if (not_built) {
            std::cerr << "Need to build Corpus before using it." << std::endl;
            exit(1);
        }
        return MarkovChain(&generator, &dictionary);
    }

protected:
    /*
     * Using first word in the pair as the current state, add the second word
     * in the pair as a transition from the current state.
     */
    void
    add_pair (std::string curr, std::string next)
    {
        if (dictionary.find(curr) == dictionary.end())
            dictionary.emplace(curr, Word(curr));
        dictionary[curr].update_transition(next);
    }

    std::mt19937 generator;
    bool not_built;
    std::unordered_map<std::string, Word> dictionary;
};

int
main (int argc, char **argv)
{
    MarkovChain chain;
    Corpus corpus;
    corpus.build("sample.txt");
    chain = corpus.chain();
    while (!chain.done()) {
        printf("%s ", chain.next().c_str());
    }
    return 0;
}
