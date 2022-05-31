#include <iostream>
#include <semaphore.h>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <sstream>
#include <fstream>
#include <deque>
#include <chrono>
#include <algorithm> // not needed in clion, but required when compiling with g++


using namespace std;
/* ################ Classes ####################### */
enum types {
    DONE = -3, SPORTS=0, NEWS=1, WEATHER=2
};

class BoundedQueue {
public:
    queue<string> RegularQueue;
    mutex m;
    sem_t full;
    sem_t empty;

    explicit BoundedQueue(int size) {
        // initialize the semaphore for full to be the passed size
        sem_init(&empty, 0, size);
        // initialize the empty for full to be the passed size
        sem_init(&full, 0, 0);
    }

    // emplace a new string in the queue
    void insert(const string &s) {
        // wait until empty > 0 which means there is a free spot to insert
        sem_wait(&empty);
        // try to get mutex lock on the queue
        m.lock();
        // push into the internal queue
        RegularQueue.emplace(s);
        m.unlock();
        // increment semaphore - unlock if full > 0
        sem_post(&full);
    }

    // pop the latest string from the queue
    string remove() {
        sem_wait(&full);
        // try to get mutex lock on the queue
        m.lock();
        // remove into the internal queue
        string s = RegularQueue.front();
        RegularQueue.pop();
        m.unlock();
        // lock a semaphore to full - decrement full
        sem_post(&empty);
        return (s);
    }

    virtual ~BoundedQueue() {
        sem_destroy(&full);
        sem_destroy(&empty);
    }

};

/**
 * An unbounded queue, uses semaphores to signal the consumers that a producer made a new string
 */
class UnBoundedQueue {
public:
    queue<string> RegularQueue;
    mutex m;
    sem_t full;


    UnBoundedQueue() {
        // initialize the semaphore to 0
        sem_init(&full, 0, 0);
    }

    // push a new string into the queue
    void insert(const string &s) {
        // try to get mutex lock on the queue
        m.lock();
        // push into the internal queue
        RegularQueue.push(s);
        m.unlock();
        // lock a semaphore to full - decrement full
        sem_post(&full);
        // semaphore up - increment empty semaphore
    }

    // pop the latest string from the queue
    string remove() {
        // semaphore up - increment empty semaphore
        sem_wait(&full);
        // try to get mutex lock on the queue
        m.lock();
        // push into the internal queue
        string s = RegularQueue.front();
        RegularQueue.pop();
        m.unlock();
        // lock a semaphore to full - decrement full
        return (s);
    }

    virtual ~UnBoundedQueue() {
        sem_destroy(&full);
    }

};

/* ################ Globals ####################### */

// global vector of queues
vector<BoundedQueue *> bqVector; // vector of producers queues
UnBoundedQueue *sportsQ = nullptr, *weatherQ = nullptr, *newsQ = nullptr; // queues for editors
BoundedQueue *smq = nullptr; // screen manager queue


/* ################ Functions ####################### */

/**
 * Check the category of the passed string by trying to find keywords in the string
 * @param s the string to check
 * @return the type of string (enum)
 */
int checkCategory(string &s) {
    std::size_t found = s.find("SPORTS");
    // if sports not found continue search
    if (found == string::npos) {
        found = s.find("NEWS");
        // didn't find news
        if (found == string::npos) {
            found = s.find("WEATHER");
            // didn't find WEATHER
            if (found == string::npos) {
                found = s.find("DONE");
                //if didn't find done
                if (found == string::npos) {
                    std::cout << "Producer string error!" << std::endl;
                    exit(-1);
                }
                    //found DONE
                else {
                    return DONE;
                }
            }
                //found WEATHER
            else {
                return WEATHER;
            }
        }
            //found NEWS
        else {
            return NEWS;
        }
    }
        // found SPORTS
    else {
        return SPORTS;
    }
}

/**
 * Generate a number of string and puts them into a bounded queue for the @dispatcher to use.
 * Randomly choose a string category to create.
 * @param products number of string this producer will generate
 * @param id the id of this producer
 * @param size the size of the bouded queue for this instance of a producer
 */
void producer(int products, int id, int size) {
    int news = 0, sports = 0, weather = 0;
    auto *bq = new BoundedQueue(size); // create a bounded queue for the producer to put string into
    bqVector[id-1] = bq; // save the bounded queue of this producer in the global vector of queues
    stringstream s;
    // produce strings here
    for (int j = 0; j < products; ++j) {
        // choose a random string category
        int k = rand() % 3;
        switch (k) {
            case NEWS:
                s << "Producer " << to_string(id) << " " << "NEWS " << to_string(news);
                news++;
                break;
            case WEATHER:
                s << "Producer " << to_string(id) << " " << "WEATHER " << to_string(weather);
                weather++;
                break;
            case SPORTS:
                s << "Producer " << to_string(id) << " " << "SPORTS " << to_string(sports);
                sports++;
                break;
        }
        bq->insert(s.str());
        s.str("");
    }
    s << "DONE";
    bq->insert(s.str());
}

/**
 * Iterated over the queues of the producers in a round-robin fashion, each time removing a string from the queue.
 * the string is sorted into a queue depending on its type and then passed to the editors.
 * @param producersN the number of producers in the program
 */
void dispatcher(int producersN) {
    int counter = 0;

    // get a queue for a producer, and take out a string
    auto dispatch = [&counter](BoundedQueue *bq) {
        string s;
        // get a string
        s = bq->remove();
        // check category of
        switch (checkCategory(s)) {
            case SPORTS:
                sportsQ->insert(s);
                break;
            case NEWS:
                newsQ->insert(s);
                break;
            case WEATHER:
                weatherQ->insert(s);
                break;
            case DONE:
                auto erasePos = find(bqVector.begin(), bqVector.end(),bq);
                delete bq;
                bqVector.erase(erasePos); // find the producer that finished and remove him
                counter++;
                break;
        }
    };

    // dispatch until we receive "DONE" string from each of the producers
    while (true) {
        if(counter == producersN) break;

        // iterate over the dispatcher queues in the vector
        for(BoundedQueue* bq : bqVector){ dispatch(bq);}
    }

    // insert a DONE string to each of the editor queues to indicate finish of each queue.
    newsQ->insert("DONE");
    sportsQ->insert("DONE");
    weatherQ->insert("DONE");
}

/**
 * Until receiving a "DONE" string from it's queue removes string from it's queue and "edits" them (simulated by sleep)
 * @param type the type of string the editor operates on
 */
void editor(int type) {
    UnBoundedQueue* uq = nullptr;
    switch (type) {
        case NEWS:
            uq = newsQ;
            break;
        case WEATHER:
            uq = weatherQ;
            break;
        case SPORTS:
            uq = sportsQ;
            break;
    }
    while (true) {
        string s = uq->remove();
        this_thread::sleep_for(chrono::milliseconds(100)); // simulate an edit job
        smq->insert(s);
        if (s == "DONE") {
            return;
        }
    }
}

/**
 * Manages the string queue, printing contents to stdout, until reading DONE from each queue
 */
void screenManager() {
    int counter = 0;
    while (true) {
        // if all the queues sent a DONE string, we end
        if (counter == 3) {
            std::cout << "DONE" << std::endl;
            return;
        }
        string s = smq->remove();
        if (s == "DONE") {
            counter++;
            continue;
        }
        std::cout << s << std::endl;
    }
}

/**
 *
 * @param argc needs to be 2
 * @param argv should contain the path to the config file.
 * @return
 */
int main(int argc, const char *argv[]) {
    std::vector<std::thread> tVector;
    sportsQ = new UnBoundedQueue,
    newsQ = new UnBoundedQueue,
    weatherQ = new UnBoundedQueue;
    if (argc != 2) {
        std::cout << "Incorrect number of arguments passed." << std::endl;
        exit(-1);
    }

    // open the config file from the arguments
    ifstream config(argv[1]);

    // using config option 2
    // put all the values into a vector
    deque<string> configVec;
    std::string line;
    // read the file line by line ignoring whitespace characters
    while (getline(config, line)) {
        if (line == "\r" || line == "\n" || line.empty()) {
            continue;
        }
        configVec.emplace_back(line);
    }
    config.close();

    // get the size of the screen manager queue which is the last value
    int smSize = stoi(configVec.back());
    configVec.pop_back();
    smq = new BoundedQueue(smSize);

    // start the editor threads
    thread t2(editor, NEWS);
    thread t3(editor, WEATHER);
    thread t4(editor, SPORTS);

    int N = ((int)configVec.size() / 3); // producers number, each producer has 3 lines in the config

    // set the vector to the amount of producers.
    bqVector.resize(N);

    // iterate through the configuration files and start a thread for the producer
    for (int i = 0; i < N; ++i) {
        int id = stoi(configVec.front());
        configVec.pop_front();
        int prod_size = stoi(configVec.front());
        configVec.pop_front();
        int queue_size = stoi(configVec.front());
        configVec.pop_front();
        thread t1(&producer, prod_size, id, queue_size);
        tVector.push_back(std::move(t1));
    }
    // start the threads of the dispatcher and screen manager
    thread t5(dispatcher,N);
    thread t6(screenManager);
    t2.join();
    t3.join();
    t4.join();
    for (thread &t:tVector) {
        t.join();
    }
    t5.join();
    t6.join();
    return 0;
}


