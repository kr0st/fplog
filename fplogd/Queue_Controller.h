#include <string>
#include <queue>
#include <memory>
#include <chrono>

using namespace std::chrono;
using namespace std;

class Queue_Controller
{
    public: 

        class Algo
        {
            public:
            
                struct Result
                {
                    size_t current_size = 0;
                    size_t removed_count = 0;
                };
                
                struct Fallback_Options
                {
                    enum Type
                    {
                        Remove_Newest = 765,
                        Remove_Oldest
                    };
                };
                
                Algo(queue<string*>& mq, size_t max_size, size_t current_size = 0): mq_(mq), max_size_(max_size), current_size_(current_size){}
                virtual Result process_queue(size_t current_size) = 0;


            private:
                
                Algo();
            

            protected:    
            
                queue<string*>& mq_;
                size_t max_size_;
                int current_size_;
        };

        class Remove_Oldest: public Algo
        {
            public:
                    
                    Remove_Oldest(Queue_Controller& qc): Algo(qc.mq_, qc.max_size_, qc.mq_size_) {}
                    Result process_queue(size_t current_size);
        };        

        class Remove_Newest;
        class Remove_Newest_Below_Priority;
        class Remove_Oldest_Below_Priority;

        Queue_Controller(size_t size_limit = 20000000, size_t timeout = 30000);

        bool empty();
        string *front();
        void pop();
        void push(string *str);
        
        void change_algo(shared_ptr<Algo> algo, Algo::Fallback_Options::Type fallback_algo);


    private:
    
        int mq_size_ = 0;
        size_t max_size_ = 0;
        
        queue<string*> mq_;
        
        shared_ptr<Algo> algo_;
        shared_ptr<Algo> algo_fallback_;
        
        size_t emergency_time_trigger_ = 0;
        time_point<system_clock, system_clock::duration> timer_start_;
        
        bool state_of_emergency();
        void handle_emergency();
};


class Queue_Controller::Remove_Newest: public Algo
{
    public:
            
            Remove_Newest(Queue_Controller& qc): Algo(qc.mq_, qc.max_size_, qc.mq_size_) {}
            Result process_queue(size_t current_size);
};

namespace fplog
{
    class Priority_Filter;
};

class Queue_Controller::Remove_Newest_Below_Priority: public Algo
{
    public:
        
            Remove_Newest_Below_Priority(Queue_Controller& qc, const char* prio, bool inclusive = false):
            Algo(qc.mq_, qc.max_size_, qc.mq_size_), prio_(prio), inclusive_(inclusive) { make_filter(); }
            //~Remove_Newest_Below_Priority(){}
            
            Result process_queue(size_t current_size);


    private:

            Remove_Newest_Below_Priority();
            void make_filter();

            std::string prio_;
            bool inclusive_;
            
            std::shared_ptr<fplog::Priority_Filter> filter_;
};

class Queue_Controller::Remove_Oldest_Below_Priority: public Algo
{
    public:
        
            Remove_Oldest_Below_Priority(Queue_Controller& qc, const char* prio, bool inclusive = false):
            Algo(qc.mq_, qc.max_size_, qc.mq_size_), prio_(prio), inclusive_(inclusive) { make_filter(); }
            //~Remove_Oldest_Below_Priority(){}
            
            Result process_queue(size_t current_size);


    private:

            Remove_Oldest_Below_Priority();
            void make_filter();

            std::string prio_;
            bool inclusive_;
            
            std::shared_ptr<fplog::Priority_Filter> filter_;
};

