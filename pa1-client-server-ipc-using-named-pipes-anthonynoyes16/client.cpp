/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Anthony Noyes
	UIN: 83003968
	Date: 9/11/2022
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <iostream>
#include <string.h>
#include <chrono>

using namespace std::chrono;
using hrc = high_resolution_clock;
#define ELAPSED(st, en)			( duration_cast<duration<double>>(en - st).count() )
#define ELAPSED_MS(st, en)		( duration_cast<duration<double, std::milli>>(en - st).count() )
using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1; // patient id
	double t = -1; // time
	int e = -1; // ecg number
	int f = -1;
	int m = MAX_MESSAGE; // buffer cap (bytes)
	bool new_chan = false; // do we need to make a new channel
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				f = 1;
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	// give arguments for the server
	// server needs './server', '-m', '<val for -m arg>', 'NULL'
	
	pid_t pid = fork();
	if (pid < 0) {
		std::cerr << "Fork error" << std::endl;
	}
	else if (pid == 0) { // child process
		// convert m to char[] so we can pass it to terminal
		string m_str = to_string(m);
		vector <char> m_arr;
		for (long unsigned int i = 0; i < m_str.size(); i++) {
			m_arr.push_back(m_str[i]);
		}

		// run execvp using the server agruments.
		char* args[] = {const_cast<char*>("./server"), const_cast<char*>("-m"), &m_arr[0], nullptr};
		if (execvp(args[0], args) < 0) {
			perror("execvp");
			exit(QUIT_MSG);
		}
	}

    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	if (new_chan) {
		// send a new channel req to the server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
    	cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
		// create a variable to hold the name
		char buffer[MAX_MESSAGE]; 
		// cread the response from the server
		cont_chan.cread(buffer,sizeof(datamsg));
		string name(buffer);
		
		cout << name;
		// call the FIFORequest channel constructor with the name from the server
		FIFORequestChannel *new_channel = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE);

		// make sure to dynamically create the constructor using new keyword so it can be accessed outside the if statement
		// push the new channel into the vector
		channels.push_back(new_channel);
	}
	
	FIFORequestChannel chan = *(channels.back());

	if (p != -1 && t != -1 && e != -1) { // only run single datapoint if all three have been specified
		char buf[sizeof(datamsg)]; 
    	datamsg x(p, t, e);
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}	
	
	else if (p != -1) { // getting first 1000 lines
		t = 0.0;
		e = 1;
		datamsg x(p, t, e);
		char buf[sizeof(datamsg)]; 
		double reply;
		ofstream file;
		file.open("received/x1.csv");
		for (int i = 0; i < 1000; i++) {
			x.person = p;
			x.ecgno = 1;
			x.seconds = t;
			memcpy(buf, &x, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg));
			chan.cread(&reply, sizeof(double));
			file << t << ',' << reply << ',';
			x.ecgno = 2;
			memcpy(buf, &x, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg));
			chan.cread(&reply, sizeof(double));
			file << reply << '\n';
			t += 0.004;
		}
		file.close();
	}
	
	if (f != -1) {
		filemsg fm(0, 0);
		string fname = filename;
		
		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);  // I want the file length;

		int64_t filesize = 0;
		chan.cread(&filesize, sizeof(int64_t));

		// create the response buffer of size (m)
		char *buf3 = new char[m];

		ofstream file2;
		file2.open("received/"+ filename);

		// loop over the segments in the file filesize/buff capacity (m)
		// double elapsed = 0;
		for (int i = 0; i <= filesize/m; i += 1) {
			int remaining = filesize - m*i;
			filemsg* file_req = (filemsg*)buf2;
			file_req->offset = i*m; // set offset in the file
			file_req->length = m; // set the length, be careful of the last segment
			if (remaining < m) {
				file_req->length = remaining;
			}
			// send the req (buf2)
			// hrc::time_point st = hrc::now();        // get start time point
			chan.cwrite(buf2, len);
			// receive the response
			chan.cread(buf3, file_req->length);
			file2.write(buf3, file_req->length);
			// hrc::time_point en = hrc::now();        // get end time point
			// elapsed += ELAPSED_MS(st, en);
		}	
		file2.close();
		delete[] buf2;
		delete[] buf3;	
		// printf("Elapsed: %.2f ms\n", elapsed);
	}

	// closing all channels
	MESSAGE_TYPE msg = QUIT_MSG;
	for (long unsigned int i = 0; i < channels.size(); i++) {
		channels[i]->cwrite(&msg, sizeof(MESSAGE_TYPE));
	}

	// delete allocated memory
	if (new_chan) {
		delete channels.back();
	}
	waitpid(pid, nullptr, 0);

}
