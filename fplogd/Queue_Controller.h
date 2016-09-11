#include <string>
#include <queue>

using namespace std;

class Queue_Controller
{
    public: 

        enum Algo
        {
            Remove_Newest = 5463,
            Remove_Oldest,
            Remove_Newest_Below_Priority,
            Remove_Oldest_Below_Priority
        };

        Queue_Controller(Algo algo = Remove_Newest, size_t timeout = 30000);

        bool empty();
        string *front();
        void pop();
        void push(string *str);


    private:

        int mq_size = 0;
        const int queue_limiter = 20000000;
        std::queue<std::string*> mq_;
        
        bool state_of_emergency();
};