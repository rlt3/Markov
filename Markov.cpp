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
 * Parse should (at some point) accept a list of stop characters. Right now we
 * can just use '\n' as a stop character. The flow should be: next returns the
 * next `chunk'. `next` should skip all whitespace it finds. if the first
 * character read after skipping any whitespace is a skip character, then read
 * it and skip all other skip characters there-after. If there are no skip
 * characters read after skipping whitespace then read each character until it
 * runs into either a skip character or whitespace.
 *
 * The Corpus itself should determine what to do when it encounters a stop
 * character string. The logic of reading a corpus shouldn't be in the Parser.
 * The Parser just returns chunks of whitespace delimited words or words 
 * delimited by some stop character. This will alow us to more easily extend
 * reading in punctuation into the corpus while keeping `start->stop' sentences
 * clearly defined.
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

class Corpus {
public:
    Corpus ()
        : not_built(true)
        , current_word(std::string())
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

            //printf("->'%s' -> '%s'\n", curr.c_str(), next.c_str());
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

    std::string
    current ()
    {
        check_build();
        return current_word;
    }

    /*
     * Return the next word from the current.
     */
    std::string
    next ()
    {
        std::string key;
        check_build();

        key = current_word;
        if (key.empty()) {
            key = std::string(1, STOP_CHAR);
        }

        current_word = dictionary[key].next(generator);
        return current_word;
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

    void
    check_build ()
    {
        if (not_built) {
            std::cerr << "Need to build Corpus before using it." << std::endl;
            exit(1);
        }
    }

    std::mt19937 generator;
    bool not_built;
    std::string current_word; /* used as a key into dictionary */
    std::unordered_map<std::string, Word> dictionary;
};

int
main (int argc, char **argv)
{
    Corpus corpus;
    corpus.build("sample.txt");
    for (int i = 0; i < 20; i++) {
        printf("%s\n", corpus.next().c_str());
    }
    return 0;
}
