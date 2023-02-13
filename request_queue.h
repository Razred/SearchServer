#pragma once
#include <deque>
#include "search_server.h"
#include "document.h"
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : server(&search_server) {
    }
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        ++seconds;
        if (seconds > min_in_day_){
            requests_.pop_front();
            --seconds;
        }
        QueryResult tmp_res;
        tmp_res.count = server->FindTopDocuments(raw_query, document_predicate);
        requests_.push_back(tmp_res);
        return server->FindTopDocuments(raw_query, document_predicate);
    }

    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);

    vector<Document> AddFindRequest(const string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        vector<Document> count;
    };
    const SearchServer *server;
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int seconds = 0;
};