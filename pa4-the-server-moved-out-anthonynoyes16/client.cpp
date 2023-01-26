#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

int m = MAX_MESSAGE; // declaring m globaly so we can use it in every function necessary

void patient_thread_function (int p_no, int line_count, BoundedBuffer &req_buf) {
    // functionality of the patient threads

    // for n requests, produce a datamsg(p_no, time, ECGNO)
        // time dependent on current req
        // 0 req is at time 0, 1 req is at time 0.004, etc
    for (int i = 0; i < line_count; i++) {
        char buf[sizeof(datamsg)]; 
        datamsg x(p_no, i*0.004, EGCNO);
        memcpy(buf, &x, sizeof(datamsg));
        req_buf.push(buf, sizeof(buf));
    }
}

void file_thread_function (int filesize, BoundedBuffer &req_buf, string filename) {
    // functionality of the file thread

    // open output file and allocate memory of the file with fseek(); close the file
    string filename_open = "received/" + filename;
    FILE* pfile = fopen(filename_open.c_str(), "wb");
    fseek(pfile, filesize, SEEK_SET);
    fclose(pfile);
    // while offset < filesize produce a filesmg(offset, m) + filename and push to req buffer
        // increment offset and be careful about final message
    for (int i = 0; i <= filesize/m; i++) {
        int remaining = filesize - m*i;
        filemsg file_req(0, 0);
        file_req.offset = i*m; // set offset in the file
        file_req.length = m; // set the length, be careful of the last segment
        if (remaining < m) {
            file_req.length = remaining;
        }
        int len = sizeof(filemsg) + filename.size() + 1;
        char* temp = new char[len];
        memcpy(temp, &file_req, sizeof(filemsg)); // copy over filemsg data into temp buffer
        strcpy(temp + sizeof(filemsg), filename.c_str()); // copy over filename into temp buffer
        req_buf.push(temp, len);
        delete[] temp;
    }
}

void worker_thread_function (TCPRequestChannel* rqc, BoundedBuffer &req_buf, BoundedBuffer &resp_buf, string filename) {
    // functionality of the worker threads

    // forever loop
    while(true) {
        // pop message from req buffer
        char buf[MAX_MESSAGE];
        req_buf.pop(buf, sizeof(buf));

        /* deciding what type of message */
        MESSAGE_TYPE mes = *((MESSAGE_TYPE*) buf);
        // if quit message received, send a quit_msg to server
        // worker threads are equivalent to clients from pa1
        if (mes == QUIT_MSG) {
            rqc->cwrite(&mes, sizeof(MESSAGE_TYPE));
            break;
        }
        if (mes == DATA_MSG) {
            // datamsg pointer to point to the buffer
            datamsg* p = (datamsg*)buf;
            // send the req across the TCP channel to server
            rqc->cwrite(buf, sizeof(buf));
            // collect response
            double resp;
            rqc->cread(&resp, sizeof(double));
            // create std::pair(p_no from msg, response from server)
            std::pair temp = std::pair(p->person, resp);
            // push to the response buffer
            resp_buf.push((char*)&temp, sizeof(temp));
        }
        else if (mes == FILE_MSG) {
            filemsg f = *(filemsg*)buf;
            string filename_write = "received/" + filename;
            FILE* pfile = fopen(filename_write.c_str(), "r+b"); // open the file we want to write into
            fseek(pfile, f.offset, SEEK_SET);
            // send message across TCP channel
            int len = sizeof(filemsg) + filename.size() + 1;
            rqc->cwrite(buf, len);
            // collect response
            char* response = new char[m]; 
            rqc->cread(response, f.length);
            // write the buffer from the server
            fwrite(response, sizeof(char), f.length, pfile);
            fclose(pfile);
            delete[] response;
        }
        else {
            char a = 0;
	        rqc->cwrite(&a, sizeof(char));
        }
    }
}

void histogram_thread_function (HistogramCollection &HC, BoundedBuffer &resp_buf) {
    // functionality of the histogram threads
    char* buf = new char[m];
    // forever loop
    while(true) {
        // pop response from response buffer
        resp_buf.pop(buf, m);
        std::pair<int, double>* pair = (std::pair<int, double>*)buf;
        // if worker thread is done, quit
        if (pair->first == -1 && pair->second == -1) {
            break;
        }
        // call HC::update(response->p_no, response->double)
        HC.update(pair->first, pair->second);
    }
    delete[] buf;
}


int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed) - find out what works best for my system
    string a = "127.0.0.1";
    string r = "8020";
	m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
            case 'a':
                a = optarg;
                break;
            case 'r':
                r = optarg;
                break;
		}
	}
    
	// initialize overhead (including the control channel)
	TCPRequestChannel* chan = new TCPRequestChannel(a, r);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // array (or vector) of producer threads (if data transfer, p elements; if file transfer, 1 element which is single file thread)
    vector<thread> prod_threads = {};
    // array of TCPs (w elements)
    vector<TCPRequestChannel*> req_channels = {};
    // array of worker threads (w elements)
    vector<thread> worker_threads = {};
    // array of histogram threads (if data, h elements; if file, 0 elements)
    vector<thread> hist_threads = {};

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    // if data:
    if (f.size() == 0) {
        // create p patient threads (store in producer array)
        for (int i = 1; i <= p; i++) {
            prod_threads.push_back(thread(patient_thread_function, i, n, std::ref(request_buffer)));
        }
        // create w worker threads (store worker array)
        for (int i = 0; i < w; i++) {
            // call the TCPRequest channel constructor with the name from the server
            TCPRequestChannel *new_channel = new TCPRequestChannel(a, r);
            // push the new channel into the vector
            req_channels.push_back(new_channel);
            worker_threads.push_back(thread(worker_thread_function, req_channels.at(i), std::ref(request_buffer), std::ref(response_buffer), f));
        }           
        // create h histogram threads (store in histogram array)
        for (int i = 0; i < h; i++) {
            hist_threads.push_back(thread(histogram_thread_function, std::ref(hc), std::ref(response_buffer)));
        }
    }   
    // if file: 
    else {
        // create 1 file_thread (store in producter array)
        filemsg fm(0, 0);
        int len = sizeof(filemsg) + f.size() + 1;
        char* buf2 = new char[len];
        memcpy(buf2, &fm, sizeof(filemsg));
        strcpy(buf2 + sizeof(filemsg), f.c_str());
        chan->cwrite(buf2, len);  // I want the file size
        int64_t filesize = 0;
        chan->cread(&filesize, sizeof(int64_t)); // file size in bytes
        prod_threads.push_back(thread(file_thread_function, filesize, std::ref(request_buffer), f));
        delete[] buf2;
        // create w worker threads (store worker array)
        for (int i = 0; i < w; i++) {           
            // call the TCPRequest channel constructor with the name from the server
            TCPRequestChannel *new_channel = new TCPRequestChannel(a, r);
            // push the new channel into the vector
            req_channels.push_back(new_channel);
            worker_threads.push_back(thread(worker_thread_function, req_channels.at(i), std::ref(request_buffer), std::ref(response_buffer), f));          
        }
    }

	/* join all threads here */
    // iterate over all thread arrays and call join()
    // order in which you join is important- if you try to join worker threads first, they wont join until hist threads are done
    // call producers before consumers
    // patient threads before workers, workers before histograms
    for (long unsigned int i = 0; i < prod_threads.size(); i++) {
        prod_threads.at(i).join();
        // by joining them to the main thread, we ensure that main doesn't exit before the threads are done
    }
    for (long unsigned int i = 0; i < worker_threads.size(); i++) {
        MESSAGE_TYPE quit = QUIT_MSG;
        request_buffer.push((char*)&quit, sizeof(MESSAGE_TYPE));
    }
    for (long unsigned int i = 0; i < worker_threads.size(); i++) {
        worker_threads.at(i).join();
    }
    for (long unsigned int i = 0; i < hist_threads.size(); i++) {
        std::pair<int, double> temp(-1, -1.0);
        response_buffer.push((char*)&temp, sizeof(temp));
    }
    for (long unsigned int i = 0; i < hist_threads.size(); i++) {
        hist_threads.at(i).join();
    }
    
	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    std::cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    // quit and close all channels in TCP array
    MESSAGE_TYPE q = QUIT_MSG;
    for (long unsigned int i = 0; i < req_channels.size(); i++) {
        req_channels.at(i)->cwrite((char*)&q, sizeof(MESSAGE_TYPE));
        // if line below is commmented, when doing file transfers, worker threads leak
        delete req_channels.at(i); 
    }

	// quit and close control channel
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    std::cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	// wait(nullptr);
    return 0;
}