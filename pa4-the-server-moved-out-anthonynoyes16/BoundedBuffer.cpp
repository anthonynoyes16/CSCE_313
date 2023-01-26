#include "BoundedBuffer.h"


using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
        // use one of the vector constructors 
    vector<char> temp(msg, msg + size);

    // 2. Wait until there is room in the queue (i.e., queue length is less than cap)
        // waiting on slot_available 
    unique_lock<mutex> lock(m);
    slot_available.wait(lock, [this]{return q.size() < (long unsigned int)cap;}); // return false if the waiting should continue

    // 3. Then push the vector at the end of the queue
    q.push(temp);
    lock.unlock();

    // 4. Wake up threads that were waiting for push
        // notifying data_available
    data_available.notify_one();
}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
        // waiting on data_available
    unique_lock<mutex> lock(m);
    data_available.wait(lock, [this]{return q.size() > 0;});

    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> temp = q.front();
    q.pop();

    // 3. Convert the popped vector<char> into a char*, copy that into msg; 
    // assert that the vector<char>'s length is <= size
    lock.unlock();
    assert(temp.size() <= (unsigned int)size);
        // use vector::data()
    memcpy(msg, temp.data(), temp.size()); // .data() returns pointer to first element in the internal array of the vector

    // 4. Wake up threads that were waiting for pop
        // notifying slot_available
    slot_available.notify_one();   

    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return temp.size();
}

size_t BoundedBuffer::size () {
    return q.size();
}
