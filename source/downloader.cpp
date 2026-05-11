#ifdef ENABLE_DOWNLOAD

#include "downloader.hpp"

#ifdef __3DS__
#include <3ds.h>
#endif

#include "os.hpp"

#include <atomic>
#include <curl/curl.h>
#include <filesystem.hpp>
#include <fstream>
#include <iostream>
#include <log.hpp>
#include <mutex>
#include <sys/stat.h>

#ifdef __3DS__
static bool workerRunning = false;
#else
static std::atomic<bool> workerRunning(false);
#endif

static void N3DS_ProcessQueueThreadedWrapper(void *arg) {
    DownloadManager::processQueueThreaded();
}

bool DownloadManager::init() {
    if (isInitialized)
        return true;

    if (!OS::initWifi())
        return false;

    mtx.init();

    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (res != CURLE_OK) {
        Log::log(std::string("curl_global_init failed: ") +
                 curl_easy_strerror(res));
        return false;
    }

    isInitialized = true;
    return true;
}

void DownloadManager::deinit() {
    if (!isInitialized)
        return;

    join();

    curl_global_cleanup();

    OS::deInitWifi();

    isInitialized = false;
}

void DownloadManager::startThread() {

#ifdef __3DS__

    mtx.lock();

    if (workerRunning) {
        mtx.unlock();
        return;
    }

    workerRunning = true;

    mtx.unlock();

#else

    bool expected = false;

    if (!workerRunning.compare_exchange_strong(expected, true)) {
        return;
    }

#endif

    if (downloadThread.joinable()) {
        downloadThread.join();
    }

    downloadThread.create(
        N3DS_ProcessQueueThreadedWrapper,
        nullptr,
        8 * 1024,
        999,
        -2,
        "downloadThread"
    );
}

void DownloadManager::join() {
    if (downloadThread.joinable()) {
        downloadThread.join();
    }
}

void DownloadManager::addDownload(const std::string &url,
                                  const std::string &filepath) {

    {
        std::lock_guard lock(mtx);

        if (pendingMap.find(url) != pendingMap.end()) {
            return;
        }

        auto downloadedIt = downloadedMap.find(url);

        if (downloadedIt != downloadedMap.end()) {

            struct stat st;

            if (stat(filepath.c_str(), &st) == 0) {
                return;
            }

            downloadedMap.erase(downloadedIt);
        }

        auto item = std::make_shared<DownloadItem>();

        item->url = url;
        item->filepath = filepath;
        item->finished = false;
        item->success = false;

        pendingDownloads.push_back(item);
        pendingMap[url] = item;
    }

    startThread();
}

bool DownloadManager::isDownloading(const std::string &url) {

    std::lock_guard lock(mtx);

    return pendingMap.find(url) != pendingMap.end();
}

std::shared_ptr<DownloadItem>
DownloadManager::getDownloaded(const std::string &url) {

    std::lock_guard lock(mtx);

    auto it = downloadedMap.find(url);

    return (it != downloadedMap.end()) ? it->second : nullptr;
}

void DownloadManager::removeFromMemory(const std::string &url) {

    std::lock_guard lock(mtx);

    downloadedMap.erase(url);
}

void DownloadManager::processQueueThreaded() {

    while (true) {

        std::shared_ptr<DownloadItem> item;

        {
            std::lock_guard lock(mtx);

            if (pendingDownloads.empty()) {

#ifdef __3DS__
                workerRunning = false;
#else
                workerRunning.store(false);
#endif

                break;
            }

            item = pendingDownloads.front();
            pendingDownloads.erase(pendingDownloads.begin());
        }

        performDownload(item);
    }

    Log::log("DownloadManager: worker thread finished");
}

void DownloadManager::performDownload(std::shared_ptr<DownloadItem> item) {

    CURL *curl = curl_easy_init();

    if (!curl) {

        item->finished = true;
        item->success = false;
        item->error = "curl init failed";

        Log::logError("DownloadManager: curl init failed");

        return;
    }

    size_t lastSlash = item->filepath.find_last_of("/\\");

    if (lastSlash != std::string::npos) {

        const std::string dir =
            item->filepath.substr(0, lastSlash) + "/";

        auto err = FileSystem::createDirectory(dir.c_str());

        // Adjust depending on your API semantics
        if (err.has_value()) {

            item->finished = true;
            item->success = false;
            item->error = err.value();

            Log::logWarning(
                "Download failed: Could not create directory: " +
                err.value());

            curl_easy_cleanup(curl);

            return;
        }
    }

    std::ofstream outFile(item->filepath, std::ios::binary);

    if (!outFile.is_open()) {

        item->finished = true;
        item->success = false;
        item->error = "Failed to open file for writing";

        Log::log(
            "DownloadManager: ERROR - cannot open output file: " +
            item->filepath);

        curl_easy_cleanup(curl);

        return;
    }

    FileWriteData writeData{&outFile, 0};

    curl_easy_setopt(curl, CURLOPT_URL, item->url.c_str());

    curl_easy_setopt(curl,
                     CURLOPT_WRITEFUNCTION,
                     WriteCallback);

    curl_easy_setopt(curl,
                     CURLOPT_WRITEDATA,
                     &writeData);

    curl_easy_setopt(curl,
                     CURLOPT_FOLLOWLOCATION,
                     1L);

    curl_easy_setopt(curl,
                     CURLOPT_TIMEOUT,
                     30L);

    curl_easy_setopt(curl,
                     CURLOPT_CONNECTTIMEOUT,
                     10L);

    curl_easy_setopt(curl,
                     CURLOPT_FAILONERROR,
                     1L);

    curl_easy_setopt(curl,
                     CURLOPT_USERAGENT,
                     "SE-Downloader/1.0");

#ifndef __3DS__

    curl_easy_setopt(curl,
                     CURLOPT_SSL_VERIFYPEER,
                     1L);

    curl_easy_setopt(curl,
                     CURLOPT_SSL_VERIFYHOST,
                     2L);

#else

    // 3DS may not have proper cert bundles
    curl_easy_setopt(curl,
                     CURLOPT_SSL_VERIFYPEER,
                     0L);

    curl_easy_setopt(curl,
                     CURLOPT_SSL_VERIFYHOST,
                     0L);

#endif

    CURLcode res = curl_easy_perform(curl);

    long responseCode = 0;

    curl_easy_getinfo(curl,
                      CURLINFO_RESPONSE_CODE,
                      &responseCode);

    outFile.close();

    item->success =
        (res == CURLE_OK) &&
        (responseCode < 400);

    if (!item->success) {

        item->error = curl_easy_strerror(res);

        Log::log(
            "DownloadManager: ERROR - " +
            std::string(curl_easy_strerror(res)));

        std::remove(item->filepath.c_str());

    } else {

        Log::log(
            "DownloadManager: download complete (" +
            item->filepath + ")");
    }

    curl_easy_cleanup(curl);

    item->finished = true;

    {
        std::lock_guard lock(mtx);

        downloadedMap[item->url] = item;

        pendingMap.erase(item->url);
    }
}

size_t DownloadManager::WriteCallback(void *contents,
                                      size_t size,
                                      size_t nmemb,
                                      void *userp) {

    size_t totalSize = size * nmemb;

    FileWriteData *data =
        static_cast<FileWriteData *>(userp);

    data->file->write(
        static_cast<const char *>(contents),
        totalSize);

    if (!(*data->file)) {
        return 0;
    }

    data->bytesWritten += totalSize;

    return totalSize;
}

#endif
