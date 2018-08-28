#include <iostream>
#include <unordered_map>
#include <utility>
#include <string>
#include <random>
#include <fstream>
#include <sstream>
#include <map>

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
        , not_built(true)
        , current_word(std::string())
    { }

    int
    num_words ()
    {
        return dictionary.size();
    }

    std::string
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
    std::string
    build (std::stringstream &stream)
    {
        std::string word, next;

        stream >> word >> next;
        if (stream.fail()) {
            std::cerr << "Failed to read corpus." << std::endl;
            exit(1);
        }

        /*
         * TODO: Need starting words and ending words. So, that means some sort
         * of small parser with appropriate methods to set the list of words we
         * look for to end a sentence and to start new 'start words'.
         * Each 'start word' has some probability of being used just like any
         * regular word transition. It can be apart of the corpus and called
         * the `start_word' and we can use the transition tables like normal.
         */

        while (stream.rdbuf()->in_avail() > 0) {
            printf("%s | %s\n", word.c_str(), next.c_str());

            word = next;
            stream >> next;
            if (stream.fail()) {
                std::cerr << "Failed to read corpus." << std::endl;
                exit(1);
            }
        }
 
        ///* Build cache of transition tables for each word. */
        //for (auto &pair : dictionary)
        //    pair.second.cache();

        ///* TODO: Choose the initial value for current_word */ 

        //not_built = false;
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
        return "";
        /* TODO: call current_word.next() */
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
    std::string current_word; /* used as a key into dictionary */
    std::unordered_map<std::string, Word> dictionary;
};

int
main (int argc, char **argv)
{
    Corpus corpus;
    corpus.build("sample.txt");
    return 0;
}
