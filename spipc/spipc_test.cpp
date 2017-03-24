#include <stdio.h>
#include <thread>
#include <atomic>

#include "spipc.h"

namespace spipc { namespace testing {

std::recursive_mutex g_test_mutex;

std::vector<std::string> g_written_items;
std::vector<std::string> g_read_items;
std::map<fplog::UID, std::vector<std::string>> g_ipc_written;
std::map<fplog::UID, std::vector<std::string>> g_ipc_read;

void writer_ipc_thread(const fplog::UID uid)
{
    srand(static_cast<unsigned int>(std::hash<std::thread::id>()(std::this_thread::get_id())));
    std::vector<std::string> ipc_written;

    spipc::IPC ipc;

    ipc.connect(uid);

    char fwriter_name[255];
    FILE* fwriter = 0;

#ifndef _LINUX    
    sprintf_s(fwriter_name, "writer_%lld_%lld.txt", uid.high, uid.low);
    fopen_s(&fwriter, fwriter_name, "w");
#else
    sprintf(fwriter_name, "writer_%lld_%lld.txt", uid.high, uid.low);
    fwriter = fopen(fwriter_name, "w");
#endif

    int prefix = 0;

    if (uid.high == 18743) prefix = 0;
    if (uid.high == 18745) prefix = 3;
    if (uid.high == 18747) prefix = 6;
    
    char number[100];

    for (int i = 0; i < 150000; ++i)
    {
#ifndef _LINUX
        sprintf_s(number, "%d", i);
#else
        sprintf(number, "%d", i);
#endif
        int rnd_sz = 1 + rand() % 1295;
    
        std::vector<char> to_write;
        for (int j = 0; j < rnd_sz; ++j)
            to_write.push_back(65 + rand() % 3 + prefix);
        to_write.push_back('_');
        int num_len = static_cast<int>(strlen(number));
        for (int k = 0; k < num_len; k++)
            to_write.push_back(number[k]);
        to_write.push_back('_');
        to_write.push_back(0);

        char* write_buf = &(*to_write.begin());
        //printf("Writing: %s (%d bytes)\n", write_buf, rnd_sz + 1);
    retry_write:

        try
        {
            ipc.write(write_buf, to_write.size(), 3000);
        }
        catch(fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION in writer_ipc_thread: %s\n", e.what().c_str());
            goto retry_write;
        }
            
        ipc_written.push_back(write_buf);

        char newline[2] = { 0x0d, 0x0a };
        fwrite(write_buf, to_write.size() - 1, 1, fwriter);
        fwrite(newline, 2, 1, fwriter);
        fflush(fwriter);
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_ipc_written[uid] = ipc_written;
    
    fclose(fwriter);
    printf("thread with fplog::UID [%lld/%lld] written %lu values.\n", uid.high, uid.low, static_cast<unsigned long>(ipc_written.size()));
}

void reader_ipc_thread(const fplog::UID uid)
{
    srand(static_cast<unsigned int>(std::hash<std::thread::id>()(std::this_thread::get_id())));
    std::vector<std::string> read_items;

    spipc::IPC ipc;
    ipc.connect(uid);
    
    char freader_name[255];
    FILE* freader = 0;

#ifndef _LINUX
    sprintf_s(freader_name, "reader_%lld_%lld.txt", uid.high, uid.low);
    fopen_s(&freader, freader_name, "w");
#else
    sprintf(freader_name, "reader_%lld_%lld.txt", uid.high, uid.low);
    freader = fopen(freader_name, "w");
#endif

    for (int i = 0; i < 150000; ++i)
    {
        char read_buf[3000];
        memset(read_buf, 0, sizeof(read_buf));
    
    retry_read:

        try
        {
            ipc.read(read_buf, sizeof(read_buf), 3000);
        }
        catch(fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION in reader_ipc_thread: %s\n", e.what().c_str());
            goto retry_read;
        }

        read_items.push_back(read_buf);

        char newline[2] = { 0x0d, 0x0a };
        fwrite(read_buf, strlen(read_buf), 1, freader);
        fwrite(newline, 2, 1, freader);
        fflush(freader);

        //printf("Reading: %s\n", read_buf);
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_ipc_read[uid] = read_items;

    fclose(freader);
    printf("thread with fplog::UID [%lld/%lld] read %lu values.\n", uid.high, uid.low, static_cast<unsigned long>(read_items.size()));
}

bool N_threads_IPC_test()
{
    fplog::UID uid;
    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long begin = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();

    uid.high = 18743;
    uid.low = 18744;
    std::thread writer(writer_ipc_thread, uid);


    uid.high = 18745;
    uid.low = 18746;
    std::thread writer2(writer_ipc_thread, uid);

    uid.high = 18747;
    uid.low = 18748;
    std::thread writer3(writer_ipc_thread, uid);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    uid.high = 18743;
    uid.low = 18744;
    std::thread reader(reader_ipc_thread, uid);

    uid.high = 18745;
    uid.low = 18746;
    std::thread reader2(reader_ipc_thread, uid);

    uid.high = 18747;
    uid.low = 18748;
    std::thread reader3(reader_ipc_thread, uid);

    writer.join();
    reader.join();

    writer2.join();
    reader2.join();

    writer3.join();
    reader3.join();

    long long end = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();
    size_t read_messages = 0;

    std::map<fplog::UID, std::vector<std::string>>::iterator it(g_ipc_written.begin());
    for (; it != g_ipc_written.end(); ++it)
    {
        std::vector<std::string> v1(g_ipc_read[it->first]);
        read_messages += v1.size();

        if (v1 != it->second)
        {
            printf("ERROR: IPC read/write problem detected - written items did not match read items.\n");
            return false;
        }
    }

    printf("IPC delivers %g messages per second.\n", read_messages * 1.0 / (end * 1.0 - begin * 1.0) * 1000);
    return true;
}

void buffer_overflow_test_worker()
{
    char* data_5mb = new char[5 * 1024 * 1024];
    size_t data_sz = 5 * 1024 * 1024;
    for (int i = 0; i < data_sz; ++i)
        data_5mb[i] = 65 + i % 23;

    spipc::IPC buffer_overflow_test;
    fplog::UID uid;
    uid.high = 18743;
    uid.low = 18744;
    
    try
    {
        buffer_overflow_test.connect(uid);
        buffer_overflow_test.write(data_5mb, data_sz);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
    catch(fplog::exceptions::Generic_Exception& e)
    {
        printf("buffer_overflow_test_worker failed! reason: %s\n", e.what().c_str());
    }
    
    delete [] data_5mb;
}

bool Buffer_Overflow_Test()
{
    char* data_5mb = new char[5 * 1024 * 1024];
    size_t data_sz = 5 * 1024 * 1024;
    for (int i = 0; i < data_sz; ++i)
        data_5mb[i] = 65 + i % 23;

    size_t read_buf_sz = 256 * 1024;
    char* read_buf = new char[read_buf_sz];

    spipc::IPC buffer_overflow_test;
    fplog::UID uid;
    uid.high = 18743;
    uid.low = 18744;
    buffer_overflow_test.connect(uid);

    std::thread worker(&buffer_overflow_test_worker);

    int retries = 0;
retry:

    if (retries > 5)
    {
        printf("buffer_overflow_test.read() failed.\n");
        worker.join();
        return false;
    }

    try
    {
        read_buf_sz = buffer_overflow_test.read(read_buf, read_buf_sz);
        if ((data_sz != read_buf_sz) || (memcmp(read_buf, data_5mb, data_sz) != 0))
        {
            printf("buffer_overflow_test.read() got corrupted data.\n");
            worker.join();
            return false;
        }
    }
    catch(fplog::exceptions::Read_Failed)
    {
        printf("buffer_overflow_test.read() failed.\n");
        goto retry;
    }
    catch(fplog::exceptions::Buffer_Overflow&)
    {
        retries++;
        delete [] read_buf;
        read_buf_sz = data_sz;
        read_buf = new char[read_buf_sz];
        goto retry;
    }

    worker.join();

    delete [] read_buf;
    delete [] data_5mb;
    
    return true;
}

bool run_all_tests()
{
    g_written_items.clear();
    g_read_items.clear();
    g_ipc_written.clear();
    g_ipc_read.clear();

    //spipc::Shared_Memory_Transport::global_init();

    /*try
    {
        if (!N_threads_mem_transport_test())
            printf("N_threads_mem_transport_test failed.\n");
    }
    catch (sprot::exceptions::Exception& e)
    {
        printf("ERROR: N_threads_mem_transport_test failed with exception.\n");
        printf("%s\n", e.what().c_str());
    }*/

    try
    {
        if (!N_threads_IPC_test())
        {
            printf("N_threads_IPC_test failed.\n");
            return false;
        }
    }
    catch (sprot::exceptions::Exception& e)
    {
        printf("ERROR: N_threads_IPC_test failed with exception.\n");
        printf("%s\n", e.what().c_str());
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(13000));

    try
    {
        if (!Buffer_Overflow_Test())
        {
            printf("Buffer_Overflow_Test failed.\n");
            return false;
        }
    }
    catch (sprot::exceptions::Exception& e)
    {
        printf("ERROR: Buffer_Overflow_Test failed with exception.\n");
        printf("%s\n", e.what().c_str());
        return false;
    }

    printf("spipc tests finished OK.\n");
    return true;
}

}};

int main(int argc, char* argv[])
{
    while(spipc::testing::run_all_tests());
	return 0;
}
