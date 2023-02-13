#include "process_queries.h"


std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> res(queries.size());
    transform(std::execution::seq, queries.begin(), queries.end(), res.begin(), [search_server](string quer){return search_server.FindTopDocuments(quer);});
	return res;
}

std ::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<Document> documents;
	for (auto& doc : ProcessQueries(search_server, queries)) {
		documents.insert(documents.end(), doc.begin(), doc.end());
	}	
	return documents; 
}