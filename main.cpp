#include <iostream>
#include <semaphore.h>
#include <queue>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <vector>
#include <sstream>
#include <fstream>
#include <deque>
#include <chrono>

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

class UnBoundedQueue {
public:
    queue<string> RegularQueue;
    mutex m;
    sem_t full;


    UnBoundedQueue() {
        // initialize the empty for full to be the passed size
        sem_init(&full, 0, 0);
    }

    // emplace a new string in the queue
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

// i is the number of products of the producer
void producer(int i, int id, int size) {
    //auto position = bqVector.begin() + (id - 1);
    int news = 0, sports = 0, weather = 0;
    auto *bq = new BoundedQueue(size);
    //bqVector.insert(position,bq);
    bqVector[id-1] = bq;
    stringstream s;
    // produce strings here
    for (int j = 0; j < i; ++j) {
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
        //bqVector.push_back(bq);
        s.str("");
    }
    s << "DONE";
    bq->insert(s.str());
}

// only one exists! works in round robin
void dispatcher(int producersN) {
    int counter = 0, i = 0;
    // get a queue for a producer, and take out a story
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
                bqVector.erase(std::find(bqVector.begin(), bqVector.end(),bq)); // find the producer that finished and remove him
                counter++;
                break;
        }
    };

    // dispatch while we didn't receive done from each of the producers
    while (true) {
        if(counter == producersN) break;
        // iterate over the dispatcher queues in the vector
        for_each(bqVector.begin(), bqVector.end(), dispatch);
        //dispatch(bqVector[i % producersN]);
        //i++;
    }
    newsQ->insert("DONE");
    sportsQ->insert("DONE");
    weatherQ->insert("DONE");

}


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
        this_thread::sleep_for(chrono::milliseconds(100));
        smq->insert(s);
        if (s == "DONE") {
            return;
        }
    }
}


void screenManager(int producersN) {
    int counter = 0;
    while (true) {
        if (counter == producersN) {
            std::cout << "DONE" << std::endl;
            exit(0);
        }
        string s = smq->remove();
        if (s == "DONE") {
            counter++;
            continue;
        }
        std::cout << s << std::endl;
    }
}


int main(int argc, const char *argv[]) {
    std::vector<std::thread> tVector;
    sportsQ = new UnBoundedQueue,
    newsQ = new UnBoundedQueue,
    weatherQ = new UnBoundedQueue;
    if (argc != 2) {
        std::cout << "Incorrect number of arguments passed." << std::endl;
        exit(-1);
    }

    ifstream config(argv[1]);
//    if (!config.is_open()) {
//        std::cout << "Error reading the config file!" << std::endl;
//        exit(-1);
//    }

    // option 2
    // put all the values into a vector
    deque<string> configVec;
    std::string line;
    while (getline(config, line)) {
        if (line == "\r" || line == "\n" || line == "") {
            continue;
        }
        configVec.emplace_back(line);
    }
    config.close();

    // get the size of the screen manager queue
    int smSize = stoi(configVec.back());
    configVec.pop_back();
    smq = new BoundedQueue(smSize);

    thread t2(editor, NEWS);
    thread t3(editor, WEATHER);
    thread t4(editor, SPORTS);
    int N = (configVec.size() / 3); // producers number, each producer has 3 lines in the config
    bqVector.resize(N);
    // vector of producers queues
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
    thread t5(dispatcher,N);
    thread t6(screenManager,N);

    t6.join();
    return 0;
}


