#include <string>
#include <queue>

using namespace std;

class Queue_Controller {

public: 

    bool empty();
    void front(string *str);
    void pop();
    void push(string *str);

private: 
    int mq_size = 0;
    const int queue_limiter = 20000000;
    std::queue<std::string*> mq_;
};