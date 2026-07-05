#pragma once
#include <sys/types.h>

typedef void CURL;
typedef int CURLcode;
typedef int CURLoption;

#define CURLOPT_URL 10002
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_FOLLOWLOCATION 52

#define CURLE_OK 0
#define CURLE_COULDNT_CONNECT 7
#define CURLE_OPERATION_TIMEDOUT 28

extern "C" {
    CURL* curl_easy_init();
    CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...);
    CURLcode curl_easy_perform(CURL* curl);
    void curl_easy_cleanup(CURL* curl);
}
