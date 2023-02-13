#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <execution>
#include <string_view>
#include "document.h"
#include "read_input_functions.h"


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw invalid_argument("Some of stop words are invalid"s);
        }
    }

    explicit SearchServer(const string& stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    explicit SearchServer(const string_view stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    void AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings);

    template <typename ExecutionPolicy, typename DocumentPredicate>
    vector<Document> FindTopDocuments(ExecutionPolicy policy, string_view raw_query, DocumentStatus status, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(policy, query, document_predicate);

        sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename ExecytionPolicy>
    vector<Document> FindTopDocuments(ExecytionPolicy policy,string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
    }

    vector<Document> FindTopDocuments(string_view raw_query, DocumentStatus status) const;

    vector<Document> FindTopDocuments(string_view raw_query) const;

    int GetDocumentCount() const;

    int GetDocumentId(int index) const;

    template<typename ExecutionPolicy>
    tuple<vector<string_view>, DocumentStatus> MatchDocument(ExecutionPolicy policy,  string_view raw_query, int document_id) const {
        if (count(policy, document_ids_.begin(), document_ids_.end(), document_id) == 0)
            throw std::out_of_range("out_of_range");

        const auto query = ParseQuery(raw_query);

        if (!all_of(policy, query.plus_words.begin(), query.plus_words.end(), IsValidWord)) {
            throw invalid_argument("invalid_argument"s);
        }

        if (!all_of(policy,query.minus_words.begin(), query.minus_words.end(), IsValidWord)) {
            throw invalid_argument("invalid_argument"s);
        }

        vector<string_view> matched_words;
        for_each(policy, query.plus_words.begin(), query.plus_words.end(), [&](string_view word) {
            if (word_to_document_freqs_.at(string(word)).count(document_id)) {
                matched_words.push_back(word);
            }
        });
        for_each(policy, query.minus_words.begin(), query.minus_words.end(), [&](string_view word) {
            if (word_to_document_freqs_.at(string(word)).count(document_id)) {
                matched_words.clear();
            }
        });
        return {matched_words, documents_.at(document_id).status};
    }
    tuple<vector<string_view>, DocumentStatus> MatchDocument(string_view raw_query, int document_id) const;


    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy policy, int document_id) {
        documents_.erase(document_id);

        set<string> words;
        for (auto [word, map_]: word_to_document_freqs_) {
            if (map_.count(document_id) > 0)
                words.insert(word);
        }
        for_each(policy, words.begin(), words.end(), [&](string_view word) {
            if (word_to_document_freqs_.count(string(word)) > 0) {
                word_to_document_freqs_.at(string(word)).erase(document_id);
            }
            if (word_to_document_freqs_.at(string(word)).empty()) {
                word_to_document_freqs_.erase(string(word));
            }
        });

        document_ids_.erase(find(document_ids_.begin(), document_ids_.end(), document_id));
    }
    void RemoveDocument(int);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    bool IsStopWord(string_view word) const;

    static bool IsValidWord(string_view word);

    vector<string> SplitIntoWordsNoStop(string_view text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string_view text) const;

    struct Query {
        set<string_view> plus_words;
        set<string_view> minus_words;
    };

    Query ParseQuery(string_view text) const;

    double ComputeWordInverseDocumentFreq(const string& word) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    vector<Document> FindAllDocuments(ExecutionPolicy policy, const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for_each(policy, query.plus_words.begin(), query.plus_words.end(), [&](string_view word){
            if (word_to_document_freqs_.count(string(word)) > 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(string(word));
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(string(word))) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
                }
            }
        });

        for_each(policy, query.minus_words.begin(), query.minus_words.end(), [&](string_view word){
           if (word_to_document_freqs_.count(string(word)) > 0) {
               for (const auto [document_id, _] : word_to_document_freqs_.at(string(word))) {
                   document_to_relevance.erase(document_id);
               }
           }
        });

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(string(word)) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(string(word));
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(string(word))) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(string(word)) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(string(word))) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};