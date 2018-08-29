#include <iostream>
#include <unordered_map>
#include <utility>
#include <string>
#include <random>
#include <fstream>
#include <sstream>
#include <map>

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
        : stop_encountered(false)
        , stream(stream)
    {
        stop_chars['\n'] = 1;
    }

    std::string
    next ()
    {
        std::string buff;
        stop_encountered = false;
        char next;

        /* Skip any preceding whitespace and stop characters */
        while (!done() && (stream.peek() == ' ' || 
                           stop_chars.find(stream.peek()) != stop_chars.end())) {
            stream.get();
        }

        while (!done()) {
            next = stream.get();

            /* stop after hitting any whitespace */
            if (next == ' ')
                break;

            if (stop_chars.find(next) != stop_chars.end()) {
                stop_encountered = true;
                break;
            }

            buff += next;
        }

        return buff;
    }

    bool
    done ()
    {
        return (stream.rdbuf()->in_avail() == 0);
    }

    bool
    stop ()
    {
        return stop_encountered;
    }

protected:
    bool stop_encountered;
    std::map<char, int> stop_chars;
    std::stringstream &stream;
};

class Word {
public:
    Word ()
        : word_string(std::string())
        , delta(std::discrete_distribution<int>())
    { }

    Word (std::string word)
        : word_string(word)
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
            p.push_back(pair.second / num_transitions);
        }

        delta = std::discrete_distribution<int>(p.begin(), p.end());
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
        : generator((std::random_device())())
        , start_word(Word("-->"))
        , not_built(true)
        , current_word(std::string())
    { }

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
        Parser p(stream);

        /*
         * While there are words, get the current word and the next word. 
         * `next' is entered as a transition of `curr'.
         */
        curr = p.next();
        while (1) {
            next = p.next();

            if (p.done())
                break;

            //printf("%s -> %s\n", curr.c_str(), next.c_str());
            dictionary[curr].update_transition(next);

            /* 
             * if we encountered a stop word, then start transition over and
             * get a completely new `curr'. That word will be added as a start
             * word.
             */
            if (p.stop()) {
                curr = p.next();
                start_word.update_transition(curr);
            }
            /* else the next word becomes the current transition word */
            else {
                curr = next;
            }
        }

        /*
         * TODO: Build an actual parser for words. Should be easy enough but
         * need support for 'stop sequences', e.g. "\n", ".", etc, that are 
         * often "baked into" the word. Any whitespace that isn't a stop word
         * just carries the sentence onwards.
         */

        /*
         * TODO: Need starting words and ending words. So, that means some sort
         * of small parser with appropriate methods to set the list of words we
         * look for to end a sentence and to start new 'start words'.
         * Each 'start word' has some probability of being used just like any
         * regular word transition. It can be apart of the corpus and called
         * the `start_word' and we can use the transition tables like normal.
         */

        /* Build cache of transition tables for each word. */
        start_word.cache();
        for (auto &pair : dictionary)
            pair.second.cache();

        current_word = start_word.next(generator);
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
        check_build();
        current_word = dictionary[current_word].next(generator);
        return current_word;
    }

protected:
    /*
     * Using first word in the pair as the current state, add the second word
     * in the pair as a transition from the current state.
     */
    void
    add_pair (const std::pair<std::string, std::string> pair)
    {
        if (dictionary.find(pair.first) == dictionary.end())
            dictionary.emplace(pair.first, Word(pair.first));
        dictionary[pair.first].update_transition(pair.second);
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
    Word start_word;
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
